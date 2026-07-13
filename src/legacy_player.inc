/*
 * SDLPAL RIX Music Player
 * Copyright (C) 2026 sdlpal-rix-player contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This tool is intended to be built inside an SDLPAL source checkout and
 * uses SDLPAL's bundled AdPlug RIX decoder and OPL emulation cores.
 */

// Include the C++ standard library before SDLPAL headers. SDLPAL's common.h
// defines legacy min/max macros, which otherwise corrupt declarations in
// <algorithm>, <chrono>, <limits>, and other standard headers on newer GCC.
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <mutex>
#include <new>
#include <string>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "adplug/rix.h"
#include "adplug/emuopls.h"

// common.h defines these for the C portions of SDLPAL. They must not leak into
// this C++ translation unit because they rewrite std::min/std::max and even
// numeric_limits<T>::min()/max().
#ifdef min
# undef min
#endif
#ifdef max
# undef max
#endif

namespace {

constexpr int kOplSampleRate = 49716; // SDLPAL's default OPL sample rate.
constexpr int kRixRefreshRate = 70;
constexpr int kWindowWidth = 760;
constexpr int kWindowHeight = 360;

// Keep a healthy amount of decoded PCM queued ahead of the device. The first
// version generated audio directly inside SDL's realtime callback and also
// allocated a std::vector there. On some Windows systems that could miss a
// deadline and sound like a too-small audio buffer. Manual queueing keeps all
// OPL work and allocations on the main thread instead.
constexpr int kAudioChunkFrames = 1024;
constexpr int kAudioQueueTargetFrames = 8192; // About 165 ms at 49,716 Hz.

struct TrackInfo {
    int id = -1;
    std::uint32_t offset = 0;
    std::uint32_t size = 0;
};

struct PlayerSnapshot {
    bool loaded = false;
    bool paused = false;
    bool looping = true;
    bool ended = false;
    std::string path;
    std::string error;
    int track_id = -1;
    int track_position = -1;
    int track_count = 0;
    std::uint32_t track_size = 0;
    int volume = 100;
    double elapsed_seconds = 0.0;
    float peak = 0.0f;
};

std::uint32_t read_le32(const unsigned char *p) {
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8U) |
           (static_cast<std::uint32_t>(p[2]) << 16U) |
           (static_cast<std::uint32_t>(p[3]) << 24U);
}

bool file_exists(const std::string &path) {
    std::ifstream file(path, std::ios::binary);
    return file.good();
}

bool is_path_separator(char c) {
    return c == '/' || c == '\\';
}

bool is_absolute_path(const std::string &path) {
    if (path.empty()) {
        return false;
    }

    // POSIX absolute path, Windows rooted path, or UNC path.
    if (is_path_separator(path.front())) {
        return true;
    }

    // Windows drive-qualified absolute path, such as C:\\PAL95.
    return path.size() >= 3 &&
           std::isalpha(static_cast<unsigned char>(path[0])) != 0 &&
           path[1] == ':' && is_path_separator(path[2]);
}

std::string join_path(const std::string &directory, const std::string &child) {
    if (child.empty()) {
        return directory;
    }
    if (directory.empty() || is_absolute_path(child)) {
        return child;
    }
    if (is_path_separator(directory.back())) {
        return directory + child;
    }
    return directory + "/" + child;
}

std::string trim_copy(const std::string &text) {
    std::size_t begin = 0;
    while (begin < text.size() &&
           std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
        ++begin;
    }

    std::size_t end = text.size();
    while (end > begin &&
           std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }
    return text.substr(begin, end - begin);
}

bool ascii_iequals(const std::string &left, const std::string &right) {
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t i = 0; i < left.size(); ++i) {
        const unsigned char a = static_cast<unsigned char>(left[i]);
        const unsigned char b = static_cast<unsigned char>(right[i]);
        if (std::tolower(a) != std::tolower(b)) {
            return false;
        }
    }
    return true;
}

std::string remove_optional_quotes(std::string value) {
    value = trim_copy(value);
    if (value.size() >= 2) {
        const char first = value.front();
        const char last = value.back();
        if ((first == '"' && last == '"') ||
            (first == '\'' && last == '\'')) {
            value = value.substr(1, value.size() - 2);
        }
    }
    return trim_copy(value);
}

