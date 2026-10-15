#pragma once

#include <array>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

// Configuracion del jugador (seccion 10.2 de la especificacion), guardada
// en settings.json dentro de la carpeta de datos del usuario
// (SDL_GetPrefPath, en Windows %APPDATA%\SuperMarioClone\SuperMarioClone):
// sobrevive a recompilar y a borrar build/. La aplica Game::applySettings.
struct Settings {
    bool fullscreen = false;
    int windowScale = 0;      // 0 = automatico (lo mas grande que entre), 1..6 = multiplo fijo
    bool widescreen = true;   // 16:9 (true) o 4:3 (false)
    bool showFps = false;
    int masterVolume = 10;    // 0..10
    int musicVolume = 10;     // 0..10 (sin musica todavia, se guarda igual)
    int sfxVolume = 10;       // 0..10
    bool english = false;     // idioma: espanol (false) o ingles (true)
    bool vibration = true;
    bool playerCollision = true;  // Mario y Luigi chocan entre si (2P a la vez)
    // Mejor puntaje (el "TOP-" de la pantalla de titulo).
    int topScore = 0;
    // Se termino el juego (el 8-4) al menos una vez: desbloquea EXTRAS.
    bool gameCompleted = false;
    // Paleta de Mario/Luigi segun el tipo de mundo: apagada por ahora.
    bool playerWorldPalettes = false;
    // CONTENIDO EXTRA: logros cumplidos (desbloquean su elemento) y los
    // elementos desbloqueados que estan activados (por id, ver kExtraContent
    // en GameMenus.cpp).
    std::vector<std::string> unlockedExtras;
    // Progreso de los logros (acumulado entre partidas): los niveles de la
    // campana pasados sin recibir dano ("1-1").
    std::vector<std::string> flawlessLevels;
    std::vector<std::string> enabledExtras;
    bool extraUnlocked(const std::string& id) const;
    bool extraEnabled(const std::string& id) const;
    // Partidas guardadas, hasta 3 espacios: se guarda al pasar cada nivel.
    // world-level: desde donde sigue CONTINUAR (world 0 = espacio vacio).
    // mode: "solo", "coop" (2 a la vez) o "turns" (2 por turnos).
    struct SaveSlot {
        int world = 0;
        int level = 1;
        std::string mode = "solo";
        bool luigi = false;
        bool secondQuest = false;
        bool noDamage = false;   // desde el 1-1 sin recibir dano (logro)
        bool noSkip = false;     // desde el 1-1 sin zonas de warp (logro)
        bool noStar = false;     // desde el 1-1 sin agarrar estrellas (logro)
        bool noMushroom = false; // ... sin super champinones (logro)
        bool noFlower = false;   // ... sin flores de fuego (logro)
    };
    static constexpr int kSaveSlots = 3;
    std::array<SaveSlot, kSaveSlots> saves{};

    // Teclas reasignadas por jugador y accion (orden de Action en Input.h);
    // -1 = la de fabrica. Botones del mando para saltar y correr (los dos
    // mandos usan los mismos); -1 = A y X (formato Xbox).
    std::array<std::array<int, 6>, 2> keys{{{-1, -1, -1, -1, -1, -1}, {-1, -1, -1, -1, -1, -1}}};
    int padJumpButton = -1;
    int padRunButton = -1;

    bool load();
    bool save() const;
    nlohmann::json savesJson() const;
};
