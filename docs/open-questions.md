# Decisions and open questions

The scope is not open. ELE-OS is for current flagships, mid-range phones,
low-end phones, and older devices. Old and low-end revival is one pillar.

The seven questions the owner asked us to close are decided below. The
reasoning stays so a later change has to say what fact changed. Items after
that are still open. A working assumption is not a locked vote.

## Decided

### 1. GitHub description

**Decision.** Widen it. The owner sets this text in the repository settings:

> Open-source Android-based mobile OS for every phone class: efficient, long-term signed updates, privacy and owner-controlled lost-device recovery.

**Why.** The old description said "affordable phones," which hid flagships
and mid-range devices. This sentence names the market and the four jobs
without claiming a win over Android or iOS overall. We do not change GitHub
settings from a pull request.

### 2. Devices

**Decision.** ELE-OS does not target a Google Pixel, or any other named
phone. There is no phone partner. Cuttlefish (`ele_os_cf_x86_64`) is the
shared development target until a boot is recorded, and it stays the place
shared components are proven. The hardware image is a Treble GSI
(`ele_os_gsi_arm64`, not in the tree yet) so one system build can boot on
many Treble-compliant phones. A phone maker or a porter adds a specific
device later by following the onboarding kit in
[device-support.md](device-support.md).

**Why.** A partner phone would make the project look tied to one vendor.
The owner has none. A GSI is the honest way to reach many phones before
anyone maintains a device tree. The limit is part of the decision: a GSI
keeps the phone's vendor image, kernel, and firmware, needs an unlocked
bootloader, and cannot be Tier 3. Reset-surviving theft lock and a kernel
we build wait until a porter finishes the kit for that model.

**What was withdrawn.** An earlier decision named the Pixel 8 (`shiba`) as
the first handset and aimed it at Tier 3. That choice is cancelled. Pixel
devices remain eligible later, on the same terms as any other phone. They
are not the plan.

### 3. Release keys

**Decision.** The repository owner holds the OS release keys. They are
generated on an offline machine or a hardware token, when the first release
build is actually prepared. An encrypted backup of the private bundle is
stored in a separate physical location. The passphrase is stored apart from
that ciphertext, with a sealed copy for one named successor. The successor
may sign an already-built update or publish a retirement note with the
public fingerprint. CI, GitHub Actions, and this repository never receive
production private keys. Development and CI images use AOSP test-keys only.

**Why.** One holder is simpler than a committee that has not been named.
Offline generation plus a second physical copy covers loss of the laptop
without putting the keys where a CI log can leak them. Naming the successor
in a private paper record, not in git, covers the owner being unavailable
without inventing a third party who can unlock phones. If both the owner and
the successor are unreachable, updates stop. Replacing the AVB key on a
locked phone requires unlocking, which wipes user data. The phone owner's
Ed25519 recovery key is a different key and stays with the phone's owner.

The steps are written in the architecture doc. No keys have been generated.

### 4. Support length

**Decision.** Each device stays on a maintained branch while its vendor
ships firmware and security updates. The branch tracks those releases as
often as we can build them. After the vendor stops, the branch is labelled
extended and contains only the fixes we can actually deliver.

**What "monthly where possible" can mean.** Some vendors ship firmware
binaries about monthly. We take a drop when that vendor publishes one we can
install, and only for a device someone has onboarded. A GSI does not get
those binaries. Platform source is a different schedule. AOSP's 2026
documentation says source is published in Q2 and Q4. A monthly security
bulletin without a published source tag is not a monthly platform update.
Release notes say which of platform source, vendor binary, or neither moved.
The maintained window is whatever end date that vendor publishes, written
on the device page when the device is added. ELE-OS does not set one year
for every phone.

### 5. First extra permission