std::string read_game_path_from_config(const std::string &config_path) {
    std::ifstream config(config_path);
    if (!config) {
        return {};
    }

    std::string line;
    bool first_line = true;
    while (std::getline(config, line)) {
        if (first_line) {
            first_line = false;
            if (line.size() >= 3 &&
                static_cast<unsigned char>(line[0]) == 0xEF &&
                static_cast<unsigned char>(line[1]) == 0xBB &&
                static_cast<unsigned char>(line[2]) == 0xBF) {
                line.erase(0, 3);
            }
        }

        const std::string trimmed = trim_copy(line);
        if (trimmed.empty() || trimmed.front() == '#' || trimmed.front() == ';') {
            continue;
        }

        const std::size_t equals = trimmed.find('=');
        if (equals == std::string::npos) {
            continue;
        }

        const std::string key = trim_copy(trimmed.substr(0, equals));
        if (!ascii_iequals(key, "GamePath")) {
            continue;
        }

        return remove_optional_quotes(trimmed.substr(equals + 1));
    }

    return {};
}

std::string find_music_in_directory(const std::string &directory) {
    const std::string uppercase = join_path(directory, "MUS.MKF");
    if (file_exists(uppercase)) {
        return uppercase;
    }

    const std::string lowercase = join_path(directory, "mus.mkf");
    if (file_exists(lowercase)) {
        return lowercase;
    }
    return {};
}

std::string find_default_music_path() {
    const char *base_path_ptr = SDL_GetBasePath();
    const std::string base_path = base_path_ptr ? base_path_ptr : std::string("./");

    // First prefer a MUS.MKF distributed beside the player executable.
    std::string music_path = find_music_in_directory(base_path);
    if (!music_path.empty()) {
        return music_path;
    }

    // Otherwise honor the same GamePath setting used by SDLPAL. Relative
    // values are resolved from the directory containing sdlpal.cfg/the EXE.
    const std::string config_path = join_path(base_path, "sdlpal.cfg");
    const std::string game_path = read_game_path_from_config(config_path);
    if (game_path.empty()) {
        return {};
    }

    const std::string resolved_game_path =
        is_absolute_path(game_path) ? game_path : join_path(base_path, game_path);
    return find_music_in_directory(resolved_game_path);
}

std::string lowercase_extension(const std::string &path) {
    const std::size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) {
        return {};
    }
    std::string ext = path.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return ext;
}

