# SDLPAL RIX Music Player

這是一個非官方、輕量的 SDL3 音樂播放器，用來直接播放《仙劍奇俠傳》
原版 `MUS.MKF` 中的 RIX 音樂。

播放器使用 SDLPAL 內附的 RIX sequencer 與 OPL 模擬程式，並保留遊戲腳本
使用的原始曲號。Windows 預設會產生不需要額外 SDL3／MinGW DLL 的單一 EXE。

> 本專案與大宇資訊及 SDLPAL 開發團隊沒有官方關係，也不包含任何遊戲音樂、
> 圖像、腳本或其他原始遊戲資源。使用者需要自行從合法取得的遊戲中提供
> `MUS.MKF`。

## 功能

- 播放 `MUS.MKF` 或獨立 `.rix`
- 保留 MKF／SDLPAL 原始曲號
- 導航時自動略過空白 entry
- 支援拖放檔案
- 上一首、下一首、前後十首、第一首／最後一首與輸入曲號跳轉
- 暫停、循環、重新開始、音量、播放時間與音量表
- 使用預先排隊的 SDL3 音訊，避免 Windows 下爆音與 buffer underrun
- 啟動時自動讀取 EXE 同目錄的 `MUS.MKF`
- 找不到時讀取同目錄 `sdlpal.cfg` 的 `GamePath`
- Windows 不會另外開啟空白 command 視窗
- Windows 預設產生單一靜態連結 EXE

## MSYS2 UCRT64 編譯

先安裝：

```sh
pacman -S --needed \
  mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-sdl3 \
  mingw-w64-ucrt-x86_64-pkgconf
```

假設：

```text
D:\sdlpal-src          SDLPAL 原始碼
D:\sdlpal-rix-player   本專案
```

在 UCRT64 執行：

```sh
cd /d/sdlpal-rix-player
./build-msys2.sh /d/sdlpal-src
```

也可以直接使用 CMake：

```sh
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DRIX_PLAYER_STATIC=ON \
  -DSDLPAL_ROOT=D:/sdlpal-src
cmake --build build
```

輸出：

```text
build/sdlpal-rix-player.exe
```

## 啟動時自動找檔

未指定參數時，依序尋找：

1. EXE 同目錄的 `MUS.MKF` 或 `mus.mkf`
2. EXE 同目錄的 `sdlpal.cfg`
3. `sdlpal.cfg` 中 `GamePath` 指向目錄內的 `MUS.MKF`

例如：

```ini
GamePath=./PAL95/
```

## 操作

| 按鍵 | 功能 |
|---|---|
| 左／右 | 上一首／下一首有效原始曲號 |
| Page Up／Page Down | 前後十首 |
| Home／End | 第一首／最後一首 |
| 數字後 Enter | 跳到指定原始曲號 |
| Space | 暫停／繼續 |
| L | 切換循環 |
| 上／下 | 音量 +/-5% |
| F5 | 重新播放目前曲目 |
| Escape | 離開 |

## 上傳與發佈注意事項

請勿把 `MUS.MKF`、獨立 RIX、遊戲圖像、腳本或其他原版資源提交到 GitHub。
由於 Windows EXE 會靜態包含所選 SDLPAL checkout 的程式碼，發佈二進位檔時也應
提供當次編譯使用的精確 SDLPAL 對應原始碼。最穩妥的方式是執行：

```sh
./package-source-bundle.sh /d/sdlpal-src 0.1.0
```

再把產生的 `source-with-sdlpal.tar.gz` 與單一 EXE 一起附加到 GitHub Release。
這個腳本只匯出 Git 追蹤的檔案，會記錄兩邊的 commit ID，也不會複製本機遊戲資料。

## 授權

本專案採 GPL-3.0-or-later。詳見 `LICENSE` 與
`THIRD_PARTY_NOTICES.md`。
