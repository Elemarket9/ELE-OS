# ELE-OS

ELE-OS is open-source mobile OS research for the wider phone ecosystem:
current flagships, mid-range phones, low-end phones, and older devices that
still work. It is built on open Android so ordinary Android apps can run.
The aim is to be better than stock Android and iOS at specific jobs an owner
can name. It is not a claim to beat either system overall, and it does not
run on Apple hardware.

The GitHub description still says "affordable phones." The scope in this
document is the whole market. Widening that description is an open decision
in [docs/open-questions.md](docs/open-questions.md).

## Why

A phone is a computer the owner already paid for. Two facts get in the way of
the owner actually controlling it.

The vendor decides the update key, the account, and the recovery service.
Find My Device and iCloud Activation Lock are useful, and they require Google
or Apple. Background software is whatever the vendor image starts. That is
true of a phone bought this year, not only of a phone the vendor has
abandoned.

Vendors also stop shipping updates while the hardware still places calls,
runs apps, and holds a charge. The phone then stays in use on its last patch,
or it is replaced and becomes waste. Both outcomes are a software-support
failure. Giving those phones a lean, signed system is one job of ELE-OS. It
is a pillar, not the identity of the project. A flagship owner who wants
their own keys, and the owner of a low-RAM phone that would otherwise be
discarded, are both in scope.

Nothing in the defaults assumes a country, a carrier, or a language. Emergency
calling, radio bands, and locale stay with the device and with Android's own
per-country configuration.

## Pillars

These apply on every device class we take on. What the hardware can enforce
is spelled out in [docs/architecture.md](docs/architecture.md) and
[docs/device-support.md](docs/device-support.md).

### Performance efficiency

Less wasted background work, memory, battery, and storage. On a phone with
about 2–3 GB of RAM this decides whether the phone stays pleasant to use. On
a flagship it shows up as heat, battery life, and how quickly the system gets
out of the way. Comparisons are paired: the same device, workload, build
variant, and thermal conditions, against the stock system. A published result
can be "no meaningful difference" or a regression. This tree does not contain
performance numbers.

### Security and signed updates

Signed over-the-air updates, verified boot where the bootloader can enforce
it, and hardening in the spirit of [GrapheneOS](https://grapheneos.org/features)
on hardware that has the necessary pieces. "Long-term" means we keep shipping
the platform patches we can actually build after the vendor's window closes.
It does not mean we can patch radio firmware the vendor never released. A
phone the vendor still supports may receive firmware from that vendor sooner
than an ELE-OS build can.

### Privacy and owner control

The owner holds the keys that mean ownership. Using the phone, and recovering
it, does not require a Google account, an Apple ID, or an ELE-OS account.
The key the project uses to sign the OS is a different trust, and it is
disclosed: locking a bootloader to the project's verified-boot key means
trusting those release keys for the integrity of the OS.

### Owner-controlled lost-device recovery

Locate, remote lock, remote wipe, and a theft lock that still applies after
a factory reset, when the bootloader and the storage layout can enforce it.
The design and the threat model are in the architecture doc. The code in
this repository is a host-side signature check. No phone is protected by it.

### Old and low-end phones stay in use

A leaner image, fewer background services, and memory, battery, and storage
tuning for phones with about 2–3 GB of RAM and for phones past the vendor's
last update. This pillar uses the efficiency and update work under tighter
constraints. It does not set the defaults for a flagship build.
[Android (Go edition)](https://developer.android.com/guide/topics/androidgo)
is Google's low-RAM configuration of Android and a reference for this pillar
only. ELE-OS is not Android Go and does not claim that program's certification
or its hardware minima. Google's published Go minimums on that page run
through Android 13 (2 GB for Android 13). The page does not state an Android
16 minimum, and this project has not measured one.

## Capabilities that should reach every user

These are in scope because they help on a flagship and on an old phone. None
of them is an ELE-OS feature beyond what upstream AOSP already provides. The
architecture doc states what is feasible on which hardware.

- **Permission model.** AOSP already has runtime permissions. Further
  per-app controls (network, sensors) have working precedents in GrapheneOS
  and in CalyxOS's Datura firewall. They are not in this tree.
- **App sandbox.** Each Android app already runs in AOSP's UID, SELinux, and
  seccomp sandbox. ELE-OS's near-term job is to avoid weakening that sandbox.
  Hardware memory tagging and similar defenses exist only on chips that
  implement them.
- **Modular updates.** Android Mainline packages selected components as
  APEX or APK modules. Without Google Play, ELE-OS has to carry those
  updates on its own signed channel. Vendor firmware is not a module. Google
  documents that Mainline support for Android 11 and lower concluded in Q4
  2025.
- **Accessibility.** Font scale, contrast, right-to-left layout, and a
  working screen reader are part of shipping a phone people can actually use.
  A smaller system image is not a reason to remove them. No screen reader is
  integrated or tested in ELE-OS yet.

## What this repository contains today

This git repository is a device overlay. In an AOSP tree it is checked out at
`device/ele/os`. It is not a full operating system checkout.

| Piece | State |
| --- | --- |
| Cuttlefish product `ele_os_cf_x86_64` | Makefile registered. Image not built or booted in this repository. Signed with upstream public test-keys (development keys). |
| `ele-status` | Local shell snapshot of product name, fingerprint, two patch dates, verified-boot properties, and a memory reading. Not attestation and not proof that patches are complete. |
| `libele_owner_proof` | Ed25519 check and a one-time challenge session. Host tests only. No device enforces it. |
| External devices app | Experimental Camera2 preview of external cameras, and a line of text about whether the default network reports a satellite transport. Not a product pillar. Not validated on hardware. |

Lost-device recovery, OTA delivery, a permission toggle, a screen reader, and
every physical handset are future work. The roadmap says how each of those
is measured before anyone calls it done.

## Documents

- [Architecture](docs/architecture.md) — base OS, components, updates and
  signing, recovery and its threat model, efficiency, tiers
- [Roadmap](docs/roadmap.md) — tracks, milestones, and the evidence each one
  needs
- [Device support](docs/device-support.md) — classes, tiers, and how to add a
  device
- [Build machines](docs/build.md) — host tests versus a full AOSP build
- [Contributing](CONTRIBUTING.md)
- [Open questions](docs/open-questions.md) — decisions the maintainer still
  needs to make
- [Evidence log](docs/evidence/README.md) — measured results only; empty of
  device results today

## License

Code and documentation in this repository are under the Apache License 2.0.
See [LICENSE](LICENSE). The Linux kernel is GPL-2.0 and is not vendored here.
Proprietary vendor blobs, when a device needs them, stay under their own
terms and are not open source. Details are in
[CONTRIBUTING.md](CONTRIBUTING.md).

ELE-OS is not affiliated with Google, Apple, LineageOS, GrapheneOS, the Calyx
Institute, Murena, or postmarketOS. Their names are used here so the
relationship to their work is accurate.
