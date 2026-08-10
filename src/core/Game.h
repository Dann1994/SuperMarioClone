#pragma once

#include <SDL.h>

#include <array>
#include <string>

#include "../entities/Coin.h"
#include "../entities/FireFlower.h"
#include "../entities/Fireball.h"
#include "../entities/Goomba.h"
#include "../entities/Koopa.h"
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

// Escenarios de prueba (para no amontonar items y enemigos en un solo nivel
// mientras no existe el editor de niveles real, seccion 7): se eligen desde
// una pantalla simple al arrancar (ver Game::renderScenarioSelect).
enum class TestScenario {
    Items,
    Enemies,
};

enum class AppState {
    ScenarioSelect,
    Playing,
};

class Game {
public:
    bool init();
    void run();
    void shutdown();

private:
    void processEvents();
    void update(float dt);
    void render();
    void renderScenarioSelect();
    // Carga el nivel del escenario elegido y deja en pie solo las entidades
    // de prueba que le corresponden (las del otro escenario quedan
    // desactivadas, aunque el objeto siga vivo en memoria).
    void startScenario(TestScenario scenario);
    void trySpawnFireball(Mario& shooter, std::array<Fireball, kMaxFireballsPerPlayer>& pool);
    void resolveCharacterCollision();
    void checkMushroomPickup(Mario& character);
    void checkFireFlowerPickup(Mario& character);
    void checkStarPickup(Mario& character);
    void checkCoinPickup(Mario& character);
    // Pisoton (achata y hace rebotar) o dano de costado (como el hazard de
    // prueba), segun de donde venga el contacto.
    // prevY: character.y() de ANTES de moverse este frame (ver el comentario
    // en Game::update junto a marioPrevY) -- asi se sabe si venia de arriba
    // sin importar cuanto se haya hundido este frame en particular.
    void checkGoombaCollision(Mario& character, float prevY);
    // Fuego: misma muerte que la estrella (se da vuelta y cae, ver
    // Goomba::killFalling), y la bola explota ahi en vez de seguir volando.
    void checkFireballGoombaCollision(Fireball& fireball);
    // Segun el estado del Koopa: Walking -> pisoton lo esconde (con rebote)
    // o dano de costado; ShellIdle -> pisoton o costado lo patean por igual
    // (de costado hacia el lado contrario a quien lo toco, de arriba hacia
    // donde mira el personaje), sin rebote (para no matar a Mario/Luigi por
    // seguir cayendo sobre el caparazon ya pateado, ver la nota en el .cpp);
    // ShellMoving -> pisoton lo frena (con rebote), costado hace dano.
    // Estrella lo mata (killFalling) en cualquier estado, como al Goomba.
    void checkKoopaCollision(Mario& character, Koopa& koopa, float prevY);
    // Fuego: mata al Koopa en cualquier estado (igual que la estrella), y la
    // bola explota ahi en vez de seguir volando.
    void checkFireballKoopaCollision(Fireball& fireball, Koopa& koopa);
    // Un caparazon en movimiento mata a cualquier otro enemigo que toque y
    // sigue de largo -- este chequeo es especificamente contra el Goomba de
    // prueba (checkShellVsKoopa mas abajo cubre Koopa contra Koopa).
    void checkShellVsGoomba(Koopa& koopa);
    // Si los dos son caparazon en movimiento, se matan entre si; si solo uno
    // lo es, mata al otro (en cualquier estado) y sigue de largo.
    void checkShellVsKoopa(Koopa& a, Koopa& b);
    // Si NINGUNO es caparazon en movimiento (los dos caminando, o uno/ambos
    // caparazon quieto), no se matan: se cruzan como si chocaran una pared,
    // cada uno invierte su propia direccion. Mismo criterio para agregar
    // cuando haya mas de un Goomba, o mas tipos de enemigo.
    void checkGoombaKoopaBounce(Goomba& goomba, Koopa& koopa);
    void checkKoopaKoopaBounce(Koopa& a, Koopa& b);
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
    AppState appState_ = AppState::ScenarioSelect;
    TestScenario scenario_ = TestScenario::Items;
    WorldType worldType_ = WorldType::Overworld;
    bool characterCollisionEnabled_ = true;
    // Modo 1 jugador (solo Mario) vs 2 jugadores simultaneos (ver seccion
    // "Ideas anotadas" del README): cambia como se maneja la muerte -- en 1P
    // Mario muriendo congela y vuelve al checkpoint (como el juego clasico,
    // es "el unico jugador"); en 2P, mientras uno siga vivo el otro
    // reaparece en la plataforma mas cercana sin pausar nada.
    bool twoPlayerMode_ = false;

    Input input1_{ControlProfile::Player1};
    Input input2_{ControlProfile::Player2};
    Tilemap level_;
    Mario mario_;
    Mario luigi_;

    SpriteSheet fireballSprite_;
    SpriteSheet fireballExplosionSprite_;
    std::array<Fireball, kMaxFireballsPerPlayer> marioFireballs_;
    std::array<Fireball, kMaxFireballsPerPlayer> luigiFireballs_;

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

    // Goomba de prueba (seccion 5.2, primer enemigo), solo en el escenario
    // de enemigos: ese nivel tiene paredes en los dos bordes (ver
    // test_enemies.json) para que el rebote contra pared se pueda ver sin
    // que se termine cayendo del nivel por un borde sin pared. Tecla G lo
    // repone si se pisa o se cae.
    SpriteSheet goombaSprite_;
    Goomba goomba_;

    // Koopa Troopa verde de prueba (seccion 5.2), dos instancias para poder
    // probar que un caparazon en movimiento mate a otro (checkShellVsKoopa).
    // Mismo criterio que el Goomba: solo en el escenario de enemigos, tecla
    // K los repone si mueren o se caen.
    SpriteSheet koopaSprite_;
    Koopa koopaA_;
    Koopa koopaB_;

    // Pool de popups de puntaje (hongo/flor/estrella dan 1000, como en el
    // original; la moneda no muestra popup, solo suma al contador cuando
    // exista). Varios a la vez posibles si los dos jugadores agarran cosas
    // casi juntos.
    std::array<ScorePopup, 4> scorePopups_;
};
