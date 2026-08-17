# Spectral Dissector

VCV Rack 2 module — a high-fidelity port of **Spectral Dissector 26.08.13 (Max for Live)** by Xiehuo.

A real-time 10-band spectral dissector: 4096-point STFT by default (Hann, hop 1024, 4x overlap), zero-latency per-bin HPSS, cepstral noise-floor envelope, and cascade masks split the input into 8 harmonic layers (Band 1–8), noise (Band 9), and percussive content (Band 10). The FFT window size can be switched from the right-click menu (1024 / 2048 / 4096 / 8192; still Hann, 4x overlap). Default parameter behavior is verified sample-exact against the 26.08.13 golden model. Original Max for Live device page: https://caohaoxuan.com/project-template.html?project=spectral-dissector

## I/O

- **Audio IN** — one poly input, `ch0 = L`, `ch1 = R`; mono is duplicated `R = L`.
- **Dry** — poly output, `L/R`.
- **Band 1 … Band 8, Band 9 (Noise), Band 10 (Perc)** — poly outputs, `L/R` each.
- **Mix** — poly output: sum of all bands after the per-band gain faders. Dry is not included.
- **8 CV inputs** — Threshold / Spacing / Focus / Gate / Blur / Perc / Detail / Tilt. Each has a bipolar attenuator (-100%…+100%); at knob mid position, +/-10 V reaches the parameter extremes.

## Controls

- 46 parameters: core DSP controls (Threshold, Spacing, Focus, Tilt, Gate, Blur, Perc, Rise, Fall, Detail, Band 1–7 Offset), Dry switch, 10 band on/off switches, 10 per-band gain faders, 8 CV attenuators.
- Built-in spectrum analyzer: click the legend chips at the bottom to toggle bands; right-click the analyzer to switch log/linear scale.
- Dark and light panels: right-click the module and toggle **Panel theme → Use dark panels**. The setting is per-module and saved with the patch; it does not change Rack's global panel theme.
- FFT window size: right-click the module (or the analyzer) and choose **FFT window size → 1024 / 2048 / 4096 / 8192**. Default is 4096 (original 26.08.13 behavior). The choice is saved with the patch.

## Install

- Download/build the `.vcvplugin` package and copy it into Rack 2's plugins folder, or subscribe to it in the Rack library.
- Compatible with Rack 2.6+ (macOS arm64 build).

## Build

```bash
# 本仓库源码位于仓库根目录；把 Rack SDK 放到 ../sdk（或自行指定 RACK_DIR）
PATH=/opt/homebrew/bin:$PATH make RACK_DIR=../sdk
PATH=/opt/homebrew/bin:$PATH make RACK_DIR=../sdk dist
```

`make dist` requires `tar`, `zstd`, and macOS `rsync`.

## Changelog

See `CHANGELOG.md`.

## License

GPL-3.0-or-later. See `LICENSE` for the full GPLv3 text.

Original device reference: Spectral Dissector 26.08.13 (Max for Live), by the same author.
