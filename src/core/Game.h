#pragma once

#include <SDL.h>

#include <array>
#include <string>

#include "../entities/BuzzyBeetle.h"
#include "../entities/Coin.h"
#include "../entities/FireFlower.h"
#include "../entities/Fireball.h"
#include "../entities/Goomba.h"
#include "../entities/Koopa.h"
#include "../entities/Mario.h"
#include "../entities/Mushroom.h"
#include "../entities/PiranhaPlant.h"
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
    // Buzzy Beetle: misma logica que checkKoopaCollision (incluida la regla
    // estricta del caparazon en movimiento). El caso de fuego es aparte
    // (ver checkFireballBuzzyBeetleCollision): es inmune, no hay pisoton por
    // fuego.
    void checkBuzzyBeetleCollision(Mario& character, BuzzyBeetle& buzzy, float prevY);
    // Es inmune al fuego (ver BuzzyBeetle.h): la bola explota igual al
    // tocarlo (mismo feedback visual que contra cualquier otro enemigo),
    // pero no le hace nada -- sigue en el estado en el que estaba.
    void checkFireballBuzzyBeetleCollision(Fireball& fireball, BuzzyBeetle& buzzy);
    // Un caparazon en movimiento (de cualquiera de los dos) mata al otro;
    // si los dos lo son, mueren ambos -- mismo criterio que checkShellVsKoopa.
    void checkShellVsBuzzyBeetle(Koopa& koopa, BuzzyBeetle& buzzy);
    // El caparazon del Buzzy Beetle contra el Goomba de prueba (Goomba no
    // tiene caparazon propio para reciprocar, igual que en checkShellVsGoomba).
    void checkBuzzyShellVsGoomba(BuzzyBeetle& buzzy);
    void checkGoombaBuzzyBeetleBounce(Goomba& goomba, BuzzyBeetle& buzzy);
    void checkKoopaBuzzyBeetleBounce(Koopa& koopa, BuzzyBeetle& buzzy);
    // Sin pisoton (ver PiranhaPlant.h): cualquier contacto mientras esta
    // asomada hace dano, salvo estrella que la mata.
    void checkPiranhaPlantCollision(Mario& character, PiranhaPlant& plant);
    // Fuego si la mata (a diferencia del Buzzy Beetle): desaparece de una,
    // sin parabola (no tiene adonde volar), y la bola explota ahi.
    void checkFireballPiranhaPlantCollision(Fireball& fireball, PiranhaPlant& plant);
    // No se asoma si algun personaje esta cerca del cano -- esta clase no
    // conoce a Mario/Luigi, asi que Game.cpp resuelve la distancia cada
    // frame y le pasa el resultado a PiranhaPlant::update().
    bool isSafeForPiranhaPlant(const PiranhaPlant& plant) const;
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

    // Koopa Troopa verde de prueba (seccion 5.2), dos instancias en el piso
    // para poder probar que un caparazon en movimiento mate a otro
    // (checkShellVsKoopa), mas un "Koopa Alado" saltarin (KoopaState::Jumping)
    // para probar que un pisoton lo deje caminando derecho. En el juego
    // original de 1985 el color fija cual de los dos Alado le toca: el verde
    // va a los saltos, el rojo se hamaca en un punto fijo (ver koopaRedFlying_
    // mas abajo) -- no existe un verde volador ni un rojo saltarin.
    // Mismo criterio que el Goomba: solo en el escenario de enemigos, tecla
    // K los repone si mueren o se caen.
    SpriteSheet koopaSprite_;
    Koopa koopaA_;
    Koopa koopaB_;
    Koopa koopaJumping_;
    // Koopa rojo de prueba (ver KoopaColor): sprite propio (una sola
    // paleta, no una fila por WorldType como el verde) y setColor(Red) se
    // llama una sola vez en init(). Arranca sobre una plataforma angosta
    // agregada a test_enemies.json para poder ver que da la vuelta en el
    // borde en vez de caerse. Mas su Koopa Alado (volador, ver el comentario
    // de koopaJumping_ arriba), comparte koopaRedSprite_.
    SpriteSheet koopaRedSprite_;
    Koopa koopaRed_;
    Koopa koopaRedFlying_;

    // Buzzy Beetle de prueba (seccion 5.2, tercer enemigo): fisica y
    // caparazon identicos al Koopa verde caminando (se cae de los bordes,
    // no hay variante roja ni Alado para esta especie), salvo que es inmune
    // al fuego (ver BuzzyBeetle.h). Clase aparte en vez de otra combinacion
    // de KoopaColor/KoopaState porque esas dos diferencias (sin Alado, sin
    // rojo) no encajan como una opcion mas de Koopa. Tecla B lo repone.
    SpriteSheet buzzyBeetleSprite_;
    BuzzyBeetle buzzyBeetle_;

    // Pirana Plant de prueba (seccion 5.2, cuarto enemigo): sale de un tile
    // solido angosto en test_enemies.json (no existe el objeto "tuberia"
    // todavia, seccion 5.3) y cicla sola entre escondida y asomada -- no se
    // asoma si algun personaje esta cerca (ver isSafeForPiranhaPlant). Sin
    // caparazon ni pisoton: cualquier toque mientras esta asomada hace dano,
    // solo estrella o fuego la matan (sin parabola, ver PiranhaPlant::kill).
    // Tecla P la repone.
    SpriteSheet piranhaPlantSprite_;
    PiranhaPlant piranhaPlant_;

    // Pool de popups de puntaje (hongo/flor/estrella dan 1000, como en el
    // original; la moneda no muestra popup, solo suma al contador cuando
    // exista). Varios a la vez posibles si los dos jugadores agarran cosas
    // casi juntos.
    std::array<ScorePopup, 4> scorePopups_;
};
