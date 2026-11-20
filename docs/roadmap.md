# Roadmap

ELE-OS is for flagships, mid-range phones, low-end phones, and older devices.
The work is organized as tracks so that a modern phone is not stuck behind
the old-phone bring-up, and so the old-phone work is not dropped in favor of
a flagship. A track is finished when the evidence listed here is in
[docs/evidence/](evidence/README.md), not when a sentence in this file sounds
done.

There are no calendar dates. There are no target percentages, frame rates, or
RAM savings. A measurement that shows no improvement, or a regression, closes
the measurement milestone and sends the change back.

The virtual device comes first because it is the only product registered in
the tree. That sequencing is a build constraint. It is not a statement that
Cuttlefish is the user, or that the first handset must be an old phone.

## How a milestone is recorded

An evidence note names the git revision, the device class, the tier being
claimed, the commands, and the output that matters. It says what was not
tested. Host tests are not handset tests. Cuttlefish is not a handset.
`ele-status` is not attestation.

`sh tests/run.sh` is the standing host check for the owner-proof library and
`ele-status`. It needs a C++17 compiler that can see the standard library,
and OpenSSL. When `c++` is a Clang without those headers, `CXX=g++ sh
tests/run.sh` is the command. On 2026-11-20 that command passed on a Linux
host with g++ 13 and OpenSSL 3.0.13. The pass covers the host fixtures only.

## Track A — Boot something we can name

**A0. Foundation, now.** The overlay, the host-side owner proof, `ele-status`,
the experimental peripherals app, and these documents.

Evidence: `CXX=g++ sh tests/run.sh` passes on a documented host. The lunch
target is registered. No image boot is claimed.

**A1. Cuttlefish boots `ele_os_cf_x86_64`.** On a host that meets the AOSP and
Cuttlefish requirements in [build.md](build.md), the pinned manifest
`android-16.0.0_r1` builds, `launch_cvd` starts, and
`sh tests/check-boot.sh SERIAL` prints `product=ele_os_cf_x86_64` and a
non-zero `memory_total_kib`. The same manifest's stock Cuttlefish phone
image is identified in the note so later comparisons have a baseline.

Evidence: the command transcript. A failed build is recorded as a failure,
with the missing host feature (KVM, RAM, disk), and does not count.

**A2. First handset, any class.** A maintainer with the device in hand
follows [device-support.md](device-support.md). The device page records class
and tier. Tier 1 is a boot with a broken-hardware list. Tier 2 adds the
maintained-device checklist (calls, mobile data, Wi-Fi, audio, display,
camera, app install, encryption), each marked pass or fail.

Evidence: a second person can install from the written steps, or the note
says they have not and the device stays Tier 1. The status text names this
device's class only. It does not say "phones" in general are supported.

**A3. A second class.** A flagship or mid-range boot if A2 was a low-RAM
phone, or a low-RAM boot if A2 was a recent phone.

Evidence: two device pages whose classes differ, each with its own tier.
Until this exists, the public status lists only the classes that have a
page. The project scope in the README stays the whole market; the evidence
section stays narrower than the scope. That split is deliberate.

A2 and A3 are not blocked on each other beyond "someone has the hardware."
A Tier 3 candidate (a phone that can relock with a user-set verified-boot
key) may be the first handset. Old hardware is not a prerequisite for new
hardware, or the reverse.

## Track B — Efficiency, per class

Depends on A1 for virtual results and on A2 for phone results.

**B1. Protocol.** Write down the workload (cold start of a named app, boot to
interactive, one notification path, one battery scenario), the variant
(`user` or `userdebug`), and the thermal note. Include the stock comparison
target.

Evidence: the protocol file under `docs/evidence/`. No numbers required yet.

**B2. Paired run on Cuttlefish.** ELE-OS product versus stock Cuttlefish on
the same host and the same manifest tag.

Evidence: both `ele-status` snapshots and the workload timings from the
protocol. The conclusion may be that the overlay changed nothing, which is
the expected result until a later change removes or adds something.

