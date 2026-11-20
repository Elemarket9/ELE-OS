# 2026-11-20: first reproducible boot attempt

## Result

The ELE-OS image was **not built or booted**. The build host could not run
Cuttlefish and was far below AOSP build requirements. No emulator or
physical-device evidence was produced in this session.

## Host

| Check | Observed | Needed |
| --- | --- | --- |
| OS / kernel | Ubuntu 24.04.5 LTS, 6.18.44 (Firecracker guest) | x86-64 Linux |
| `/dev/kvm` | absent | present, user in `kvm` group (Cuttlefish) |
| CPU virtualization flags | no `vmx`/`svm`; `hypervisor` flag only | `vmx` or `svm` exposed (nested virtualization) |
| CPU | 2 vCPU, Intel Xeon @ 2.10 GHz | ≥ 6 cores is workable, 16+ typical |
| RAM | 8 GB (7.3 GB available), no swap | ≥ 32 GB recommended for `m` |
| Free disk | 28 GB | ≥ 400 GB for checkout + build |
| Network | `android.googlesource.com` and `dl.google.com` reachable | reachable |

The missing KVM device alone prevents `launch_cvd`. RAM and disk prevent a
full `repo sync` and `m` before reaching the emulator step.

## Branch state inspected

`main` at `670020397ed26c5b98c7d12b3f03a986d7812686` (no newer commits).
Product registration: `AndroidProducts.mk` → `ele_os_cf_x86_64.mk`, which
inherits `device/google/cuttlefish/vsoc_x86_64_only/phone/aosp_cf.mk` and adds
`ele-status` and `ElePeripherals`. Lunch target
`ele_os_cf_x86_64-trunk_staging-userdebug`. The README pins
`android-16.0.0_r1`.

`sh tests/run.sh` passed on this host (owner proof, reset fixture, replay,
status snapshot).

## Commands for a capable host

Run these unchanged on a host that meets the table above. Record the output
of each in a new file in this directory.

```sh
ls -l /dev/kvm && grep -cE 'vmx|svm' /proc/cpuinfo && free -g && df -h .
repo init -u https://android.googlesource.com/platform/manifest -b android-16.0.0_r1
repo sync -c -j8
git clone https://github.com/Elemarket9/ELE-OS.git device/ele/os
source build/envsetup.sh
lunch ele_os_cf_x86_64-trunk_staging-userdebug
m
launch_cvd --daemon
adb devices                       # note the Cuttlefish serial, e.g. 0.0.0.0:6520
sh device/ele/os/tests/check-boot.sh SERIAL
adb -s SERIAL shell am start -W -n org.eleos.peripherals/.DevicesActivity
adb -s SERIAL shell cmd locale set-app-locales org.eleos.peripherals --locales ar-XB
adb -s SERIAL shell am start -W -n org.eleos.peripherals/.DevicesActivity
adb -s SERIAL exec-out screencap -p > ar-XB.png
```

The `--pseudo-localize` flag is always applied to app resources by Soong
(`build/soong/java/aar.go`), so en-XA and ar-XB are present in userdebug
builds if the product locale list keeps them.
