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
    // Niveles de la campana ya pasados ("1-2"): los que se pueden elegir en
    // ELEGIR MUNDO.
    std::vector<std::string> clearedLevels;
    // Vidas ocultas (1-UP de los bloques) ya encontradas ("1-1:64,9"), logro
    // VIDAS INFINITAS.
    std::vector<std::string> foundOneUps;
    std::vector<std::string> enabledExtras;
    // Tienda: lo desbloqueado hay que comprarlo con las monedas juntadas en
    // las partidas (coinBank, la "alcancia").
    std::vector<std::string> purchasedExtras;
    int coinBank = 0;
    static constexpr int kMaxCoinBank = 999999;   // tope de la alcancia
    // Tabla de puntajes (al terminar una partida): los mejores, de mayor a menor.
    struct HighScore {
        std::string name;
        int score = 0;
        bool plus = false;   // PARTIDA +
    };
    static constexpr int kMaxHighScores = 10;
    std::vector<HighScore> highScores;
    // Progreso de los logros de las flores: Hammer Bros eliminados
    // pisandolos en las partidas (y los de PARTIDA +).
    int hammerBroStomps = 0;
    int hammerBroStompsPlus = 0;
    // Monedas juntadas en las partidas en total (no baja al comprar): ROCOLA.
    int totalCoins = 0;
    bool extraUnlocked(const std::string& id) const;
    bool extraEnabled(const std::string& id) const;
    bool extraPurchased(const std::string& id) const;
    // Desbloqueado y comprado: ya se puede usar.
    bool extraOwned(const std::string& id) const { return extraUnlocked(id) && extraPurchased(id); }
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
        bool allHammerBros = false; // ... eliminando a todos los Hammer Bros (logro)
        float playTime = 0.0f;      // segundos jugados de la partida (sin pausas, logro)
        // Para el resumen del final: enemigos eliminados, muertes y puntaje
        // de cada jugador (Mario / Luigi; en 1 jugador, el primero).
        int enemies = 0;
        int deathsMario = 0;
        int deathsLuigi = 0;
        int scoreMario = 0;
        int scoreLuigi = 0;
        bool onlyLuigi = false;     // desde el 1-1 con Luigi, 1 jugador (logro)
    };
    static constexpr int kSaveSlots = 5;
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
    nlohmann::json highScoresJson() const;
};