bool parse_track_index(const std::string &path,
                       std::vector<TrackInfo> &tracks,
                       std::string &error) {
    tracks.clear();

    const std::string extension = lowercase_extension(path);
    if (extension != ".mkf" && extension != ".rix") {
        error = "Expected a .MKF archive or a standalone .RIX file.";
        return false;
    }

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "Cannot open file: " + path;
        return false;
    }

    file.seekg(0, std::ios::end);
    const std::streamoff end_position = file.tellg();
    if (end_position < 2) {
        error = "The file is too small to contain RIX data.";
        return false;
    }
    const std::uint64_t file_size = static_cast<std::uint64_t>(end_position);
    file.seekg(0, std::ios::beg);

    unsigned char header[4] = {0, 0, 0, 0};
    file.read(reinterpret_cast<char *>(header),
              static_cast<std::streamsize>(std::min<std::uint64_t>(4, file_size)));
    if (!file && file.gcount() < 2) {
        error = "Failed to read the file header.";
        return false;
    }

    // A standalone RIX file starts with the little-endian 0x55AA signature.
    if (header[0] == 0xAA && header[1] == 0x55) {
        if (extension != ".rix") {
            error = "The file contains standalone RIX data but does not use a .RIX extension.";
            return false;
        }
        tracks.push_back({0, 0, static_cast<std::uint32_t>(
                                     std::min<std::uint64_t>(
                                         file_size,
                                         std::numeric_limits<std::uint32_t>::max()))});
        return true;
    }

    if (extension != ".mkf") {
        error = "The .RIX file does not start with the 0x55AA RIX signature.";
        return false;
    }

    if (file_size < 8) {
        error = "Not a standalone RIX file or a valid MKF archive.";
        return false;
    }

    const std::uint32_t first_offset = read_le32(header);
    if (first_offset < 8 || first_offset % 4 != 0 || first_offset > file_size) {
        error = "Invalid MKF offset table.";
        return false;
    }

    const std::size_t offset_count = first_offset / 4U;
    if (offset_count < 2 || offset_count > 65536) {
        error = "Unreasonable MKF offset count.";
        return false;
    }

    std::vector<unsigned char> table(first_offset);
    file.clear();
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char *>(table.data()),
              static_cast<std::streamsize>(table.size()));
    if (!file) {
        error = "Failed to read the MKF offset table.";
        return false;
    }

    std::vector<std::uint32_t> offsets(offset_count);
    for (std::size_t i = 0; i < offset_count; ++i) {
        offsets[i] = read_le32(table.data() + i * 4U);
    }

    // MKF archives may legitimately begin with one or more empty entries.
    // MUS.MKF commonly keeps music ID 0 empty because SDLPAL uses ID 0 to
    // mean "stop music". Repeated leading offsets are therefore valid; the
    // bytes at first_offset still begin the first non-empty RIX entry.
    if (offsets.front() != first_offset || offsets.size() < 2 ||
        offsets.back() > file_size) {
        error = "The MKF offset table is inconsistent.";
        return false;
    }

    for (std::size_t i = 1; i < offsets.size(); ++i) {
        if (offsets[i] < offsets[i - 1] || offsets[i] > file_size) {
            error = "The MKF offsets are not monotonic or exceed the file size.";
            return false;
        }
    }

    unsigned char first_signature[2] = {0, 0};
    file.clear();
    file.seekg(offsets[0], std::ios::beg);
    file.read(reinterpret_cast<char *>(first_signature), 2);
    if (!file || first_signature[0] != 0xAA || first_signature[1] != 0x55) {
        error = "The first non-empty MKF entry is not valid RIX data.";
        return false;
    }

    for (std::size_t i = 0; i + 1 < offsets.size(); ++i) {
        const std::uint32_t begin = offsets[i];
        const std::uint32_t end = offsets[i + 1];
        if (begin > end || end > file_size || end - begin < 2) {
            continue;
        }

        unsigned char signature[2] = {0, 0};
        file.clear();
        file.seekg(begin, std::ios::beg);
        file.read(reinterpret_cast<char *>(signature), 2);
        if (file && signature[0] == 0xAA && signature[1] == 0x55) {
            tracks.push_back({static_cast<int>(i), begin, end - begin});
        }
    }

    if (tracks.empty()) {
        error = "No playable RIX entries were found in this MKF file.";
        return false;
    }

    return true;
}

class RixEngine {
public:
    RixEngine() = default;

    ~RixEngine() {
        std::lock_guard<std::mutex> lock(mutex_);
        destroy_decoder_locked();
    }

