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

Cuttlefish stays the shared target until `ele_os_cf_x86_64` boots. The first
real phone after that is a Pixel 8 (`shiba`). A low-RAM or older phone is a
later milestone. That order is a bring-up decision. The project scope stays
the whole market.

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
and these documents. The External devices app is in the tree and out of the
default image. It is not a milestone.

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

**A2. First handset: Pixel 8 (`shiba`).** Starts only after A1. The unit must
be one on which OEM unlocking can be turned on. The device page records class
(current flagship) and the tier the evidence supports. The aim is Tier 3.
Until relock, a custom AVB key, and the theft-lock checks pass, the page
stays at Tier 1 or Tier 2. Tier 1 is a boot with a broken-hardware list.
Tier 2 adds the maintained-device checklist (calls, mobile data, Wi-Fi,
audio, display, camera, app install, encryption), each marked pass or fail.

Evidence: a second person can install from the written steps, or the note
says they have not and the device stays Tier 1. The status text names the
Pixel 8. It does not say phones in general are supported.

**A3. A later low-RAM or older phone.** Starts after A2 has a recorded boot.
This is the second class, not a second first device.

Evidence: a device page whose class is low-end or older, with its own tier.
Until that page exists, the public status lists Cuttlefish and, once it
boots, the Pixel 8. The README scope stays the whole market.

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
that does not boot after the update has not met C2. The private key used to
sign the package was not available to CI. The public fingerprint matches the
one the owner generated offline.

**C2a. Support window.** The Pixel 8 branch stays maintained while Google
still ships firmware and security updates for it: seven years from US store
availability, October 2023 through October 2030, unless Google publishes
another end date. Each release note says whether the platform source tag
moved, whether a Pixel firmware binary moved, or whether that month had
nothing we could build. AOSP source publication in 2026 is Q2 and Q4, so a
monthly bulletin without a source tag is not labelled as a full platform
update. After the vendor window, the branch is labelled extended and the
note lists only the fixes that image contains.

Evidence: the end date on `docs/devices/shiba.md`, and one release note that
uses the split above. The page does not exist until A2 starts.

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

**D2. Per-app Network toggle on Cuttlefish.** Decided. One on/off control per
app, reimplemented from the idea of GrapheneOS's network permission: it is
the app's existing `INTERNET` grant, and a denial is presented to the app as
the network being down. It is granted by default so install compatibility
holds. We do not copy GrapheneOS source in this milestone. CalyxOS's Datura
per-transport firewall is a later control, not this one.

An app that is allowed network can open a socket. An app that is denied
cannot. The baseband is unaffected, and the note says so.

Evidence: the two app results on Cuttlefish, plus a list of behaviors that
changed (sync, push, login).

**D3. The same toggle on every maintained class, including low-RAM.** The
control is part of the product on the Pixel 8 and on the later low-RAM
image. Measure its cost with the Track B protocol and publish the number.
A class where the toggle cannot be implemented says so on the device page
and does not advertise the control.

**D4. Per-transport firewall.** After D3 has shipped, a Datura-style choice
of Wi-Fi, mobile data, and VPN can be designed. It is not the first permission
and it is not required for D2 to count as done.

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
accessibility service. Product UI follows these rules. The External devices
app is an optional experiment, not the product UI. Its strings currently live
in the activity source. A change that builds it still follows F1, and still
needs a device check before anyone calls that app accessible.

Evidence: review against those rules for each new product UI. Automated host
checks count when they exist. They do not replace a device pass.

