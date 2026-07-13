/* Traditional Chinese UI wrapper for the original player implementation. */
#include <algorithm>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>
#ifdef _WIN32
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# ifndef NOMINMAX
#  define NOMINMAX 1
# endif
# include <windows.h>
#endif
#include "track_metadata.h"

namespace zh_ui {
TTF_Font *font = nullptr;
std::unordered_map<std::string, SDL_Texture *> cache;

SDL_IOStream *open_font_stream() {
#ifdef _WIN32
    HMODULE module = GetModuleHandleW(nullptr);
    HRSRC resource = FindResourceW(
        module,
        MAKEINTRESOURCEW(FUSION_PIXEL_FONT_RESOURCE_ID),
        MAKEINTRESOURCEW(10)); // RT_RCDATA
    if (!resource) return nullptr;
    HGLOBAL loaded = LoadResource(module, resource);
    if (!loaded) return nullptr;
    const DWORD size = SizeofResource(module, resource);
    const void *data = LockResource(loaded);
    return data && size ? SDL_IOFromConstMem(data, static_cast<size_t>(size)) : nullptr;
#else
    const char *base = SDL_GetBasePath();
    std::string path = base ? base : "./";
    path += FUSION_PIXEL_FONT_FILENAME;
    return SDL_IOFromFile(path.c_str(), "rb");
#endif
}

bool ensure_font() {
    if (font) return true;
    if (!TTF_WasInit() && !TTF_Init()) return false;
    SDL_IOStream *io = open_font_stream();
    if (!io) return false;
    font = TTF_OpenFontIO(io, true, 12.0f);
    return font != nullptr;
}

std::string translate(const char *raw) {
    const std::string text = raw ? raw : "";
    if (text == "SDLPAL RIX MUSIC PLAYER") return u8"仙劍奇俠傳 RIX 音樂播放器";
    if (text == "DROP MUS.MKF HERE") return u8"請將 MUS.MKF 拖曳至此";
    if (text == "or pass the file path on the command line.") return u8"也可以從命令列指定檔案路徑";
    if (text == "LEFT/RIGHT TRACK   PGUP/PGDN +/-10   SPACE PAUSE   L LOOP")
        return u8"←/→ 切換曲目　PageUp/PageDown ±10　Space 暫停　L 循環";
    if (text == "UP/DOWN VOLUME   F5 RESTART   HOME/END   TYPE ID + ENTER   ESC QUIT")
        return u8"↑/↓ 音量　F5 重播　Home/End 首尾　輸入編號 + Enter　Esc 離開";
    if (text == "OPL2 LEVEL") return u8"OPL2 音量";
    if (text.rfind("TRACK ", 0) == 0) {
        int id = -1;
        std::sscanf(text.c_str(), "TRACK %d", &id);
        char number[32];
        std::snprintf(number, sizeof(number), u8"曲目 %03d", id);
        std::string result = number;
        if (const TrackMetadata *meta = find_track_metadata(id)) {
            result += u8"　";
            result += meta->name_zh_hant;
        }
        return result;
    }
    if (text.rfind("PLAYING", 0) == 0) return u8"播放中" + text.substr(7);
    if (text.rfind("PAUSED", 0) == 0) return u8"已暫停" + text.substr(6);
    if (text.rfind("ENDED", 0) == 0) return u8"已結束" + text.substr(5);
    if (text.rfind("PLAYABLE ENTRY ", 0) == 0) return u8"有效曲目 " + text.substr(15);
    if (text.rfind("LOOP ON", 0) == 0) return u8"循環 開" + text.substr(7);
    if (text.rfind("LOOP OFF", 0) == 0) return u8"循環 關" + text.substr(8);
    if (text.rfind("FILE: ", 0) == 0) return u8"檔案：" + text.substr(6);
    if (text.rfind("JUMP TO RAW TRACK ID: ", 0) == 0) return u8"跳到原始曲號：" + text.substr(22);
    if (text.rfind("ERROR: ", 0) == 0) return u8"錯誤：" + text.substr(7);
    return text;
}

bool render(SDL_Renderer *renderer, float x, float y, const char *raw) {
    if (!ensure_font()) return SDL_RenderDebugText(renderer, x, y, raw);
    const std::string text = translate(raw);
    const auto found = cache.find(text);
    SDL_Texture *texture = found == cache.end() ? nullptr : found->second;
    if (!texture) {
        SDL_Color white{255, 255, 255, 255};
        SDL_Surface *surface = TTF_RenderText_Solid(font, text.c_str(), 0, white);
        if (!surface) return false;
        texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_DestroySurface(surface);
        if (!texture) return false;
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
        cache.emplace(text, texture);
    }
    float w = 0.0f, h = 0.0f;
    SDL_GetTextureSize(texture, &w, &h);
    SDL_FRect destination{x, y, w, h};
    return SDL_RenderTexture(renderer, texture, nullptr, &destination);
}

bool set_title(SDL_Window *window, const char *raw) {
    std::string title = raw ? raw : "";
    const std::string prefix = "SDLPAL RIX Music Player - Track ";
    if (title.rfind(prefix, 0) == 0) {
        const std::string id_text = title.substr(prefix.size(), 3);
        int id = -1;
        try { id = std::stoi(id_text); } catch (...) {}
        title = u8"仙劍 RIX 音樂播放器 - 曲目 " + id_text;
        if (const TrackMetadata *meta = find_track_metadata(id)) {
            title += u8" ";
            title += meta->name_zh_hant;
        }
        if (raw && std::string(raw).find("[Paused]") != std::string::npos) title += u8" [暫停]";
    } else if (title.find("Drop MUS.MKF") != std::string::npos) {
        title = u8"仙劍 RIX 音樂播放器 - 請拖入 MUS.MKF";
    }
    return SDL_SetWindowTitle(window, title.c_str());
}
} // namespace zh_ui

#define SDL_RenderDebugText zh_ui::render
#define SDL_SetWindowTitle zh_ui::set_title
#include "src/legacy_player.inc"
