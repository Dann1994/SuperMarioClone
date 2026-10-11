#pragma once

#include <array>
#include <string>

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

    // Teclas reasignadas por jugador y accion (orden de Action en Input.h);
    // -1 = la de fabrica. Botones del mando para saltar y correr (los dos
    // mandos usan los mismos); -1 = A y X (formato Xbox).
    std::array<std::array<int, 5>, 2> keys{{{-1, -1, -1, -1, -1}, {-1, -1, -1, -1, -1}}};
    int padJumpButton = -1;
    int padRunButton = -1;

    bool load();
    bool save() const;
};
