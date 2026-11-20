# 2026-11-20 Faith voice host run

Prototype only. This is not a Cuttlefish boot, not a GSI boot, and not a
handset. Nobody listened to the audio. The wake phrase was not detected.

Base of the tree: `4076222f` (`Merge pull request #3`). The commands below
were run on the working tree that adds the voice prototype, before this note
was committed. The note does not change those commands.

## Policy

```sh
CXX=g++ sh tests/run.sh
sh tests/check-build-contract.sh
```

Host: `g++` 13.3.0 (`13.3.0-6ubuntu2~24.04.1`), OpenSSL libcrypto 3.0.13,
`javac` 21.0.13 with `--release 17`.

Result, verbatim:

```
Reset fixture, device binding, rotation, restart and concurrent replay checks passed
Host snapshot checks passed
Faith voice policy checks passed
Build contract checks passed (manifest and makefiles only; AOSP was not built)
```

`tests/faith_voice.sh` checks the phrase "Hey Faith", a replacement phrase,
listening left off, and cloud speech left off. It does not open a microphone
and it does not send a request.

## Pico and whisper.cpp

Direct command, repeated three times, same file each time:

```sh
pico2wave -l en-US -w /tmp/s1.wav "Hey Faith. What time is it?"
```

`pico2wave` from [ihuguet/picotts](https://github.com/ihuguet/picotts)
`21089d223e177ba3cb7e385db8613a093dff74b5`, installed under `/tmp` and not
committed. WAV sha256
`fbaf1c79082a6f31a2132ff4a86a144fd1c61917d7526268079ce1d59cb9c7d5`,
195628 bytes, 1 channel, 16-bit, 16000 Hz, 97792 frames, RMS 5094.

whisper.cpp `95b3b2f855e09404499529228de15734cdeeb6a1` (`whisper-cli`,
version string `1.9.6-dev`), model
`ggml-tiny.en.bin` sha256
`921e4cf8686fdd993dcd081a5da5b6c365bfde1162e72b08d75ac75289920b1f`,
downloaded from
`https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-tiny.en.bin`
and not committed. CPU only. The model load log said `n_vocab = 51864` and
`type = 1 (tiny)`.

```sh
whisper-cli --no-gpu --no-timestamps --language en \
  -m ggml-tiny.en.bin -f /tmp/s1.wav
```

Stdout:

```
 N is under sport under sport 1.0.0, hyphen 0-2.
```

Total time printed by whisper.cpp: 556.57 ms. That transcript does not
contain "Hey Faith".

The Java adapter's probe, after it deletes the empty output placeholder
(`pico2wave` wrote a different 228908-byte file when that path already
existed):

```sh
FAITH_PICO2WAVE=/tmp/faith-src/pico-install/bin/pico2wave \
FAITH_WHISPER_CLI=/tmp/faith-src/whisper-build/bin/whisper-cli \
FAITH_WHISPER_MODEL=/tmp/faith-src/models/ggml-tiny.en.bin \
sh tests/faith_engines.sh
```

```
PICO bytes=195584 samples=97792 rms=5094 rate=16000 language=en-US lingware=en-US_lh0_sg.bin
WHISPER raw=N is under sport under sport 1.0.0, hyphen 0-2.
WHISPER transcript=N is under sport under sport 1.0.0, hyphen 0-2.
PHRASE_GATE match=false command=
Host engine probe finished
```

`PHRASE_GATE match=false` is the result. This does not show that wake
detection or speech recognition works. The engines are not in the system
image. `tests/faith_engines.sh` is not part of `tests/run.sh`.
