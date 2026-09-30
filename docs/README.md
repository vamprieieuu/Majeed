# MAJEED native After Effects plug-in (C++ / AE SDK)

One `MAJEED.aex` containing five effects (menu: Effect > MAJEED). **All pixels are rendered by the C++ engine in `core/`.
No native AE effect (Noise, Fractal Noise, Wave Warp, Venetian Blinds, Channel Blur...) is used, and the reference images
are never used as textures. The JSX in `reference/` is reference only.**

| Effect | Engine | What is genuinely different |
|---|---|---|
| MAJEED Grain | core/grain.cpp | 4 generators: Gaussian value-noise lattice, Voronoi film crystals, scan-line (tape) grain, fBm clustered grain. Grain size 4-64 mm -> px via `px = mm * layerWidthPx / PrintWidthMm` (Print Width param, default 2400 mm), resolution/downsample independent. |
| MAJEED Random Lines | core/lines.cpp | Per-line hashed geometry (centre, angle, length, kink segments, Bezier curvature, thickness, opacity, brightness, lifetime, drift, jitter), analytic AA stroking, tiled multithreaded. |
| MAJEED Dither | core/dither.cpp | 15 error-diffusion kernels (FS, serpentine, JJN, Stucki, Atkinson, Burkes, Sierra 3/2/Lite, Fan, Shiau-Fan, skip variants), Bayer 2-16, void-and-cluster blue noise, IGN, white noise, 27 halftone/line/pattern screens. |
| MAJEED VHS | core/vhs.cpp | Per-row tracking/head-switch/jitter displacement, YIQ chroma bleed, dashes, streaks, scanlines/interlace, flicker, tape grain. |
| MAJEED Channels | core/channels.cpp | True per-channel sub-pixel resampling, linear/radial RGB separation, per-row random offsets, gains, hue, tint. |

## Build (Windows)
1. Install Visual Studio 2022 (Desktop C++ workload) and download the Adobe After Effects SDK.
2. Set env var `AE_SDK` to the SDK's `Examples` folder (must contain `Headers`, `Util`, `Resources`, `Resources\PiPLTool.exe`).
3. Open `vs/MAJEED.sln`, configuration Release | x64, Build. Output: `vs/bin/Release/MAJEED.aex`.
4. Copy to `C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\` (or set `AE_PLUGIN_DIR` to auto-copy). Restart AE.

## Verify the engine without AE (Linux/Windows g++)
`cd tools && g++ -O2 -std=c++17 -pthread -I../core selftest.cpp ../core/*.cpp -o selftest && ./selftest out`
(measured vs the reference images: lines mean 42.7/43.0, bright fraction 0.337/0.354; grain sd 0.102/0.103, horizontal autocorrelation > vertical.)
`tools/sdk_stub` is a tiny fake of the SDK used only to syntax-check `ae/*.cpp`.

## Honest status / limits
* **The `.aex` was NOT compiled**: the build sandbox has no Adobe SDK, no Visual Studio, no network. The AE glue (`ae/`) was only syntax-checked against my own stub, never against the real SDK or run in After Effects. Expect small header/flag fixes on first build (PiPL flag values in `ae/Majeed_PiPL.r` must equal `GlobalSetup`; AE prints the expected numbers if they differ).
* Glue assumes AE buffers are premultiplied and that the checked-out layer starts at the layer origin.
* Grain crystal type with fine+coarse+smooth evolution is slow (~10 s/1080p single thread here; it is multithreaded per core in AE).
* "Skip Neighbours" dither kernels and the pattern spot functions (Circuit, Clock, Knit, Bi-thread...) are my definitions of the JSX names, not a copy of the original Photoshop plug-in.
* JSX Preprocess (blur/sharpen/denoise) is not re-implemented; use AE's own effects for that.
* Grain size slider is labelled mm; AE sliders cannot show a unit suffix, so a "Size Preset" menu (4/8/12/16/24/32/48/64 mm) is provided too.


## Cloud build
See `CLOUD_BUILD.md` and `.github/workflows/build.yml`. The workflow uses a Windows 2022 GitHub runner and produces `MAJEED.aex` when a legitimate Adobe After Effects SDK is supplied to the runner.
