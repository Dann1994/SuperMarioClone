#pragma once

#include <array>
#include <string>

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
    // Partidas guardadas (seccion 6 de la especificacion: se guarda al
    // completar un mundo entero), hasta 3 espacios. world: el mundo desde
    // el que sigue CONTINUAR (0 = espacio vacio). mode: "solo", "coop" (2 a
    // la vez) o "turns" (2 por turnos).
    struct SaveSlot {
        int world = 0;
        std::string mode = "solo";
        bool luigi = false;
        bool secondQuest = false;
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