    bool load(const std::string &path, int preferred_track = -1) {
        std::vector<TrackInfo> parsed_tracks;
        std::string parse_error;
        if (!parse_track_index(path, parsed_tracks, parse_error)) {
            std::lock_guard<std::mutex> lock(mutex_);
            error_ = parse_error;
            return false;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        destroy_decoder_locked();

        opl_ = CEmuopl::CreateEmuopl(OPLCORE::DBFLT,
                                     Copl::TYPE_OPL2,
                                     kOplSampleRate);
        if (!opl_) {
            error_ = "Could not create the DBFLT OPL2 emulator.";
            return false;
        }

        player_ = new (std::nothrow) CrixPlayer(opl_);
        if (!player_) {
            delete opl_;
            opl_ = nullptr;
            error_ = "Out of memory while creating the RIX decoder.";
            return false;
        }

        try {
            if (!player_->load(path, CProvider_Filesystem())) {
                destroy_decoder_locked();
                error_ = "SDLPAL's RIX decoder rejected the file.";
                return false;
            }
        } catch (const std::exception &e) {
            destroy_decoder_locked();
            error_ = std::string("RIX decoder exception: ") + e.what();
            return false;
        }

        path_ = path;
        tracks_ = std::move(parsed_tracks);
        track_position_ = choose_initial_position_locked(preferred_track);
        error_.clear();
        paused_ = false;
        ended_ = false;
        rewind_locked();
        return true;
    }

    void render(std::int16_t *destination, int frame_count) {
        if (!destination || frame_count <= 0) {
            return;
        }

        std::fill(destination, destination + frame_count, 0);
        std::lock_guard<std::mutex> lock(mutex_);

        if (!player_ || !opl_ || paused_ || ended_ || track_position_ < 0) {
            return;
        }

        int output_position = 0;
        int local_peak = 0;

        while (output_position < frame_count) {
            if (frames_until_tick_ <= 0) {
                if (!player_->update()) {
                    if (!looping_) {
                        ended_ = true;
                        break;
                    }
                    rewind_locked();
                    if (!player_->update()) {
                        ended_ = true;
                        error_ = "The selected RIX entry ended immediately.";
                        break;
                    }
                }

                tick_fraction_ += kOplSampleRate;
                frames_until_tick_ = tick_fraction_ / kRixRefreshRate;
                tick_fraction_ %= kRixRefreshRate;
            }

            const int available = frame_count - output_position;
            const int count = std::min(available, frames_until_tick_);
            opl_->update(destination + output_position, count);

            for (int i = 0; i < count; ++i) {
                int sample = destination[output_position + i];
                sample = sample * volume_ / 100;
                sample = std::max<int>(std::numeric_limits<std::int16_t>::min(),
                                       std::min<int>(std::numeric_limits<std::int16_t>::max(),
                                                     sample));
                destination[output_position + i] = static_cast<std::int16_t>(sample);
                local_peak = std::max(local_peak, std::abs(sample));
            }

            output_position += count;
            frames_until_tick_ -= count;
            elapsed_frames_ += static_cast<std::uint64_t>(count);
        }

        peak_ = std::max(peak_, local_peak);
    }

    bool select_track(int track_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto found = std::find_if(tracks_.begin(), tracks_.end(),
                                        [track_id](const TrackInfo &track) {
                                            return track.id == track_id;
                                        });
        if (found == tracks_.end()) {
            error_ = "Track " + std::to_string(track_id) + " is empty or invalid.";
            return false;
        }
        track_position_ = static_cast<int>(std::distance(tracks_.begin(), found));
        error_.clear();
        paused_ = false;
        rewind_locked();
        return true;
    }

    void move_track(int delta) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (tracks_.empty()) {
            return;
        }
        const int count = static_cast<int>(tracks_.size());
        int next = track_position_ + delta;
        next %= count;
        if (next < 0) {
            next += count;
        }
        track_position_ = next;
        error_.clear();
        paused_ = false;
        rewind_locked();
    }

    void first_track() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!tracks_.empty()) {
            track_position_ = 0;
            paused_ = false;
            rewind_locked();
        }
    }

    void last_track() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!tracks_.empty()) {
            track_position_ = static_cast<int>(tracks_.size()) - 1;
            paused_ = false;
            rewind_locked();
        }
    }

    void restart() {
        std::lock_guard<std::mutex> lock(mutex_);
        rewind_locked();
    }

    void toggle_pause() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (player_) {
            paused_ = !paused_;
        }
    }

    void toggle_loop() {
        std::lock_guard<std::mutex> lock(mutex_);
        looping_ = !looping_;
    }

    void set_loop(bool value) {
        std::lock_guard<std::mutex> lock(mutex_);
        looping_ = value;
    }

    void adjust_volume(int delta) {
        std::lock_guard<std::mutex> lock(mutex_);
        volume_ = std::max(0, std::min(100, volume_ + delta));
    }

    PlayerSnapshot snapshot() {
        std::lock_guard<std::mutex> lock(mutex_);
        PlayerSnapshot result;
        result.loaded = player_ != nullptr && track_position_ >= 0;
        result.paused = paused_;
        result.looping = looping_;
        result.ended = ended_;
        result.path = path_;
        result.error = error_;
        result.volume = volume_;
        result.track_count = static_cast<int>(tracks_.size());
        result.track_position = track_position_;
        if (track_position_ >= 0 && track_position_ < static_cast<int>(tracks_.size())) {
            result.track_id = tracks_[track_position_].id;
            result.track_size = tracks_[track_position_].size;
        }
        result.elapsed_seconds = static_cast<double>(elapsed_frames_) /
                                 static_cast<double>(kOplSampleRate);
        result.peak = std::min(1.0f,
                               static_cast<float>(peak_) /
                                   static_cast<float>(std::numeric_limits<std::int16_t>::max()));
        peak_ = peak_ * 3 / 4;
        return result;
    }

