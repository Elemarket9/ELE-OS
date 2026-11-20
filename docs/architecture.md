# Architecture

This document is the technical shape of ELE-OS. It separates decisions that
are already visible in the tree from designs that are not built, and it
separates what a flagship can enforce from what an unlocked low-RAM phone can
enforce. Nothing here is a benchmark.

ELE-OS targets the whole phone market. Current flagships, mid-range phones,
low-end phones, and older devices are all in scope. Reviving old and low-end
phones is one pillar. The pillars that apply on every class are performance
efficiency, security with signed updates, privacy and owner control,
owner-controlled lost-device recovery, and Faith, the built-in assistant,
which runs on the device.

## Base OS

Ordinary Android applications are a requirement, so the base is open Android:
the [Android Open Source Project](https://source.android.com/) and, where a
handset needs it, a device tree in the style maintained by
[LineageOS](https://lineageos.org/). [postmarketOS](https://postmarketos.org/)
is a real Linux distribution for phones and a serious way to keep hardware
alive with a mainline kernel. It is not the base. Android apps there run, if
at all, inside a nested Android system such as Waydroid. That is a different
product.

### What we will use, and what it costs

| Upstream | What it actually is | Trade-off for ELE-OS |
| --- | --- | --- |
| [AOSP](https://source.android.com/) | The open Android platform. Most userspace is Apache-2.0. The kernel is GPL-2.0. Provides the app runtime, verified boot, OTA machinery, Mainline modules, and the Cuttlefish virtual device. | A full checkout is large (see [build.md](build.md)). Google Mobile Services are not included, so many apps lose push, maps, and Play Integrity. Since 2026, AOSP source publication is described as Q2 and Q4, with `android-latest-release` pointing at the newest published release. Platform patches exist in the tree only for the releases Google has published. |
| [LineageOS](https://lineageos.org/) | A community Android distribution with per-device trees, its own signing keys, and a written [device-support charter](https://github.com/LineageOS/charter/blob/main/device-support-requirements.md). | The fastest legal starting point for many handsets, old and current. Their charter is their policy: official builds are userdebug, verity is disabled on those system images, and quality depends on the maintainer. ELE-OS does not adopt that charter by linking a tree. |
| [GrapheneOS](https://grapheneos.org/) | A hardened Android OS with production support for Pixel devices. Relocks the bootloader onto its own verified-boot key. Adds userspace hardening and extra permission toggles. The [FAQ](https://grapheneos.org/faq) states that most other hardware cannot meet the bar, and that GrapheneOS is not a Generic System Image. | The reference for hardening and for the network and sensors permission toggles. We do not ship GrapheneOS, do not claim its attestation story, and do not imply a non-Pixel device gained those properties by installing ELE-OS. |
| [CalyxOS](https://calyxos.org/) | A privacy-focused Android OS. As of its [device-support page](https://calyxos.org/docs/guide/device-support/) in 2026, supported hardware includes Pixels, Fairphone, some Motorola moto g models, and SHIFT. [Datura](https://calyxos.org/docs/tech/datura-details/) is its per-app firewall. microG is optional. | A reference for per-app network controls on more than one brand. microG and signature spoofing are a compatibility and security trade-off, not a default here. |
| [/e/OS](https://doc.e.foundation/os/product-description/) (Murena) | A de-Googled Android system. Murena's product description says it runs on more than 200 models and includes microG and Murena online services. | A reference for wide device coverage and for a usable de-Googled image. Their cloud account is part of that product. ELE-OS recovery must work with keys the owner holds, with no project account required. |
| [Treble / GSI](https://source.android.com/docs/core/tests/vts/gsi) | Project Treble splits the vendor implementation from the Android framework. A Generic System Image is a system image built to boot on a Treble-compliant device. AOSP uses GSIs for VTS and CTS-on-GSI. | ELE-OS's hardware image for many phones, with no named partner. The vendor partition, kernel, and firmware stay on the phone's existing build, so a GSI does not patch those. Flashing can fail when the system partition is too small. An unlocked bootloader is required, and unlocking wipes user data on compliant devices. A GSI is not a relockable Tier 3 product. |

### Working decision

Shared components are developed on AOSP against the Cuttlefish product in
this repository. The manifest tag pinned by the current instructions is
`android-16.0.0_r1`. That pin is a development baseline. It is not a
statement that the tag contains the latest security patches. Moving it is an
open question, because `android-latest-release` moves when Google publishes
source.

ELE-OS has no phone partner and does not pick a first handset. The hardware
image is a Treble GSI, so the same system image can boot on many
Treble-compliant phones. That product is not registered in this tree yet.
The intended lunch target is `ele_os_gsi_arm64`, built from the AOSP generic
system image plus the ELE-OS packages. `arm` is added only if a porter shows
a device that needs it.

A phone maker or a porter who wants a maintained device, vendor updates, or
Tier 3 follows the onboarding kit in [device-support.md](device-support.md).
The result is a per-device tree at `device/ele/<codename>` in its own git
repository, plus a kernel repository. Where an existing LineageOS or AOSP
device tree is licensed so they can reuse it, they start from that tree. We
do not fork LineageOS as a whole, and we do not take another project's
release keys or updater.

A handset tracks the newest AOSP release its vendor code can boot. A current
flagship and a 2018 phone will not share that release. Android 16 on a device
whose vendor HAL stopped at an older version is not a goal we can declare in
advance.

## Repository and system components

This repository is the overlay at `device/ele/os`. It registers one virtual
product:

- `AndroidProducts.mk` registers `ele_os_cf_build.mk`, which inherits
  `ele_os_cf_x86_64.mk`. The lunch target is
  `ele_os_cf_x86_64-aosp_current-userdebug`. `android-16.0.0_r1` has no
  `trunk_staging` release config; `aosp_current` is the alias that tag maps
  to `bp2a`. `lunch` has not been run.
- `ele_os_cf_x86_64.mk` inherits
  `device/google/cuttlefish/vsoc_x86_64_only/phone/aosp_cf.mk` and adds
  `ele-status` to the default image. `ElePeripherals` stays in the tree and
  is not installed unless a local build adds it.
- The image, when someone builds it, is signed with AOSP's public test-keys.
  Those keys are published in the AOSP tree. They are acceptable for a
  virtual development build and unacceptable for a phone offered to anyone
  else. See [Sign builds for release](https://source.android.com/docs/core/ota/sign_builds).

### In the tree now

**`ele-status`** (`status/ele-status.sh`) prints `ro.product.name`,
`ro.build.fingerprint`, `ro.build.version.security_patch`,
`ro.vendor.build.security_patch`, `ro.boot.verifiedbootstate`,
`ro.boot.flash.locked`, and `MemTotal` / `MemAvailable` from `/proc/meminfo`.
A missing property is printed as `unknown`. The host test feeds it an unlocked
fixture (`verified_boot_state=orange`, `bootloader_locked=0`). A green or
locked reading from this command is a local property dump. It is not remote
attestation and it does not prove that every partition is patched.

**Owner proof** (`activation/`) is a C++17 static library,
`libele_owner_proof`, linked against OpenSSL's libcrypto. `VerifyReactivation`
checks an Ed25519 signature over the exact bytes of the ASCII string
`ELE-OS/reactivate/v1` (no trailing NUL), then a 32-byte device id, then a
32-byte challenge. The message carries no name, phone number, or email.
`ReactivationSession` holds at most one challenge, replaces it when `Begin`
is called again, and grants `Consume` once. The enrollment is accepted only
when its device id matches the hardware id the caller supplies. A missing or
truncated record does not start a session. Challenges come from `RAND_bytes`.
The session state is in process memory, so constructing a new session drops
outstanding challenges.

The host test writes a fixture file outside a temporary userdata directory,
deletes that directory, and checks replay, replacement, device binding, and
a torn record. The comment in the test says what the fixture is not: hardware
still has to authenticate the enrollment record. A future device service
still has to gate every setup path, use a fresh challenge from a secure
random source, consume it once even across crashes, and refuse to treat a
corrupt record as "this phone was never enrolled." Owner transfer and loss of
the recovery credential are not implemented. No device is protected.

**External devices** (`peripherals/`) is an optional experiment,
`org.eleos.peripherals`. It previews Camera2 cameras that report
`LENS_FACING_EXTERNAL`, and it shows whether the default network's
capabilities include `TRANSPORT_SATELLITE`. On a handset, a preview needs USB
host mode, a UVC kernel path, and an external camera provider. A camera that
only speaks PTP needs a different integration. A satellite terminal attached
over Wi-Fi can look like ordinary Wi-Fi. An SMS-only satellite path does not
have to create a default IP route. None of this has been validated on
hardware. It is outside the core roadmap and is not in `PRODUCT_PACKAGES`.
A developer who wants it in a local image adds `ElePeripherals` themselves.

### Planned components

These names describe jobs. They are not packages in the tree.

| Job | Role | Feasibility |
| --- | --- | --- |
| Product config per device class | Which packages and `ro.config.low_ram` settings a build gets | The virtual product has neither a lean package list nor `low_ram` set. Flagship and low-RAM configs must be different products. |
| Update client | Apply a signed OTA with AOSP `update_engine` or recovery | Depends on A/B, virtual A/B, or non-A/B layout. All three exist in the phone market. |
| Release signer | Offline signing of target files, APEXes, and OTA packages | Procedure decided below. Keys do not exist yet and must not be committed. CI uses test-keys only. |
| Owner record | Device id plus owner Ed25519 public key, stored so a userdata wipe does not delete it | Only on devices that have such storage and a bootloader that will not casually erase it. |
| Reactivation gate | Setup refuses to continue until a fresh owner proof is consumed | Requires the record and a setup path we control. userdebug `adb root` bypasses a careless gate. |
| Recovery command channel | Signed locate, lock, and wipe messages | Optional, off until the phone's owner enables it. Needs a network path that owner chose. |
| Network toggle | One per-app on/off control for IP networking | Decided, not built. Userspace on any device running our system image, including low-RAM. Does not constrain the baseband. Per-transport firewall is later. |
| TalkBack | Screen reader on every class, including low-RAM | Decided: build [google/talkback](https://github.com/google/talkback), Apache-2.0. Not yet in the image. A plain AOSP lunch target does not include it. |

## Update pipeline and signing

AOSP already has the release tools. ELE-OS has not run them on a phone.
The intended pipeline, once a device is maintained, follows
[Sign builds for release](https://source.android.com/docs/core/ota/sign_builds)
and [Build OTA packages](https://source.android.com/docs/core/ota/tools):

1. `m dist` produces a target-files archive for a named product.
2. `sign_target_files_apks` re-signs APKs, APEXes, and OTA certificates with
   release keys kept offline. The public test-keys stay on Cuttlefish
   userdebug builds only.
3. `ota_from_target_files` builds a full package and, when the layout
   supports it, an incremental package. Recovery verifies the package against
   the keys in its ramdisk. The running system verifies against
   `/system/etc/security/otacerts.zip`. Sideloading does not skip that check.
4. A metadata file (build fingerprint, platform patch level, vendor patch
   level, package hash, URL) is published over HTTPS. The metadata is signed
   so a mirror cannot swap the package.

Three keys must stay distinct:

| Key | Who holds it | What it authorizes |
| --- | --- | --- |
| OS release keys (APK, APEX, OTA, and the AVB key for `vbmeta`) | The repository owner, offline | Integrity of the ELE-OS image and of updates. Anyone who locks a device to this AVB key trusts those keys for the OS. |
| Owner recovery key (Ed25519) | The phone's owner, never the repository | Reactivation after wipe, and remote locate / lock / wipe. |
| AOSP test-keys | Published, everyone, including CI | Development images only. |

Verified boot colors, from the
[boot flow](https://source.android.com/docs/security/features/verifiedboot/boot-flow)
documentation:

- `green` — locked, verified with the built-in root of trust. For a custom
  OS this is rare, because the built-in key is the OEM's.
- `yellow` — locked, verified with a user-set root of trust. This is the
  state GrapheneOS-class installs aim for. AOSP describes it as optional;
  Pixels implement it via `avb_custom_key`. Setting or clearing it requires
  physical confirmation, and the device should boot either the OEM key or
  the user key.
- `orange` — unlocked. The bootloader warns and boots an image that fails
  verification. Integrity is not guaranteed by the bootloader.

[Locking and unlocking](https://source.android.com/docs/core/architecture/bootloader/locking_unlocking)
on a compliant device wipes user data on each transition. An unlocked
bootloader can flash another system, which removes ELE-OS and any policy
ELE-OS was enforcing.

Rollback protection exists only where the bootloader stores a rollback index
somewhere the OS cannot rewind (fuses, a secure element, RPMB, or an
equivalent). GrapheneOS documents that Pixel firmware does this. Many other
phones do not. A device page that cannot show the index advancing must say
"no rollback protection."

### What an update can and cannot contain

Release notes for a maintained device split three lines, matching the two
properties `ele-status` already prints plus firmware:

- **Platform** — `ro.build.version.security_patch`, from the AOSP tree we
  built. This is the line we can move when the corresponding source exists.
- **Vendor** — `ro.vendor.build.security_patch`, from the vendor image or the
  kernel we compiled. Frozen when we cannot rebuild those pieces.
- **Firmware / baseband** — updated only by binaries the vendor still signs,
  or by source we actually have. A flagship under vendor support is often
  ahead of us here. A phone after end of support is often stuck.

### How long a device is maintained

Decided. Each device keeps a maintained branch for as long as its vendor
ships firmware and security updates for that model. During that window the
branch tracks security releases as often as we can actually build them.

Two clocks run at once, and they are not the same clock:

- Vendor firmware and the vendor security patch move when that vendor
  publishes binaries we can install. Some vendors ship those about monthly.
  The cadence belongs to the vendor, and it stops when they say it stops.
  A GSI does not carry this clock: the vendor image stays the one already
  on the phone.
- Platform source moves when AOSP publishes it. AOSP's 2026 documentation
  says source is published in Q2 and Q4. A monthly Android Security Bulletin
  does not, by itself, put that month's platform fixes in a tree we can
  compile. A release that only had a firmware binary says so. A month with
  neither is a month we did not ship.

Kernel fixes we write ourselves can land between those drops. That work is
per device. It is not a claim that the whole bulletin was applied.

There is no first-phone end date, because there is no partner phone. When a
device is onboarded, its page states the vendor's published end of support.
Until that page exists, ELE-OS does not quote a year.

After the vendor stops, the branch is labelled **extended**. An extended
branch lists only the fixes that build contains: platform changes we can
still compile, and nothing we pretend arrived as a vendor firmware update.
Extended is a clear label, not a second full-support promise.

### Release keys, backup, and succession

Decided. The repository owner holds the OS release keys. They are generated
when the first release build is prepared, not before, and not on a CI runner.

- Generate them on an offline machine, or on a hardware token. The machine
  is not the one used for ordinary browsing, and it is not a cloud builder.
- The public certificates and their fingerprints are what gets committed.
  Private keys (`.pk8` and the AVB private key) stay off the git history,
  off GitHub Actions, and off any secret store attached to this repository.
- CI and Cuttlefish development builds keep using AOSP's public test-keys.
  A test-key image is labelled as one.
- Encrypt a copy of the private-key bundle with a passphrase-based tool the
  owner can run offline. Keep that ciphertext in a separate physical place
  from the machine or token that generated it. Keep the passphrase somewhere
  else again, not next to the ciphertext.
- Write down, on paper held by the owner, the name of one successor. A sealed
  copy of the passphrase is stored so that successor can open the backup
  without the passphrase sitting in the repository or in the same drawer as
  the ciphertext. The successor's only jobs are to sign an update the
  maintainers have already built, or to publish a signed note that the keys
  are retired, together with the public fingerprint.
- If neither the owner nor the successor can reach the keys, maintained
  updates stop. Phones already locked to the old key keep running the last
  image they trust. Installing a different key means unlocking the
  bootloader, which wipes user data, and enrolling the new key. There is no
  third escrow and no cloud copy of the private keys.

### Modular updates

[Mainline](https://source.android.com/docs/core/ota/modular-system),
introduced in Android 10, packages selected system components as APEX or APK
modules so they can update outside a full OS release. Module sets are version
specific. The AOSP page lists ART as an APEX from Android 12 and
PermissionController as a module from Android 10, among others. Updated
modules in a package install or roll back together.

On phones with Google Play, Google delivers those updates as "Google Play
system updates." ELE-OS does not include Play, so a Mainline module moves
only if our OTA carries it. That is the modular-update work: our signing
keys, our package, AOSP's APEX mechanism.

Limits, which are properties of Android rather than of a device class:

- Android 10 and newer have the mechanism. The same AOSP page says Mainline
  support for Android 11 and lower concluded in Q4 2025, meaning Google's
  module stream for those versions has stopped. An ELE-OS build can still
  put fixes in a full OTA that we compile.
- APEX does not update the bootloader, the modem, or a vendor partition we
  do not build.
- Older phones that are not Treble-complete often cannot take a newer module
  even when the XML says the module exists.
- Modular updates are useful on a flagship and on a mid-ranger running a
  recent Android. They are the wrong thing to promise on a pre-Treble phone.

The first shipped update for a device is a full signed OTA. A module update
is a later milestone on a device that can install one. See the roadmap.

### Hardening, in the spirit of GrapheneOS

"In the spirit of" means we copy the approach where the hardware matches, and
we say so on the device page when it does not. Examples that are real on
GrapheneOS and conditional here:

- Relock after install, with the OS verified on every boot.
- A user build, SELinux enforcing, no bundled `su`.
- Reduced privileged surface: packages we add need a stated reason.
- Userspace hardening that the Android version already supports (allocator,
  exploit mitigations) without renaming them as if we had upstreamed a new
  one.
- Hardware memory tagging, pointer authentication, and a secure element, only
  on chips and boards that have them. GrapheneOS documents memory tagging on
  Pixel 8 and later. A flagship without that CPU feature does not grow it
  by flashing ELE-OS. A low-end chip from an earlier generation does not
  have it.

GrapheneOS's own FAQ is the reason this is tiered: they can build for other
AOSP device trees, and they still refuse production support when the firmware
cannot provide a trustworthy device.

## Owner recovery

The feature is locate, remote lock, remote wipe, and a theft lock that
survives a factory reset of user data. It is owner-controlled: the project
does not have an account that can find or unlock the phone, and the
verification message is not allowed to carry a name, phone number, or email.
That last rule is already true of the prototype's signed bytes.

This section is a design. The prototype does not implement storage, the setup
wizard, the network protocol, or the bootloader.

### Keys and daily unlock

The Ed25519 owner key is not the screen lock. Day-to-day unlock stays
Android's local credential (PIN, password, or biometric) on top of file-based
encryption. The owner key authorizes reactivation after a wipe and any remote
command. Someone who still has the phone and knows the PIN can keep using it
without the owner key, until the userdata is wiped. Someone who wipes the
phone without the owner key is who the theft lock is for.

File-based encryption is the baseline on a maintained device. Android 13
removed full-disk encryption; the LineageOS charter records the consequence
for devices whose keymaster cannot do file-based encryption. A device in that
state has to say so. Shipping with encryption off is not a maintained
configuration.

### Enrollment record

The record is the 32-byte device id and the 32-byte owner public key, in a
versioned blob that is itself authenticated. The prototype's fixture file is
not authenticated. The device id has to be a value the bootloader or the
hardware supplies, not a file the person holding an unlocked phone can edit
and then re-enroll.

The record has to live outside userdata or a factory reset deletes the
ownership. AOSP's persistent data block (`ro.frp.pst`, used by
`PersistentDataBlockService`) is the usual place a factory-reset protection
flag survives a wipe. A dedicated partition is acceptable when the device
tree actually has one and the bootloader does not hand out arbitrary erases.
RPMB or a secure element is stronger and is often unreachable from a custom
OS on low-end hardware. If the only copy we can write is inside userdata, the
device page says that a factory reset clears ownership. We do not describe
that as a theft lock.

A missing record on a never-enrolled phone means "not enrolled." A missing or
corrupt record on a phone that was enrolled must not silently become a new
phone. That requires a marker written with the record. The failure mode is
a phone that will not finish setup until the owner proves the key or a
documented transfer runs. That failure mode is acceptable only when the owner
opted into it, with a warning that losing the key and then wiping the phone
leaves it unable to be set up. There is no project escrow and no staff
override. Split-key backup schemes are not designed here.

### Reactivation

After a wipe, every normal setup path checks the record and runs the session
logic already tested on the host: one live challenge, one grant, wrong device
rejected, replaced challenge rejected, torn record rejected. The purpose
string `ELE-OS/reactivate/v1` is domain-separated. Later commands need their
own prefixes (`ELE-OS/locate/v1`, `ELE-OS/lock/v1`, `ELE-OS/wipe/v1` are
reserved names, not an implemented protocol) so a reactivation signature
cannot be replayed as a wipe. Each command includes the device id and a fresh
nonce or a monotonic counter.

Owner transfer is a separate signed operation that replaces the public key.
It has to exist before theft lock is offered to anyone but the developer,
because a sold phone whose previous owner retains the key is not recovered,
it is still theirs.

### Locate, lock, and wipe

Locate runs when the owner has turned lost mode on. The location payload is
encrypted to the owner key. The phone sends it to an endpoint the owner
configured. The project does not operate a required location server. A relay,
self-hosted or otherwise, sees source IP and timing. It does not see the
location if the payload encryption holds. GNSS and network location both cost
battery; the check-in interval is part of the efficiency measurements, not an
always-on tracker.

Remote lock tells the running system to demand the local credential. After a
wipe, "locked" means the reactivation gate, because the old lockscreen is
gone with userdata.

Remote wipe erases userdata and leaves the owner record in place. The command
is signed by the owner key, so a relay that stores packets still cannot forge
a wipe.

### When a reset-surviving theft lock is real

All of the following have to be true:

1. The owner record survives a normal factory reset (settings or recovery
   wipe of userdata).
2. Every setup path on an ELE-OS user build checks it. A userdebug image with
   `adb root` is not that build.
3. The bootloader is locked to a root of trust that verifies ELE-OS (yellow
   with a user-set key, or an equivalent the device can demonstrate).
4. While locked, fastboot cannot erase the owner record or flash a different
   OS. GrapheneOS documents this property for its supported Pixels: locking
   blocks fastboot flash, format, and erase.

Missing any one of those, the honest description is narrower:

| Situation | What the owner actually has |
| --- | --- |
| ELE-OS is installed, bootloader stays unlocked | Lock screen and encryption protect a powered-off or locked phone. A factory reset that leaves ELE-OS in place can demand the owner key only if the record survived. Anyone who can use fastboot can flash another OS and walk away. Unlocking a compliant bootloader also wipes user data, which protects the previous owner's files and ends ELE-OS's control in the same step. |
| Bootloader can lock only with the OEM key | Relocking boots the vendor OS, which does not honor the ELE-OS record. ELE-OS stays on an unlocked bootloader, or it is not installed. |
| Record lives only in userdata | Factory reset deletes ownership along with the user's files. |
| Pre-Treble phone, no verified boot | There is no boot-time guarantee about which system is installed. Recovery features are whatever the running system still enforces. |

A new flagship does not automatically get the strong row. A Pixel-class
device with a user-settable root of trust can. A carrier-locked phone that
cannot be unlocked cannot run ELE-OS at all. An older unlocked phone usually
stays on the weaker row, and it can still get signed platform updates and
the efficiency work.

### Threat model

Assets: the owner's files, the ability to set the phone up again, the
owner's location, and the binding between the phone and the owner key.

| Adversary | What we claim |
| --- | --- |
| Person who finds a locked phone and does not open the bootloader | File-based encryption with a non-trivial credential stops them, on a device where encryption works. This is Android's property. ELE-OS must not turn it off. |
| Person who factory-resets from the system UI or recovery, then stays on ELE-OS | Stopped only when the record survives and the setup gate runs. |
| Person who unlocks the bootloader or flashes another image | Wins control of the device. On compliant devices the unlock wipes user data. We do not claim the phone is still bound to the owner after that. |
| Person who steals the owner key and the phone | Can reactivate and can send remote commands. Key storage is the owner's responsibility (a second device, a hardware token, or a paper backup kept offline). |
| Network attacker or a hostile relay | Cannot forge a signed command. Can drop commands, so lost mode may never arrive. Learns IP and timing, not the location contents, when payload encryption is in place. |
| Previous owner after a sale | Retains control until a transfer replaces the key. Shipping theft lock without transfer would be a defect. |
| ELE-OS release-key holders | Can sign an OS update that a locked device will trust. They cannot sign a recovery command unless they also have the owner key. Location is not escrowed with them. |
| Forensic attacker with chip-off equipment | Out of scope on hardware without a secure element enforcing the binding. We will not claim otherwise. |

Out of scope on purpose: a hidden unlock for law enforcement, tracking a
phone whose owner has not turned lost mode on, and matching the coverage of
Apple's or Google's find-my networks.

## Faith

Faith is ELE-OS's built-in assistant, in the same role as Google Assistant or
Siri: the owner can speak or type, and the phone can answer and can take a
limited set of actions. That is the role. It is not a claim about answer
quality, latency, or language coverage. The name is decided. Nothing in this
section has been built, and this repository contains no model and no Faith
package.

The suggested wake phrase is "Hey Faith". It is the phrase a new install
listens for once the owner turns listening on. The owner can replace it with
another phrase. An engine that cannot be pointed at a phrase the owner chose
does not meet this requirement. The engine itself is still unchosen.

Faith does not use Google services. No Gemini, no Google speech
recognition, no Google text-to-speech, and no Google account. AOSP's speech
APIs on a device without Play are not a substitute we can point at and call
done.

### On the device first

Speech and the language model run on the phone. Cloud inference exists only
after the owner opts in, through a provider interface they configure. The
cloud policy beyond that opt-in rule is an open decision.

The engines named here are the ones whose licenses were checked for this
design. They are candidates, not a promise that a particular binary is fast.

| Piece | What was checked | What that means for ELE-OS |
| --- | --- | --- |
| [llama.cpp](https://github.com/ggml-org/llama.cpp) | MIT, copyright 2023–2026 The ggml authors | Eligible runtime for a local small language model. The MIT grant covers the engine. It does not cover model weights. |
| [whisper.cpp](https://github.com/ggml-org/whisper.cpp) and [OpenAI Whisper](https://github.com/openai/whisper) | Both engines are MIT (ggml authors; OpenAI, 2022) | Eligible on-device speech-to-text. A weight file ships only when that file's own license allows redistribution. |
| [Piper](https://github.com/OHF-Voice/piper1-gpl) | Current engine is GPL-3.0-or-later. The project's [licensing note](https://github.com/OHF-Voice/piper1-gpl/blob/main/docs/LICENSING.md) says Piper grants no license over voice models. Many published English voices are fine-tuned from the Lessac corpus, which that note describes as limited to research and excluding commercial use. | "Piper-class" means local neural text-to-speech. This GPL-3 engine is not linked into the Apache-2.0 system image. It can be considered only as a separate process, and only with a voice whose model card allows us to redistribute it. A Lessac-derived voice is not the default. |
| AOSP Pico TTS (svox) | Apache-2.0, already in AOSP | License-clean speech for early builds. It is not a neural voice, and it is not the quality bar. |
| [openWakeWord](https://github.com/dscripka/openWakeWord) | Apache-2.0 | Eligible wake-word candidate for the phrase "Hey Faith". The engine is not chosen. Proprietary detectors, including Picovoice Porcupine, are not eligible. |

A model weight is a separate work from the runtime. Llama, Gemma, Qwen, and
other families use their own licenses, some of which restrict use or
redistribution. The default model is an open decision. Until it is chosen,
no weight file is added to the tree.

### What the owner can do with Faith

Text input always works. Voice input uses the on-device recognizer. Spoken
replies use the on-device synthesizer. Once an engine is chosen, Faith
listens for "Hey Faith", or for the phrase the owner set instead, only while
the owner has turned that listening on. The microphone is not held open when
listening is off.

Actions go through a small tool API, one Android intent per action, not a
blanket privilege. Each category has its own switch, off until the owner
turns that category on:

- Place a call
- Send a message
- Set or cancel an alarm
- Change a setting
- Launch an app

Answering a question does not require those switches. A category that is off
does not run, even if the model emits the tool call. The gate is code in our
process, checked before the intent is fired. Faith does not get a
signature permission that skips the switch.

### Device class

The model file, its runtime, the speech engines, and the key/value cache
share RAM with the system and the user's apps. On a phone with about 2–3 GB
of RAM that sum can push the low-memory killer into killing Faith or
the foreground app. On a flagship there is room for a larger local model.
Those are constraints, not measurements. ELE-OS has not recorded a resident
set, a tokens-per-second figure, or a word-error rate.

- **Flagship class.** The local model is the default path on a phone with
  room for it. The first runtime is llama.cpp on the CPU. A GPU backend or a
  vendor NPU is adopted only after a run on that device shows it works. A
  vendor accelerator is not a llama.cpp backend we can claim from the chip's
  name.
- **Low-RAM and many older phones.** The image carries a smaller local model
  when a measured run shows the phone still usable with it loaded, or the
  owner opts into a cloud model under the policy still to be written. If
  neither path has been measured, the device page says the local model is
  not available. Text, and any action categories the owner has enabled, can
  still exist without a large model. The page says Faith is present and
  that the local model is not.
- **Mid-range.** Whichever of those two paths the measurement supports. The
  class does not get the flagship model by default.

Listening for "Hey Faith", and keeping a large model loaded, both cost
battery. They stay off until the owner enables them, and Track B measures
them before either becomes a default.

### What this does not include

No quality comparison with Google Assistant, Siri, or Gemini. No claim that
every language Whisper or Piper can be pointed at will ship. No always-on
cloud transcript. No provider compiled in as a requirement. The default
model, the wake-word engine, and the rest of the cloud policy are listed in
[open-questions.md](open-questions.md).

## Performance strategy

Efficiency is a cross-device pillar. The low-RAM pillar is the same work
under the tightest constraint, not a separate OS.

Rules:

- Measure before and after, on the same device, against that device's stock
  OS (or against stock Cuttlefish, for the virtual product). Record the build
  variant, the workload, and the thermal conditions. The existing README
  already required this for any future comparison. It still holds.
- Publish the pairing. A result of no difference is a finished measurement.
- Do not take a Cuttlefish number and describe it as a phone result. Do not
  take a 2 GB result and describe it as a flagship result.
- `ele-status`'s memory snapshot is a point reading of `MemTotal` and
  `MemAvailable`. It is one input to a comparison, not a score.

Workloads the project has already named, and will use once a boot exists:
cold app start, time from boot to interactive, delivery of a background
notification, and battery across a stated scenario. Storage free after
install matters on small eMMC parts and is recorded as a number from that
device, not as a target invented here.

Levers that are real, and the class they belong to:

| Lever | Where it applies | Constraint |
| --- | --- | --- |
| Leave GMS out of the system image | Every class | Largest obvious source of resident vendor software on a stock Play image. Many apps depend on it. Removing it is a compatibility decision, not a free speedup. We have not measured the resident set. |
| Do not start services this project does not need | Every class | Each removed package needs a reason, and the accessibility checklist has to pass afterward. |
| Our own daemons, including a recovery check-in | Every class | A locate client that wakes the radio is an efficiency regression until its cost is measured. Default is off. |
| Faith's model and wake phrase | Every class, different weights | Both stay off until the owner enables them. The suggested phrase is "Hey Faith", and the owner can change it. Resident set is measured per class before a model becomes the default. A 2–3 GB phone may be unable to keep a flagship-sized model loaded. |
| `ro.config.low_ram` | Low-RAM products only | AOSP's [`lmkd`](https://source.android.com/docs/core/perf/lmkd) uses this property to distinguish low-RAM behavior from high-performance behavior. Setting it on a flagship product changes kill behavior and is not the default. The Cuttlefish product does not set it today. |
| zram and similar kernel swap compression | Where the kernel we boot supports it | Common on low-RAM Android products. Adopting it is a measured change per device, not a flag in this overlay. |
| Android Go configuration | Reference for the low-RAM pillar | Google's configuration, trademark, and certification. We may reuse the same kind of AOSP flags. We do not claim a Go device. |

Kernel tuning (governors, I/O schedulers) waits until we build that device's
kernel from source. Copying a vendor kernel binary and then claiming a
scheduler win is not a measurement we will publish. LineageOS's charter list
of allowed governors is a useful caution against shipping exotic schedulers;
it is not our charter.

## Device classes and support tiers

Classes say who the phone is for. Tiers say what we will stand behind.
A flagship can be Tier 1 if all we have is a boot log. A low-RAM phone can
be Tier 2 for years and never become Tier 3. The class does not promote the
tier.

**Classes**, all in scope:

- **Current flagship and upper tier**, including phones the vendor still
  updates. The owner is choosing keys and update policy. On firmware, the
  vendor may remain the faster source until they stop.
- **Mid-range**, the bulk of the market. Same pillars. Many of these phones
  unlock and still cannot relock onto a custom verified-boot key.
- **Low-end and older**, including phones around 2–3 GB of RAM and phones
  after the vendor's last security update. The revival pillar. Efficiency
  work is mandatory on this class. A 1 GB phone may be unable to run any
  Android release we can still patch; that is decided per device after a
  boot attempt, not by a cutoff in this document.

**Tiers:**

| Tier | Name | Who it is for | What we stand behind |
| --- | --- | --- | --- |
| 0 | Virtual | Cuttlefish `ele_os_cf_x86_64` | Component development. Not a phone. Verified-boot state here says nothing about a handset. The product makefile exists. A boot is not yet recorded. |
| 1 | Boot experiment | Any class | The image boots far enough to collect `ele-status`. Known-broken hardware is listed. No OTA. No theft lock. Bootloader stays unlocked. |
| 2 | Maintained | Any class whose bootloader unlocks and whose kernel we can build from source | Daily-driver candidate. Signed user or userdebug builds as the open-questions decision allows. OTA for the parts we rebuild, for as long as that branch is maintained. Encryption on, or the exception documented. Reset-surviving theft lock is not claimed. |
| 3 | Relockable | Devices that implement a user-settable verified-boot key and a rollback story we can demonstrate | Tier 2, plus the bootloader locked to ELE-OS, owner record outside userdata, setup gate on a user build. Hardware-specific hardening (memory tagging, secure element) is listed only when that board has it. This tier is where a flagship with a modern bootloader belongs if we support it. It is not implied by the price of the phone. |

No handset is in any tier yet, and none is named. ELE-OS has no phone
partner. Cuttlefish is the shared development target. The hardware image is
a Treble GSI, which can boot on many Treble-compliant phones and stays at
Tier 1 for each phone until a porter completes the onboarding kit. Tier 2
and Tier 3 arrive per device, when that device's evidence says so. A GSI
cannot be Tier 3: the bootloader is unlocked, and the kernel and vendor
image are not ours. GrapheneOS states the same limit for their own work.
They are a reference, not a partner, and ELE-OS does not ship their OS.

The onboarding kit is [device-support.md](device-support.md). A phone maker
or any porter can add a device later. Nothing in this repository is a
promise that a particular brand will boot.

## Licensing

This repository is Apache-2.0. AOSP userspace is predominantly Apache-2.0,
with other permissive licenses mixed in; AOSP states this in its
[license notes](https://source.android.com/docs/setup/contribute/licenses).
The kernel is GPL-2.0 and lives in kernel repositories, not in this tree.
GPL-3.0 code, including the current Piper TTS engine, is not linked into
this Apache-2.0 tree. MIT engines such as llama.cpp and whisper.cpp may be
vendored only with their copyright and license notices kept, and only when
the model weights beside them allow redistribution. Vendor blobs are
proprietary. A device that needs them is partly closed, and the device page
names the factory image they were extracted from.

Trademarks of Android, Google, Pixel, iOS, Apple, LineageOS, GrapheneOS,
CalyxOS, and others are not ours. Citing them is how we describe upstream
work.

## Related documents

- [Roadmap](roadmap.md)
- [Device support](device-support.md)
- [Build machines](build.md)
- [Open questions](open-questions.md)
