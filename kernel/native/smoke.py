#!/usr/bin/env python3
"""Isolated x86-64 QEMU boot test; does not boot Android or a real handset."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent


def newc_entry(name, mode, data=b"", major=0, minor=0, inode=1):
    name_bytes = name.encode("ascii") + b"\0"
    fields = (inode, mode, 0, 0, 1, 0, len(data), 0, 0, major, minor,
              len(name_bytes), 0)
    head = b"070701" + b"".join(f"{x:08x}".encode("ascii") for x in fields)
    first = head + name_bytes
    return first + b"\0" * (-len(first) % 4) + data + b"\0" * (-len(data) % 4)


def initramfs(init):
    return b"".join(newc_entry(name, mode, data, major, minor, i)
                    for i, (name, mode, data, major, minor) in enumerate((
        ("dev", 0o40755, b"", 0, 0),
        ("dev/console", 0o20600, b"", 5, 1),
        ("proc", 0o40755, b"", 0, 0),
        ("sys", 0o40755, b"", 0, 0),
        ("init", 0o100755, init, 0, 0),
        ("TRAILER!!!", 0, b"", 0, 0)), 1))


def verify_artifact(dist):
    metadata = json.loads((dist / "ele-build.json").read_text())
    if metadata["arch"] != "x86_64":
        raise ValueError("smoke test supports x86_64 only")
    image = dist / "bzImage"
    with image.open("rb") as f:
        digest = hashlib.file_digest(f, "sha256").hexdigest()
    if digest != metadata["artifact_sha256"]["bzImage"]:
        raise ValueError("kernel artifact hash mismatch")
    return image


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("dist", type=Path)
    p.add_argument("log", type=Path)
    p.add_argument("--timeout", type=int, default=180)
    a = p.parse_args()
    if a.log.exists() or a.timeout < 1:
        p.error("log must be new and timeout positive")
    try:
        image = verify_artifact(a.dist.resolve())
        with tempfile.TemporaryDirectory(prefix="ele-smoke-") as t:
            root = Path(t)
            subprocess.run(["cc", "-static", "-O2", "-Wall", "-Wextra", "-Werror",
                            str(HERE / "smoke_init.c"), "-o", str(root / "init")], check=True)
            (root / "initramfs").write_bytes(initramfs((root / "init").read_bytes()))
            with a.log.open("xb") as log:
                result = subprocess.run(["qemu-system-x86_64", "-accel", "tcg",
                    "-cpu", "max", "-m", "1024", "-smp", "2", "-nographic",
                    "-no-reboot", "-kernel", str(image), "-initrd", str(root / "initramfs"),
                    "-append", "console=ttyS0 panic=-1 rdinit=/init"],
                    stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT,
                    timeout=a.timeout)
            text = a.log.read_text(errors="replace")
            if (result.returncode or "ELE_KERNEL_SMOKE_FAIL" in text or
                    not any(line.strip() == "ELE_KERNEL_SMOKE_PASS" for line in text.splitlines())):
                raise ValueError("kernel smoke failed; inspect log")
    except (OSError, KeyError, ValueError, subprocess.SubprocessError) as exc:
        p.exit(1, f"ELE-Kernel smoke failed: {exc}\n")
    print("ELE-Kernel native virtual boot smoke passed; Android/device boot unverified.")


if __name__ == "__main__":
    main()
