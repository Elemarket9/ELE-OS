# Contributing

ELE-OS is open-source mobile OS research for flagships, mid-range phones,
low-end phones, and older devices. Contributions are welcome that move one
of the pillars — efficiency, signed updates, privacy and owner control,
lost-device recovery — or the revival of old and low-end phones, or the
cross-device capabilities in the architecture doc (permission controls, the
sandbox baseline, modular updates, accessibility).

The tree is early. The most useful change is often a measurement, a device
page, or a small patch with a test, not a large untested subsystem.

## License

This repository is under the Apache License 2.0
([LICENSE](LICENSE)). Sending a pull request is a Contribution under
section 5 of that license unless you mark it otherwise. Only send work you
have the right to license that way.

The Linux kernel is GPL-2.0. Kernel changes belong in the kernel repository
for that device, with GPL headers kept intact, not in this Apache-2.0
overlay. Do not copy GPL code into `activation/`, `status/`, `peripherals/`,
or the makefiles.

Proprietary vendor blobs do not belong in git history here. An extract
script may point at a factory image the builder downloads themselves. Name
the image fingerprint in the device page.

If you import code from AOSP, LineageOS, GrapheneOS, CalyxOS, /e/OS, or
anywhere else, keep the original copyright and license headers. Do not
relicense someone else's work by omitting the header.

Android, Google, Pixel, iOS, Apple, LineageOS, GrapheneOS, CalyxOS, Murena,
and postmarketOS are trademarks of their owners. ELE-OS is not affiliated
with them. Do not write copy that implies they ship or endorse this project.

## What we will not merge

- Private release keys, owner private keys, or credential dumps.
- Unlock tools that are leaked, paid, or not redistributable.
- A claim that a device is supported, that a theft lock holds, or that a
  build is faster or safer, without the evidence the
  [roadmap](docs/roadmap.md) names.
- Benchmark numbers that are not paired with the stock system on the same
  device and checked in under `docs/evidence/`.
- A "lean image" change that removes the accessibility framework or a screen
  reader the product has committed to, or that turns off encryption, or that
  sets `ro.config.low_ram` on a flagship product without a written reason.
- `su` in an image offered as a daily driver, or SELinux left permissive on a
  build called maintained.
- Device support limited to one country, carrier, or language.
- An ELE-OS account, or a project-held copy of the owner recovery key, as a
  requirement for using or recovering a phone.

## How to develop

Host tests, from a Linux machine with a C++17 toolchain and OpenSSL:

```sh
CXX=g++ sh tests/run.sh
```

Use `CXX` when `c++` is not the compiler that has the standard library
headers. Details and the AOSP build-host requirements are in
[docs/build.md](docs/build.md). A full system image needs that AOSP host, not
this git repository alone.

Changes to `activation/` need the host tests to pass, including the reset
fixture and the replay checks. A change to the signed message format is a
new purpose string (`ELE-OS/reactivate/v2`, or a new command prefix). Do not
silently alter `ELE-OS/reactivate/v1`. The message stays free of names,
phone numbers, and email addresses.

`ele-status` must keep printing missing properties as `unknown` and must not
grow a mode that claims attestation or patch completeness.

UI work uses resource strings, start/end layout, and font scale. The
External devices app is an optional experiment. It is not in the default
image and not on the core roadmap. Do not add `ElePeripherals` back to
`PRODUCT_PACKAGES` as a drive-by. Strings in that activity are technical
debt, not a pattern to copy. Product UI ships TalkBack built from
`google/talkback` (Apache-2.0) once that milestone is done. Do not drop it
from a low-RAM product to save space.

## Device proposals

Open an issue using the record in
[docs/device-support.md](docs/device-support.md). You need the device, an
unlock method you can document, and, for anything past a boot experiment,
kernel source. Say which class the phone is (flagship, mid-range, low-end or
older) and which tier you are actually claiming. Tier 1 is the right claim
for a first boot.

## Commit messages

Write a complete sentence that says what changed, in the imperative, in the
style of the existing history (`Add device-bound owner proof verifier for
post-reset activation`). Mention why when the diff does not make it obvious.
Do not rewrite published history to add trailers.

## Conduct

Disagree on the technical merits, and keep device-owner data out of issues
and logs. Location traces, serial numbers, and owner keys do not belong in
a public ticket. Trim `ele-status` output if a fingerprint or serial is more
than the review needs.

## Security reports

There is no release build and no security contact yet. Until this file names
one, send the details privately to the repository owner (GitHub account
@Elemarket9) and keep them out of a public issue. A flaw in the host-side
owner-proof binding matters because a later phone may rely on that contract.
Treat it as a defect to fix before any device enrolls, and keep a bypass
write-up off the public issue tracker.
