# ELE-OS
Open-source mobile OS research for affordable phones, focused on lightweight performance, secure updates, and owner-controlled lost-device recovery.

## First development target

ELE-OS defines an x86-64 Cuttlefish phone product based on AOSP. It includes
`ele-status`, a local shell command for recording the build fingerprint,
system and vendor patch dates, boot state, and a memory snapshot. These values
come from the running system. They do not establish patch completeness or
provide remote attestation.

## Build and boot

Use an x86-64 Linux AOSP build host with KVM and the
[Cuttlefish host prerequisites](https://source.android.com/docs/devices/cuttlefish/get-started).
The initial integration baseline is the fixed Android 16 tag below. This is
an emulator development build using upstream development keys.

From a new AOSP workspace with Google's repo tool installed:

```sh
repo init -u https://android.googlesource.com/platform/manifest -b android-16.0.0_r1
repo sync -c
git clone https://github.com/Elemarket9/ELE-OS.git device/ele/os
source build/envsetup.sh
lunch ele_os_cf_x86_64-trunk_staging-userdebug
m
launch_cvd --daemon
adb wait-for-device
adb shell getprop sys.boot_completed
adb shell /product/bin/ele-status
```

A successful boot reports `1` for `sys.boot_completed`. The snapshot should
report `product=ele_os_cf_x86_64`. Run the same workload on stock Cuttlefish
before drawing performance conclusions.

## Host test

Run `sh tests/status.sh` in this repository. It exercises the shipped shell
code on Linux with fixture Android properties and real kernel memory counters.
Full image compilation and emulator boot remain unverified. Lost Mode, OTA
delivery, and physical handset support are future milestones.
