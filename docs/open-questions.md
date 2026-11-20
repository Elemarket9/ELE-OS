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

### 2. First device

**Decision.** Cuttlefish (`ele_os_cf_x86_64`) stays the shared target until a
boot is recorded. The first real phone is a Google Pixel 8, codename `shiba`,
aimed at Tier 3. A low-RAM or older phone is a later milestone (roadmap A3),
after that Pixel has booted.

**Why this model.** It has to be a recent Pixel with a relockable verified
boot, a user-set custom key, published kernel source, and strong upstream
support. AOSP's Pixel kernel guide (updated 2026-07-13) lists `shiba` and the
Pixel 8 Pro (`husky`) on `android-gs-shusky-6.1-android16`. Pixels enroll a
custom key with `avb_custom_key`. GrapheneOS and CalyxOS already relock
production builds on `shiba`. Pixel 8 and later have hardware memory tagging.
Google's update policy is seven years from first US store availability;
the Pixel 8 went on sale in October 2023, so the vendor window runs through
October 2030.

**Why not the neighbours.** The Pixel 8 Pro shares that kernel and adds
camera hardware the first port does not need. The Pixel 8a has its own kernel
branch and a later vendor end date (May 2031 on CalyxOS's table), with less
accumulated port history than `shiba`. Pixel 9 and later have longer
remaining vendor support and can be later targets. That same kernel guide
stops at the Pixel 9a and does not list a Pixel 10 or Pixel 11 branch, so
those models are not the first port until their source URL is checked the
same way. A carrier unit whose OEM unlocking switch cannot be enabled cannot
be the unit we use.

`shiba` is not supported yet. There is no device tree and no boot log.

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

**What "monthly where possible" can mean.** Pixel firmware drops have
historically been monthly, and we take a drop when Google publishes a binary
we can install. Platform source is a different schedule. AOSP's 2026
documentation says source is published in Q2 and Q4. A monthly security
bulletin without a published source tag is not a monthly platform update.
Release notes say which of platform source, vendor binary, or neither moved.
For the Pixel 8 the maintained window is October 2023 through October 2030
under Google's seven-year policy, unless Google publishes another end date.

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

## Still open

### Android version pin

The instructions pin `android-16.0.0_r1`. AOSP's current documentation says
source is published in Q2 and Q4 and that `android-latest-release` tracks the
newest drop.

Working assumption: do not move the pin until a Cuttlefish boot is recorded
on the current tag. The Pixel 8 tree then uses the newest AOSP release that
device's vendor code can boot. Moving the virtual product to
`android-latest-release` is a separate change because the branch moves
underneath comparisons.

### Starting the Pixel tree from an existing port

Working assumption: reuse an existing AOSP or LineageOS device tree for
`shiba` when the license allows, build it against the AOSP tag we can boot,
and do not import that project's release keys, updater, or policies we have
not adopted. LineageOS's charter requires userdebug and disables system
verity on those builds. Tier 3 images for ELE-OS are user builds.

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

Working assumption: userdebug for Cuttlefish development and for Tier 1. A
user build is required before the Pixel 8 is described as a daily driver
with a lock, and before Tier 3.

### Reproducible builds

Not a milestone. AOSP images are difficult to reproduce bit for bit.
Working assumption: publish the manifest tag, the ELE-OS git revision, and
the release-key fingerprint first. Bit-for-bit reproducibility waits until a
boot and an OTA exist.
