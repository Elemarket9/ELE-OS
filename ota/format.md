# Update package format

Prototype layout for an ELE-OS update package. A future publisher can copy this directory onto storage they operate. This repository does not name a host and does not run an update server.

## Directory

```text
<package-id>/
  metadata.txt
  metadata.sig
  payload
```

`<package-id>` matches `[A-Za-z0-9._-]{1,64}`. `metadata.sig` is the raw 64-byte Ed25519 signature over the exact bytes of `metadata.txt`. `payload` is an opaque fixture of at most 1048576 bytes. The signature covers the metadata, and the metadata carries the SHA-256 of `payload`.

`metadata.txt` is ASCII, ends in a newline, and lists these fields in this order after the header `ELE-OS-UPDATE/1`:

| Field | Prototype value |
| --- | --- |
| `branch-phase` | `maintained` or `extended` |
| `build-fingerprint` | `ele-os/prototype/<product>/<version-code>/test-keys` |
| `channel` | `test` |
| `firmware-note` | `not-claimed` |
| `incremental` | same integer as `version-code` |
| `payload-kind` | `virtual-device-fixture` for Cuttlefish, `system-image-fixture` for a Treble GSI |
| `payload-sha256` | 64 lowercase hex characters |
| `payload-size` | decimal byte count |
| `payload-url` | `file:payload` |
| `platform-security-patch` | `unknown` |
| `platform-source` | `aosp-tag:android-<version>` while maintained, `fixes-only` on an extended branch |
| `product` | `ele_os_cf_x86_64` or `ele_os_gsi_arm64` |
| `prototype` | `true` |
| `target` | `cuttlefish` or `treble-gsi` |
| `vendor-partition` | `not-in-payload` |
| `vendor-security-patch` | `not-in-payload` |
| `version-code` | positive decimal integer |

A maintained `platform-source` names a versioned AOSP release tag such as `aosp-tag:android-16.0.0_r1`. A branch name is not accepted. The tag is a label in the fixture. This tool does not download or compile it.

`ele_os_cf_x86_64` is the Cuttlefish product makefile in this repository. `ele_os_gsi_arm64` is the intended Treble GSI lunch target. Its makefile is not in this repository, and the fixture payload is not a system image.

Vendor partition bytes, vendor patch level, and phone firmware stay out of the package. `platform-security-patch` stays `unknown` until a real build records `ro.build.version.security_patch`.

## Targets

A channel directory holds one subdirectory per package. Point a client at the directory for one target:

```text
channel/cuttlefish/<package-id>/
channel/ele_os_gsi_arm64/<package-id>/
```

The two targets share this format. Cuttlefish is the virtual device built from `ele_os_cf_x86_64.mk`. A Treble GSI package, when a real payload exists later, is a system image. The device vendor image already on a Treble phone stays in place. This prototype does not read a phone bootloader or a phone vendor partition.
