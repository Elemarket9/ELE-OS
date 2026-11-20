#!/usr/bin/env python3
"""Build an experimental ELE-OS Android Common Kernel; never flash devices."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile
import xml.etree.ElementTree as ET

HERE = Path(__file__).resolve().parent
TARGETS = {"x86_64": "bzImage", "aarch64": "Image"}
BRANCH = "common-android16-6.12"
REQUIRED = ("CONFIG_BPF_SYSCALL", "CONFIG_BPF_UNPRIV_DEFAULT_OFF",
            "CONFIG_IKCONFIG", "CONFIG_IKCONFIG_PROC")


def run(args, cwd):
    subprocess.run(args, cwd=cwd, check=True)


def capture(args, cwd):
    return subprocess.check_output(args, cwd=cwd)


def check_config(config):
    values = {}
    for line in config.splitlines():
        match = re.fullmatch(r"(CONFIG_[A-Z0-9_]+)=(.*)", line)
        if match:
            key, value = match.groups()
        else:
            match = re.fullmatch(r"# (CONFIG_[A-Z0-9_]+) is not set", line)
            if not match:
                continue
            key, value = match[1], "n"
        if key in values:
            raise ValueError(f"duplicate config: {key}")
        values[key] = value
    missing = [key for key in REQUIRED if values.get(key) != "y"]
    if missing:
        raise ValueError("kernel hardening config missing: " + ", ".join(missing))


def check_manifest(raw):
    root = ET.fromstring(raw)
    if root.tag != "manifest":
        raise ValueError("not a repo manifest")
    projects = root.findall(".//project")
    if not projects or not any(p.get("path", p.get("name")) == "common" for p in projects):
        raise ValueError("missing common kernel project")
    for p in projects:
        if not re.fullmatch(r"[0-9a-f]{40}", p.get("revision", "")):
            raise ValueError("manifest has unpinned project: " + str(p.get("name")))


def digest(path):
    with path.open("rb") as f:
        return hashlib.file_digest(f, "sha256").hexdigest()


def verify_image(path, arch):
    with path.open("rb") as f:
        f.seek(0x202 if arch == "x86_64" else 0x38)
        magic = f.read(4)
    if magic != (b"HdrS" if arch == "x86_64" else b"ARM\x64"):
        raise ValueError("wrong kernel image format: " + str(path))


def build(workspace, output, arch):
    if not (workspace / ".repo").is_dir():
        raise ValueError("expected a synced Android kernel repo workspace")
    if output.exists():
        raise ValueError("output must not already exist")
    if output.is_relative_to(workspace):
        raise ValueError("output must be outside the kernel workspace")
    makefile = (workspace / "common/Makefile").read_text()
    for name, value in (("VERSION", "6"), ("PATCHLEVEL", "12")):
        if not re.search(rf"^{name}\s*=\s*{value}\s*$", makefile, re.M):
            raise ValueError("expected Linux 6.12")
    # Source hashes are meaningful only for a clean checkout.
    run(["repo", "forall", "-e", "-c", 'test -z "$(git status --porcelain)"'], workspace)
    manifest = capture(["repo", "manifest", "-r"], workspace)
    check_manifest(manifest)
    package = workspace / "eleos_kernel_config"
    package.mkdir()  # Never overwrite an existing workspace package.
    try:
        fragment = (HERE / "eleos_defconfig").read_bytes()
        (package / "eleos_defconfig").write_bytes(fragment)
        (package / "BUILD.bazel").write_text('exports_files(["eleos_defconfig"])\n')
        output.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="ele-kernel-", dir=output.parent) as temp:
            stage = Path(temp) / "dist"
            stage.mkdir()
            target = f"//common-modules/virtual-device:virtual_device_{arch}_dist"
            run(["tools/bazel", "run", "--defconfig_fragment=//eleos_kernel_config:eleos_defconfig",
                 target, "--", f"--destdir={stage}"], workspace)
            image = stage / TARGETS[arch]
            ramdisk = stage / "initramfs.img"
            for artifact in (image, ramdisk):
                if not artifact.is_file() or artifact.is_symlink() or not artifact.stat().st_size:
                    raise ValueError("missing regular artifact: " + str(artifact))
            verify_image(image, arch)
            config = capture(["bash", "common/scripts/extract-ikconfig", str(image)], workspace)
            check_config(config.decode("ascii"))
            if manifest != capture(["repo", "manifest", "-r"], workspace):
                raise ValueError("source revisions changed during build")
            run(["repo", "forall", "-e", "-c", 'test -z "$(git status --porcelain)"'], workspace)
            (stage / "eleos-source-manifest.xml").write_bytes(manifest)
            (stage / "eleos-kernel.config").write_bytes(config)
            (stage / "eleos-applied-defconfig").write_bytes(fragment)
            files = {str(p.relative_to(stage)): digest(p) for p in sorted(stage.rglob("*"))
                     if p.is_file() and not p.is_symlink()}
            (stage / "eleos-build.json").write_text(json.dumps({
                "arch": arch, "target": target, "kernel_family": BRANCH,
                "profile": "experimental-unprivileged-bpf-default-off",
                "device_boot_verified": False, "sha256": files}, indent=2) + "\n")
            if output.exists():
                raise ValueError("output appeared during build")
            stage.rename(output)
    finally:
        for name in ("eleos_defconfig", "BUILD.bazel"):
            (package / name).unlink(missing_ok=True)
        package.rmdir()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("workspace", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--arch", choices=TARGETS, default="x86_64")
    args = parser.parse_args()
    try:
        build(args.workspace.resolve(), args.output.resolve(), args.arch)
    except (OSError, ValueError, ET.ParseError, subprocess.CalledProcessError) as exc:
        parser.exit(1, f"kernel build failed: {exc}\n")
    print("Kernel artifacts verified; device boot NOT verified.")


if __name__ == "__main__":
    main()
