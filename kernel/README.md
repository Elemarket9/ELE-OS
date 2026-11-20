# ELE-OS kernel engineering: initial experimental target

This is an actual Kleaf build entry point, not a replacement Linux kernel.
It keeps the Android 16 overlay unchanged and targets Android Common Kernel
6.12 for Cuttlefish. No GPL kernel source is copied into this Apache-2.0
overlay. A Treble GSI cannot replace a handset's vendor kernel by itself.
No handset, emulator boot, KMI result, or performance improvement is claimed.

## Build a development kernel

Use a Linux kernel build host with Python 3.11+, git, Google's repo launcher,
and sufficient disk/RAM for the upstream source and build. Start with a
fresh, dedicated workspace outside the ELE-OS checkout, and no other
concurrent build in that workspace.

```sh
mkdir -p "$HOME/ele-ack"
cd "$HOME/ele-ack"
repo init -u https://android.googlesource.com/kernel/manifest -b common-android16-6.12
repo sync -c -j4
python3 /path/to/ELE-OS/kernel/build.py "$PWD" "$HOME/ele-kernel-x86"
# Optional ARM64 virtual-device candidate (not a handset boot image):
python3 /path/to/ELE-OS/kernel/build.py "$PWD" "$HOME/ele-kernel-arm64" --arch aarch64
```

The output must be new and outside the kernel workspace. The script rejects
dirty source projects and records the resolved source revisions, applied
fragment, embedded config, artifact hashes and target. This is provenance,
not a signed attestation or a claim of byte-for-byte reproducibility. The
initial sync follows an upstream branch; preserve the recorded pinned
manifest and all referenced revisions for later reproduction.

The experimental config makes unprivileged BPF disabled by default on the
kernel, while preserving BPF for privileged Android services. It does not
prevent a privileged process from changing the sysctl. The final binary
must embed this setting; the script fails if it cannot extract and validate
the config. It checks the image header and a nonempty ramdisk, not their
bootability. A successful build is not a production approval.

## Boot gate

On a supported Cuttlefish host with a matching built Android system image,
use the upstream kernel development procedure with the new dist artifacts:

```sh
cvd create -kernel_path="$HOME/ele-kernel-x86/bzImage" \
  -initramfs_path="$HOME/ele-kernel-x86/initramfs.img"
adb -s SERIAL shell uname -r
adb -s SERIAL shell cat /proc/sys/kernel/unprivileged_bpf_disabled
```

Substitute the actual serial. Verify boot completion, BPF loader, SELinux
enforcing, networking, binder, storage, suspend/resume and module
compatibility. Check that the sysctl is nonzero; an absent or inaccessible
value is not a pass. Run the same checks on an unmodified baseline. Record
the exact artifacts, device serial and results under docs/evidence before
claiming boot or compatibility. Do not flash a phone from this dist.

## Host tests

`sh tests/host-kernel.sh` tests config, manifest parsing, image-header
validation and output safeguards. It is included in tests/run.sh through
the existing host-test discovery. It does not compile Linux.

## Upstream references

- https://source.android.com/docs/devices/cuttlefish/kernel-dev
- https://source.android.com/docs/setup/build/building-kernels
- https://android.googlesource.com/kernel/build/+/refs/heads/main/kleaf/docs/kernel_config.md

Next kernel milestones: real artifact build and Cuttlefish boot; KMI/module
validation; scheduler and PSI baselines; low-RAM reclaim and zram experiments;
security and kselftest/KUnit regressions; ARM64 device-specific integration.
Choose device branches only after confirming bootloader, vendor drivers and
firmware support. Do not replace measured policies with untested scheduler
or memory code.
