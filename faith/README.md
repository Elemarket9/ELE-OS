# Faith voice

Prototype. This is the wake phrase, the speech adapters, and the voice
screen. It is not a Cuttlefish result and not a handset result. The wake-word
engine is still unchosen. Nothing here heard "Hey Faith".

The screen is for Cuttlefish (`ele_os_cf_x86_64`) and a Treble GSI
(`ele_os_gsi_arm64`). The GSI product is not in this tree. No phone maker is
assumed. `EleFaithVoice` is not in `PRODUCT_PACKAGES`, so it is not in the
default image. The activity was not compiled and was not shown on a device.
It does not request the microphone or the network.

## What the owner gets

A new install uses the phrase "Hey Faith". The owner can replace it.
Listening starts off. While it is off, audio is not given to a detector, a
recognizer, or a cloud provider. Typed text still goes to the assistant
client. Cloud speech starts off. The image includes no named provider. An
endpoint the owner types is kept only when it is http or https and the host
is not a Google speech host. The provider shipped here does not open a
socket. Turning cloud speech on does not fall through from a failed
on-device recognizer, and a failed cloud call does not fall through to the
on-device recognizer.

The assistant service is not in this package. `AssistantClient` is the text
interface that service implements. This package does not run a model and
does not place calls, send messages, set alarms, change settings, or launch
apps.

## Engines

Checked 2026-11-20. None of these engines or weights are vendored.

| Piece | License | In this change |
| --- | --- | --- |
| Code under `faith/` and the new tests | Apache-2.0 | In the tree. |
| [whisper.cpp](https://github.com/ggml-org/whisper.cpp) `95b3b2f855e09404499529228de15734cdeeb6a1` | MIT, copyright 2023–2026 The ggml authors | Host probe only. Not linked into the image. |
| `ggml-tiny.en.bin` from `https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-tiny.en.bin`, sha256 `921e4cf8686fdd993dcd081a5da5b6c365bfde1162e72b08d75ac75289920b1f` | The whisper.cpp tree is MIT. The weight is not in git. | One host transcription. Not shipped. |
| Pico TTS, [ihuguet/picotts](https://github.com/ihuguet/picotts) `21089d223e177ba3cb7e385db8613a093dff74b5` | Apache-2.0. Engine notice is the AOSP SVOX notice. `pico2wave.c` is copyright 2009 Mathieu Parent, Apache-2.0. | Host probe only. `en-US` lingware `en-US_lh0_sg.bin`. Not a neural voice. |
| [Piper](https://github.com/OHF-Voice/piper1-gpl) | GPL-3.0-or-later | Not used and not linked. |
| [openWakeWord](https://github.com/dscripka/openWakeWord) | Code Apache-2.0. The project's README says the included pretrained models are CC BY-NC-SA 4.0. | Not used. Those models are not an owner-chosen phrase, and the non-commercial models are not shipped. |
| Picovoice Porcupine | Proprietary | Not eligible. |

`libpopt` is a host build dependency of `pico2wave`. It is not part of the image.

## Host checks

`sh tests/faith_voice.sh` checks the phrase, the listening gate, and the
cloud gate. It does not hear or speak. `tests/run.sh` calls it.

`sh tests/faith_engines.sh` is optional and is not part of `tests/run.sh`.
It needs `FAITH_PICO2WAVE`, `FAITH_WHISPER_CLI`, and `FAITH_WHISPER_MODEL`
already on disk. The transcript of one run is in
[docs/evidence/2026-11-20-faith-voice.md](../docs/evidence/2026-11-20-faith-voice.md).
Pico wrote non-silent audio. whisper.cpp printed a transcript that is not
the sentence Pico was given, and the phrase gate did not match. That is not
a recognition result.
