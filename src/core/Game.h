#pragma once

#include <SDL.h>

#include <array>
#include <string>

#include "../entities/Coin.h"
#include "../entities/FireFlower.h"
#include "../entities/Fireball.h"
#include "../entities/Mario.h"
#include "../entities/Mushroom.h"
#include "../entities/ScorePopup.h"
#include "../entities/StarItem.h"
#include "../render/SpriteSheet.h"
#include "../world/Tilemap.h"
#include "../world/WorldType.h"
#include "Input.h"

// Resolucion interna estilo NES; la ventana puede ser mas grande, SDL2
// escala automaticamente (ver seccion 3 de la especificacion: resolucion
// ajustable, ventana/pantalla completa, este valor logico se mantiene fijo
// aunque la ventana cambie de tamano).
constexpr int kLogicalWidth = 256;
constexpr int kLogicalHeight = 240;

// Maximo de bolas de fuego en pantalla por personaje, como en el original.
constexpr int kMaxFireballsPerPlayer = 2;

class Game {
public:
    bool init();
    void run();
    void shutdown();

private:
    void processEvents();
    void update(float dt);
    void render();
    void trySpawnFireball(Mario& shooter, std::array<Fireball, kMaxFireballsPerPlayer>& pool);
    void resolveCharacterCollision();
    void checkDamageHazard(Mario& character);
    void checkMushroomPickup(Mario& character);
    void checkFireFlowerPickup(Mario& character);
    void checkStarPickup(Mario& character);
    void checkCoinPickup(Mario& character);
    // Popup de puntaje flotante (seccion 6): busca un slot libre del pool.
    void spawnScorePopup(float x, float y, const std::string& text);
    // Punto de reaparicion en la plataforma solida mas cercana a aroundX,
    // buscando hacia los costados (para no reaparecer sobre un pozo).
    SDL_Point findRespawnPoint(float aroundX, int characterHeight) const;
    // Muerte de uno solo (el otro sigue jugando): reaparece en la
    // plataforma mas cercana, parpadeando invulnerable un rato.
    void respawnNearestPlatform(Mario& character);
    // Murieron los dos a la vez: placeholder de "reiniciar desde el
    // checkpoint" hasta que ese sistema exista -- por ahora vuelve al
    // spawn original del nivel.
    void respawnAtCheckpoint(Mario& character, float spawnX, float spawnY);

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    bool running_ = false;
    bool showDebugOverlay_ = true;
    int currentFps_ = 0;
    WorldType worldType_ = WorldType::Overworld;
    bool characterCollisionEnabled_ = true;
    // Modo 1 jugador (solo Mario) vs 2 jugadores simultaneos (ver seccion
    // "Ideas anotadas" del README): cambia como se maneja la muerte -- en 1P
    // Mario muriendo congela y vuelve al checkpoint (como el juego clasico,
    // es "el unico jugador"); en 2P, mientras uno siga vivo el otro
    // reaparece en la plataforma mas cercana sin pausar nada.
    bool twoPlayerMode_ = true;

    Input input1_{ControlProfile::Player1};
    Input input2_{ControlProfile::Player2};
    Tilemap level_;
    Mario mario_;
    Mario luigi_;

    SpriteSheet fireballSprite_;
    SpriteSheet fireballExplosionSprite_;
    std::array<Fireball, kMaxFireballsPerPlayer> marioFireballs_;
    std::array<Fireball, kMaxFireballsPerPlayer> luigiFireballs_;

    // Placeholder de peligro (seccion 12: se prueba con algo simple antes de
    // tener enemigos reales) para poder probar takeDamage() jugando, no solo
    // por tecla de debug. Un rectangulo fijo en el nivel de prueba.
    SDL_Rect damageHazard_{200, 208, 16, 16};

    // Hongo de prueba: todavia no hay bloques de interrogacion que lo
    // hagan salir, asi que arranca ya puesto en el nivel (F11 lo repone).
    SpriteSheet mushroomSprite_;
    Mushroom mushroom_;

    // Flor de fuego de prueba, mismo criterio que el hongo (F12 la repone).
    SpriteSheet fireFlowerSprite_;
    FireFlower fireFlower_;

    // Estrella de prueba, mismo criterio que el hongo (F4 la repone).
    SpriteSheet starSprite_;
    StarItem star_;

    // Moneda de prueba (tecla C la repone).
    SpriteSheet coinSprite_;
    Coin coin_;

    // Pool de popups de puntaje (hongo/flor/estrella dan 1000, como en el
    // original; la moneda no muestra popup, solo suma al contador cuando
    // exista). Varios a la vez posibles si los dos jugadores agarran cosas
    // casi juntos.
    std::array<ScorePopup, 4> scorePopups_;
};
