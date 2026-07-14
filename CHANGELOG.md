# Changelog

## Unreleased

- Add a Traditional Chinese UI rendered through SDL3_ttf.
- Use Fusion Pixel Font 12px proportional `zh_hant`; embed it in Windows builds.
- Add source-aware track metadata and display Track 086 as `酒劍仙`.
- Feed PCM from SDL's audio-demand callback instead of the window loop.
- Smooth loop, restart, track-change, and natural-end discontinuities.

## 0.1.0 - 2026-07-13

Initial public release.

- Play `MUS.MKF` and standalone RIX files through SDLPAL's RIX/OPL code.
- Preserve raw MKF music IDs while skipping empty entries during navigation.
- Drag-and-drop, direct track entry, pause, loop, restart, volume, and meter.
- Buffered SDL3 audio queue to avoid Windows crackling and underruns.
- Automatic startup lookup beside the EXE or through `GamePath` in `sdlpal.cfg`.
- Native Windows GUI subsystem without an extra console window.
- Self-contained static Windows build by default.
