# Building Tiny Studio

Clone this repository normally; `firmware/engines/` is included as source, pinned to the revision in [PROVENANCE.md](docs/PROVENANCE.md). No submodule fetch is needed.

## Prerequisites

- Python 3 with Pillow and fontTools (`python3 -m pip install Pillow fonttools`, preferably in a virtual environment).
- The JieLi Linux x86-64 pi32v2 toolchain: `tools/get_toolchain.sh` installs the version used by the upstream build to `~/.jieli/toolchain`.
- On macOS, Docker Desktop or a compatible runtime; Apple Silicon needs x86-64 execution support. The tested build used a dedicated Colima VM and Rosetta.
- The pinned SDK, whose three required package files are checked by the builder:

```sh
git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 \
  https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK
```

- Emscripten (`emcc`) for the browser build; Clang for native regressions.
- Node.js, Playwright and Chrome for browser tests.

The [archived upstream build guide](docs/UPSTREAM-BUILDING.md) explains toolchain internals and build flags. Use the Tiny Studio wrapper below for the correct package identity and version.

## Firmware and browser

```sh
JIELI_TOOLCHAIN="$HOME/.jieli/toolchain" \
  python3 tools/build_bongo.py --sdk "$HOME/fw-AC79_AIoT_SDK"
sh web/emu/build.sh
```

Do not add `--release`: the wrapper supplies `FM-1_980` and `0.4.0-rc1`. Outputs are `build/bongo-cat-studio-0.4.0-rc1.fwsc`, `build/emu/` and `build/BONGO-SHA256SUMS`.

```sh
python3 -m http.server 8766 --bind 127.0.0.1 --directory build/emu
```

## Checks

After building once to generate `build/gen`:

```sh
python3 tools/test_bongo.py
node tests/bongo_worklet_order_test.mjs
```

With the browser server above running, install Playwright into your local environment and point the test at its module:

```sh
PLAYWRIGHT_MODULE=/absolute/path/to/playwright/index.mjs \
  node tests/bongo_browser_release_check.mjs
```

Six sanitizer suites cover musical behaviour, metronome, bass, editors, sound A/B, live controls, saves and cold boots. The browser suite uses real WASM/AudioWorklet. [BUILDING-BONGO.txt](BUILDING-BONGO.txt) describes the target, update and hardware checks; [verification](docs/verification/VERIFICATION.json) records what was actually run for the release.

The upstream test collection is retained, but contains expectations for the original Felucca interface and identity. Tiny Studio's package simulation checks must use `FM-1_980` in place of `FM-1_900`; do not treat an unmodified upstream test run as complete Tiny Studio verification.

## Recreate README GIFs

After the generated assets exist:

```sh
mkdir -p build/media
clang -O1 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src \
  tools/capture_bongo_media.c -lm -o build/capture_bongo_media
build/capture_bongo_media build/media
python3 tools/encode_bongo_media.py
```

These captures boot a fresh native emulator. No connected device or saved performance is read.
