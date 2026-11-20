# ELE-Kernel: native engineering track

ELE-Kernel is intended to become an independently maintained Linux-derived
kernel, not a claim of a new kernel written from scratch. The experimental
upstream target is Linux v7.3.5 (2026-11-17); Android 17's 6.18 GKI family is
the required Android compatibility target; the existing AOSP product remains
unchanged. Neither target has been built here, and newer version numbers
alone are not evidence of superior performance. Do not assume an Android 16
vendor image can boot with either kernel.

## First implemented subsystem: memory pressure classification

The GPL-2.0-only native pressure module samples estimated available system
memory at one-second intervals and exposes a coherent read-only snapshot
at /sys/kernel/ele_kernel/pressure. It escalates at 15% and 7% available
memory and recovers only after three consecutive samples at 10% or 20%
depending on the current level. These are experimental thresholds, not
measured mobile-device optima. The policy does not kill processes or modify
reclaim, scheduling, Android LMKD, or sysctls. Global estimated available
memory is not a substitute for PSI or memory-cgroup pressure.

Build against a configured, compiled Linux source tree or matching headers:

    make -C /path/to/prepared/kernel M="$PWD/kernel/native/pressure" modules
    sh tests/host-ele-kernel.sh

Module loading must be done only on an isolated test machine running the
matching kernel; never load an experimental module on a production phone.
Android integration additionally requires KMI/export, module signing,
SELinux, and vendor-policy validation. This is not included in the image.

## Evidence, 2026-11-20

The C policy assertions passed with cc -std=c11 -Wall -Wextra -Werror.
The module compiled and linked into ele_pressure.ko against available
Debian Linux 6.12.107 x86-64 headers using kbuild. The full 7.3.5 source,
6.18 Android build tree, matching running test kernel and physical phones
were unavailable. No module load, kernel boot, battery measurement,
performance increase, or device integration was verified.

## Development order

1. Establish a pinned upstream GPL source fork and source/release provenance.
2. Compile this subsystem on v7.3.5 and ARM64; separately assess 6.18 GKI
   exported-symbol and module-signing requirements.
3. Boot virtual test kernels, test module lifetime and concurrency, measure
   idle wakeups and memory pressure transitions; replace polling if needed.
4. Extend native work to PSI and cgroup-aware reclamation, scheduling,
   energy and thermal management, hardening and driver reliability.
5. Compare identical workloads against baseline kernels before promoting
   changes. Preserve upstream security updates and hardware-driver support.

No device flash or change of the AOSP baseline is authorized by this
experimental module alone.
