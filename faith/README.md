# Faith prototype

Host prototype of ELE-OS's text assistant. The sources in this directory are Apache-2.0.

This is not a Cuttlefish boot and not a Treble GSI boot. `ele_os_cf_x86_64.mk` does not install the module. The GSI product is not in this tree yet. Speech recognition, speech synthesis, and the wake phrase are not implemented here.

## What the host test runs

`CXX=g++ sh tests/run.sh` compiles this library and `faith-text`, then checks the permission gate. The test does not load a language model and does not open a network connection.

`faith-text` reads one prompt on stdin. With no `--runtime`, it reports that no local runtime is configured and that cloud inference is off. It does not invent an answer.

## Targets

The image targets are the Cuttlefish product and a Treble GSI. The code does not select behavior from a brand, a manufacturer, or a device codename, and it does not read `ro.product` or `ro.boot`.

## Actions

Five switches start off: `call`, `message`, `alarm`, `setting`, and `app`. A model reply is a tool call only when the whole reply is one JSON object. Any other text is an answer. The gate runs before an intent is recorded. This process does not fire the intent, place a call, send a message, or launch an app. Tests use the fixture number `+15555550100` and the fixture alarm label `fixture`.

| Tool | Intent recorded when the switch is on |
| --- | --- |
| `call.place` | `android.intent.action.CALL` with a `tel:` URI |
| `message.send` | `android.intent.action.SENDTO` with an `sms:` URI and `sms_body` |
| `alarm.set` | `android.intent.action.SET_ALARM`, with `SKIP_UI` recorded as false |
| `alarm.cancel` | `android.intent.action.DISMISS_ALARM`, by label or by time, not both |
| `setting.open` | one of `wifi`, `bluetooth`, `sound`, `display`, `airplane` |
| `app.launch` | `android.intent.action.MAIN` plus `LAUNCHER`, package recorded, activity not resolved |

A category that is off does not produce an action, even when the model emits the tool. There is no signature permission that skips the switch.

## Runtime

`--runtime PATH` executes that file. It is not a shell string. The prompt is stdin and the reply is stdout. The program must not perform actions itself. No runtime is built into `faith-text`.

## Cloud

Cloud inference is off unless `--cloud-enable` is set and the owner supplies an `https` endpoint and a model name. No provider is compiled in. Hosts ending in `google.com`, `googleapis.com`, `gstatic.com`, or `google.dev` are refused. `faith-text` has no HTTP client: enabling cloud still sends nothing, and the process says so.

## Memory budget

`--memory-budget-bytes` and `--weight-bytes` compare a weight file's size with a budget. If the file is larger, the runtime is not started. That comparison is not a resident-set measurement, and an unset budget is not a claim that a model fits.
