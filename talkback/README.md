# TalkBack

Prototype pin for the ELE-OS screen reader. The reader is not in the image.
Lunch targets do not require an APK.
Host checks of this pin passed.
Nothing has booted.
The reader has not spoken.

## Upstream

| Field | Value |
| --- | --- |
| Source | https://github.com/google/talkback |
| Revision | `229212fdf5842191d0a93fc95d9ca1423b346866` |
| Default branch | `master` |
| Commit | `Push to GitHub 2026-03-18`, committer date 2026-03-23 |
| Version name | `TfPu_release_16_2` (`version.gradle`) |
| Application id | `com.android.talkback` |
| Flavor | `phone` |
| minSdk | 26 |
| Project license | Apache-2.0 |
| `LICENSE` git blob | `7a4a3ea2424c09fbe48d455aed1eaa94d9124835` |
| `LICENSE` sha256 | `58d1e17ffe5109a7ae296caafcadfdbe6a7d176f0bc4ab01e12a689b0499d8bd` |

The revision is the tip of `master` checked on 2026-11-20. Upstream publishes
source in batches. A newer Android Accessibility Suite build can exist
outside this revision. ELE-OS uses this pin.

The tree is about 42 MB and contains further third-party code under other
licenses, so it is not vendored. `talkback/PIN` and
`talkback/ele-talkback.xml` record the pin. Copy the xml to
`.repo/local_manifests/ele-talkback.xml` in an AOSP workspace before
`repo sync`. The checkout path there is `external/talkback`.

## License

The upstream `LICENSE` file at this revision is the Apache License 2.0. That
is the project license, and it matches the ELE-OS tree. ELE-OS glue in this
directory is Apache-2.0.

The phone flavor does not build from that file alone. On this revision:

- `shared.gradle` depends on
  `com.google.android.gms:play-services-mlkit-text-recognition:19.0.1` and
  `com.google.android.play:core:1.10.3`. Those are Maven artifacts. They are
  not in the Apache-2.0 `LICENSE`. `OcrController.java` imports ML Kit.
  `TalkBackApplication.java` extends `SplitCompatApplication` from Play Core.
  `PackageManagerUtils.java` and `GoogleApiAvailabilityManager.java` import
  `com.google.android.gms`.
- `talkback/build.gradle` implements `:brailleime`, `:brailleinterfaces`, and
  `:brailledisplay`. `:brailledisplay` implements `:braillebrltty` and the
  translate module. `liblouis.h` in the translate tree is LGPL-2.1-or-later.
  `vstp_transfer.c` in the bundled BRLTTY tree is GPL-2.0-or-later. Those
  headers were read at this revision. They are not a license survey of every
  braille file.
- `shared.gradle` also depends on
  `com.google.protobuf:protobuf-javalite:4.29.3` (protobuf is BSD-3-Clause)
  and on AndroidX and Material artifacts. The jars were not fetched, and
  each jar was not re-audited here.
- `shared.gradle` sets `abiFilters` to `armeabi-v7a` and `arm64-v8a`.

`talkback/sync-pin.sh` checks out the revision, checks the `LICENSE` sha256,
and checks that those Maven coordinates and the two headers are still
present. It does not run Gradle and it does not copy an APK. There is no
licence-clean build of this pin that leaves out Play Services, ML Kit, Play
Core, liblouis, and BRLTTY. Until that build exists, those components stay
out of every ELE-OS product. A missing APK is not a slot to fill with the
stock phone build.

## Products

Every registered ELE-OS product inherits `talkback/ele_talkback.mk`. That
file sets `ELE_TALKBACK_IN_IMAGE := false` and adds no packages. Lunch does
not require `EleTalkBack.apk`.

| Product | Role |
| --- | --- |
| `ele_os_cf_x86_64` | Cuttlefish virtual phone, registered as `ele_os_cf_build.mk`. Lunch release config `aosp_current`. Does not set `ro.config.low_ram`. |
| `ele_os_cf_x86_64_lowram` | Same virtual phone with `ro.config.low_ram=true`. |
| `ele_os_gsi_arm64` | Device-neutral Treble GSI, generic ARM64, no phone maker. |
| `ele_os_gsi_arm64_lowram` | Same GSI with `ro.config.low_ram=true`. |

The low-RAM makefiles are prototypes. They do not install the reader. No
image is published, because nothing has been built or booted. A 32-bit
`arm` GSI is not registered.

Play Services, ML Kit, and Play Core are not wired into these products.
liblouis (LGPL-2.1-or-later) and BRLTTY (GPL-2.0-or-later) are not in the
image. Upstream `abiFilters` are `armeabi-v7a` and `arm64-v8a`. Cuttlefish
is x86_64. That combination is untested, and no service has been started.

## What a later build has to show

A licence-clean APK from this pin, with Play Services, ML Kit, Play Core,
liblouis, and BRLTTY absent from the image, then a recorded session on
Cuttlefish and one on the Treble GSI where that build is the service that
speaks. A low-RAM product stays unpublished until that image boots with the
reader. A run of some other TalkBack build is a different result.
