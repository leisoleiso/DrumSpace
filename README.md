# DrumSpace

A VST3 / Standalone MIDI generator that turns a 2D control space into drum patterns via Magenta's **MusicVAE** latent-space interpolation.

Drag a ball across a circular space between user-defined "drum nodes". Each node holds a 2-bar drum pattern; the ball's position blends their latent vectors, and MusicVAE decodes the result into a new drum pattern that is sent out as MIDI to your DAW's drum synth.

```
2D space → SpatialEngine weights → MusicVAE latent interpolation → decode → MIDI drum pattern
```

## Features

- **Circular 2D control space** — draggable nodes + ball, equidistant default layout, resizable window.
- **Drum nodes** — assign a pattern preset (Four-on-the-Floor, Rock, Funk, Hip-Hop, Breakbeat) or capture your own from the DAW piano roll.
- **Latent-space interpolation** — the ball blends node latents with inverse-distance weighting; near a node it plays that node's pattern verbatim.
- **Seamless switching** — a newly generated pattern waits for the current drum hit to finish before swapping in, so there's no cut-off glitch.
- **Custom drum key mapping** — map the 9 drum pieces (kick/snare/hats/toms/cymbals) to any MIDI note layout your drum synth uses; save/load/export/import named maps.
- **Pure MIDI output** — no built-in sound; route the MIDI to FPC, Drumaxx, Analog V, or any drum VST.

## Model

Uses Magenta's pretrained **[`cat-drums_2bar_small.hikl`](https://storage.googleapis.com/magentadata/models/music_vae/checkpoints/cat-drums_2bar_small.hikl.tar)** (53 MB, z=256, 2 bars, 9 drum classes), the variant trained for better reconstruction and interpolation.

| Drum class | GM pitch |
|---|---|
| Kick | 36 (C2) |
| Snare | 38 (D2) |
| Closed Hi-Hat | 42 (F#2) |
| Open Hi-Hat | 46 (A#2) |
| Low Tom | 45 (A2) |
| Mid Tom | 48 (C3) |
| High Tom | 50 (D3) |
| Crash | 49 (C#3) |
| Ride | 51 (D#3) |

## Installation (end users)

**Download a release** (includes the plugin + the bundled inference server) and run `install.ps1` as administrator. The inference server starts automatically when the plugin loads.

Requirements: **Windows 10/11 x64**, a DAW that hosts VST3.

## Building from source

See [BUILD.md](BUILD.md) for the full build instructions.

## Usage

1. Load **DrumSpace** in your DAW and route its **MIDI Out Port** to a drum synth's **MIDI In Port** (or drop the standalone's MIDI out onto a drum track).
2. Drag the ball to blend between nodes and generate patterns.
3. Click a node, then use **Pattern + Apply** to assign a preset, or **Capture** to record your own pattern from the piano roll.
4. Use **DRUM MAP** to remap the 9 drum pieces to your drum synth's key layout.

## Project layout

```
drum-space/
├── plugin/            # JUCE plugin (C++)
│   ├── src/           #   processor, editor, spatial engine, canvas, model interface
│   └── CMakeLists.txt
├── server/            # local inference server (Python)
│   ├── server.py      #   HTTP /encode /decode over MusicVAE
│   └── drumspace_server.spec  # PyInstaller spec
├── checkpoints/       # cat-drums_2bar_small.hikl model weights
└── install.ps1        # one-click installer
```

## License

The DrumSpace plugin code is provided as-is for evaluation. The MusicVAE model is from [Magenta](https://github.com/magenta/magenta) and is subject to its own license terms.
