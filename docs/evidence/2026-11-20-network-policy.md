# 2026-11-20 network policy

Code under test: `7a71b04` (`Add a per-app network on/off control`). This
note was written after that run. It does not change the code the tests
compiled.

Command:

```sh
CXX=g++ sh tests/run.sh
```

Host:

- `g++` 13.3.0 (Ubuntu package `13.3.0-6ubuntu2~24.04.1`)
- OpenSSL libcrypto 3.0.13 (`pkg-config --modversion libcrypto`)
- `javac` 21.0.13 (`javac -version` reported `javac 21.0.13.0.3`)

Result, verbatim:

```
Reset fixture, device binding, rotation, restart and concurrent replay checks passed
Host snapshot checks passed
Network policy checks passed (59)
Network product wiring checks passed
```

The network portion checks the on/off choice: missing entries stay granted,
a denial is `network-down` with errno `ENETUNREACH`, apps are independent,
the file format round-trips, a corrupt file is left unchanged, and the
screen rows list only apps that declare `INTERNET`. It also checks that
`EleNetwork` is installed from `network/ele_privacy.mk` on both the
Cuttlefish makefile and `ele_os_gsi_arm64.mk`.

This run does not compile `EleNetwork` against the Android SDK, build
AOSP, boot Cuttlefish, boot a GSI, or open a socket. A denied app has not
been shown to fail a connection. The cellular baseband was not exercised.
