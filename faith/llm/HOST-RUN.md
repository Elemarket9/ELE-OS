# Host run on 2026-11-20

This is one host process. It is not a Cuttlefish boot, not a Treble GSI boot, and not roadmap H1. No quality score is given. The runtime log also printed sampling times. Those are not a speed target and are not copied here.

Host: Linux 6.12.113+ x86_64, compiler g++ 13.3.0. UTC time of the run: 2026-11-20T10:30:40Z.

Binary: `llama-completion` from llama.cpp commit `b1b770d90d94cad9e9ea5b993bdbc383a522fb28`. It prints `version: 1.0.0-dev (build 1, commit b1b770d)`, built with GNU 13.3.0 for Linux x86_64. The cmake cache for this build had `LLAMA_BUILD_SERVER=ON`, `GGML_CUDA=OFF`, and `CMAKE_CXX_COMPILER=/usr/bin/g++`. The program executed was `llama-completion`. `fetch.sh` configures a fresh tree with `LLAMA_BUILD_SERVER=OFF` because `llama-cli` at this tag pulls in a web UI.

Weight file: `SmolLM2-135M-Instruct.Q4_K_M.gguf`, 105454144 bytes, SHA-256 `8030f04528538d47bda434f6f0bdf3952c40a58123e4d5e755332f23731a8684`.

Command: the prompt `Reply with the single word hello.` was piped to `faith/llm/faith-llama.sh`. GNU `time -v` wrapped that process.

Exit status: 0.

Stdout bytes, exactly: `Hello, hello! [end of text]` followed by three newline bytes.

Maximum resident set size reported for that process: 200532 kbytes. That number is not a phone measurement and not a Cuttlefish measurement.
