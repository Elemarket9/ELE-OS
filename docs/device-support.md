# Device support

ELE-OS supports devices by class and by tier. The classes are current
flagships (including phones the vendor still updates), mid-range phones, and
low-end or older phones. The tiers say how much of the architecture we will
stand behind on that hardware. A newer phone is not automatically a higher
tier. An older phone is not the only kind of phone we will add.

ELE-OS has no phone partner and no named first handset. Two images are the
project's own targets:

- **Cuttlefish** (`ele_os_cf_x86_64`) is the shared development target. The
  product makefile exists. A boot has not been recorded.
- **A Treble GSI** is the hardware image, so one system build can boot on
  many Treble-compliant phones. The product is not in this tree yet. The
  intended target is `ele_os_gsi_arm64`. It installs the ELE-OS system on top
  of the vendor image already on the phone. It does not replace that vendor
  image, the kernel, or the firmware.

Per-device pages will live in `docs/devices/`. That directory has no device
page yet. See [devices/README.md](devices/README.md). A page appears when a
phone maker or a porter finishes the onboarding kit below. A GSI boot on a
phone is a Tier 1 note for that phone. It does not make the phone a partner
and it does not make the phone Tier 3.

When a device is later maintained, its branch lasts while that vendor ships
firmware and security updates. The end date is the vendor's published date,
written on the device page. Updates in that window track vendor binaries and
AOSP source when those artifacts exist. They are not automatically monthly
platform patches: AOSP's 2026 source publication is Q2 and Q4. After the
vendor window the branch is labelled extended and contains only fixes we can
still deliver. The GSI has no vendor window of its own.

Unlocking a compliant bootloader wipes user data. A unit whose OEM unlocking
toggle cannot be switched on is out of scope, whoever sold it.

