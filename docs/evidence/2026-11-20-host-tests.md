# 2026-11-20 host tests

Code under test: `6700203` (`Add external camera preview and satellite data
route status to ELE-OS`). This note was written after that run. It does not
change the code the tests compiled.

Command:

```sh
CXX=g++ sh tests/run.sh
```

Host:

- `g++` 13.3.0 (Ubuntu package `13.3.0-6ubuntu2~24.04.1`)
- OpenSSL libcrypto 3.0.13 (`pkg-config --modversion libcrypto`)
- `c++` on this host was Clang 18.1.3 and could not see C++ standard library
  headers, so the default `sh tests/run.sh` failed until `CXX=g++`

Result, verbatim:

```
Reset fixture, device binding, rotation, restart and concurrent replay checks passed
Host snapshot checks passed
```

This run covers the in-process owner-proof fixtures and the `ele-status`
shell snapshot against a fake `getprop`. It does not build AOSP, boot
Cuttlefish, or exercise a handset. The External devices app is not part of
`tests/run.sh`.
