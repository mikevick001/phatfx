# PhatFX

A native, open rebuild of a classic 2011 multi-effect plug-in. The original
shipped only as an Intel/PowerPC-era VST2. This version builds as a **universal
(Apple Silicon + Intel) VST3, AU and Standalone app** with JUCE 8.

The signal chain, parameter set, ranges and curves were reconstructed by
analysing the original binary's behaviour. The code, interface and name are
new. Nothing from the original binary (code, artwork, presets or copy
protection) is included.

## Build on macOS (Apple Silicon)

Requirements: Xcode Command Line Tools and CMake 3.22 or newer.

```bash
xcode-select --install
```

```bash
brew install cmake
```

Build from the `PhatFX` folder. The first configure downloads JUCE 8.0.4:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

```bash
cmake --build build --config Release -j 8
```

The build installs the plug-ins automatically:

- `~/Library/Audio/Plug-Ins/VST3/PhatFX.vst3`
- `~/Library/Audio/Plug-Ins/Components/PhatFX.component` (AU, for Logic and GarageBand)
- Standalone app: `build/PhatFX_artefacts/Release/Standalone/PhatFX.app`

To confirm the binary is native:

```bash
lipo -archs ~/Library/Audio/Plug-Ins/VST3/PhatFX.vst3/Contents/MacOS/PhatFX
```

This should print `x86_64 arm64`. Rescan plug-ins in your DAW afterwards. For
Logic, check that `auval -v aufx Phfx Phfx` passes.

The headless DSP tests build alongside the plug-in. Run them with:

```bash
./build/phatfx_tests
```

## Original look and factory presets

If you own the original plug-in, PhatFX can use its skin and preset banks. It
reads them at runtime from the original data folder:

- `/Library/Application Support/Camel Audio/CamelPhatData`, or
- `~/Library/Application Support/Camel Audio/CamelPhatData`

When that folder exists, the plug-in window uses the original skin, and the
factory banks appear in the preset menu (click the value readout; the arrows
next to it step through presets) and as host programs. Without the folder,
PhatFX uses its own interface. No skin or preset files are distributed with
PhatFX.

To install the data folder from your own original installer DMG without
running its old installer package, run this in Terminal:

```bash
DMG=$(find ~/Downloads ~/Desktop -maxdepth 2 -name "CamelPhat*.dmg" | head -1); MNT=$(hdiutil attach -nobrowse "$DMG" | grep -o '/Volumes/.*' | head -1); cd ~ && pax -r -f "$(find "$MNT" -path '*Install Support Files.pkg/Contents/Archive.pax' | head -1)"; hdiutil detach "$MNT" -quiet; ls ~/Library/Application\ Support/Camel\ Audio/CamelPhatData
```

This unpacks the skin and banks into `~/Library/Application Support/Camel Audio/`,
and the original manual into `~/Applications/Camel Audio/`. Reopen the plug-in
window afterwards.

## Signal chain

Processing runs in 64-sample blocks, and the LFOs update once per block, as in
the original:

```
Bandpass ─► Xcita ─► BitCrush ─► Mech ─► Multimode filter ─► Flanger ─► Tube
        ─► MagicEQ ─► Compressor ─► (+ band-reject remainder × BR Mix)
        ─► Master mix (wet/dry) × Volume ─► clip ±0.9999
```

Note that Tube comes after the filter and flanger, while the other
distortions come before them.

| Section | Controls | Behaviour (from analysis) |
|---|---|---|
| Bandpass | Low, High, Q Low, Q High, BR Mix, Link | 2× cascaded RBJ high-pass and 2× low-pass (24 dB/oct each). Q range is 0.6–1.6 (low) and 0.25–1.75 (high). The rejected band (input minus band) can be mixed back after the chain. |
| Distortion | Bit, Mech, Tube, Xcita | **Bit**: 16→1 bit quantiser. **Mech**: gain 1–11× into a linear knee at 0.75. **Tube**: gain 1–5× into a rounded knee at 0.6. **Xcita**: 7 kHz high-pass band boosted by its own envelope (up to 3×), blended by amount. |
| Filter | Type (33), Cutoff 80 Hz–16 kHz, Res, Env, Attack 0.01–1 s, Decay 0.2–3 s, Mix | An envelope follower scales the cutoff. The output has a soft knee at 0.7. |
| Flanger | Amount, Rate 0.02–2 Hz | Mono-fed delay of 1.2–5.5 ms, sine LFO with a 90° offset between L and R, wet mix up to 50%. |
| MagicEQ | Amount, Tune 60–120 Hz, Mode | +10 dB peak (Q 1) at Tune, then a 40 Hz high-pass. Limit mode adds a slow-recovery limiter; Clip mode clips hard. |
| Compressor | Amount (0 to −40 dB threshold), Release 100–2000 ms, Mode | Makeup gain is applied first (1/(1−amount)), then a soft knee from 0.31 up to a 1.0 ceiling, instant attack and linear release. Smooth mode detects with a 30/80 ms envelope instead of raw peaks. |
| Master | Mix, Volume (taper x^2.737, 0 dB at max) | Wet/dry is taken around the whole chain. |
| LFO 1/2 | Target (23 destinations), Shape (10), Depth, Rate, Sync | Synced rates run from 1/32T to 4/1 dotted. Modulation adds ±depth/2 to the target. |
| XY Morph | X and Y target (27 destinations) | Dragging the pad writes both target parameters, which the host can automate. |

Filter types: LP2/BP2/HP2 BQ and Fat, Formant, Peaking, Notch, CombP, CombM,
RingMod, Tube, Mech, Bit Red, and the "HQ" analog models. The HQ models are
LP2/BP2/HP2 SVF and K20, plus LP2/LP4/BP2/BP4/HP2/HP4 XP and MG.

## What is not identical

- **Exact sound.** The routing, gain staging, curves and section algorithms
  follow the original. The 33 filter cores are new designs in the same style:
  zero-delay SVF, MS-20-style SVF with diode clipping (K20), OTA cascade (XP)
  and transistor-ladder style (MG). Expect a close match, not a bit-exact one.
- **Factory presets.** These lived in the original's data folder, which wasn't
  in the upload, so none are included.
- **Interface.** The interface is new. It is not a copy of the original skin.
- **Free-running LFO rate** uses an exponential 0.02–8 Hz curve so slow rates
  are easier to set.
- **Comp Mode / MagicEQ Mode labels** are inferred from what each flag does.