private:
    int choose_initial_position_locked(int preferred_track) const {
        if (tracks_.empty()) {
            return -1;
        }
        if (preferred_track >= 0) {
            const auto found = std::find_if(tracks_.begin(), tracks_.end(),
                                            [preferred_track](const TrackInfo &track) {
                                                return track.id == preferred_track;
                                            });
            if (found != tracks_.end()) {
                return static_cast<int>(std::distance(tracks_.begin(), found));
            }
        }

        // RIX music ID 0 means "stop" to SDLPAL's game logic, so prefer ID 1.
        const auto track_one = std::find_if(tracks_.begin(), tracks_.end(),
                                            [](const TrackInfo &track) {
                                                return track.id == 1;
                                            });
        if (track_one != tracks_.end()) {
            return static_cast<int>(std::distance(tracks_.begin(), track_one));
        }
        return 0;
    }

    void rewind_locked() {
        if (!player_ || track_position_ < 0 ||
            track_position_ >= static_cast<int>(tracks_.size())) {
            return;
        }
        player_->rewind(tracks_[track_position_].id);
        frames_until_tick_ = 0;
        tick_fraction_ = 0;
        elapsed_frames_ = 0;
        peak_ = 0;
        ended_ = false;
    }

    void destroy_decoder_locked() {
        delete player_;
        player_ = nullptr;
        delete opl_;
        opl_ = nullptr;
        tracks_.clear();
        track_position_ = -1;
        frames_until_tick_ = 0;
        tick_fraction_ = 0;
        elapsed_frames_ = 0;
        peak_ = 0;
        ended_ = false;
        path_.clear();
    }

    std::mutex mutex_;
    Copl *opl_ = nullptr;
    CrixPlayer *player_ = nullptr;
    std::vector<TrackInfo> tracks_;
    std::string path_;
    std::string error_;
    int track_position_ = -1;
    int frames_until_tick_ = 0;
    int tick_fraction_ = 0;
    std::uint64_t elapsed_frames_ = 0;
    int peak_ = 0;
    int volume_ = 100;
    bool paused_ = false;
    bool looping_ = true;
    bool ended_ = false;
};

bool pump_audio(SDL_AudioStream *stream, RixEngine &engine) {
    if (!stream) {
        return false;
    }

    constexpr int bytes_per_frame = static_cast<int>(sizeof(std::int16_t));
    constexpr int target_bytes = kAudioQueueTargetFrames * bytes_per_frame;

    int queued_bytes = SDL_GetAudioStreamQueued(stream);
    if (queued_bytes < 0) {
        return false;
    }

    std::array<std::int16_t, kAudioChunkFrames> buffer{};
    while (queued_bytes < target_bytes) {
        const int missing_bytes = target_bytes - queued_bytes;
        const int missing_frames =
            std::max(1, (missing_bytes + bytes_per_frame - 1) / bytes_per_frame);
        const int frames = std::min(kAudioChunkFrames, missing_frames);

        engine.render(buffer.data(), frames);
        const int bytes = frames * bytes_per_frame;
        if (!SDL_PutAudioStreamData(stream, buffer.data(), bytes)) {
            return false;
        }
        queued_bytes += bytes;
    }

    return true;
}

std::string shorten_path(const std::string &path, std::size_t max_length) {
    if (path.size() <= max_length) {
        return path;
    }
    if (max_length <= 3) {
        return path.substr(path.size() - max_length);
    }
    return "..." + path.substr(path.size() - (max_length - 3));
}

std::string format_time(double seconds) {
    const int total = std::max(0, static_cast<int>(seconds));
    const int minutes = total / 60;
    const int remaining = total % 60;
    char text[32];
    std::snprintf(text, sizeof(text), "%02d:%02d", minutes, remaining);
    return text;
}

void set_color(SDL_Renderer *renderer,
               Uint8 red,
               Uint8 green,
               Uint8 blue,
               Uint8 alpha = 255) {
    SDL_SetRenderDrawColor(renderer, red, green, blue, alpha);
}

void draw_text(SDL_Renderer *renderer,
               float x,
               float y,
               const std::string &text,
               Uint8 red = 230,
               Uint8 green = 235,
               Uint8 blue = 242) {
    set_color(renderer, red, green, blue);
    SDL_RenderDebugText(renderer, x, y, text.c_str());
}