**B3. Paired run on a handset.** Same protocol against that phone's stock OS,
before any "lean" change.

Evidence: the pairing, labeled with the device class. One class's result is
not quoted for another class.

**B4. One configuration change, then the pairing again.** Examples that are
in scope: a package not started, a daemon not added, `ro.config.low_ram` on
a low-RAM product only. The accessibility checklist from track F is re-run.
A change that breaks calls, notifications the protocol cares about, or
accessibility is reverted.

Evidence: the change list with a reason per item, the new pairing, and the
accessibility result. No numeric target is set in advance.

The revival pillar is B4 on a low-RAM or post-support device, plus track C's
updates for that device. It is not a separate roadmap that has to finish
before a flagship build exists.

## Track C — Signed updates, then modular updates

Depends on A2 at Tier 2 for a phone OTA. Packaging can be rehearsed on
Cuttlefish earlier.

**C1. Release keys exist offline.** Public certificates and their
fingerprints are published. Private keys are not in git. The Cuttlefish
development image remains on AOSP test-keys and is labeled that way.

Evidence: fingerprint of the public key, and a note of where the private key
is held. No private material in the repository history.

**C2. Full OTA on one maintained device.** Version N installs, version N+1
installs over it, user data remains, and a package signed by a different key
is rejected. The note states whether the layout is A/B, virtual A/B, or
non-A/B recovery, because downtime and rollback behavior differ. Release
notes split platform patch, vendor patch, and firmware.

Evidence: the two fingerprints and the rejected-package result. A device
that does not boot after the update has not met C2.

**C3. One modular update, where the Android version supports APEX.** Ship a
single signed module update on the ELE-OS channel, or record why this device
cannot (pre-Android 10, vendor image rejects the module, partition size).
Google's Mainline stream for Android 11 and lower ended in Q4 2025; C3 is
our package, not that stream.

Evidence: the module name, the Android version, and either a successful
install-and-rollback or an explicit "full OTA only" line on the device page.
Both outcomes are valid. A flagship on a recent Android release is the
device class where a successful module update is most plausible. A pre-Treble
phone is the class where "full OTA only" is the honest result.

## Track D — Sandbox baseline and permission controls

The sandbox is AOSP's. The milestone is that we have not weakened it, and
then that one extra control works.

**D1. Baseline on the first user or userdebug build we actually boot.**
SELinux is enforcing. The image does not contain `su`. A userdebug image is
labeled userdebug, because `adb root` is a hole in any setup gate.

Evidence: `getenforce` and the package list check, pasted in the evidence
note. This is pass/fail, not a hardening score.

**D2. One extra permission control on Cuttlefish.** The working assumption,
until the maintainer picks otherwise, is a per-app network control modeled
on the idea of GrapheneOS's network permission: the app's existing
`INTERNET` grant, denied in a way the app treats as network-down, rather than
a packet filter alone. CalyxOS's Datura is the other precedent, with per
transport toggles. We reimplement. We do not vendor either project's code in
this milestone.

The control is on by default for compatibility, matching those projects'
published choice, unless a written compatibility note justifies default-deny.
An app that is allowed network can open a socket. An app that is denied
cannot. The baseband is unaffected, and the note says so.

Evidence: the two app results on Cuttlefish, plus a list of behaviors that
changed (sync, push, login). Not a phone result until repeated on a device
in A2.

**D3. Repeat D2 on each maintained device class** before the control is
described as shipping there. Low-RAM devices get the same control only if
B4 says the memory cost is acceptable. If it is not, the device page says
the control is absent, and the image does not advertise it.

Hardware memory tagging and other chip features are not a track of their
own. A Tier 3 device page may add one, with the chip's capability and a test,
when someone does that work. Pixel 8-class memory tagging is not assumed on
other flagships.

## Track E — Owner recovery

Depends on the host library, which exists, and on A1 before any virtual
end-to-end claim. Depends on Tier 3 before anyone says a wipe cannot defeat
the lock.

**E1. Keep the host contract.** Every change to `activation/` passes
`tests/run.sh`, including the reset fixture, replay, wrong device, and torn
record.

