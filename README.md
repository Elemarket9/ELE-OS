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

## Owner authorization after reset

The target behavior is that erasing user data preserves the ownership binding.
Reactivation requires proof from an enrolled owner or an explicitly authorized
recovery process. The prototype uses a public key as the ownership identifier.
The verification message contains no owner name, phone number or email address.

The first component, `libele_owner_proof`, verifies an Ed25519 signature over
the exact ASCII bytes of `ELE-OS/reactivate/v1` (no terminator), a 32-byte
device identifier, then a fresh 32-byte challenge. Build the module in AOSP
with `m libele_owner_proof`. For a Linux host with a C++ compiler and OpenSSL
development headers, run:

```sh
c++ -std=c++17 -Wall -Wextra -Werror -Iactivation activation/owner_proof.cpp tests/owner_proof.cpp -lcrypto -o /tmp/ele-owner-test
/tmp/ele-owner-test
```

This component checks a signature only. Device enforcement still requires
OEM-supported verified boot, a trusted owner record outside wiped userdata,
fresh challenges from a secure random source, atomic one-time consumption,
and a mandatory activation check before normal setup. Missing or corrupt
enrollment evidence must never silently turn an enrolled handset into a new
handset. Owner transfer and loss of the recovery credential need explicit
recovery flows. No device is currently protected by this prototype.

Performance comparisons will use the same handset, workload, build variant,
and thermal conditions, recording cold app launch time, boot-to-interactive
time, and background notification delivery alongside battery use. Android
compatibility checks must include calls, camera, notifications, and app
installation after authorized reactivation.

### Setup session and reset exercise

The reactivation session now issues a random challenge bound to the enrolled
device and owner key. Starting another challenge replaces the old one. The
caller receives at most one successful grant for a challenge, even when
requests race. Recreating the session drops all outstanding challenges.

Run `sh tests/run.sh` with a C++17 compiler and OpenSSL development headers.
The integration test writes a fixture enrollment outside a temporary userdata
directory, removes the userdata directory, reloads the enrollment, and exercises
owner proof and replay rejection. The fixture models persistence only. A service
must still authenticate the enrollment record, enforce one live setup session
per device, handle crashes when committing activation, and gate every normal
setup path. Recovery, hardware-backed storage, and bootloader integration remain
device work.

After launching the ELE-OS Cuttlefish image, run
`sh tests/check-boot.sh SERIAL` from a Linux host with adb and GNU timeout.
It waits for boot completion and checks the ELE-OS product and memory report.

## External cameras and satellite connectivity

The External devices app previews Camera2 external cameras and offers a picker
when multiple cameras are exposed. On a handset, USB host mode, UVC kernel
support and a configured external camera provider are prerequisites. Test the
exact Canon model and USB mode, or a compatible HDMI-to-UVC capture adapter.
A camera exposing only PTP needs a separate import/tethering integration.

The app also displays whether Android identifies the default data network as
satellite. A satellite terminal connected over Wi-Fi can appear as ordinary
Wi-Fi. SMS-only satellite service need not create a default IP route.
Direct handset messaging requires supported modem firmware, provisioning,
and a participating network. Live device, camera and carrier validation is
pending. For comparison with stock firmware, record preview startup latency,
frame drops, unplug/reconnect behavior, and satellite message delivery receipts
on matching hardware and service plans.
