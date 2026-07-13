# 繁體中文介面

本分支使用 SDL3_ttf 與 Fusion Pixel Font 的 12px proportional `zh_hant`
版本顯示繁體中文。Windows 建置會將字型嵌入 EXE，維持單一執行檔。

MSYS2 UCRT64 需要另外安裝：

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-sdl3-ttf
```

第一次執行 CMake 時會從 Fusion Pixel Font 的 GitHub Release 下載字型；也可
離線指定：

```sh
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DRIX_PLAYER_STATIC=ON \
  -DSDLPAL_ROOT=D:/sdlpal-src \
  -DFUSION_PIXEL_FONT_FILE=D:/fonts/fusion-pixel-12px-proportional-zh_hant.ttf
```

## 曲目名稱

目前只加入已考證的項目：

- Raw Track 086 (`0x56`)：酒劍仙

資料結構會保留名稱來源狀態；尚未考證的曲目仍只顯示原始曲號，避免把社群
暱稱誤標為官方曲名。