Tiers are defined in the [architecture](architecture.md#device-classes-and-support-tiers).
Short form:

| Tier | Requirement to enter | Theft lock after factory reset |
| --- | --- | --- |
| 0 Virtual | The Cuttlefish product | Not offered |
| 1 Boot experiment | Any class, boots far enough for `ele-status` | Not offered |
| 2 Maintained | Unlockable bootloader, kernel built from source, basic functions tested or listed as broken, signed updates for the parts we build | Not claimed. Lock screen and encryption only. |
| 3 Relockable | Tier 2, plus a demonstrated user-set verified-boot key, a user build, and an owner record that survives a userdata wipe and cannot be fastboot-erased while locked | In scope |

## Hard requirements

A device that fails one of these is out of scope, whether it is new or old.

**Unlockable bootloader.** There is a documented method the owner can use,
preferably the vendor's. Unlocking a compliant bootloader wipes user data;
the install notes say that before any command. Carrier locks that the carrier
will not remove are out of scope. Paid unofficial unlock services, leaked
OEM tools, and exploits we cannot legally ship are out of scope. A flagship
sold locked to a carrier is not "modern support." It is unsupported.

**Kernel source for Tier 2 and above.** The Linux kernel is GPL-2.0. Vendors
are obliged to offer the corresponding source. Tier 2 means we build that
kernel, or, for a GKI device, we build the modules we can from source and
record the GKI image we used. A prebuilt non-GKI kernel keeps the device at
Tier 1, and we do not redistribute that binary from this project. LineageOS
states the same split in its
[device-support requirements](https://github.com/LineageOS/charter/blob/main/device-support-requirements.md):
non-GKI devices must not ship a prebuilt kernel; GKI devices may use Google's
GKI image and still build feasible modules from source. We use that as a
reference rule, not as a claim that we follow the rest of their charter.

**A maintainer with the device.** Porting without the hardware produces
device trees nobody can test. The person who proposes a device has that
device, or a named co-maintainer does.

**Redistributable blobs, or none.** Proprietary HALs are often required for
calls, camera, and graphics. They are extracted from a publicly released
factory image and the device page names that image's build fingerprint. Blobs
are not committed to the Apache-2.0 tree unless their license actually allows
redistribution, which is uncommon. A device that needs them is not "fully
open," and we do not describe it as such.

**SELinux enforcing** on any build we call maintained. A build that only
boots in permissive mode stays at Tier 1 until that is fixed.

## Treble and GSI

[Project Treble](https://source.android.com/docs/core/architecture) and the
[Generic System Image](https://source.android.com/docs/core/tests/vts/gsi)
are how ELE-OS reaches many phones without a partner.

- Devices that launched with Android 8 or later were supposed to separate
  vendor code from the framework. In practice the vendor interface is often
  incomplete. A GSI boot is Tier 1 evidence for that phone. It is not a
  maintained-device acceptance test.
- AOSP's GSI documentation treats the upstream GSI as the system image for
  VTS and CTS-on-GSI. ELE-OS's GSI is our system image for Treble phones.
  The vendor partition, kernel, and radio firmware stay at the versions
  already on the phone, so the update pillar for those partitions is unmet
  until a porter builds them through the kit.
- Android 9 and later GSI assumptions (system-as-root, mandatory VNDK,
  64-bit binder) exclude a lot of older phones. Those phones can still be
  Tier 2 with a dedicated device tree and an older Android branch. They are
  poor GSI candidates.
- Dynamic partitions and a small system partition produce flash failures
  (`Not enough space to resize partition` is the AOSP-documented symptom).
  Deleting the vendor's `product` partition to make room is a destructive
  experiment, not an install guide, until a maintainer writes recovery steps
  for that exact model.
- GrapheneOS explicitly does not support being used as a GSI, because their
  hardening depends on the kernel they ship. ELE-OS's Tier 3 goal has the
  same shape. A GSI cannot be our relockable product.

Record, for every proposal, whether a GSI was tried: architecture (arm64 or
otherwise), Android version the device launched with, VNDK version if known,
A/B or not, dynamic partitions or not, what booted, and what failed. "Not
tried" is an acceptable entry.

## What to record about the hardware

The proposal issue contains all of the following. Unknown is an acceptable
value. Invented is not.

- Marketing name, model number, and codename
- Class: flagship / mid-range / low-end or older
- SoC, RAM, and storage size, from the device itself or the vendor's spec
- Partition layout: non-A/B, A/B, or virtual A/B; dynamic partitions or not
- Launch Android version, current vendor security patch, whether the vendor
  still publishes firmware
- Unlock method and whether relock with a custom AVB key works. "Unknown"
  means Tier 3 is not claimed. A quick way to see the current state later is
  `ro.boot.verifiedbootstate` (`green`, `yellow`, `orange`) and
  `ro.boot.flash.locked`, which is what `ele-status` prints
- Kernel source URL and the commit or tag that matches the shipping kernel
- Where blobs would come from (factory image fingerprint)
- Telephony: calls, SMS, mobile data, and emergency calling with and without
  a SIM. Emergency numbers stay with the radio interface and Android's
  per-country configuration. The project does not hardcode one country's
  numbers.
- Radios and bands the maintainer can actually test. A device page does not
  claim a band nobody tried
- File-based encryption, or the reason it cannot work
- Whether `ro.frp.pst` or another non-userdata block exists
- Accessibility hardware the phone has (touch screen, physical keyboard,
  audio) and that the AOSP accessibility framework still starts

RAM around 2–3 GB marks the low-end class we tune for. It is not an entry
requirement and not a rejection. A phone with more RAM is in scope. A phone
with less RAM is in scope until a boot shows the chosen Android branch cannot
run.

## How to add a device

1. Read this page and the tier table. Decide which tier you are aiming at,
   and expect to land at Tier 1 until evidence says otherwise.
2. Open an issue with the hardware record above and the hard requirements
   answered. Tag it as a device proposal.
3. Do not flash anything until the unlock warning (data wipe) is in the
   issue, and do not flash a phone you do not have permission to wipe.
4. Boot the ELE-OS GSI if the phone is Treble-compliant, and put the log in
   the issue. A boot is Tier 1 evidence only. Phones that are not
   Treble-compliant skip this step and say why.
5. Device tree and kernel live in their own git repositories, intended to be
   checked out at `device/ele/<codename>` and the kernel path that tree
   names. They are not folders in this overlay. This repository stays
   `device/ele/os`, the shared virtual product. A local AOSP manifest entry
   is added when there is a real repository URL, not before.
6. Build from published kernel source. Keep proprietary files out of the
   Apache-2.0 history. An extract script that copies them from a factory
   image the builder already downloaded is the usual pattern.
7. Add `docs/devices/<codename>.md` in this repository. It holds the class,
   the tier actually achieved, the install steps a second person can follow,
   the function checklist, the patch-level split (platform, vendor,
   firmware), the verified-boot state, and where the owner record lives if
   recovery is attempted. Known failures are part of the page.
8. Promotion from Tier 1 to Tier 2 or Tier 3 is a change to that page with
   the evidence next to it. A maintainer's word without the log does not
   promote the device.

There is no first brand. A flagship, a mid-range phone, and a low-RAM or
older phone are all onboarded the same way. The public status lists a phone
only after its page exists. A GSI boot log is not that page.

## Maintained-device checklist

Each line is pass, fail, or not tested. Failures stay visible.

- Boot and `ele-status` output, including both patch dates and the
  verified-boot properties
- Display at the panel's native resolution
- Audio: media, earpiece, speaker, headset if the phone has one
- Wi-Fi and, if present, Bluetooth
- Cellular: registration, calls, SMS, data, emergency call with a SIM
- Sensors that the stock OS exposed and that we claim
- Camera stills, and video if we claim it
- File-based encryption enabled
- Install an APK from a file the user chose, without Play
- A notification from an app that does not need Google push, and a separate
  note that apps which do need it will not get it
- Screen reader from Track F starts, and font scale does not clip the setup
  UI
- SELinux enforcing
- OTA signature rejection, once Track C has started
- Battery across the stated scenario, once Track B has a protocol

CTS is the industry definition of Android compatibility. ELE-OS has not run
CTS. Passing this checklist is not a CTS result, and failing to mention that
would overstate app compatibility. Apps that require Play Services or Play
Integrity may refuse to run on every tier.

## Worldwide devices

Support is per model and codename. The same marketing name often hides
different radios, different bootloaders, and carrier locks. A page for one
region's variant does not cover another variant. We do not ship a
country-specific default, a single-language image, or a recovery server that
only works in one place. The owner-configured recovery endpoint can be
wherever the owner puts it.
