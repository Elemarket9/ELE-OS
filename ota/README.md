# Signed update prototype

This directory is a host prototype of the ELE-OS update package. It signs fixture bytes with the repository test key and checks that signature. It does not install an update. `installed=no` is the only install field the tools print.

The shared targets are Cuttlefish (`ele_os_cf_x86_64`) and a Treble GSI. The GSI product field is `ele_os_gsi_arm64`, the lunch target named in the architecture doc. That makefile is not in this repository. No phone maker is part of this pipeline.

## Test key

The Ed25519 seed is public: 32 bytes of `0x11`, compiled into `ota/test_key.h`. `ota/keys/test/public.hex` is the matching public key. CI uses that seed. `ele-ota-sign` refuses any other private key, so this binary cannot sign with an OS release key. The tree does not contain a PEM file.

Release keys stay offline with the owner. Before a real signed release, the owner:

1. Generates the OS release keys on an offline machine or a hardware token when the first release build is prepared.
2. Keeps the private keys offline. An encrypted backup lives in a separate physical place. The passphrase lives apart from that backup, with a sealed copy for one successor named on paper.
3. Publishes the public certificates and their fingerprints.
4. Signs the release on that offline machine with AOSP `sign_target_files_apks` and `ota_from_target_files`.
5. Leaves this test key out of any device trust store.
6. Records an install only from a Cuttlefish or device log that shows the new build. This repository has no install log.

No release key has been generated.

## Sign a fixture

From the repository root, with a C++17 compiler and OpenSSL:

```sh
c++ -std=c++17 -Wall -Wextra -Werror -Iota ota/metadata.cpp ota/sign_main.cpp -lcrypto -o /tmp/ele-ota-sign
printf 'fixture\n' > /tmp/payload
/tmp/ele-ota-sign \
  --payload /tmp/payload \
  --out-dir /tmp/ele-update \
  --target cuttlefish \
  --branch-phase maintained \
  --version-code 1 \
  --platform-source aosp-tag:android-16.0.0_r1
```

The command writes `metadata.txt`, `metadata.sig`, and `payload`. The stdout line `status=signed-prototype` means the fixture was signed. It does not mean an image was built or flashed.

`sh tests/ota.sh` builds the tool, signs both targets, refuses a different key, and runs the library checks.

## Check a local channel

`ele-ota-client` reads a directory of package folders. It verifies the test-key signature and the payload hash, then prints a decision. The status for a package it accepts is `verified-not-installed`.

```sh
c++ -std=c++17 -Wall -Wextra -Werror -Iota ota/metadata.cpp ota/client.cpp ota/client_main.cpp -lcrypto -o /tmp/ele-ota-client
/tmp/ele-ota-client \
  --channel /tmp/channel/cuttlefish \
  --target cuttlefish \
  --product ele_os_cf_x86_64 \
  --current-version 0
```

The client accepts the pinned test public key only. It does not open a network connection. `sh tests/ota_client.sh` covers a newer package, a downgrade, a bad signature, a payload byte change, an ambiguous version, and both targets.

`m libele_ota` has not been run. The Soong module is `libele_ota` in `ota/Android.bp`.

Package bytes are described in [format.md](format.md). The key is described in [keys/test/README.md](keys/test/README.md).

The tree is Apache-2.0, the same license as `LICENSE` at the repository root.
