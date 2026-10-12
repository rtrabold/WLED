# Dynamic speed continuity in WLED 16.0.1

This patch is based exclusively on the supplied `WLED-16.0.1.zip` (SHA-256 `df4bfaff80b9bebf3632e6f7ccd6f18378a5e8b9fe29ef5ac1aeda93188583c8`). External downloads were used for build tools/dependencies, not to replace or analyze another WLED version.

The inventory covers all **216 core effect registrations** in this source, including conditional 1D/2D and particle implementations. **129 registered effects use a modified implementation or shared helper.** Reserved IDs are not effects. Additional third-party usermods are outside this inventory.

## Behavior

Absolute-time multiplication/division/modulo is replaced where a rate edit could reconstruct an unrelated phase. The Q32.32 clocks accumulate elapsed motion independently per segment and oscillator. Changing a rate at the same sampled timestamp preserves every phase bit. Millisecond clocks seed from the original synchronized time but measure elapsed uptime; frame-based effects retain their frame domain.

Timer changes preserve their remaining fractional interval, with at most one event per rendered frame. Stateful fixes retain ripple distance, bounce age, twinkle timing, scene state and particle trajectories. There is no per-frame heap allocation for the clocks. Effect changes reset them; Segment copies and crossfades own separate value copies.

Palette is covered for both **Animate Shift** and **Animate Rotation**. Static Shift/Rotation remain absolute controls. IDs, names, metadata, JSON/XML control handling, presets interface and `platformio.ini` are unchanged; RGBBridge can continue using the same `sx`, `ix`, `c1`, `c2`, `c3` and option fields. End-to-end operation with RGBBridge and a physical controller has not been tested here.

## Boundaries and retained semantics

- Speed zero retains each effect's original minimum-rate meaning, except where it explicitly means stop: phase/position is now retained when stopping. It does not make every effect pause.
- Geometry controls are still geometry controls: Plasma/Plasmoid phase spacing, Firenoise scale, Tartan scale, Spots spread, and similar controls can visibly alter a pattern.
- Noisefire also couples sx to spatial scale; its temporal coordinate is fixed, but spatial changes remain visible. PS Springy's stiffness still changes forces and the original stretch constraint.
- Sunrise still changes mode at 0/1, 60/61 and 120/121. PS Sparkler still selects random positioning at 255. Candle's high settings still select its original flickering regime. Palette wrap policy at speed zero is retained.
- Frizzles' eight sx-driven X oscillators are fixed; the independent ix Y-frequency control is unchanged. This is not a general smoothing filter for every slider.
- Effects that already schedule a saved deadline may apply speed on the next interval. TV Simulator retains its current scene and applies duration edits to later scenes. Slow Transition already rebases from its current displayed color.
- Phase now depends on the history of rates. Devices with different histories are not guaranteed to stay phase-locked merely because their current speed matches. Re-entering the effect initializes from the shared time again.

## Validation

- Native C++ regression: all 256 speed values, pause/resume, long uptime, millisecond rollover, 8 independent channels, value copies/reset, invalid inputs, reference particle velocity and timer periods 1–512 frames. Constant-rate error is bounded by one Q32 fractional bit per sampled interval.
- Actual `mode_palette()` extracted from this source and compiled in both float and ESP8266 integer branches: all adjacent speed edits, animated rotation, 1D/2D, timebase corrections, continuing motion, and static Shift. The supplied original function fails the continuity comparison as expected. Trigonometry and hardware/color writes are host test substitutes, so this is not an MCU timing benchmark.
- `npm test`: **16 passed**, zero failed.
- PlatformIO `esp32dev` and `nodemcuv2`: successful release builds with the supplied profiles. Build logs accompany the delivery. Compiler warnings in unchanged dependencies/usermod infrastructure remain; no unrelated dependency upgrades were made.
- Public contract comparison: IDs, names/control metadata, effect registration, API source and build profiles match the supplied ZIP.

The ESP32 build targets the classic ESP32 `esp32dev` profile with 4 MB flash and no PSRAM requirement. The application binary is for the normal WLED update path; it is not a merged initial-install image. No device was flashed and no physical LED, Wi-Fi, audio or long-running hardware test was performed.

The new clock bank occupies 104 bytes in the native layout (at most 3,328 bytes for 32 segments, plus independent transition copies). Ripple records add 2 bytes each. PS Blobs and PS Dancing Shadows allocate 5 reference-velocity bytes per reserved particle and retain the existing allocation-failure fallback. Static RAM usage from the linker does not include these dynamic effect buffers.

## Reproduction

```sh
npm ci
npm run build
npm test
pio run -e esp32dev
pio run -e nodemcuv2
python test/verify_effect_contract.py ../WLED-16.0.1.zip
c++ -std=c++14 -O2 test/effect_clocks.cpp -o effect_clocks
./effect_clocks
python test/generate_palette_test.py palette_generated.cpp ../WLED-16.0.1.zip
c++ -std=c++14 -O2 -Iwled00 palette_generated.cpp -o palette_test
./palette_test
c++ -std=c++14 -O2 -DESP8266 -Iwled00 palette_generated.cpp -o palette_integer_test
./palette_integer_test
```

