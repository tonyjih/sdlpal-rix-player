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

file(WRITE "${OUTPUT}" "${player}")
