#!/usr/bin/env python3
"""Build a pinned Linux-derived ELE-Kernel with the native driver built in."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
PIN = json.loads((HERE / "upstream.json").read_text())
ARCHES = {"x86_64": ("x86/boot/bzImage", "bzImage"),
          "arm64": ("arm64/boot/Image", "Image")}
COMMON = ("ELE_PRESSURE", "MODULES", "BLK_DEV_INITRD", "DEVTMPFS",
          "PROC_FS", "SYSFS", "PRINTK", "CGROUPS", "MEMCG", "PSI",
          "BPF_SYSCALL", "BPF_UNPRIV_DEFAULT_OFF", "IKCONFIG", "IKCONFIG_PROC",
          "STACKPROTECTOR_STRONG", "STRICT_KERNEL_RWX", "STRICT_MODULE_RWX",
          "RANDOMIZE_BASE")
ARCH_FLAGS = {"x86_64": ("SERIAL_8250", "SERIAL_8250_CONSOLE"),
              "arm64": ("SERIAL_AMBA_PL011", "SERIAL_AMBA_PL011_CONSOLE")}


def run(*args, cwd=None):
    subprocess.run([str(x) for x in args], cwd=cwd, check=True)


def capture(*args, cwd=None):
    return subprocess.check_output([str(x) for x in args], cwd=cwd, text=True).strip()


def sha(path):
    with path.open("rb") as f:
        return hashlib.file_digest(f, "sha256").hexdigest()


def verify_config(text, arch):
    values = {}
    for line in text.splitlines():
        match = re.fullmatch(r"CONFIG_(\w+)=(.*)", line)
        if match:
            name, value = match.groups()
        else:
            match = re.fullmatch(r"# CONFIG_(\w+) is not set", line)
            if not match:
                continue
            name, value = match[1], "n"
        if name in values:
            raise ValueError("duplicate config: " + name)
        values[name] = value
    required = {name: "y" for name in COMMON + ARCH_FLAGS[arch]}
    required.update(LOCALVERSION='"' + PIN["localversion"] + '"',
                    LOCALVERSION_AUTO="n", PSI_DEFAULT_DISABLED="n")
    bad = [x for x, v in required.items() if values.get(x) != v]
    if bad:
        raise ValueError("missing/incorrect kernel options: " + ", ".join(bad))


def check_source(source):
    if capture("git", "rev-parse", "HEAD", cwd=source) != PIN["commit"]:
        raise ValueError("source HEAD does not match pinned upstream commit")
    if capture("git", "status", "--porcelain", "--untracked-files=all", cwd=source):
        raise ValueError("source checkout must be clean")


def integrate(source):
    target = source / "drivers/ele"
    if target.exists():
        raise ValueError("ELE driver directory already exists")
    kconfig = source / "drivers/Kconfig"
    makefile = source / "drivers/Makefile"
    original = kconfig.read_text()
    marker = 'source "drivers/ele/Kconfig"'
    if marker in original or original.count("\nendmenu") < 1:
        raise ValueError("unexpected upstream drivers/Kconfig structure")
    before, after = original.rsplit("\nendmenu", 1)
    target.mkdir()
    for name in ("ele_pressure.c", "ele_pressure_policy.h"):
        shutil.copyfile(HERE / "pressure" / name, target / name)
    shutil.copyfile(HERE / "Kconfig", target / "Kconfig")
    (target / "Makefile").write_text(
        "# SPDX-License-Identifier: GPL-2.0-only\nobj-$(CONFIG_ELE_PRESSURE) += ele_pressure.o\n")
    kconfig.write_text(before + "\n" + marker + "\nendmenu" + after)
    with makefile.open("a") as f:
        f.write("\nobj-$(CONFIG_ELE_PRESSURE) += ele/\n")


def build(source, output, arch, jobs, cross):
    if output.exists() or output.is_relative_to(source):
        raise ValueError("output must be new and outside source")
    if jobs < 1:
        raise ValueError("jobs must be positive")
    if cross and not re.fullmatch(r"[\w./+-]+", cross):
        raise ValueError("invalid cross compiler prefix")
    check_source(source)
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="ele-native-", dir=output.parent) as tmp:
        root = Path(tmp)
        work = root / "src"
        obj = root / "obj"
        stage = root / "dist"
        stage.mkdir()
        run("git", "worktree", "add", "--detach", work, PIN["commit"], cwd=source)
        try:
            integrate(work)
            make = ["make", "-C", work, f"O={obj}", f"ARCH={arch}", "LOCALVERSION="]
            if cross:
                make.append(f"CROSS_COMPILE={cross}")
            run(*make, "defconfig")
            config = [str(work / "scripts/config"), "--file", str(obj / ".config")]
            for name in COMMON + ARCH_FLAGS[arch]:
                config += ["--enable", name]
            config += ["--set-str", "LOCALVERSION", PIN["localversion"],
                       "--disable", "LOCALVERSION_AUTO", "--disable", "PSI_DEFAULT_DISABLED"]
            run(*config)
            run(*make, "olddefconfig")
            verify_config((obj / ".config").read_text(), arch)
            release = capture(*make, "-s", "kernelrelease")
            if release != PIN["version"] + PIN["localversion"]:
                raise ValueError("unexpected kernel release: " + release)
            run(*make, f"-j{jobs}", ARCHES[arch][1], "modules")
            run(*make, f"INSTALL_MOD_PATH={stage}", "modules_install")
            # Never package links pointing to the temporary source/build tree.
            for name in ("build", "source"):
                link = stage / "lib/modules" / release / name
                if link.is_symlink():
                    link.unlink()
            relative, image_name = ARCHES[arch]
            image = stage / image_name
            shutil.copyfile(obj / "arch" / relative, image)
            if image.stat().st_size < 4096:
                raise ValueError("truncated kernel image")
            with image.open("rb") as f:
                f.seek(0x202 if arch == "x86_64" else 0x38)
                if f.read(4) != (b"HdrS" if arch == "x86_64" else b"ARM\x64"):
                    raise ValueError("invalid kernel image header")
            embedded = capture("bash", work / "scripts/extract-ikconfig", image)
            verify_config(embedded, arch)
            (stage / "kernel.config").write_text(embedded + "\n")
            shutil.copyfile(obj / "System.map", stage / "System.map")
            shutil.copyfile(obj / "vmlinux", stage / "vmlinux")
            shutil.copyfile(work / "COPYING", stage / "COPYING")
            inputs = {name: sha(HERE / name) for name in
                      ("upstream.json", "build.py", "Kconfig",
                       "pressure/ele_pressure.c", "pressure/ele_pressure_policy.h")}
            files = {str(p.relative_to(stage)): sha(p) for p in sorted(stage.rglob("*"))
                     if p.is_file() and not p.is_symlink()}
            (stage / "ele-build.json").write_text(json.dumps({
                "upstream": PIN, "release": release, "arch": arch,
                "input_sha256": inputs, "artifact_sha256": files,
                "boot_verified": False
            }, indent=2) + "\n")
            if output.exists():
                raise ValueError("output appeared during build")
            stage.rename(output)
        finally:
            run("git", "worktree", "remove", "--force", work, cwd=source)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("source", type=Path)
    p.add_argument("output", type=Path)
    p.add_argument("--arch", choices=ARCHES, default="x86_64")
    p.add_argument("--jobs", type=int, default=min(4, os.cpu_count() or 1))
    p.add_argument("--cross-compile", default="")
    a = p.parse_args()
    try:
        build(a.source.resolve(), a.output.resolve(), a.arch, a.jobs, a.cross_compile)
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        p.exit(1, f"ELE-Kernel build failed: {exc}\n")
    print("ELE-Kernel artifacts generated; boot remains unverified.")


if __name__ == "__main__":
    main()
