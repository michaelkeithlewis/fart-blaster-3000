# FART BLASTER 3000

The Ultimate Flatulence Experience. A VST3/AU audio plugin that plays randomized fart sounds at configurable intervals with delay and reverb processing. Made by [Truck Packer](https://truckpacker.com).

## Controls

- **HOW MUCH?** — Controls how often farts play. All the way left = OFF. All the way right = nonstop back-to-back farts. The current interval is displayed below the knob.
- **HOW WET?** — Blends in a stereo delay (340ms echo with feedback) and reverb. At 0 = pure dry farts. Crank it up for farts in a cathedral.

## Download

Grab the latest release from the [Releases page](../../releases). The zip contains:

- `Fart Blaster 3000.vst3` — VST3 plugin
- `Fart Blaster 3000.component` — AU plugin (macOS only)
- `Fart Blaster 3000.app` — Standalone app (no DAW needed)

## Installation (macOS)

### VST3
Copy `Fart Blaster 3000.vst3` to:
```
~/Library/Audio/Plug-Ins/VST3/
```

### AU (Audio Unit)
Copy `Fart Blaster 3000.component` to:
```
~/Library/Audio/Plug-Ins/Components/
```

### Standalone
Just double-click `Fart Blaster 3000.app` — no installation needed. Select your audio output device in the settings dialog that appears on first launch.

### After installing plugins
Restart your DAW, then look for **Fart Blaster 3000** (manufacturer: FartCo) in your plugin list. Load it as an insert effect on any track.

## Installation (Windows)

### VST3
Copy `Fart Blaster 3000.vst3` to:
```
C:\Program Files\Common Files\VST3\
```

> Note: the current release is macOS ARM only. Windows builds coming eventually (or build from source).

## Building from Source

Requires CMake 3.22+ and a C++17 compiler. JUCE is fetched automatically.

```bash
git clone https://github.com/michaelkeithlewis/fart-blaster-3000.git
cd fart-blaster-3000
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release -j$(nproc)
```

The built plugins will be in `build/FartBlaster_artefacts/Release/` and automatically copied to your system plugin folders.

## What's Inside

- 100 premium artisanal fart samples (16-bit WAV, embedded in the binary)
- 16-voice polyphony (yes, 16 simultaneous farts)
- Stereo delay with feedback
- Freeverb-based reverb
- Animated stink line GUI in Papyrus font
- Clickable Truck Packer branding

## License

Do whatever you want with this. It's a fart plugin.