MSVC `/std:c++14 /EHsc /O2` was used for the native checks in this delivery. The C++14 harness can also be built using another native compiler. Do not define `NDEBUG`: checks use assertions. The original ZIP is optional for the rendering test, required for the contract check.

## Per-effect review

See `speed-continuity-effects.csv` for the complete original control metadata and code references. Findings are source review results; they do not claim 216 separate hardware tests.

| ID | Effect | Review | Finding | Implementation |
|---:|---|---|---|---|
| 0 | Solid | No sx timing dependency | No speed-dependent temporal expression found in this effect and its local helper path. | wled00/FX.cpp:177 |
| 77 | Copy Segment | No sx timing dependency | No speed-dependent temporal expression found in this effect and its local helper path. | wled00/FX.cpp:185 |
| 1 | Blink | Patched | Continuous phase/timer or retained animation state in blink. | wled00/FX.cpp:269 |
| 2 | Breathe | Patched | Continuous phase/timer or retained animation state in mode_breath. | wled00/FX.cpp:480 |
| 3 | Wipe | Patched | Continuous phase/timer or retained animation state in color_wipe. | wled00/FX.cpp:360 |
| 4 | Wipe Random | Patched | Continuous phase/timer or retained animation state in color_wipe. | wled00/FX.cpp:379 |
| 5 | Random Colors | Patched | Continuous phase/timer or retained animation state in mode_random_color. | wled00/FX.cpp:400 |
| 6 | Sweep | Patched | Continuous phase/timer or retained animation state in color_wipe. | wled00/FX.cpp:369 |
| 7 | Dynamic | Patched | Continuous phase/timer or retained animation state in mode_dynamic. | wled00/FX.cpp:434 |
| 8 | Colorloop | Patched | Continuous phase/timer or retained animation state in mode_rainbow. | wled00/FX.cpp:561 |
| 9 | Rainbow | Patched | Continuous phase/timer or retained animation state in mode_rainbow_cycle. | wled00/FX.cpp:577 |
| 10 | Scan | Patched | Continuous phase/timer or retained animation state in scan. | wled00/FX.cpp:543 |
| 11 | Scan Dual | Patched | Continuous phase/timer or retained animation state in scan. | wled00/FX.cpp:552 |
| 12 | Fade | Patched | Continuous phase/timer or retained animation state in mode_fade. | wled00/FX.cpp:501 |
| 13 | Theater | Patched | Continuous phase/timer or retained animation state in running. | wled00/FX.cpp:622 |
| 14 | Theater Rainbow | Patched | Continuous phase/timer or retained animation state in running. | wled00/FX.cpp:632 |
| 15 | Running | Patched | Continuous phase/timer or retained animation state in running_base. | wled00/FX.cpp:683 |
| 16 | Saw | Patched | Continuous phase/timer or retained animation state in running_base. | wled00/FX.cpp:692 |
| 17 | Twinkle | Patched | Continuous phase/timer or retained animation state in mode_twinkle. | wled00/FX.cpp:702 |
| 18 | Dissolve | Patched | Continuous phase/timer or retained animation state in dissolve. | wled00/FX.cpp:790 |
| 19 | Dissolve Rnd | Patched | Continuous phase/timer or retained animation state in dissolve. | wled00/FX.cpp:799 |
| 21 | Sparkle Dark | Stored interval | The interval/deadline is saved when scheduling the next event. Editing speed does not reinterpret elapsed phase. | wled00/FX.cpp:828 |
| 22 | Sparkle+ | Stored interval | The interval/deadline is saved when scheduling the next event. Editing speed does not reinterpret elapsed phase. | wled00/FX.cpp:848 |
| 23 | Strobe | Patched | Continuous phase/timer or retained animation state in blink. | wled00/FX.cpp:287 |
| 24 | Strobe Rainbow | Patched | Continuous phase/timer or retained animation state in blink. | wled00/FX.cpp:296 |
| 25 | Strobe Mega | Patched | Continuous phase/timer or retained animation state in mode_multi_strobe. | wled00/FX.cpp:870 |
| 26 | Blink Rainbow | Patched | Continuous phase/timer or retained animation state in blink. | wled00/FX.cpp:278 |
| 27 | Android | Stored interval | The interval/deadline is saved when scheduling the next event. Editing speed does not reinterpret elapsed phase. | wled00/FX.cpp:898 |
| 28 | Chase | Patched | Continuous phase/timer or retained animation state in chase. | wled00/FX.cpp:1007 |
| 29 | Chase Random | Patched | Continuous phase/timer or retained animation state in chase. | wled00/FX.cpp:1016 |
| 30 | Chase Rainbow | Patched | Continuous phase/timer or retained animation state in chase. | wled00/FX.cpp:1025 |
| 31 | Chase Flash | Stored interval | The interval/deadline is saved when scheduling the next event. Editing speed does not reinterpret elapsed phase. | wled00/FX.cpp:1127 |
| 32 | Chase Flash Rnd | Stored interval | The interval/deadline is saved when scheduling the next event. Editing speed does not reinterpret elapsed phase. | wled00/FX.cpp:1165 |
| 33 | Rainbow Runner | Patched | Continuous phase/timer or retained animation state in chase. | wled00/FX.cpp:1039 |
| 34 | Colorful | Patched | Continuous phase/timer or retained animation state in mode_colorful. | wled00/FX.cpp:1053 |
| 35 | Traffic Light | Patched | Continuous phase/timer or retained animation state in mode_traffic_light. | wled00/FX.cpp:1096 |
| 36 | Sweep Random | Patched | Continuous phase/timer or retained animation state in color_wipe. | wled00/FX.cpp:388 |
| 37 | Chase 2 | Patched | Continuous phase/timer or retained animation state in running. | wled00/FX.cpp:1208 |
| 38 | Aurora | Incremental animation state | Uses an accumulated position, phase, velocity or brightness; speed changes future increments rather than rebuilding elapsed motion. | wled00/FX.cpp:5007 |
| 218 | Color Clouds | Patched | Continuous phase/timer or retained animation state in mode_ColorClouds. | wled00/FX.cpp:5053 |
| 39 | Stream | Patched | Continuous phase/timer or retained animation state in mode_running_random. | wled00/FX.cpp:1217 |
| 40 | Scanner | Patched | Continuous phase/timer or retained animation state in mode_larson_scanner. | wled00/FX.cpp:1255 |
| 43 | Rain | Patched | Continuous phase/timer or retained animation state in mode_rain. | wled00/FX.cpp:1377 |
| 63 | Pride 2015 | Incremental animation state | Uses an accumulated position, phase, velocity or brightness; speed changes future increments rather than rebuilding elapsed motion. | wled00/FX.cpp:2054 |
| 67 | Colorwaves | Incremental animation state | Uses an accumulated position, phase, velocity or brightness; speed changes future increments rather than rebuilding elapsed motion. | wled00/FX.cpp:2062 |
| 42 | Fireworks | No sx timing dependency | No speed-dependent temporal expression found in this effect and its local helper path. | wled00/FX.cpp:1335 |
| 44 | Tetrix | Retained | Speed is assigned to newly created drops; existing positions advance incrementally. | wled00/FX.cpp:4038 |
| 45 | Fire Flicker | Patched | Continuous phase/timer or retained animation state in mode_fire_flicker. | wled00/FX.cpp:1415 |
| 46 | Gradient | Patched | Continuous phase/timer or retained animation state in gradient_base. | wled00/FX.cpp:1469 |
| 47 | Loading | Patched | Continuous phase/timer or retained animation state in gradient_base. | wled00/FX.cpp:1478 |
| 49 | Fairy | Stored interval | The interval/deadline is saved when scheduling the next event. Editing speed does not reinterpret elapsed phase. | wled00/FX.cpp:1519 |
| 50 | Two Dots | Patched | Continuous phase/timer or retained animation state in mode_two_dots. | wled00/FX.cpp:1486 |
| 51 | Fairytwinkle | Patched | Continuous phase/timer or retained animation state in mode_fairytwinkle. | wled00/FX.cpp:1598 |
| 52 | Running Dual | Patched | Continuous phase/timer or retained animation state in running_base. | wled00/FX.cpp:674 |
| 53 | Image | Patched | Normalizes the current GIF frame interval; changing speed does not skip frames or restart decoding. | wled00/FX.cpp:4730 |
| 54 | Chase 3 | Patched | Continuous phase/timer or retained animation state in tricolor_chase. | wled00/FX.cpp:1672 |
| 55 | Tri Wipe | Patched | Continuous phase/timer or retained animation state in mode_tricolor_wipe. | wled00/FX.cpp:1753 |
| 56 | Tri Fade | Patched | Continuous phase/timer or retained animation state in mode_tricolor_fade. | wled00/FX.cpp:1792 |
| 57 | Lightning | Stored interval | The interval/deadline is saved when scheduling the next event. Editing speed does not reinterpret elapsed phase. | wled00/FX.cpp:1961 |
| 58 | ICU | Stored interval | The interval/deadline is saved when scheduling the next event. Editing speed does not reinterpret elapsed phase. | wled00/FX.cpp:1681 |
| 60 | Scanner Dual | Patched | Continuous phase/timer or retained animation state in mode_larson_scanner. | wled00/FX.cpp:1301 |
| 61 | Stream 2 | Patched | Continuous phase/timer or retained animation state in mode_random_chase. | wled00/FX.cpp:1870 |
| 62 | Oscillate | Patched | Continuous phase/timer or retained animation state in mode_oscillate. | wled00/FX.cpp:1911 |
| 64 | Juggle | Patched | Continuous phase/timer or retained animation state in mode_juggle. | wled00/FX.cpp:2069 |
| 65 | Palette | Patched | Independent continuous clocks for Animate Shift (sx) and Animate Rotation (c1). With animation disabled, these controls remain absolute offsets. | wled00/FX.cpp:2088 |
| 68 | Bpm | Patched | Continuous phase/timer or retained animation state in mode_bpm. | wled00/FX.cpp:2274 |
| 69 | Fill Noise | Patched | Continuous phase/timer or retained animation state in mode_fillnoise8. | wled00/FX.cpp:2284 |
| 70 | Noise 1 | Incremental animation state | Uses an accumulated position, phase, velocity or brightness; speed changes future increments rather than rebuilding elapsed motion. | wled00/FX.cpp:2295 |
| 71 | Noise 2 | Incremental animation state | Uses an accumulated position, phase, velocity or brightness; speed changes future increments rather than rebuilding elapsed motion. | wled00/FX.cpp:2314 |
| 72 | Noise 3 | Incremental animation state | Uses an accumulated position, phase, velocity or brightness; speed changes future increments rather than rebuilding elapsed motion. | wled00/FX.cpp:2330 |
| 73 | Noise 4 | Patched | Continuous phase/timer or retained animation state in mode_noise16_4. | wled00/FX.cpp:2350 |
| 74 | Colortwinkles | Incremental fade/physics | Changes the per-frame fade or cooling amount; retained pixels are not reconstructed from absolute time times speed. | wled00/FX.cpp:2361 |
| 75 | Lake | Patched | Continuous phase/timer or retained animation state in mode_lake. | wled00/FX.cpp:2419 |
| 76 | Meteor | Patched | Continuous phase/timer or retained animation state in mode_meteor. | wled00/FX.cpp:2439 |
| 78 | Railway | Patched | Continuous phase/timer or retained animation state in mode_railway. | wled00/FX.cpp:2509 |
| 79 | Ripple | Patched | Continuous phase/timer or retained animation state in ripple_base. | wled00/FX.cpp:2610 |
| 80 | Twinklefox | Patched | Continuous phase/timer or retained animation state in twinklefox_base. | wled00/FX.cpp:2770 |
| 81 | Twinklecat | Patched | Continuous phase/timer or retained animation state in twinklefox_base. | wled00/FX.cpp:2777 |
| 82 | Halloween Eyes | Patched | Normalizes the remaining off interval while retaining its random duration factor; eye-on intensity behavior is unchanged. | wled00/FX.cpp:2786 |
| 83 | Solid Pattern | Spatial control | The sx slot changes pattern geometry/phase spacing, not temporal velocity. Its original response is retained. | wled00/FX.cpp:2948 |
| 84 | Solid Pattern Tri | No sx timing dependency | No speed-dependent temporal expression found in this effect and its local helper path. | wled00/FX.cpp:2967 |
| 85 | Spots | Spatial control | The sx slot changes pattern geometry/phase spacing, not temporal velocity. Its original response is retained. | wled00/FX.cpp:3018 |
| 86 | Spots Fade | Patched | Continuous phase/timer or retained animation state in mode_spots_fade. | wled00/FX.cpp:3026 |
| 41 | Lighthouse | Patched | Continuous phase/timer or retained animation state in mode_comet. | wled00/FX.cpp:1310 |
| 66 | Fire 2012 | Incremental fade/physics | Changes the per-frame fade or cooling amount; retained pixels are not reconstructed from absolute time times speed. | wled00/FX.cpp:2215 |
| 90 | Fireworks 1D | Incremental animation state | Uses an accumulated position, phase, velocity or brightness; speed changes future increments rather than rebuilding elapsed motion. | wled00/FX.cpp:3808 |
| 20 | Sparkle | Patched | Continuous phase/timer or retained animation state in mode_sparkle. | wled00/FX.cpp:808 |
| 87 | Glitter | Patched | Freezes phase at zero instead of resetting it. The original palette-wrap option at zero is retained. | wled00/FX.cpp:3473 |
| 103 | Solid Glitter | No sx timing dependency | No speed-dependent temporal expression found in this effect and its local helper path. | wled00/FX.cpp:3491 |
| 59 | Multi Comet | Patched | Continuous phase/timer or retained animation state in mode_multi_comet. | wled00/FX.cpp:1833 |
| 48 | Rolling Balls | Retained | Position integrates velocity using time since lastBounceUpdate, which is refreshed each frame. Speed does not multiply the whole bounce age. | wled00/FX.cpp:3133 |
| 89 | Fireworks Starburst | Incremental animation state | Uses an accumulated position, phase, velocity or brightness; speed changes future increments rather than rebuilding elapsed motion. | wled00/FX.cpp:3690 |
| 112 | Dancing Shadows | Patched | Continuous phase/timer or retained animation state in mode_dancing_shadows. | wled00/FX.cpp:4592 |
| 88 | Candle | Retained | Brightness advances toward a target. Speed selects update/fade regimes; the highest settings intentionally select a flickering regime. | wled00/FX.cpp:3656 |
| 91 | Bouncing Balls | Patched | Continuous phase/timer or retained animation state in mode_bouncing_balls. | wled00/FX.cpp:3047 |
| 95 | Popcorn | Incremental animation state | Uses an accumulated position, phase, velocity or brightness; speed changes future increments rather than rebuilding elapsed motion. | wled00/FX.cpp:3512 |
| 96 | Drip | Incremental animation state | Uses an accumulated position, phase, velocity or brightness; speed changes future increments rather than rebuilding elapsed motion. | wled00/FX.cpp:3944 |
| 92 | Sinelon | Patched | Continuous phase/timer or retained animation state in sinelon_base. | wled00/FX.cpp:3450 |
| 93 | Sinelon Dual | Patched | Continuous phase/timer or retained animation state in sinelon_base. | wled00/FX.cpp:3456 |
| 94 | Sinelon Rainbow | Patched | Continuous phase/timer or retained animation state in sinelon_base. | wled00/FX.cpp:3462 |
| 97 | Plasma | Spatial control | The sx slot changes pattern geometry/phase spacing, not temporal velocity. Its original response is retained. | wled00/FX.cpp:4117 |
| 98 | Percent | Retained | Moves aux1 toward the requested fill level by a per-frame step. Speed 255 intentionally uses a large step. | wled00/FX.cpp:4139 |
| 99 | Ripple Rainbow | Patched | Continuous phase/timer or retained animation state in ripple_base. | wled00/FX.cpp:2622 |
| 100 | Heartbeat | Patched | Continuous phase/timer or retained animation state in mode_heartbeat. | wled00/FX.cpp:4192 |
| 101 | Pacifica | Retained | Wave counters already accumulate per-frame deltas. The temporary strip.now scaling does not drive beat helpers in this release: util.cpp uses millis(). | wled00/FX.cpp:4261 |
| 102 | Candle Multi | Retained | Uses the same incremental candle helper; high-speed regime selection is intentional. | wled00/FX.cpp:3663 |
| 104 | Sunrise | Patched | Duration changes preserve normalized progress within sunrise/sunset; breathing phase is integrated. Boundaries 0/1, 60/61, 120/121 still select different modes. | wled00/FX.cpp:4342 |
| 105 | Phased | Incremental animation state | Uses an accumulated position, phase, velocity or brightness; speed changes future increments rather than rebuilding elapsed motion. | wled00/FX.cpp:4418 |
| 106 | Twinkleup | Patched | Continuous phase/timer or retained animation state in mode_twinkleup. | wled00/FX.cpp:4432 |
| 107 | Noise Pal | Patched | Continuous phase/timer or retained animation state in mode_noisepal. | wled00/FX.cpp:4451 |
| 108 | Sine | Incremental animation state | Uses an accumulated position, phase, velocity or brightness; speed changes future increments rather than rebuilding elapsed motion. | wled00/FX.cpp:4488 |
| 109 | Phased Noise | Incremental animation state | Uses an accumulated position, phase, velocity or brightness; speed changes future increments rather than rebuilding elapsed motion. | wled00/FX.cpp:4424 |
| 110 | Flow | Patched | Freezes phase at zero instead of resetting it. Original palette wrapping and zone geometry are retained. | wled00/FX.cpp:4508 |
| 111 | Chunchun | Patched | Continuous phase/timer or retained animation state in mode_chunchun. | wled00/FX.cpp:4540 |
| 113 | Washing Machine | Incremental animation state | Uses an accumulated position, phase, velocity or brightness; speed changes future increments rather than rebuilding elapsed motion. | wled00/FX.cpp:4713 |
| 115 | Blends | Patched | Continuous phase/timer or retained animation state in mode_blends. | wled00/FX.cpp:4747 |
| 116 | TV Simulator | Patched | A speed edit no longer requests a new scene. Current scene and color fade continue; the new duration setting is used by later scenes. | wled00/FX.cpp:4796 |
| 117 | Dynamic Smooth | Patched | Continuous phase/timer or retained animation state in mode_dynamic. | wled00/FX.cpp:468 |
| 151 | PacMan | Patched | Continuous phase/timer or retained animation state in mode_pacman. | wled00/FX.cpp:3242 |
| 219 | Slow Transition | Retained | A duration edit rebases the transition from currentPalette/currentWhite/currentCCT; it does not jump back to the starting color. | wled00/FX.cpp:11053 |
| 128 | Pixels | Incremental fade/physics | Changes the per-frame fade or cooling amount; retained pixels are not reconstructed from absolute time times speed. | wled00/FX.cpp:7316 |
| 129 | Pixelwave | Patched | Continuous phase/timer or retained animation state in mode_pixelwave. | wled00/FX.cpp:7204 |
| 130 | Juggles | Patched | Continuous phase/timer or retained animation state in mode_juggles. | wled00/FX.cpp:7063 |
| 131 | Matripix | Patched | Continuous phase/timer or retained animation state in mode_matripix. | wled00/FX.cpp:7081 |
| 132 | Gravimeter | Incremental animation state | Uses an accumulated position, phase, velocity or brightness; speed changes future increments rather than rebuilding elapsed motion. | wled00/FX.cpp:7045 |
| 133 | Plasmoid | Spatial control | The sx slot changes pattern geometry/phase spacing, not temporal velocity. Its original response is retained. | wled00/FX.cpp:7237 |
| 134 | Puddles | Incremental fade/physics | Changes the per-frame fade or cooling amount; retained pixels are not reconstructed from absolute time times speed. | wled00/FX.cpp:7307 |
| 135 | Midnoise | Incremental fade/physics | Changes the per-frame fade or cooling amount; retained pixels are not reconstructed from absolute time times speed. | wled00/FX.cpp:7115 |
| 136 | Noisemeter | Incremental fade/physics | Changes the per-frame fade or cooling amount; retained pixels are not reconstructed from absolute time times speed. | wled00/FX.cpp:7175 |
| 137 | Freqwave | Patched | Continuous phase/timer or retained animation state in mode_freqwave. | wled00/FX.cpp:7529 |
| 138 | Freqmatrix | Patched | Continuous phase/timer or retained animation state in mode_freqmatrix. | wled00/FX.cpp:7435 |
| 140 | Waterfall | Patched | Continuous phase/timer or retained animation state in mode_waterfall. | wled00/FX.cpp:7637 |
| 141 | Freqpixels | Patched | Continuous phase/timer or retained animation state in mode_freqpixels. | wled00/FX.cpp:7489 |
| 143 | Noisefire | Patched | Integrates the temporal noise coordinate. Speed also changes spatial noise scale; that original geometry response remains. | wled00/FX.cpp:7148 |
| 144 | Puddlepeak | Incremental fade/physics | Changes the per-frame fade or cooling amount; retained pixels are not reconstructed from absolute time times speed. | wled00/FX.cpp:7302 |
| 145 | Noisemove | Patched | Continuous phase/timer or retained animation state in mode_noisemove. | wled00/FX.cpp:7580 |
| 147 | Perlin Move | Patched | Continuous phase/timer or retained animation state in mode_perlinmove. | wled00/FX.cpp:5129 |
| 148 | Ripple Peak | No sx timing dependency | No speed-dependent temporal expression found in this effect and its local helper path. | wled00/FX.cpp:6787 |
| 155 | Freqmap | Patched | Continuous phase/timer or retained animation state in mode_freqmap. | wled00/FX.cpp:7404 |
| 156 | Gravcenter | Incremental animation state | Uses an accumulated position, phase, velocity or brightness; speed changes future increments rather than rebuilding elapsed motion. | wled00/FX.cpp:7028 |
| 157 | Gravcentric | Incremental animation state | Uses an accumulated position, phase, velocity or brightness; speed changes future increments rather than rebuilding elapsed motion. | wled00/FX.cpp:7036 |
| 158 | Gravfreq | Incremental animation state | Uses an accumulated position, phase, velocity or brightness; speed changes future increments rather than rebuilding elapsed motion. | wled00/FX.cpp:7054 |
| 159 | DJ Light | Patched | Continuous phase/timer or retained animation state in mode_DJLight. | wled00/FX.cpp:7375 |
| 163 | Blurz | Patched | Continuous phase/timer or retained animation state in mode_blurz. | wled00/FX.cpp:7344 |
| 179 | Flow Stripe | Patched | Continuous phase/timer or retained animation state in mode_FlowStripe. | wled00/FX.cpp:5169 |
| 184 | Wavesins | Patched | Continuous phase/timer or retained animation state in mode_wavesins. | wled00/FX.cpp:5149 |
| 185 | Rocktaves | No sx timing dependency | No speed-dependent temporal expression found in this effect and its local helper path. | wled00/FX.cpp:7603 |
| 161 | Shimmer | Patched | Zero speed freezes the current position; resume retains it. Flow modulation has its own phase. Starting at zero retains the centered initial position. | wled00/FX.cpp:5193 |
| 114 | Rotozoomer | Incremental animation state | Uses an accumulated position, phase, velocity or brightness; speed changes future increments rather than rebuilding elapsed motion. | wled00/FX.cpp:6736 |
| 118 | Spaceships | Incremental fade/physics | Changes the per-frame fade or cooling amount; retained pixels are not reconstructed from absolute time times speed. | wled00/FX.cpp:6219 |
| 119 | Crazy Bees | Stored interval | The interval/deadline is saved when scheduling the next event. Editing speed does not reinterpret elapsed phase. | wled00/FX.cpp:6260 |
| 120 | Ghost Rider | Incremental fade/physics | Changes the per-frame fade or cooling amount; retained pixels are not reconstructed from absolute time times speed. | wled00/FX.cpp:6332 |
| 121 | Blobs | Retained | Position accumulates stored velocity; new velocities are chosen at initialization or boundary bounces. | wled00/FX.cpp:6419 |
| 122 | Scrolling Text | Patched | Continuous phase/timer or retained animation state in mode_2Dscrollingtext. | wled00/FX.cpp:6517 |
| 123 | Drift Rose | Incremental fade/physics | Changes the per-frame fade or cooling amount; retained pixels are not reconstructed from absolute time times speed. | wled00/FX.cpp:6710 |
| 124 | Distortion Waves | Patched | Continuous phase/timer or retained animation state in mode_2Ddistortionwaves. | wled00/FX.cpp:7905 |
| 139 | GEQ | Patched | Continuous phase/timer or retained animation state in mode_2DGEQ. | wled00/FX.cpp:7691 |
| 146 | Noise2D | Patched | Continuous phase/timer or retained animation state in mode_2Dnoise. | wled00/FX.cpp:5965 |
| 149 | Firenoise | Spatial control | The sx slot changes pattern geometry/phase spacing, not temporal velocity. Its original response is retained. | wled00/FX.cpp:5454 |
| 150 | Squared Swirl | No sx timing dependency | No speed-dependent temporal expression found in this effect and its local helper path. | wled00/FX.cpp:6104 |
| 152 | DNA | Patched | Continuous phase/timer or retained animation state in mode_2Ddna. | wled00/FX.cpp:5355 |
| 153 | Matrix | Patched | Continuous phase/timer or retained animation state in mode_2Dmatrix. | wled00/FX.cpp:5833 |
| 154 | Metaballs | Patched | Continuous phase/timer or retained animation state in mode_2Dmetaballs. | wled00/FX.cpp:5904 |
| 160 | Funky Plank | Patched | Continuous phase/timer or retained animation state in mode_2DFunkyPlank. | wled00/FX.cpp:7750 |
| 162 | Pulser | Patched | Continuous phase/timer or retained animation state in mode_2DPulser. | wled00/FX.cpp:6056 |
| 164 | Drift | Patched | Continuous phase/timer or retained animation state in mode_2DDrift. | wled00/FX.cpp:5426 |
| 165 | Waverly | Incremental fade/physics | Changes the per-frame fade or cooling amount; retained pixels are not reconstructed from absolute time times speed. | wled00/FX.cpp:6901 |
| 166 | Sun Radiation | Spatial control | The sx slot changes pattern geometry/phase spacing, not temporal velocity. Its original response is retained. | wled00/FX.cpp:6134 |
| 167 | Colored Bursts | Patched | Continuous phase/timer or retained animation state in mode_2DColoredBursts. | wled00/FX.cpp:5302 |
| 168 | Julia | No sx timing dependency | No speed-dependent temporal expression found in this effect and its local helper path. | wled00/FX.cpp:5701 |
| 172 | Game Of Life | Patched | Continuous phase/timer or retained animation state in mode_2Dgameoflife. | wled00/FX.cpp:5511 |
| 173 | Tartan | Spatial control | The sx slot changes pattern geometry/phase spacing, not temporal velocity. Its original response is retained. | wled00/FX.cpp:6182 |
| 174 | Polar Lights | Patched | Continuous phase/timer or retained animation state in mode_2DPolarLights. | wled00/FX.cpp:6024 |
| 175 | Swirl | Patched | Continuous phase/timer or retained animation state in mode_2DSwirl. | wled00/FX.cpp:6863 |
| 176 | Lissajous | Patched | Continuous phase/timer or retained animation state in mode_2DLissajous. | wled00/FX.cpp:5806 |
| 177 | Frizzles | Patched | Independent phase for all eight X oscillators controlled by sx. Y frequency (ix), a separate control, is unchanged. | wled00/FX.cpp:5485 |
| 178 | Plasma Ball | Patched | Continuous phase/timer or retained animation state in mode_2DPlasmaball. | wled00/FX.cpp:5988 |
| 180 | Hiphotic | Patched | Continuous phase/timer or retained animation state in mode_2DHiphotic. | wled00/FX.cpp:5671 |
| 181 | Sindots | Patched | Continuous phase/timer or retained animation state in mode_2DSindots. | wled00/FX.cpp:6076 |
| 182 | DNA Spiral | Patched | Continuous phase/timer or retained animation state in mode_2DDNASpiral. | wled00/FX.cpp:5377 |
| 183 | Black Hole | Patched | Continuous phase/timer or retained animation state in mode_2DBlackHole. | wled00/FX.cpp:5267 |
| 125 | Soap | Incremental animation state | Uses an accumulated position, phase, velocity or brightness; speed changes future increments rather than rebuilding elapsed motion. | wled00/FX.cpp:8038 |
| 126 | Octopus | Incremental animation state | Uses an accumulated position, phase, velocity or brightness; speed changes future increments rather than rebuilding elapsed motion. | wled00/FX.cpp:8089 |
| 127 | Waving Cell | Patched | Continuous phase/timer or retained animation state in mode_2Dwavingcell. | wled00/FX.cpp:8146 |
| 186 | Akemi | Patched | Continuous phase/timer or retained animation state in mode_2DAkemi. | wled00/FX.cpp:7836 |
| 187 | PS Volcano | Incremental particle state | Speed sets velocity, force, emission parameters or future lifetimes; retained positions advance through the particle engine. | wled00/FX.cpp:8433 |
| 188 | PS Fire | Patched | Continuous phase/timer or retained animation state in mode_particlefire. | wled00/FX.cpp:8506 |
| 189 | PS Fireworks | Incremental particle state | Speed sets velocity, force, emission parameters or future lifetimes; retained positions advance through the particle engine. | wled00/FX.cpp:8290 |
| 190 | PS Vortex | Incremental particle state | Speed sets velocity, force, emission parameters or future lifetimes; retained positions advance through the particle engine. | wled00/FX.cpp:8176 |
| 191 | PS Fuzzy Noise | Incremental particle state | Speed sets velocity, force, emission parameters or future lifetimes; retained positions advance through the particle engine. | wled00/FX.cpp:8833 |
| 192 | PS Ballpit | Incremental particle state | Speed sets velocity, force, emission parameters or future lifetimes; retained positions advance through the particle engine. | wled00/FX.cpp:8608 |
| 193 | PS Box | Patched | Continuous phase/timer or retained animation state in mode_particlebox. | wled00/FX.cpp:8747 |
| 194 | PS Attractor | Incremental particle state | Speed sets velocity, force, emission parameters or future lifetimes; retained positions advance through the particle engine. | wled00/FX.cpp:9002 |
| 195 | PS Impact | Stored interval | The interval/deadline is saved when scheduling the next event. Editing speed does not reinterpret elapsed phase. | wled00/FX.cpp:8894 |
| 196 | PS Waterfall | Incremental particle state | Speed sets velocity, force, emission parameters or future lifetimes; retained positions advance through the particle engine. | wled00/FX.cpp:8676 |
| 197 | PS Spray | Incremental particle state | Speed sets velocity, force, emission parameters or future lifetimes; retained positions advance through the particle engine. | wled00/FX.cpp:9093 |
| 198 | PS GEQ 2D | Incremental particle state | Speed sets velocity, force, emission parameters or future lifetimes; retained positions advance through the particle engine. | wled00/FX.cpp:9166 |
| 199 | PS GEQ Nova | Incremental particle state | Speed sets velocity, force, emission parameters or future lifetimes; retained positions advance through the particle engine. | wled00/FX.cpp:9240 |
| 200 | PS Ghost Rider | Incremental particle state | Speed sets velocity, force, emission parameters or future lifetimes; retained positions advance through the particle engine. | wled00/FX.cpp:9308 |
| 201 | PS Blobs | Patched | Continuous phase/timer or retained animation state in mode_particleblobs. | wled00/FX.cpp:9390 |
| 217 | PS Galaxy | Incremental particle state | Speed sets velocity, force, emission parameters or future lifetimes; retained positions advance through the particle engine. | wled00/FX.cpp:9469 |
| 202 | PS DripDrop | Incremental particle state | Speed sets velocity, force, emission parameters or future lifetimes; retained positions advance through the particle engine. | wled00/FX.cpp:9575 |
| 203 | PS Pinball | Patched | Continuous phase/timer or retained animation state in mode_particlePinball. | wled00/FX.cpp:9689 |
| 204 | PS Dancing Shadows | Patched | Continuous phase/timer or retained animation state in mode_particleDancingShadows. | wled00/FX.cpp:9811 |
| 205 | PS Fireworks 1D | Incremental particle state | Speed sets velocity, force, emission parameters or future lifetimes; retained positions advance through the particle engine. | wled00/FX.cpp:9931 |
| 206 | PS Sparkler | Patched | Preserves emitter direction through zero. The original speed=255 random-position mode remains intentional. | wled00/FX.cpp:10053 |
| 207 | PS Hourglass | Stored interval | The interval/deadline is saved when scheduling the next event. Editing speed does not reinterpret elapsed phase. | wled00/FX.cpp:10127 |
| 208 | PS Spray 1D | Incremental particle state | Speed sets velocity, force, emission parameters or future lifetimes; retained positions advance through the particle engine. | wled00/FX.cpp:10250 |
| 209 | PS 1D Balance | Patched | Continuous phase/timer or retained animation state in mode_particleBalance. | wled00/FX.cpp:10300 |
| 210 | PS Chase | Patched | Continuous phase/timer or retained animation state in mode_particleChase. | wled00/FX.cpp:10382 |
| 211 | PS Starburst | Stored interval | The interval/deadline is saved when scheduling the next event. Editing speed does not reinterpret elapsed phase. | wled00/FX.cpp:10481 |
| 212 | PS GEQ 1D | Incremental particle state | Speed sets velocity, force, emission parameters or future lifetimes; retained positions advance through the particle engine. | wled00/FX.cpp:10539 |
| 213 | PS Fire 1D | Incremental particle state | Speed sets velocity, force, emission parameters or future lifetimes; retained positions advance through the particle engine. | wled00/FX.cpp:10611 |
| 214 | PS Sonic Stream | Incremental particle state | Speed sets velocity, force, emission parameters or future lifetimes; retained positions advance through the particle engine. | wled00/FX.cpp:10674 |
| 215 | PS Sonic Boom | Incremental particle state | Speed sets velocity, force, emission parameters or future lifetimes; retained positions advance through the particle engine. | wled00/FX.cpp:10777 |
| 216 | PS Springy | Patched | Integrates the driven sine phase and damping cadence. Stiffness still changes physical forces and the original stretch constraint. | wled00/FX.cpp:10868 |
