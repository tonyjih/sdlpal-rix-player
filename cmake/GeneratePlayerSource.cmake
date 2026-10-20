# Generate the player implementation with portable QoL preferences applied.
# Keeping these focused replacements here lets the historical player source
# remain readable while producing a single, deterministic translation unit.

if(NOT DEFINED SOURCE OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "GeneratePlayerSource.cmake requires SOURCE and OUTPUT")
endif()

file(READ "${SOURCE}" player)

function(replace_required old_text new_text description)
    string(FIND "${player}" "${old_text}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "Could not apply player source update: ${description}")
    endif()
    string(REPLACE "${old_text}" "${new_text}" player "${player}")
    set(player "${player}" PARENT_SCOPE)
endfunction()

replace_required([=[std::string find_default_music_path() {
]=] [=[struct PlayerPreferences {
    int last_track = -1;
    int volume = 100;
    bool loop = true;
};

std::string preferences_path() {
    const char *base_path_ptr = SDL_GetBasePath();
    const std::string base_path = base_path_ptr ? base_path_ptr : std::string("./");
    return join_path(base_path, "rix-player.cfg");
}

bool parse_bool_value(const std::string &value, bool fallback) {
    if (ascii_iequals(value, "1") || ascii_iequals(value, "true") ||
        ascii_iequals(value, "on") || ascii_iequals(value, "yes")) return true;
    if (ascii_iequals(value, "0") || ascii_iequals(value, "false") ||
        ascii_iequals(value, "off") || ascii_iequals(value, "no")) return false;
    return fallback;
}

PlayerPreferences load_preferences(const std::string &path) {
    PlayerPreferences result;
    std::ifstream file(path);
    if (!file) return result;

    std::string line;
    while (std::getline(file, line)) {
        const std::string trimmed = trim_copy(line);
        if (trimmed.empty() || trimmed.front() == '#' || trimmed.front() == ';') continue;
        const std::size_t equals = trimmed.find('=');
        if (equals == std::string::npos) continue;
        const std::string key = trim_copy(trimmed.substr(0, equals));
        const std::string value = trim_copy(trimmed.substr(equals + 1));
        try {
            if (ascii_iequals(key, "LastTrack")) {
                result.last_track = std::max(-1, std::stoi(value));
            } else if (ascii_iequals(key, "Volume")) {
                result.volume = std::max(0, std::min(100, std::stoi(value)));
            } else if (ascii_iequals(key, "Loop")) {
                result.loop = parse_bool_value(value, result.loop);
            }
        } catch (...) {
            // Ignore malformed values and retain the default for that field.
        }
    }
    return result;
}

bool save_preferences(const std::string &path, const PlayerPreferences &preferences) {
    const std::string temporary_path = path + ".tmp";
    {
        std::ofstream file(temporary_path, std::ios::trunc);
        if (!file) return false;
        file << "# SDLPAL RIX Music Player preferences\n";
        file << "LastTrack=" << preferences.last_track << "\n";
        file << "Volume=" << preferences.volume << "\n";
        file << "Loop=" << (preferences.loop ? 1 : 0) << "\n";
        file.flush();
        if (!file) return false;
    }
    std::remove(path.c_str());
    if (std::rename(temporary_path.c_str(), path.c_str()) != 0) {
        std::remove(temporary_path.c_str());
        return false;
    }
    return true;
}

std::string find_default_music_path() {
]=] "preference file support")

replace_required([=[    void adjust_volume(int delta) {
        std::lock_guard<std::mutex> lock(mutex_);
        volume_ = std::max(0, std::min(100, volume_ + delta));
    }
]=] [=[    void set_volume(int value) {
        std::lock_guard<std::mutex> lock(mutex_);
        volume_ = std::max(0, std::min(100, value));
    }

    void adjust_volume(int delta) {
        std::lock_guard<std::mutex> lock(mutex_);
        volume_ = std::max(0, std::min(100, volume_ + delta));
    }
]=] "absolute volume setter")

replace_required([=[struct CommandLine {
    std::string path;
    int track = -1;
    bool loop = true;
    bool show_help = false;
};
]=] [=[struct CommandLine {
    std::string path;
    int track = -1;
    bool loop = true;
    bool loop_overridden = false;
    bool show_help = false;
};
]=] "command-line override tracking")

replace_required([=[        } else if (argument == "--no-loop") {
            result.loop = false;
]=] [=[        } else if (argument == "--no-loop") {
            result.loop = false;
            result.loop_overridden = true;
        } else if (argument == "--loop") {
            result.loop = true;
            result.loop_overridden = true;
]=] "loop command-line overrides")

replace_required([=[    RixEngine engine;
    engine.set_loop(command_line.loop);

    std::string initial_path = command_line.path;
]=] [=[    const std::string settings_path = preferences_path();
    PlayerPreferences preferences = load_preferences(settings_path);

    RixEngine engine;
    engine.set_loop(command_line.loop_overridden ? command_line.loop : preferences.loop);
    engine.set_volume(preferences.volume);

    std::string initial_path = command_line.path;
]=] "load saved preferences")

replace_required([=[    if (!initial_path.empty()) {
        engine.load(initial_path, command_line.track);
    }
]=] [=[    if (!initial_path.empty()) {
        const int initial_track = command_line.track >= 0
                                      ? command_line.track
                                      : preferences.last_track;
        engine.load(initial_path, initial_track);
    }
]=] "restore the last track")

# SDLK_VOLUMEUP and SDLK_VOLUMEDOWN are operating-system media keys. On
# Windows they change the master output volume regardless of the focused app.
# Leave them to the OS and use only ordinary Up/Down for player-local gain.
replace_required([=[            case SDLK_VOLUMEUP:
]=] "" "leave system volume-up key to the operating system")
replace_required([=[            case SDLK_VOLUMEDOWN:
]=] "" "leave system volume-down key to the operating system")

replace_required("engine.adjust_volume(5);" "engine.adjust_volume(1);" "one-percent volume up")
replace_required("engine.adjust_volume(-5);" "engine.adjust_volume(-1);" "one-percent volume down")

replace_required([=[        if (state.loaded) {
            char title[128];
]=] [=[        PlayerPreferences current_preferences;
        current_preferences.last_track = state.loaded ? state.track_id : preferences.last_track;
        current_preferences.volume = state.volume;
        current_preferences.loop = state.looping;
        if (current_preferences.last_track != preferences.last_track ||
            current_preferences.volume != preferences.volume ||
            current_preferences.loop != preferences.loop) {
            if (save_preferences(settings_path, current_preferences)) {
                preferences = current_preferences;
            }
        }

        if (state.loaded) {
            char title[128];
]=] "save changed preferences")

replace_required([=[    SDL_DestroyAudioStream(audio_stream);
]=] [=[    const PlayerSnapshot final_state = engine.snapshot();
    PlayerPreferences final_preferences;
    final_preferences.last_track = final_state.loaded ? final_state.track_id : preferences.last_track;
    final_preferences.volume = final_state.volume;
    final_preferences.loop = final_state.looping;
    save_preferences(settings_path, final_preferences);

    SDL_DestroyAudioStream(audio_stream);
]=] "save preferences on shutdown")

string(REPLACE "[--track N] [--no-loop]" "[--track N] [--loop|--no-loop]" player "${player}")

# Generate PCM from SDL's audio-demand callback instead of the video/UI loop.
# This prevents window rendering, text cache misses, file writes, or scheduler
# stalls on the main thread from starving the audio device.
replace_required([=[// Keep a healthy amount of decoded PCM queued ahead of the device. The first
// version generated audio directly inside SDL's realtime callback and also
// allocated a std::vector there. On some Windows systems that could miss a
// deadline and sound like a too-small audio buffer. Manual queueing keeps all
// OPL work and allocations on the main thread instead.
constexpr int kAudioChunkFrames = 1024;
constexpr int kAudioQueueTargetFrames = 8192; // About 165 ms at 49,716 Hz.
]=] [=[// SDL asks for PCM from its audio thread as the device consumes it. Use a
// fixed-size stack buffer in the callback so the UI thread cannot starve audio
// and the callback never needs a dynamic allocation.
constexpr int kAudioChunkFrames = 1024;
constexpr int kTransitionRampFrames = std::max(1, kOplSampleRate * 5 / 1000);
]=] "audio callback constants")

# Smooth discontinuities at loop, restart, and track-change boundaries. Five
# milliseconds is short enough to be inaudible as a fade but long enough to
# remove the single-sample jump that causes a click.
replace_required([=[                sample = std::max<int>(std::numeric_limits<std::int16_t>::min(),
                                       std::min<int>(std::numeric_limits<std::int16_t>::max(),
                                                     sample));
                destination[output_position + i] = static_cast<std::int16_t>(sample);
                local_peak = std::max(local_peak, std::abs(sample));
]=] [=[                sample = std::max<int>(std::numeric_limits<std::int16_t>::min(),
                                       std::min<int>(std::numeric_limits<std::int16_t>::max(),
                                                     sample));
                if (transition_frames_remaining_ > 0) {
                    const int elapsed =
                        kTransitionRampFrames - transition_frames_remaining_;
                    const std::int64_t blended =
                        static_cast<std::int64_t>(transition_start_sample_) *
                            transition_frames_remaining_ +
                        static_cast<std::int64_t>(sample) * elapsed;
                    sample = static_cast<int>(blended / kTransitionRampFrames);
                    --transition_frames_remaining_;
                }
                destination[output_position + i] = static_cast<std::int16_t>(sample);
                last_output_sample_ = sample;
                local_peak = std::max(local_peak, std::abs(sample));
]=] "transition ramp while rendering")

replace_required([=[        peak_ = std::max(peak_, local_peak);
    }
]=] [=[        if (ended_ && output_position < frame_count && last_output_sample_ != 0) {
            const int fade_frames =
                std::min(kTransitionRampFrames, frame_count - output_position);
            for (int i = 0; i < fade_frames; ++i) {
                const int sample = static_cast<int>(
                    static_cast<std::int64_t>(last_output_sample_) *
                    (fade_frames - i - 1) / fade_frames);
                destination[output_position + i] = static_cast<std::int16_t>(sample);
                local_peak = std::max(local_peak, std::abs(sample));
            }
            last_output_sample_ = 0;
        }

        peak_ = std::max(peak_, local_peak);
    }
]=] "natural-end fade out")

replace_required([=[    void rewind_locked() {
        if (!player_ || track_position_ < 0 ||
            track_position_ >= static_cast<int>(tracks_.size())) {
            return;
        }
        player_->rewind(tracks_[track_position_].id);
]=] [=[    void rewind_locked() {
        if (!player_ || track_position_ < 0 ||
            track_position_ >= static_cast<int>(tracks_.size())) {
            return;
        }
        transition_start_sample_ = last_output_sample_;
        transition_frames_remaining_ = kTransitionRampFrames;
        player_->rewind(tracks_[track_position_].id);
]=] "begin ramp at playback discontinuities")

replace_required([=[        peak_ = 0;
        ended_ = false;
        path_.clear();
]=] [=[        peak_ = 0;
        last_output_sample_ = 0;
        transition_start_sample_ = 0;
        transition_frames_remaining_ = 0;
        ended_ = false;
        path_.clear();
]=] "reset transition state with decoder")

replace_required([=[    int peak_ = 0;
    int volume_ = 100;
]=] [=[    int peak_ = 0;
    int last_output_sample_ = 0;
    int transition_start_sample_ = 0;
    int transition_frames_remaining_ = 0;
    int volume_ = 100;
]=] "transition state members")

replace_required([=[bool pump_audio(SDL_AudioStream *stream, RixEngine &engine) {
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
]=] [=[void SDLCALL audio_stream_callback(void *userdata,
                                     SDL_AudioStream *stream,
                                     int additional_amount,
                                     int total_amount) {
    (void)total_amount;
    if (!userdata || !stream || additional_amount <= 0) {
        return;
    }

    auto &engine = *static_cast<RixEngine *>(userdata);
    constexpr int bytes_per_frame = static_cast<int>(sizeof(std::int16_t));
    int remaining_frames =
        std::max(1, (additional_amount + bytes_per_frame - 1) / bytes_per_frame);
    std::array<std::int16_t, kAudioChunkFrames> buffer{};

    while (remaining_frames > 0) {
        const int frames = std::min(kAudioChunkFrames, remaining_frames);
        engine.render(buffer.data(), frames);
        if (!SDL_PutAudioStreamData(
                stream, buffer.data(), frames * bytes_per_frame)) {
            return;
        }
        remaining_frames -= frames;
    }
}
]=] "on-demand SDL audio callback")

replace_required([=[    // Ask SDL for a moderately large device buffer, then feed the stream from
    // the main loop instead of generating audio in a realtime callback. SDL
    // may adjust or ignore this hint depending on the backend.
]=] [=[    // Ask SDL for a moderately large device buffer. PCM is supplied by the
    // stream callback, independently of the window/event loop.
]=] "audio setup comment")

replace_required([=[    SDL_AudioStream *audio_stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
        &audio_spec,
        nullptr,
        nullptr);
]=] [=[    SDL_AudioStream *audio_stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
        &audio_spec,
        audio_stream_callback,
        &engine);
]=] "install the audio stream callback")

replace_required([=[    const PlayerSnapshot initial_state = engine.snapshot();
    if (initial_state.loaded && !initial_state.paused && !initial_state.ended &&
        !pump_audio(audio_stream, engine)) {
        std::fprintf(stderr, "Could not prefill audio stream: %s\n", SDL_GetError());
    }

]=] "" "remove main-thread audio prefill")

replace_required([=[    auto reset_queued_audio = [&]() {
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
]=] [=[    auto reset_queued_audio = [&]() {
        if (!SDL_ClearAudioStream(audio_stream)) {
            std::fprintf(stderr, "Could not clear audio stream: %s\n", SDL_GetError());
        }
        const PlayerSnapshot state = engine.snapshot();
        if (state.paused) {
            SDL_PauseAudioStreamDevice(audio_stream);
        } else {
            SDL_ResumeAudioStreamDevice(audio_stream);
        }
    };
]=] "callback-aware stream reset")

replace_required([=[                } else {
                    if (!pump_audio(audio_stream, engine)) {
                        std::fprintf(stderr, "Could not refill audio stream: %s\n",
                                     SDL_GetError());
                    }
                    SDL_ResumeAudioStreamDevice(audio_stream);
                }
]=] [=[                } else {
                    SDL_ResumeAudioStreamDevice(audio_stream);
                }
]=] "resume callback-driven playback")

replace_required([=[        PlayerSnapshot state = engine.snapshot();
        if (state.loaded && !state.paused && !state.ended) {
            if (!pump_audio(audio_stream, engine)) {
                std::fprintf(stderr, "Could not queue audio data: %s\n", SDL_GetError());
            }
            state = engine.snapshot();
        }

]=] [=[        PlayerSnapshot state = engine.snapshot();

]=] "remove main-loop audio pumping")

string(REPLACE
    "Buffered SDL3 queue + SDLPAL AdPlug DBFLT OPL2 @ 49716 Hz"
    "SDL3 callback + SDLPAL AdPlug DBFLT OPL2 @ 49716 Hz"
    player "${player}")

file(WRITE "${OUTPUT}" "${player}")