void draw_scaled_text(SDL_Renderer *renderer,
                      float x,
                      float y,
                      float scale,
                      const std::string &text,
                      Uint8 red,
                      Uint8 green,
                      Uint8 blue) {
    SDL_SetRenderScale(renderer, scale, scale);
    draw_text(renderer, x / scale, y / scale, text, red, green, blue);
    SDL_SetRenderScale(renderer, 1.0f, 1.0f);
}

void draw_panel(SDL_Renderer *renderer,
                float x,
                float y,
                float width,
                float height,
                Uint8 red,
                Uint8 green,
                Uint8 blue) {
    set_color(renderer, red, green, blue);
    const SDL_FRect rectangle{x, y, width, height};
    SDL_RenderFillRect(renderer, &rectangle);
}

void render_ui(SDL_Renderer *renderer,
               const PlayerSnapshot &state,
               const std::string &numeric_input) {
    int output_width = kWindowWidth;
    int output_height = kWindowHeight;
    SDL_GetRenderOutputSize(renderer, &output_width, &output_height);

    set_color(renderer, 13, 17, 24);
    SDL_RenderClear(renderer);

    draw_panel(renderer, 18.0f, 18.0f,
               static_cast<float>(output_width - 36), 58.0f,
               28, 35, 48);
    draw_scaled_text(renderer, 34.0f, 31.0f, 2.0f,
                     "SDLPAL RIX MUSIC PLAYER",
                     125, 211, 252);

    draw_panel(renderer, 18.0f, 90.0f,
               static_cast<float>(output_width - 36), 132.0f,
               22, 28, 38);

    if (state.loaded) {
        char track_text[64];
        std::snprintf(track_text, sizeof(track_text), "TRACK %03d", state.track_id);
        draw_scaled_text(renderer, 36.0f, 108.0f, 3.0f,
                         track_text,
                         250, 204, 21);

        const std::string status = state.paused
                                       ? "PAUSED"
                                       : (state.ended ? "ENDED" : "PLAYING");
        draw_text(renderer, 38.0f, 162.0f,
                  status + "   " + format_time(state.elapsed_seconds),
                  state.paused ? 252 : 132,
                  state.paused ? 165 : 230,
                  state.paused ? 165 : 176);

        char details[160];
        std::snprintf(details, sizeof(details),
                      "PLAYABLE ENTRY %d / %d    RAW SIZE %u BYTES",
                      state.track_position + 1,
                      state.track_count,
                      state.track_size);
        draw_text(renderer, 38.0f, 184.0f, details, 166, 177, 193);

        char options[96];
        std::snprintf(options, sizeof(options),
                      "LOOP %s    VOLUME %d%%",
                      state.looping ? "ON" : "OFF",
                      state.volume);
        draw_text(renderer, 38.0f, 202.0f, options, 166, 177, 193);

        const float meter_width = std::max(80.0f,
                                            static_cast<float>(output_width) - 470.0f);
        draw_panel(renderer, 420.0f, 154.0f, meter_width, 18.0f, 42, 50, 65);
        draw_panel(renderer, 420.0f, 154.0f,
                   meter_width * state.peak, 18.0f,
                   74, 222, 128);
        draw_text(renderer, 420.0f, 180.0f, "OPL2 LEVEL", 145, 157, 174);
    } else {
        draw_scaled_text(renderer, 36.0f, 112.0f, 2.0f,
                         "DROP MUS.MKF HERE",
                         250, 204, 21);
        draw_text(renderer, 38.0f, 154.0f,
                  "or pass the file path on the command line.",
                  166, 177, 193);
    }

    draw_text(renderer, 26.0f, 236.0f,
              "FILE: " + shorten_path(state.path.empty() ? "(none)" : state.path, 86),
              145, 157, 174);

    if (!numeric_input.empty()) {
        draw_panel(renderer, 18.0f, 258.0f,
                   static_cast<float>(output_width - 36), 30.0f,
                   50, 43, 24);
        draw_text(renderer, 28.0f, 269.0f,
                  "JUMP TO RAW TRACK ID: " + numeric_input + "  [ENTER]",
                  250, 204, 21);
    } else if (!state.error.empty()) {
        draw_panel(renderer, 18.0f, 258.0f,
                   static_cast<float>(output_width - 36), 30.0f,
                   55, 28, 32);
        draw_text(renderer, 28.0f, 269.0f,
                  shorten_path("ERROR: " + state.error, 88),
                  255, 151, 151);
    } else {
        draw_text(renderer, 26.0f, 270.0f,
                  "LEFT/RIGHT TRACK   PGUP/PGDN +/-10   SPACE PAUSE   L LOOP",
                  145, 157, 174);
    }

    draw_text(renderer, 26.0f, 304.0f,
              "UP/DOWN VOLUME   F5 RESTART   HOME/END   TYPE ID + ENTER   ESC QUIT",
              145, 157, 174);
    draw_text(renderer, 26.0f, 326.0f,
              "Buffered SDL3 queue + SDLPAL AdPlug DBFLT OPL2 @ 49716 Hz",
              104, 117, 135);

    SDL_RenderPresent(renderer);
}

