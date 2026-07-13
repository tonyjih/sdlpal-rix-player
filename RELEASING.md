# Release checklist

1. Update `project(... VERSION ...)` in `CMakeLists.txt`.
2. Update `CHANGELOG.md`.
3. Build from a clean directory with `RIX_PLAYER_STATIC=ON`.
4. Verify imported DLLs with `objdump -p`.
5. Test startup with:
   - `MUS.MKF` beside the EXE
   - `GamePath` in `sdlpal.cfg`
   - an explicit command-line path
   - drag and drop
6. Confirm that no game data is staged by Git.
7. Commit, create an annotated tag, and push it:

```sh
git tag -a v0.1.0 -m "SDLPAL RIX Music Player v0.1.0"
git push origin main --tags
```

8. Build a corresponding-source bundle containing the exact tracked SDLPAL
   source used by the static EXE:

```sh
./package-source-bundle.sh /d/sdlpal-src 0.1.0
```

9. Create a GitHub Release from the tag. Attach:
   - `sdlpal-rix-player.exe`
   - `sdlpal-rix-player-0.1.0-source-with-sdlpal.tar.gz`
   - optional SHA-256 checksums

Do not attach `MUS.MKF`, `.rix` files, saves, graphics, scripts, or any other
game data.
