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
    playerWorldPalettes = json.value("player_world_palettes", playerWorldPalettes);
    try {
        unlockedExtras = json.value("unlocked_extras", std::vector<std::string>{});
        enabledExtras = json.value("enabled_extras", std::vector<std::string>{});
        purchasedExtras = json.value("purchased_extras", std::vector<std::string>{});
        coinBank = json.value("coin_bank", 0);
        flawlessLevels = json.value("flawless_levels", std::vector<std::string>{});
    } catch (const std::exception&) {
    }
    const auto readSlot = [](const nlohmann::json& entry) {
        SaveSlot slot;
        if (!entry.is_object()) return slot;
        slot.world = std::clamp(entry.value("world", 0), 0, 8);
        slot.level = std::clamp(entry.value("level", 1), 1, 4);
        slot.mode = entry.value("mode", std::string("solo"));
        slot.luigi = entry.value("luigi", false);
        slot.secondQuest = entry.value("second_quest", false);
        slot.noDamage = entry.value("no_damage", false);
        slot.noSkip = entry.value("no_skip", false);
        slot.noStar = entry.value("no_star", false);
        slot.noMushroom = entry.value("no_mushroom", false);
        slot.noFlower = entry.value("no_flower", false);
        return slot;
    };
    if (json.contains("saves") && json["saves"].is_array()) {
        for (size_t i = 0; i < saves.size() && i < json["saves"].size(); ++i) saves[i] = readSlot(json["saves"][i]);
    } else if (json.contains("save")) {
        saves[0] = readSlot(json["save"]);   // la version de un solo espacio
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

bool Settings::extraUnlocked(const std::string& id) const {
    return std::find(unlockedExtras.begin(), unlockedExtras.end(), id) != unlockedExtras.end();
}

bool Settings::extraPurchased(const std::string& id) const {
    return std::find(purchasedExtras.begin(), purchasedExtras.end(), id) != purchasedExtras.end();
}

bool Settings::extraEnabled(const std::string& id) const {
    return std::find(enabledExtras.begin(), enabledExtras.end(), id) != enabledExtras.end();
}

nlohmann::json Settings::savesJson() const {
    nlohmann::json list = nlohmann::json::array();
    for (const SaveSlot& slot : saves) {
        list.push_back({{"world", slot.world}, {"level", slot.level}, {"mode", slot.mode}, {"luigi", slot.luigi},
                        {"second_quest", slot.secondQuest}, {"no_damage", slot.noDamage},
                        {"no_skip", slot.noSkip}, {"no_star", slot.noStar},
                        {"no_mushroom", slot.noMushroom}, {"no_flower", slot.noFlower}});
    }
    return list;
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
        {"player_world_palettes", playerWorldPalettes},
        {"unlocked_extras", unlockedExtras},
        {"enabled_extras", enabledExtras},
        {"purchased_extras", purchasedExtras},
        {"coin_bank", coinBank},
        {"flawless_levels", flawlessLevels},
        {"saves", savesJson()},
        {"keys", keys},
        {"pad_jump_button", padJumpButton},
        {"pad_run_button", padRunButton},
    };
    std::ofstream file(settingsPath());
    if (!file.is_open()) return false;
    file << json.dump(2) << '\n';
    return true;
}