struct CommandLine {
    std::string path;
    int track = -1;
    bool loop = true;
    bool show_help = false;
};

CommandLine parse_command_line(int argc, char **argv) {
    CommandLine result;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--help" || argument == "-h") {
            result.show_help = true;
        } else if (argument == "--no-loop") {
            result.loop = false;
        } else if (argument == "--track" && i + 1 < argc) {
            try {
                result.track = std::stoi(argv[++i]);
            } catch (...) {
                result.track = -1;
            }
        } else if (!argument.empty() && argument[0] != '-' && result.path.empty()) {
            result.path = argument;
        }
    }
    return result;
}

void print_usage(const char *program) {
    std::printf("SDLPAL RIX Music Player\n\n");
    std::printf("Usage: %s [MUS.MKF|track.rix] [--track N] [--no-loop]\n", program);
    std::printf("Without a music path, the player checks its own directory and then\n");
    std::printf("uses GamePath from a neighboring sdlpal.cfg.\n");
    std::printf("The program contains no game music. Supply your own original MUS.MKF.\n");
}

int keypad_digit(SDL_Keycode key) {
    if (key >= SDLK_0 && key <= SDLK_9) {
        return static_cast<int>(key - SDLK_0);
    }
    switch (key) {
    case SDLK_KP_0: return 0;
    case SDLK_KP_1: return 1;
    case SDLK_KP_2: return 2;
    case SDLK_KP_3: return 3;
    case SDLK_KP_4: return 4;
    case SDLK_KP_5: return 5;
    case SDLK_KP_6: return 6;
    case SDLK_KP_7: return 7;
    case SDLK_KP_8: return 8;
    case SDLK_KP_9: return 9;
    default: return -1;
    }
}

} // namespace

