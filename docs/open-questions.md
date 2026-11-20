# Open questions

Decisions the maintainer still has to make. Each one has a working
assumption so contributors do not block on silence. The assumption is not a
locked vote. Override it in a commit that updates this file.

The scope itself is not open. ELE-OS is for current flagships, mid-range
phones, low-end phones, and older devices. Old and low-end revival is one
pillar.

## 1. GitHub description

The repository description still says "affordable phones." The README now
says the wider market.

Working assumption: leave the GitHub string until the maintainer edits it.
Suggested replacement, when they want one: "Open-source mobile OS research
for phones across the market, focused on efficiency, signed updates, privacy
and owner control, and owner-controlled lost-device recovery."

## 2. Which phone is first

Working assumption: the first handset is whichever phone a maintainer has,
can unlock, and can get kernel source for. Record its class. Do not describe
that one phone as coverage of the market. A Tier 3 flagship is a valid first
device. A 3 GB phone after vendor support is a valid first device.

## 3. When a second class is required

Working assumption: the status page lists only classes that have an evidence
note. A second class is roadmap milestone A3. It does not block the first
boot, and it does block any sentence that says we already serve the whole
range in practice.

## 4. Android version pin

The instructions pin `android-16.0.0_r1`. AOSP's current documentation says
source is published in Q2 and Q4 and that `android-latest-release` tracks the
newest drop.

Working assumption: do not move the pin until a Cuttlefish boot is recorded
on the current tag. Handset trees then use the newest AOSP release that
device's vendor code can boot, which may be older than the Cuttlefish pin.
Moving the virtual product to `android-latest-release` is a separate change
because the branch moves underneath comparisons.

## 5. Starting a handset tree from LineageOS

Working assumption: reuse an existing device tree when the license allows,
build it against the AOSP tag we can boot, and do not import LineageOS
release keys, their updater, or a policy we have not adopted (their charter
requires userdebug and disables system verity on those builds).

## 6. Google software and app compatibility

Working assumption: the system image contains no Google Mobile Services, no
microG, and no sandboxed Play. Apps that need those stacks will fail or lose
push. Revisit when a named set of apps fails and the maintainer accepts the
security cost of signature spoofing (microG) or of shipping Play in a
sandbox (the GrapheneOS approach). That revisit is per the compatibility
goal, not a quiet default.

## 7. First extra permission control

Working assumption: design a per-app network control in the spirit of
GrapheneOS's network permission, granted by default so install compatibility
holds, with the denial mode documented. CalyxOS Datura's per-transport
toggles are the alternative if the maintainer would rather have a firewall
UI. Neither control exists in the tree. The baseband stays out of reach of
both.

## 8. Screen reader

Working assumption: every class of device we call a daily driver ships a
screen reader, including the low-RAM image. Which open component that is has
not been chosen. Removing it to save memory is not an option; if the low-RAM
image cannot boot with it, that image is not offered as maintained.

## 9. Modular updates in the first release

Working assumption: the first update a device gets is a full signed OTA.
An APEX or Mainline-style module update is a follow-on, and only on Android
versions that can install one. Pre-Treble phones and, in practice, many
post-support phones stay on full OTAs. Google's own Mainline stream for
Android 11 and lower ended in Q4 2025.

## 10. Recovery transport

Working assumption: the owner configures the endpoint, including a
self-hosted one. ELE-OS does not run a location server. Lost mode is off
until the owner turns it on. The owner key never leaves the owner.

## 11. Lost owner key

Working assumption: the owner key is not the daily PIN. Losing the key
blocks post-wipe reactivation and remote commands, and does not lock a phone
the owner can still unlock locally. There is no project escrow. Strict
theft lock is opt-in at enrollment, with that warning on the screen. Phones
can ship with the recovery features unused.

## 12. Who holds the OS release keys

Not decided. Working assumption: nobody generates them until track C starts,
they are generated offline, and the public fingerprint is committed without
the private key. A single-maintainer project should say what happens to the
keys if that person is unavailable. That operational answer is still open.

## 13. How long updates last

Not decided. The constraint is technical: platform patches follow the AOSP
source Google publishes (Q2 and Q4 in the 2026 docs), vendor and firmware
patches follow what we can rebuild or what the vendor still signs, and a
device branch can be declared done. Working assumption: each device page
states the branch and the date of the last build we actually shipped, and
we do not print a multi-year promise until at least one OTA has landed.

## 14. External devices app

The Camera2 external-camera preview and the satellite-transport line are in
the tree and are not pillars.

Working assumption: they stay experimental. They can be removed or split out
if they start blocking the product. They are not a commitment to satellite
messaging or to a particular camera.

## 15. userdebug versus user

LineageOS ships official builds as userdebug. A theft lock and a setup gate
do not hold on a build where `adb root` works.

Working assumption: userdebug for development and for Tier 1. A user build
is required before a device is described as a daily driver with a lock, and
before Tier 3.

## 16. Reproducible builds

Not a milestone. AOSP images are difficult to reproduce bit for bit.
Working assumption: publish the manifest tag, the ELE-OS git revision, and
the release-key fingerprint first. Bit-for-bit reproducibility waits until a
boot and an OTA exist.
