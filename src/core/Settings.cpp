#include "Settings.h"

#include <SDL.h>

#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>

namespace {

std::string settingsPath() {
    char* base = SDL_GetPrefPath("SuperMarioClone", "SuperMarioClone");
    std::string path = base ? std::string(base) + "settings.json" : "settings.json";
    SDL_free(base);
    return path;
}

}  // namespace

bool Settings::load() {
    std::ifstream file(settingsPath());
    if (!file.is_open()) return false;
    nlohmann::json json;
    try {
        file >> json;
    } catch (const std::exception&) {
        return false;  // archivo roto: se queda con los valores de fabrica
    }
    fullscreen = json.value("fullscreen", fullscreen);
    windowScale = std::clamp(json.value("window_scale", windowScale), 0, 6);
    widescreen = json.value("widescreen", widescreen);
    showFps = json.value("show_fps", showFps);
    masterVolume = std::clamp(json.value("master_volume", masterVolume), 0, 10);
    musicVolume = std::clamp(json.value("music_volume", musicVolume), 0, 10);
    sfxVolume = std::clamp(json.value("sfx_volume", sfxVolume), 0, 10);
    english = json.value("language", std::string("es")) == "en";
    vibration = json.value("vibration", vibration);
    playerCollision = json.value("player_collision", playerCollision);
    topScore = json.value("top_score", topScore);
    gameCompleted = json.value("game_completed", gameCompleted);
    if (json.contains("save") && json["save"].is_object()) {
        const auto& save = json["save"];
        saveWorld = std::clamp(save.value("world", 0), 0, 8);
        saveMode = save.value("mode", std::string("solo"));
        saveLuigi = save.value("luigi", false);
        saveSecondQuest = save.value("second_quest", false);
    }
    if (json.contains("keys")) {
        try {
            // Tolerante con el largo: las versiones viejas guardaban 5
            // acciones (sin "arriba"); lo que falte queda en -1 (la de fabrica).
            const auto saved = json["keys"].get<std::vector<std::vector<int>>>();
            for (size_t p = 0; p < keys.size() && p < saved.size(); ++p) {
                for (size_t a = 0; a < keys[p].size() && a < saved[p].size(); ++a) keys[p][a] = saved[p][a];
            }
        } catch (const std::exception&) {
        }
    }
    padJumpButton = json.value("pad_jump_button", padJumpButton);
    padRunButton = json.value("pad_run_button", padRunButton);
    return true;
}

bool Settings::save() const {
    nlohmann::json json{
        {"fullscreen", fullscreen},
        {"window_scale", windowScale},
        {"widescreen", widescreen},
        {"show_fps", showFps},
        {"master_volume", masterVolume},
        {"music_volume", musicVolume},
        {"sfx_volume", sfxVolume},
        {"language", english ? "en" : "es"},
        {"vibration", vibration},
        {"player_collision", playerCollision},
        {"top_score", topScore},
        {"game_completed", gameCompleted},
        {"save", {{"world", saveWorld}, {"mode", saveMode}, {"luigi", saveLuigi}, {"second_quest", saveSecondQuest}}},
        {"keys", keys},
        {"pad_jump_button", padJumpButton},
        {"pad_run_button", padRunButton},
    };
    std::ofstream file(settingsPath());
    if (!file.is_open()) return false;
    file << json.dump(2) << '\n';
    return true;
}