int main(int argc, char **argv) {
    const CommandLine command_line = parse_command_line(argc, argv);
    if (command_line.show_help) {
        print_usage(argv[0]);
        return 0;
    }

    if (!SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    if (!SDL_CreateWindowAndRenderer("SDLPAL RIX Music Player",
                                     kWindowWidth,
                                     kWindowHeight,
                                     SDL_WINDOW_RESIZABLE,
                                     &window,
                                     &renderer)) {
        std::fprintf(stderr, "Could not create window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    RixEngine engine;
    engine.set_loop(command_line.loop);

    std::string initial_path = command_line.path;
    if (initial_path.empty()) {
        initial_path = find_default_music_path();
    }
    if (!initial_path.empty()) {
        engine.load(initial_path, command_line.track);
    }

    SDL_AudioSpec audio_spec{};
    audio_spec.format = SDL_AUDIO_S16;
    audio_spec.channels = 1;
    audio_spec.freq = kOplSampleRate;

    // Ask SDL for a moderately large device buffer, then feed the stream from
    // the main loop instead of generating audio in a realtime callback. SDL
    // may adjust or ignore this hint depending on the backend.
    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, "2048");

    SDL_AudioStream *audio_stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
        &audio_spec,
        nullptr,
        nullptr);
    if (!audio_stream) {
        std::fprintf(stderr, "Could not open audio device: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    const PlayerSnapshot initial_state = engine.snapshot();
    if (initial_state.loaded && !initial_state.paused && !initial_state.ended &&
        !pump_audio(audio_stream, engine)) {
        std::fprintf(stderr, "Could not prefill audio stream: %s\n", SDL_GetError());
    }

    if (!SDL_ResumeAudioStreamDevice(audio_stream)) {
        std::fprintf(stderr, "Could not resume audio device: %s\n", SDL_GetError());
    }

    auto reset_queued_audio = [&]() {
        SDL_ClearAudioStream(audio_stream);
        const PlayerSnapshot state = engine.snapshot();
        if (state.loaded && !state.paused && !state.ended &&
            !pump_audio(audio_stream, engine)) {
            std::fprintf(stderr, "Could not refill audio stream: %s\n", SDL_GetError());
        }
        if (!state.paused) {
            SDL_ResumeAudioStreamDevice(audio_stream);
        }
    };

    bool running = true;
    std::string numeric_input;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
                continue;
            }

            if (event.type == SDL_EVENT_DROP_FILE && event.drop.data) {
                numeric_input.clear();
                if (engine.load(event.drop.data)) {
                    reset_queued_audio();
                }
                continue;
            }

            if (event.type != SDL_EVENT_KEY_DOWN) {
                continue;
            }

            const SDL_Keycode key = event.key.key;
            const int digit = keypad_digit(key);
            if (digit >= 0) {
                if (numeric_input.size() < 6) {
                    numeric_input.push_back(static_cast<char>('0' + digit));
                }
                continue;
            }

            switch (key) {
            case SDLK_ESCAPE:
                running = false;
                break;
            case SDLK_BACKSPACE:
                if (!numeric_input.empty()) {
                    numeric_input.pop_back();
                }
                break;
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                if (!numeric_input.empty()) {
                    try {
                        if (engine.select_track(std::stoi(numeric_input))) {
                            reset_queued_audio();
                        }
                    } catch (...) {
                        // Ignore malformed input; only digits are accepted.
                    }
                    numeric_input.clear();
                }
                break;
            case SDLK_LEFT:
            case SDLK_MEDIA_PREVIOUS_TRACK:
                numeric_input.clear();
                engine.move_track(-1);
                reset_queued_audio();
                break;
            case SDLK_RIGHT:
            case SDLK_MEDIA_NEXT_TRACK:
                numeric_input.clear();
                engine.move_track(1);
                reset_queued_audio();
                break;
            case SDLK_PAGEUP:
                numeric_input.clear();
                engine.move_track(-10);
                reset_queued_audio();
                break;
            case SDLK_PAGEDOWN:
                numeric_input.clear();
                engine.move_track(10);
                reset_queued_audio();
                break;
            case SDLK_HOME:
                numeric_input.clear();
                engine.first_track();
                reset_queued_audio();
                break;
            case SDLK_END:
                numeric_input.clear();
                engine.last_track();
                reset_queued_audio();
                break;
            case SDLK_SPACE:
            case SDLK_MEDIA_PLAY_PAUSE: {
                engine.toggle_pause();
                const PlayerSnapshot pause_state = engine.snapshot();
                if (pause_state.paused) {
                    SDL_PauseAudioStreamDevice(audio_stream);
                } else {
                    if (!pump_audio(audio_stream, engine)) {
                        std::fprintf(stderr, "Could not refill audio stream: %s\n",
                                     SDL_GetError());
                    }
                    SDL_ResumeAudioStreamDevice(audio_stream);
                }
                break;
            }
            case SDLK_L:
                engine.toggle_loop();
                break;
            case SDLK_UP:
            case SDLK_VOLUMEUP:
                engine.adjust_volume(5);
                break;
            case SDLK_DOWN:
            case SDLK_VOLUMEDOWN:
                engine.adjust_volume(-5);
                break;
            case SDLK_F5:
                engine.restart();
                reset_queued_audio();
                break;
            default:
                break;
            }
        }

        PlayerSnapshot state = engine.snapshot();
        if (state.loaded && !state.paused && !state.ended) {
            if (!pump_audio(audio_stream, engine)) {
                std::fprintf(stderr, "Could not queue audio data: %s\n", SDL_GetError());
            }
            state = engine.snapshot();
        }

        if (state.loaded) {
            char title[128];
            std::snprintf(title, sizeof(title),
                          "SDLPAL RIX Music Player - Track %03d%s",
                          state.track_id,
                          state.paused ? " [Paused]" : "");
            SDL_SetWindowTitle(window, title);
        } else {
            SDL_SetWindowTitle(window, "SDLPAL RIX Music Player - Drop MUS.MKF");
        }

        render_ui(renderer, state, numeric_input);
        SDL_Delay(16);
    }

    SDL_DestroyAudioStream(audio_stream);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
