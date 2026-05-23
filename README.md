# FART BLASTER 3000

The Ultimate Flatulence Experience. A VST3/AU audio plugin that plays randomized fart sounds at configurable intervals with delay and reverb processing. Made by [Truck Packer](https://truckpacker.com).

## Controls

- **HOW MUCH?** — Controls how often farts play. All the way left = OFF. All the way right = nonstop back-to-back farts. The current interval is displayed below the knob.
- **HOW WET?** — Blends in a stereo delay (340ms echo with feedback) and reverb. At 0 = pure dry farts. Crank it up for farts in a cathedral.
- **STEREO** — Toggles stereophonic mode. ON: each fart is panned randomly across the field and the delay ping-pongs L↔R for a wide stereo image. OFF: classic mono behavior — farts in the center, delay echoes stay put.

## Download

Grab the latest release from the [Releases page](../../releases). The zip contains:

- `Fart Blaster 3000.vst3` — VST3 plugin
- `Fart Blaster 3000.component` — AU plugin (macOS only)
- `Fart Blaster 3000.app` — Standalone app (no DAW needed)

## Installation (macOS)

Release builds are signed with a Developer ID certificate and notarized by Apple.

### Recommended: use the installer

Download `FartBlaster3000-Installer.pkg`, double-click it, and follow the prompts. The installer:

- Removes any previous Fart Blaster 3000 from both `/Library` and `~/Library`
- Installs the VST3 to `/Library/Audio/Plug-Ins/VST3/`
- Installs the AU to `/Library/Audio/Plug-Ins/Components/`
- Installs the standalone to `/Applications/`

You'll be prompted for your admin password once.

### Alternative: manual install from the zip

If you'd rather not run the installer, download `FartBlaster3000-macOS.zip` and copy the bundles yourself:

- `Fart Blaster 3000.vst3` → `~/Library/Audio/Plug-Ins/VST3/`
- `Fart Blaster 3000.component` → `~/Library/Audio/Plug-Ins/Components/`
- `Fart Blaster 3000.app` → drag anywhere (e.g. `/Applications`)

Both releases are signed and notarized — Gatekeeper will accept them with no extra steps.

> Building from source produces an ad-hoc signed binary, not a notarized one — if you build it yourself and Gatekeeper complains, run `xattr -cr *` in the build output folder to clear the quarantine xattr.

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

### Signed release builds (maintainers only)

To produce a Developer-ID-signed build, pass your identity to CMake:

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release \
    -DAPPLE_DEV_ID="Developer ID Application: YOUR NAME (TEAMID)" \
    -DAPPLE_TEAM_ID=TEAMID
cmake --build . --config Release -j$(nproc)
```

After building, notarize the zipped output with:

```bash
xcrun notarytool submit FartBlaster3000-macOS.zip --keychain-profile <profile> --wait
xcrun stapler staple "Fart Blaster 3000.vst3" "Fart Blaster 3000.component" "Fart Blaster 3000.app"
```

## What's Inside

- 100 premium artisanal fart samples (16-bit WAV, embedded in the binary)
- 16-voice polyphony (yes, 16 simultaneous farts)
- Ping-pong stereo delay with feedback (switchable mono/stereo)
- Freeverb-based reverb with per-voice random panning in stereo mode
- Animated stink line GUI in Papyrus font
- Clickable Truck Packer branding

## License

Do whatever you want with this. It's a fart plugin.
