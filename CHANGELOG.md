# Changelog

## 2.1.1 — 2026-08-21

- While **Spectral Dissector R** is attached, the main module's 12 outputs now switch to mono `ch0` (L-only) and the right channel is routed to the expander; removing the expander restores the normal 2ch poly outputs. This avoids accidental L+R downmixing in mono destinations that sum poly channels.

## 2.1.0 — 2026-08-21

- New expander module **Spectral Dissector R**: place it to the right of Spectral Dissector for a mono right-channel input and 12 mono right-channel outputs (Dry R / Band 1–10 R / Mix R). Audio only; the main module receives a minimal, no-effect-when-absent right-expander input hook.

## 2.0.0 — 2026-08-18

- Initial public release for VCV Rack 2.
- 10-band spectral dissection with per-band gain and MIX output.
- Poly audio I/O: 1 input, 12 outputs (Dry / B1–B10 / Mix).
- 8 CV inputs with bipolar attenuators (Threshold / Spacing / Focus / Gate / Blur / Perc / Detail / Tilt).
- Built-in spectrum analyzer with interactive legend chips for analyzer show/hide and log/linear scale.
- Right-click menu: selectable FFT window size (1024 / 2048 / 4096 / 8192, Hann + 4x overlap). Default remains 4096 (original 26.08.13 behavior). Saved with the patch.
- Per-module dark/light panel setting, saved with the patch.
- Dual-theme SVG panel resources and layout designer tooling.
