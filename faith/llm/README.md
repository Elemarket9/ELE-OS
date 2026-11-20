# Faith prototype runtime

This directory pins how a host can fetch an on-device runtime and one open weight. It does not vendor either. Nothing here is a Cuttlefish boot or a Treble GSI boot, and it is not the product default. `docs/open-questions.md` still leaves the default model unset until a Cuttlefish run records the memory cost.

The image targets stay the Cuttlefish product and a Treble GSI. This pin does not name a phone maker.

## Runtime

[llama.cpp](https://github.com/ggml-org/llama.cpp) v1.0.0, commit `b1b770d90d94cad9e9ea5b993bdbc383a522fb28`, MIT, copyright 2023–2026 The ggml authors. The MIT grant covers the engine, not the weights.

`fetch.sh` builds `llama-completion` for the CPU. At this tag, `llama-cli` is compiled only with the server tools, and that build embeds a web UI. On the host used to write this pin, that UI's published files did not satisfy the tag's own asset check, so the completion program is the binary this prototype runs. GPU, Metal, Vulkan, and a vendor accelerator are not enabled.

## Weight

SmolLM2-135M-Instruct, published by HuggingFaceTB under Apache-2.0. Upstream revision `12fd25f77366fa6b3b4b768ec3050bf629380bac`. The license file is the Apache-2.0 text at the model repo. Apache-2.0 allows redistribution in an OS image. The weight is still not committed, because the pinned file is 105,454,144 bytes.

The file fetched is `SmolLM2-135M-Instruct.Q4_K_M.gguf` from `QuantFactory/SmolLM2-135M-Instruct-GGUF`. QuantFactory describes it as a llama.cpp quantization of the HuggingFaceTB model and marks the card Apache-2.0. This tree does not re-quantize the official safetensors. The pin is the SHA-256 computed for that exact file:

`8030f04528538d47bda434f6f0bdf3952c40a58123e4d5e755332f23731a8684`

`sh faith/llm/check.sh` checks the pin and, when a cached copy exists, its hash. It does not download or infer.

## What a host run is

`sh faith/llm/fetch.sh` clones, builds, and downloads into `$XDG_CACHE_HOME/ele-os/faith` (or `~/.cache/ele-os/faith`). `faith/llm/faith-llama.sh` reads a prompt on stdin. If the weight or the binary is missing, it exits. It does not invent a reply. `FAITH_MEMORY_BUDGET_BYTES` refuses to load the file when the file alone is larger than the budget. That comparison is not a resident-set measurement.

No quality score and no tokens-per-second figure are part of this pin.
