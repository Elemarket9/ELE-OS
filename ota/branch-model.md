# Update branches

This is the prototype model for monthly update notes. `ele-ota-note` checks a text file. It does not compile a tree, publish a file, or run a server.

The shared targets are:

| Target | Product field | What a later payload would be |
| --- | --- | --- |
| `cuttlefish` | `ele_os_cf_x86_64` | The virtual device from the makefile in this repository |
| `treble-gsi` | `ele_os_gsi_arm64` | The Treble GSI lunch target named in the architecture doc, system only |

`ele_os_gsi_arm64` is not registered in this repository. No phone maker is part of the pipeline. A note has nowhere to put a phone model. `firmware=not-claimed` on every note. `vendor-partition=not-in-payload` on every note. The tools do not read a phone bootloader or a phone vendor partition.

## Maintained

For these two targets, a maintained note may still name a new AOSP platform release. The git names to open when that work starts are `maintained/ele_os_cf_x86_64` and `maintained/ele_os_gsi_arm64`. This change does not create them.

One note per target per month uses `branch-phase=maintained` and one of these:

| `platform-source` | `aosp-tag` | Meaning |
| --- | --- | --- |
| `moved` | `aosp-tag:android-<version>` | The note names a versioned AOSP release tag |
| `unchanged` | `-` | No new tag is named this month |
| `not-shipped` | `-` | No package was prepared this month |

[AOSP's build-numbers page](https://source.android.com/docs/setup/reference/build-numbers) says that, effective in 2026, source is published in Q2 and Q4. A month that does not name a versioned tag stays `unchanged` or `not-shipped`.

`bulletin-is-platform-update=no` on every note this tool accepts. A security bulletin is recorded as `platform-source=moved` only when the same note names an AOSP release tag. The checker does not fetch that tag.

`built=no` on every note. `ele-ota-note` rejects `built=yes`. The note does not certify an image.

The GSI has no vendor support window. Its note can move the platform line only. A vendor-firmware line belongs on a device page after a porter onboards that phone, and that page does not exist. This checker has no field for a vendor binary, so it cannot record one.

## Extended

When one of these targets stops taking new platform releases, later notes use `branch-phase=extended`. The git names to open then are `extended/ele_os_cf_x86_64` and `extended/ele_os_gsi_arm64`. This change does not create them.

An onboarded phone, once a device page exists, stays maintained while that phone's vendor still ships firmware and security updates. The end date is the date that vendor publishes, written on the device page. After that window the phone's branch is labelled extended. No such page exists, and this tool does not invent the date.

An extended note sets `platform-source=fixes-only`, `aosp-tag=-`, and `fixes` to a comma-separated list of fix ids that branch contains. `platform-source=moved` is rejected. Firmware stays `not-claimed`. The vendor partition stays out of the note. The list is the whole claim: the fixes named there, and nothing presented as a new platform release or a firmware drop.

## Keys and delivery

Release private keys stay offline with the owner. The steps before a real signed release are in [README.md](README.md). CI and these tools use the test key under `keys/test/`.

A package directory is the on-disk format in [format.md](format.md). A note is a separate file. Neither one names a host. `ele-ota-client` reads a local directory and leaves `installed=no`.

## Checker

```sh
c++ -std=c++17 -Wall -Wextra -Werror -Iota ota/metadata.cpp ota/note.cpp ota/note_main.cpp -lcrypto -o /tmp/ele-ota-note
/tmp/ele-ota-note write \
  --target cuttlefish \
  --month 2026-11 \
  --branch-phase maintained \
  --platform-source not-shipped \
  --out /tmp/note.txt
/tmp/ele-ota-note check /tmp/note.txt
```

`status=note-written` means the file was written. `server=none` means it was not published. `sh tests/ota_note.sh` runs the maintained and extended cases, including notes the checker must reject.