**Decision.** A per-app Network on/off toggle, in the spirit of GrapheneOS's
network permission. Granted by default. Denial looks to the app like the
network is down. It ships on every class, including low-RAM images, once it
is built. The per-transport firewall (CalyxOS Datura's model) comes later.

**Why.** One switch is the smallest control an owner can understand. It
reuses the `INTERNET` permission apps already declare. A transport matrix is
more UI and more policy for a first version. The toggle does not exist in
the tree yet, and it does not constrain the baseband. We reimplement the
behavior. We do not vendor GrapheneOS code for this milestone.

### 6. Screen reader

**Decision.** Ship a build of TalkBack from
[google/talkback](https://github.com/google/talkback) on every class,
including low-RAM images.

**License check.** That repository's `LICENSE` is the Apache License 2.0,
the same license as this tree. The GitHub project describes itself as the
open-source incarnation of TalkBack. Source is published in batches. At the
time of this decision the latest push on the default branch was 2026-03-23,
while newer builds have shipped on Play as Android Accessibility Suite. We
pin a commit, keep upstream's license and copyright, and build the APK. We
do not ship the Play binary. If a later drop is no longer Apache-2.0, we
stop and choose another open reader before installing it. TalkBack is not in
the image yet. A low-RAM image that cannot boot with it is not offered until
that conflict is fixed.

### 7. External devices app

**Decision.** Keep `peripherals/` as an optional experiment. It is outside
the core roadmap and not in the default image. `PRODUCT_PACKAGES` installs
`ele-status` only. A local build can add `ElePeripherals`.

**Why.** The camera preview and the satellite-transport line are useful
experiments and are not pillars. Leaving the sources in the tree keeps the
work. Installing the app on every phone would make an unvalidated experiment
look like a feature.

## Open: Faith

Faith is the built-in assistant. The name is decided. Three choices are not:
the default model, the wake-word engine, and the cloud policy. Constraints
that are already fixed sit in the architecture doc: on-device first, no
Google backend, MIT runtimes for the language model and for Whisper-class
speech recognition, GPL-3 Piper not linked into the system image, action
categories off until the owner enables each one, and no published quality or
speed numbers until a run exists.

### Name

**Decided.** The assistant is named Faith.

**Why.** The owner chose it. The docs use that name rather than "the
assistant" alone. It is not Google Assistant and not Siri. We have not
commissioned a trademark search; if a conflict shows up later, the owner
picks the replacement.

### Wake phrase

**Decided as the default, and changeable.** A new install listens for
"Hey Faith" once the owner turns listening on. Settings let the owner
replace that phrase. The wake-word engine is still unchosen. An engine that
cannot be aimed at "Hey Faith", and at a phrase the owner types instead,
does not qualify.

### Default model

Unset. llama.cpp (MIT) is the runtime to build against. The weight file is a
separate license. A model is eligible only when its license allows us to
redistribute it in an OS image and the Cuttlefish run in roadmap H1 records
its memory cost. Flagship and low-RAM builds may ship different weights. No
family (Llama, Gemma, Qwen, or any other) is selected.

### Wake-word engine

Unset. The phrase is "Hey Faith" unless the owner changes it. The detector
has to be open source, on-device, and off until the owner enables it.
openWakeWord (Apache-2.0) is eligible to evaluate. Picovoice Porcupine and
other proprietary detectors are not. The choice waits on H1–H3 so listening
is not the first thing we build.

### Cloud policy

Partly constrained, not settled. Cloud inference is off unless the owner
opts in, and the provider is pluggable: the owner supplies the endpoint.
No Google endpoint ships. Still open: whether the image may list any named
provider at all, or only a blank endpoint; whether a request may send audio
or only text the on-device recognizer already produced; and what the owner
is told about retention before the first request. Until those are decided,
roadmap H4 does not test a cloud path.

## Still open

### Android version pin

The instructions pin `android-16.0.0_r1`. AOSP's current documentation says
source is published in Q2 and Q4 and that `android-latest-release` tracks the
newest drop.

Working assumption: do not move the pin until a Cuttlefish boot is recorded
on the current tag. The GSI tracks that same pin until a boot says otherwise.
An onboarded device uses the newest AOSP release that device's vendor code
can boot. Moving the virtual product to `android-latest-release` is a
separate change because the branch moves underneath comparisons.

### Starting a device tree from an existing port

Working assumption: when a porter onboards a phone, they may reuse an
existing AOSP or LineageOS device tree when the license allows, build it
against the AOSP tag that phone can boot, and not import that project's
release keys, updater, or policies ELE-OS has not adopted. LineageOS's
charter requires userdebug and disables system verity on those builds.
Tier 3 images for ELE-OS are user builds.

### Google software and app compatibility

Working assumption: the system image contains no Google Mobile Services, no
microG, and no sandboxed Play. Apps that need those stacks will fail or lose
push. Revisit when a named set of apps fails and the owner accepts the
security cost of signature spoofing (microG) or of shipping Play in a
sandbox (the GrapheneOS approach).

### Modular updates in the first release

Working assumption: the first update a device gets is a full signed OTA.
An APEX or Mainline-style module update is a follow-on, and only on Android
versions that can install one. Google's Mainline stream for Android 11 and
lower ended in Q4 2025.

### Recovery transport

Working assumption: the phone's owner configures the endpoint, including a
self-hosted one. ELE-OS does not run a location server. Lost mode is off
until that owner turns it on. The owner recovery key never leaves them.

### Lost owner recovery key

Working assumption: the recovery key is not the daily PIN. Losing it blocks
post-wipe reactivation and remote commands, and does not lock a phone the
owner can still unlock locally. There is no project escrow. Strict theft
lock is opt-in at enrollment, with that warning on the screen.

### userdebug versus user

LineageOS ships official builds as userdebug. A theft lock and a setup gate
do not hold on a build where `adb root` works.

Working assumption: userdebug for Cuttlefish, for the GSI bring-up, and for
Tier 1. A user build is required before any onboarded device is described as
a daily driver with a lock, and before Tier 3.

### Reproducible builds

Not a milestone. AOSP images are difficult to reproduce bit for bit.
Working assumption: publish the manifest tag, the ELE-OS git revision, and
the release-key fingerprint first. Bit-for-bit reproducibility waits until a
boot and an OTA exist.