Evidence: the test output at that revision.

**E2. Gate on Cuttlefish.** A record stored outside the wiped userdata
directory blocks a setup stand-in until a valid proof, and a wrong key does
not pass. This still uses a file the emulator can see. It does not prove a
handset theft lock.

Evidence: the session transcript and the path of the record relative to the
wiped directory.

**E3. Device storage.** On a real phone, document where the record lives and
what a normal factory reset does to it. Then document what `fastboot` can
erase while the bootloader is unlocked. If the record dies with userdata,
the device stays at "ownership does not survive a reset" and Tier 3 is not
claimed.

Evidence: the two experiments, command by command.

**E4. Tier 3 theft lock, only on a qualifying bootloader.** Relocked device
reports the yellow verified-boot state when using a user-set key (or the
device's documented equivalent). An unsigned or differently signed image does
not boot. Factory reset of userdata still demands the owner key. The image
is a user build. Transfer of the owner key to a new owner is implemented
before anyone but the developer enrolls.

Evidence: boot-state properties, the rejected image, the post-reset prompt,
and a transfer test. If no such phone is in the project, E4 stays open and
no user-facing text says the theft lock exists.

**E5. Locate, lock, and wipe commands.** After E2's gate exists, add signed
commands with purpose strings other than `ELE-OS/reactivate/v1`. Locate is
off unless the owner enables it. The payload is encrypted to the owner key.
The endpoint is one the owner configured. Measure the check-in against the
Track B protocol before the default interval is chosen. There is no default
interval in this document.

Evidence: a rejected replay, a rejected command signed by the wrong key, a
locate payload that is unreadable without the owner key, and the battery
note from the protocol. A self-hosted endpoint is enough. A project-operated
server is not part of this milestone.

## Track F — Accessibility

Applies to every class, including the lean low-RAM image.

**F1. Rules for our own UI.** Layouts use start/end constraints, text comes
from resources, font scale can grow, and a control's name is available to an
accessibility service. The External devices app is the only UI in the tree
today. Its strings currently live in the activity source. It has not had an
on-device accessibility pass. A change that internationalizes it has to meet
F1 and still has to be checked on a device.

Evidence: review against those rules for each new UI. Automated host checks
count when they exist. They do not replace a device pass.

**F2. A screen reader on Cuttlefish.** Select an open screen reader, put it
in the product, and show that it reads the setup UI and one settings screen.
A plain AOSP lunch target does not guarantee Google's TalkBack binary is
present. "The framework has accessibility APIs" does not close F2.

Evidence: the component name and a recorded session. TalkBack on a stock
Pixel is not this result.

**F3. The lean-image rule.** No B4 change removes the accessibility framework
or the screen reader chosen in F2. If a low-RAM build cannot boot with that
reader, the device page says accessibility is unmet and the build is not
offered as a daily driver.

Evidence: F2's reader still starts after the lean change, on that class of
device.

Contrast, magnification, and captions already in AOSP stay in the product
unless a note explains a replacement that has been tested. We are not
claiming a certification, and we are not claiming a better reader than
TalkBack or VoiceOver.

## Track G — What we will not schedule as a promise

- Beating stock Android or iOS on camera, app compatibility, or raw speed.
- Play Integrity, or banking apps that require it.
- Patching baseband firmware we cannot obtain.
- Reset-surviving theft lock on unlocked-only bootloaders.
- A project account that can locate a phone.
- Android Go certification.
- Support for a device whose bootloader cannot be unlocked by a method we
  can legally document.

## Suggested order

The dependencies above allow this order without hiding a class of device:

1. A1, B1, D1's definition, F1.
2. In parallel: B2, E2, F2, and the search for the first handset (A2).
3. A2's class drives B3, C2, D3, and either the revival case of B4 or a
   flagship case of C3.
4. A3 fills the class we do not have yet.
5. E4 only if a Tier 3 device is actually on the desk.

The maintainer can swap A2's class. The evidence rules do not change if they
do.
