# Building DrumSpace from source

DrumSpace has two buildable parts: the **JUCE plugin** (C++) and the **inference server** (Python, optionally bundled to an exe with PyInstaller).

## Prerequisites

### Plugin (C++)

- Windows 10/11 x64
- Visual Studio 2022 (C++ desktop workload)
- CMake ≥ 3.22
- [JUCE](https://github.com/juce-framework/JUCE) (tested with JUCE 9; the CMakeLists references an existing clone — see below)

### Inference server (Python)

- Python 3.7 (TensorFlow 1.15 is not available for newer Python on Windows)
- A conda/virtualenv with:
  - `tensorflow==1.15.0`
  - `magenta==1.1.7` (installed with `--no-deps` to avoid dependency downgrades)
  - `numpy==1.21.6`

> The environment is finicky. The known-good setup is Python 3.7.12 + TF 1.15.0 + magenta 1.1.7 (`--no-deps`) + numpy 1.21.6.

## 1. Get the model

```powershell
mkdir checkpoints
curl -L -o checkpoints\cat-drums_2bar_small.hikl.tar `
  "https://storage.googleapis.com/magentadata/models/music_vae/checkpoints/cat-drums_2bar_small.hikl.tar"
tar -xf checkpoints\cat-drums_2bar_small.hikl.tar -C checkpoints
```

Extracts `cat-drums_2bar_small.hikl.ckpt.index` + `cat-drums_2bar_small.hikl.ckpt.data-00000-of-00001`.

## 2. Build the plugin

Edit `plugin/CMakeLists.txt` and set `JUCE_DIR` to your JUCE clone, then:

```powershell
cd plugin
cmake -S . -B ../build -G "Visual Studio 17 2022" -A x64
cmake --build ../build --config Release --target DrumSpacePlugin_VST3 DrumSpacePlugin_Standalone
```

Outputs:

- `../build/DrumSpacePlugin_artefacts/Release/VST3/DrumSpace.vst3`
- `../build/DrumSpacePlugin_artefacts/Release/Standalone/DrumSpace.exe`

## 3. Run the inference server (development)

From the `drum-space` root (so `server.py` can find `../checkpoints`):

```powershell
& "C:\path\to\python.exe" server\server.py
```

It listens on `127.0.0.1:8766` (`/encode`, `/decode`, `/health`).

The plugin auto-detects an already-running server, so in development you can start it manually; in a packaged install the plugin launches the bundled exe itself.

## 4. Bundle the server into an exe (for distribution)

```powershell
cd server
& "C:\path\to\env\Scripts\pyinstaller.exe" --clean --noconfirm drumspace_server.spec
```

Produces `server/dist/drumspace_server.exe` (~300 MB, TensorFlow is large). The model is **not** bundled inside the exe — it must sit next to it in `checkpoints/`.

## 5. Package and install

Place these side by side:

```
DrumSpace.vst3
drumspace_server.exe
checkpoints/   (the two .ckpt files)
install.ps1
```

Run `install.ps1` as administrator. It copies the VST3 to `C:\Program Files\Common Files\VST3\` and the server + model to `%ProgramData%\DrumSpace\server\`.

## How the plugin finds the server

On load, the plugin:

1. Checks `http://127.0.0.1:8766/health` — if already up, does nothing.
2. Otherwise looks for `drumspace_server.exe` in `%ProgramData%\DrumSpace\server\`, then `%APPDATA%\DrumSpace\server\`, then next to the plugin.
3. Launches it in the background and kills it on unload.

## Troubleshooting

- **"Generation failed"** — the server isn't running or the model isn't found next to the exe.
- **"Latent load failed"** — the server reached but encode failed (usually an empty/invalid pattern).
- **No sound** — check the MIDI port routing and the drum key map (DRUM MAP button) against your drum synth's layout.
