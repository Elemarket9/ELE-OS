# Building independent ELE-Kernel

The standalone track pins the exact upstream Linux v7.3.5 commit in
upstream.json, applies ELE code to a disposable git worktree, and builds
a kernel named 7.3.5-eleos-dirty. The `-dirty` suffix identifies the local
ELE source overlay applied to the pinned upstream worktree; the manifest records
the clean upstream commit separately. It does not modify the AOSP product, install a
kernel on the host, or build Android's separate ACK 6.18 track.

On a Linux build machine, install git, Python 3.11+, make, GCC, binutils,
bc, bison, flex, libelf-dev, libssl-dev, dwarves, cpio, rsync, and QEMU
(qemu-system-x86). ARM64 cross builds also need a matching aarch64 compiler
and binutils (on Debian/Ubuntu gcc-aarch64-linux-gnu).

```sh
git clone --depth 1 --branch v7.3.5 https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git linux-ele-upstream
python3 kernel/native/build.py linux-ele-upstream "$HOME/ele-native-x86" --jobs 4
python3 kernel/native/smoke.py "$HOME/ele-native-x86" "$HOME/ele-native-x86-boot.log"
# On x86 Linux with ARM64 cross toolchain:
python3 kernel/native/build.py linux-ele-upstream "$HOME/ele-native-arm64" --arch arm64 --cross-compile aarch64-linux-gnu- --jobs 4
```

The source must match the pinned commit and be clean. Use a new output
directory. The builder compiles the memory pressure driver into the
kernel, requires selected hardening and pressure accounting options
after Kconfig resolves dependencies, and verifies the configuration
embedded in the final image. The distribution includes the image, modules,
vmlinux, System.map, config, upstream and input provenance and artifact hashes.
This is a development build, not a signed release or reproducible-build proof.
The pinned source and ELE overlay together are needed for GPL distribution;
retain the upstream source and exact ELE revision for any binary release.

The virtual smoke test boots the exact hashed image, mounts proc/sysfs,
checks the ELE telemetry ABI, PSI and default BPF restriction, and requests
guest poweroff. A timeout, missing marker or QEMU failure is a failure.
On 2026-11-20, GitHub Actions compiled the pinned Linux v7.3.5 ELE overlay for
x86_64 and booted the exact hashed image in QEMU. Smoke checks covered pressure/PSI,
unprivileged-BPF default policy, RWX mmap/mprotect denial, and unsigned module
rejection. This does not verify ARM64, physical devices, Android compatibility,
production signing, performance, pressure-stress thresholds, or writable aliases.

The native track remains experimental: CPU scheduling, reclaim policy,
cgroup pressure, GPU/NPU, device drivers, power and thermal integration
need their own implementation and baseline comparisons. Upstream security
updates must be evaluated and repinned with tests.

## Security enforcement

ELE-MemSafe denies simultaneous writable/executable mappings via the common
mmap and mprotect security dispatch path. It does not prevent writable aliases,
RW-to-RX transitions, or in-kernel compromise. The strict profile may break
legacy apps or JITs requiring RWX; Android compatibility is not established.
The build requires signed modules, slab hardening, fortified copies, seccomp,
zero-on-allocation and zero-on-free, separate slab buckets, and hardened
usercopy. Zero-on-free wipes freed page/slab contents to reduce stale-data
exposure; upstream warns of a typical 3–5% performance cost, which has not
been measured on ELE-OS devices. The QEMU smoke runs denial tests, but does
not directly inspect freed allocator memory.
The kernel-generated signing key is development-only and is not distributed;
production keys, verified boot and rollback protection are separate work.
