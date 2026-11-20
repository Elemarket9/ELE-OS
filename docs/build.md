# Build machines

Two different machines are involved, and they are easy to mix up.

- **This repository's host tests** compile the owner-proof library and run
  `ele-status` against fixture properties. They do not build Android.
- **An AOSP build host** checks out the full platform and compiles a system
  image. Cuttlefish additionally needs KVM.

This repository has host-test results. It does not have a recorded AOSP build
or a recorded Cuttlefish boot. Treat any blog post or chat log that says
otherwise as unofficial until an evidence note lands in
[docs/evidence/](evidence/README.md).

## Host tests

Needs:

- A 64-bit Linux host. `tests/status.sh` reads `/proc/meminfo` and puts a
  fake `getprop` on `PATH`.
- A C++17 compiler with the C++ standard library headers, and OpenSSL's
  libcrypto (headers and library). Debian and Ubuntu package that as
  `g++` and `libssl-dev`.
- POSIX `sh`.

`tests/run.sh` uses `c++` unless `CXX` is set. On a machine where `c++` is
Clang and the libstdc++ headers are installed only for GCC, point `CXX` at
`g++`:

```sh
CXX=g++ sh tests/run.sh
```

The script builds `tests/owner_proof.cpp` and `tests/reactivation.cpp`, runs
the reset fixture, and runs `tests/status.sh`. Success prints the reset-fixture
line and `Host snapshot checks passed`.

On 2026-11-20, `CXX=g++ sh tests/run.sh` passed with g++ 13.3.0 and OpenSSL
3.0.13. That run does not boot an image, does not prove a handset, and does
not exercise the External devices app.

`tests/check-boot.sh SERIAL` is not a host test. It needs `adb`, GNU
`timeout`, and a running emulator whose serial you pass explicitly. It waits
until `sys.boot_completed` is `1`, runs `/product/bin/ele-status`, and checks
the product name and a non-zero memory total. Nobody has recorded that pass
in this repository.

The owner-proof library can also be built inside AOSP, once a tree exists,
with `m libele_owner_proof`. That path has not been recorded either.

## AOSP build host

Google's current requirements are published in
[Set up for AOSP development (9.0 or later)](https://source.android.com/docs/setup/start/requirements).
The page we are citing was last updated 2026-11-04, and it describes the
current release branch. ELE-OS's pinned tag is `android-16.0.0_r1`, which is
the same class of build. This project has not completed a build, so it has
no measured exception and does not adopt a smaller machine than upstream
publishes.

From that page:

- 64-bit x86.
- At least 400 GB free disk to check out and build (250 GB for the checkout
  and 150 GB for the build). A full mirror needs more. Cloning this ELE-OS
  git repository is not that checkout. The 400 GB figure is the AOSP tree
  plus its output.
- A minimum of 64 GB of RAM. Google's example of a 72-core machine with 64 GB
  of RAM is about 40 minutes for a full build, and their example of a 6-core
  machine with 64 GB of RAM is about 6 hours. Those are Google's figures for
  their hardware, not ELE-OS measurements.
- 64-bit Linux with glibc 2.17 or newer. macOS is not supported for AOSP
  development as of 22 June 2021 (Android 11).
- Ubuntu 18.04 or later for Android 11 and higher, plus the package list on
  that page (`git-core`, `gnupg`, `flex`, `bison`, `build-essential`, `zip`,
  `curl`, `zlib1g-dev`, `libc6-dev-i386`, `x11proto-core-dev`, `libx11-dev`,
  `lib32z1-dev`, `libgl1-mesa-dev`, `libxml2-utils`, `xsltproc`, `unzip`,
  `fontconfig`).
- Repo, OpenJDK, Make, and Python 3. Current Android release branches ship
  prebuilt OpenJDK, Make, and Python 3. Repo's launcher is installed from
  the distribution or from the steps on the same page. The case-sensitive
  filesystem of a normal Linux install is required.

The same page says that, effective in 2026, AOSP source is published in Q2
and Q4, and `android-latest-release` tracks the newest published release.
ELE-OS instructions below stay on the pinned tag until a recorded boot gives
a reason to move. See [open questions](open-questions.md).

### Cuttlefish

Cuttlefish needs KVM (`/dev/kvm`), a user who can open that device, and the
host packages in the
[Cuttlefish getting started guide](https://source.android.com/docs/devices/cuttlefish/get-started).
A virtual machine that does not expose `vmx` or `svm` cannot run the
emulator even when the disk and RAM would be enough to compile. Nested
virtualization has to be turned on for a cloud VM. Compiling on one machine
and running Cuttlefish on another is fine if both are documented in the
evidence note.

### Commands for the pinned baseline

These are the integration steps for a host that meets the requirements
above. They use upstream development keys. They are not a release.

```sh
repo init -u https://android.googlesource.com/platform/manifest -b android-16.0.0_r1
mkdir -p .repo/local_manifests
cp /path/to/ELE-OS/manifest/ele-os-local.xml .repo/local_manifests/ele-os.xml
repo sync -c
source build/envsetup.sh
lunch ele_os_cf_x86_64-aosp_current-userdebug
m
launch_cvd --daemon
adb wait-for-device
adb shell getprop sys.boot_completed
adb shell /product/bin/ele-status
```

Copy `manifest/ele-os-local.xml` from a checkout of this repository. `repo sync`
then places the overlay at `device/ele/os`. Do not also git-clone into that
path. The local manifest adds only that project.

A completed boot reports `1` for `sys.boot_completed`, and `ele-status`
reports `product=ele_os_cf_x86_64`. From a checkout of this repository on
that host:

```sh
sh tests/check-boot.sh SERIAL
```

Run the same workload on a stock Cuttlefish image of the same manifest tag
before writing a performance conclusion. Record both transcripts under
`docs/evidence/`.

`lunch` only works after `source build/envsetup.sh`. The lunch combo is the
one `AndroidProducts.mk` registers:
`ele_os_cf_x86_64-aosp_current-userdebug`. On `android-16.0.0_r1` the
release config map aliases `aosp_current` to `bp2a` and does not define
`trunk_staging`. `lunch` has not been run. Moving the pin is a separate
change, and the combo has to name a release config that pin declares.

### Keys

The repository owner generates production release keys on an offline machine
or hardware token, keeps an encrypted backup in a separate physical place,
and follows the succession steps in the architecture doc. CI never sees
those private keys. This Cuttlefish product stays a test-key image until an
evidence note says a specific handset image was signed with the published
release key. Signing steps, when a release exists, are AOSP's
[sign_target_files_apks](https://source.android.com/docs/core/ota/sign_builds)
flow.

### What this overlay does not build by itself

There is no script in this repository that downloads AOSP or compiles an
image. A wrapper that printed success without compiling would be worse than
the commands above. Device trees for handsets are separate repositories; see
[device support](device-support.md).