**F2. TalkBack on Cuttlefish, from source.** Build the screen reader from
[google/talkback](https://github.com/google/talkback), which is Apache-2.0,
pin the commit, and keep upstream's `LICENSE` and copyright. Install that
APK in the product. Show that it reads the setup UI and one settings screen.
Do not ship the Play Store "Android Accessibility Suite" binary. Public
source is pushed in batches (the tree's latest push at the time of this
decision was 2026-03-23) and can lag the Play build. A newer Play version
with no matching source is not ours to ship. If a future drop changes the
license away from Apache-2.0, stop and pick another open reader before
shipping it. "The framework has accessibility APIs" does not close F2.

Evidence: the pinned commit, the license file in the build inputs, and a
recorded session. TalkBack running on a stock Pixel is not this result.

**F3. The same reader on every class, including low-RAM.** No B4 change
removes the accessibility framework or the TalkBack build from F2. The
low-RAM image includes it. If a low-RAM build cannot boot with it, that
image is not offered until the conflict is fixed. Shipping a silent build
without the reader is not the fix.

Evidence: F2's reader still starts after the lean change, on Cuttlefish with
a low-RAM product if one exists, and on the later low-RAM handset.

Contrast, magnification, and captions already in AOSP stay in the product
unless a note explains a replacement that has been tested. We are not
claiming a certification, and we are not claiming a better reader than a
current Play Store TalkBack or than VoiceOver.

## Track H — Built-in assistant

Depends on A1 before any on-device run is claimed. The design and the license
limits are in the architecture doc. No milestone below is done.

**H1. Text in, text out, on Cuttlefish, with a local model.** A small
language model runs through an MIT-licensed runtime (llama.cpp unless a later
decision names another). No wake word, no microphone, no cloud. Record the
process resident set from the system, and record which weight file was used
and under which license. Publish no quality score and no tokens-per-second
target. If the model file's license does not allow redistribution, it is not
a candidate.

Evidence: the command transcript, the license of the runtime and of the
weights, and the memory reading. A crash or an out-of-memory kill is a
failed run and is written down as one.

**H2. On-device speech.** Speech-to-text from a MIT Whisper-class engine, and
a spoken reply from a synthesizer we are allowed to ship. The current Piper
engine is GPL-3.0-or-later and stays out of the Apache-2.0 image. A
Lessac-derived Piper voice is not the default. AOSP Pico TTS (Apache-2.0)
may speak in this milestone. A neural voice replaces it only when that
voice's model card allows redistribution.

Evidence: a spoken prompt becomes text, and a reply is spoken, on
Cuttlefish. The note names the engines and the commits.

**H3. Permissioned actions.** Calls, messages, alarms, settings, and app
launch are separate switches, each off until the owner enables that
category. A tool call for a category that is off does not fire the intent.
The test uses a fixture contact and a fixture alarm, not a live carrier.

Evidence: one allowed action and one refused action per category, on
Cuttlefish.

**H4. Class split.** On the Pixel 8, repeat H1 with the local model and
record memory beside the Track B protocol. On a low-RAM image, repeat H1
with a smaller model. If that run is killed or leaves the phone unusable
under the protocol, the device page says the local model is unavailable.
Opt-in cloud, if the open cloud policy allows a provider at all, is tested
only with a provider the owner configured, and the note lists what left the
device (text, audio, or both).

Evidence: two memory readings, labelled by device class. No cross-class
comparison presented as a score.

**H5. Wake word.** Starts after the engine is chosen. Listening is off until
the owner enables it. The detector runs on the device. Proprietary engines
are out. Battery and extra RAM are part of the Track B note before the
detector is on by default. The default stays off.

Evidence: the wake word starts a session when enabled, and the microphone
path is idle when it is disabled.

## Track G — What we will not schedule as a promise

- Beating stock Android or iOS on camera, app compatibility, or raw speed.
- Play Integrity, or banking apps that require it.
- Patching baseband firmware we cannot obtain.
- Reset-surviving theft lock on unlocked-only bootloaders.
- A project account that can locate a phone.
- Android Go certification.
- Support for a device whose bootloader cannot be unlocked by a method we
  can legally document.
- The External devices app. It stays in the tree as an optional experiment
  and is not part of this roadmap.
- A claim that the assistant matches Google Assistant, Siri, or any cloud
  model on quality or speed.
- A Google speech, Gemini, or other Google-hosted assistant backend.
- Linking the GPL-3 Piper engine into the Apache-2.0 system image.

## Suggested order

1. A1, B1, D1's definition, F1.
2. In parallel once A1 exists: B2, E2, F2, H1, then A2 on a Pixel 8 whose OEM
   unlocking toggle works. H2 and H3 follow H1 on Cuttlefish. H4 waits until
   the Pixel 8 and, later, the low-RAM device can take a measurement. H5
   waits on the wake-word decision.
3. A2 then drives B3, C2, C2a, D3, and E4. E4 is in scope for this phone
   because the hardware can relock. It is still unmet until the evidence
   exists.
4. A3, the low-RAM or older phone, after A2 has booted. B4's low-RAM case
   waits for A3. F3 on a handset waits for that device too.

The Pixel 8 is the first handset. Swapping it for another model means
updating the decided entry in [open-questions.md](open-questions.md) with a
kernel-source URL and a relock check of the same kind.
