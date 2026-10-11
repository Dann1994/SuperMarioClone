#pragma once

#include <SDL.h>

#include <array>
#include <string>

#include "../audio/SoundManager.h"
#include "../entities/Axe.h"
#include "../entities/BlockCoin.h"
#include "../entities/Blooper.h"
#include "../entities/Bowser.h"
#include "../entities/BowserFire.h"
#include "../entities/BrickDebris.h"
#include "../entities/BulletBill.h"
#include "../entities/BuzzyBeetle.h"
#include "../entities/CheepCheep.h"
#include "../entities/Coin.h"
#include "../entities/FireFlower.h"
#include "../entities/Fireball.h"
#include "../entities/Flagpole.h"
#include "../entities/Goomba.h"
#include "../entities/Hammer.h"
#include "../entities/HammerBro.h"
#include "../entities/Koopa.h"
#include "../entities/Lakitu.h"
#include "../entities/Mario.h"
#include "../entities/Mushroom.h"
#include "../entities/PiranhaPlant.h"
#include "../entities/Podoboo.h"
#include "../entities/ScorePopup.h"
#include "../entities/Spiny.h"
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

// Maximo de martillos en pantalla a la vez (pool, mismo criterio que
// Fireball), compartido entre HammerBro (2 alcanzan de sobra, ver
// HammerBro::kThrowInterval) y la rafaga de Bowser (ver
// Bowser::kHammerBurstCount -- con intervalo corto entre cada uno,
// facilmente hay varios en pantalla a la vez antes de que los primeros
// salgan por el borde).
constexpr int kMaxHammers = 8;

// Huevos de Spiny en pantalla a la vez (pool, mismo criterio que Hammer):
// a diferencia del martillo, el Spiny no desaparece solo al eclosionar y
// puede quedar caminando bastante tiempo, asi que hace falta mas margen que
// los 2 del Hammer Bro para que el Lakitu de prueba no se quede sin tirar
// mientras los anteriores siguen vivos.
constexpr int kMaxSpinies = 4;

// Cheep Cheep Jumping en pantalla a la vez (pool, mismo criterio que
// Spiny/Hammer): con el intervalo entre apariciones (ver
// kCheepCheepJumpIntervalMin/Max en Game.cpp) alcanza para que se vean
// varias saltando mientras el personaje camina sin que el timer se quede
// sin lugar en el pool.
constexpr int kMaxCheepCheepJumps = 3;

// Fuegos de Bowser en pantalla a la vez (pool, mismo criterio que
// Hammer/hammers_): alcanza con 2 dado el intervalo entre ataques (ver
// Bowser::kAttackIntervalMin/Max).
constexpr int kMaxBowserFires = 2;

// Pedazos volando a la vez (pool): 4 por ladrillo roto (una esquina cada
// uno), con margen para dos roturas casi juntas sin robarse el pool.
constexpr int kMaxBrickDebris = 8;

// Sacudidas de ladrillo (chico golpeandolo sin romperlo) activas a la vez.
constexpr int kMaxBlockBumps = 2;
// Cuanto sube y cuanto dura la sacudida (sube y vuelve a bajar en este lapso).
constexpr float kBumpDuration = 0.22f;
constexpr float kBumpHeight = 6.0f;

// Una sacudida en curso: que tile y cuanto le queda (ver Game::bumpBrickBlock).
// hasPendingItem/pendingMovingRight: solo lo usa checkQuestionBlockHit (ver
// ese comentario) para que el item recien aparezca cuando termina de
// sacudirse el bloque, en vez de al toque -- bumpBrickBlock (ladrillo comun,
// sin item) nunca los toca, quedan en false.
struct BlockBump {
    bool active = false;
    int col = 0;
    int row = 0;
    float timer = 0.0f;
    bool hasPendingItem = false;
    bool pendingMovingRight = false;
};

// Que item le toca a cada bloque de interrogacion (seccion 5.3) en el
// escenario de Items: uno por item de la fase inicial (5.4), asignado por
// posicion de tile en vez de una entidad de bloque propia por instancia --
// el bloque en si (Tilemap::isQuestionTile/isQuestionUsed) no sabe nada de
// items, es Game::checkQuestionBlockHit quien busca aca cual le toca a la
// celda que golpearon.
enum class BlockItem {
    Mushroom,
    FireFlower,
    Star,
    Coin,
    OneUp,
};

struct QuestionBlockSpawn {
    int col;
    int row;
    BlockItem item;
    // Cuantas veces se puede golpear antes de quedar "usado" para siempre
    // (ver Tilemap::questionHitCount/dispenserHitCount): 1 por defecto
    // (todos los items excepto Coin), asi las entradas existentes no
    // necesitan tocarse. Un bloque "de N monedas" clasico (pedido del
    // usuario, investigado: en el original de 1985 en realidad es por
    // tiempo entre golpes, no por cantidad fija -- se simplifica aca a un
    // numero fijo de golpes, mas facil de parametrizar y de probar) pone
    // esto en N con item = Coin.
    int maxHits = 1;
};

// A que tuberia le corresponde cada entrada valida (Tilemap::isPipeTopTile)
// del escenario de Items, y a donde manda -- ver Game::checkPipeWarp.
struct PipeWarp {
    int col;      // columna izquierda del tope de la tuberia (donde hay que pararse)
    int row;      // fila del tope
    float destX;
    float destY;
};

// Cuanto tarda en "salir" del todo un item que emerge de un bloque (ver
// ItemEmerge mas abajo) -- Mushroom/FireFlower/Star no tienen animacion de
// emerger propia (ningun otro item/enemigo del elenco la tiene tampoco), asi
// que Game.cpp la simula moviendolo el mismo de golpeY_ inicial (a la altura
// del bloque, tapado por el Z-order) hasta su altura final, sin dejar que
// actualice su propia fisica mientras tanto (ver Game::update). BlockCoin es
// la excepcion: tiene su propio salto (ver BlockCoin.h), no pasa por aca.
constexpr float kItemEmergeDuration = 0.3f;

// Item emergiendo de un bloque en este momento (ver el comentario de
// kItemEmergeDuration) -- uno solo a la vez alcanza para este escenario de
// prueba (un personaje no puede golpear dos bloques en el mismo instante).
struct ItemEmerge {
    bool active = false;
    BlockItem item = BlockItem::Mushroom;
    float x = 0.0f;
    float startY = 0.0f;  // altura del bloque: ahi arranca, tapado por el Z-order
    float endY = 0.0f;    // altura final de descanso, arriba del bloque
    float timer = 0.0f;
    bool movingRight = true;
};

// Escenarios de prueba (para no amontonar items y enemigos en un solo nivel
// mientras no existe el editor de niveles real, seccion 7): se eligen desde
// una pantalla simple al arrancar (ver Game::renderScenarioSelect).
enum class TestScenario {
    Items,
    Enemies,
    HammerBro,
    LakituSpiny,
    Blooper,
    CheepCheep,
    BulletBill,
    // Escenario largo con scroll horizontal (a diferencia de todos los
    // demas, de una sola pantalla): el personaje camina normal (worldType_
    // Overworld) mientras Cheep Cheep Jumping van saltando desde abajo del
    // borde inferior de pantalla a medida que avanza (ver
    // cheepCheepJumps_/cheepCheepJumpSpawnTimer_).
    CheepCheepJump,
    Podoboo,
    Bowser,
    // Tramo final de un nivel (escalera + bandera de meta, como el 1-1),
    // test_flagpole.json. Sin tecla numerica: se elige con la flecha.
    Flagpole,
};

// Cantidad de opciones del selector de escenario (una por TestScenario).
constexpr int kMenuOptionCount = static_cast<int>(TestScenario::Flagpole) + 1;

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
    void moveMenuSelection(int delta);
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
    // Bajo el agua Mario nada libremente (no cae por gravedad como en
    // tierra), asi que la geometria del pisoton -- que necesita "venir
    // cayendo desde arriba" -- casi nunca se da nadando: por pedido
    // explicito del usuario, ahi se desactiva el pisoton del todo y
    // cualquier contacto hace dano como un toque de costado (en el resto de
    // los mundos el pisoton sigue igual que siempre). Devuelve true si ya
    // aplico el dano y el llamador tiene que cortar ahi (no seguir con la
    // logica de pisoton); false si no es Underwater y el llamador sigue
    // normal. Un solo lugar para este criterio en vez de repetirlo en cada
    // checkXCollision.
    bool handleUnderwaterContactDamage(Mario& character);
    // Dano de contacto "normal" (tocar a un enemigo/hazard de costado, no
    // pisandolo ni con estrella): chico muere, grande/fuego se achica -- con
    // su sonido correspondiente (Sfx::Death/Sfx::PowerDown), solo si
    // realmente arranco la secuencia (startDeath()/takeDamage() devuelven
    // false si ya estaba en una o ya era invulnerable). Un solo lugar para
    // este mismo if/else que se repetia en cada checkXCollision.
    void applyContactDamage(Mario& character);
    // Mata a un enemigo "volteandolo" (fuego, estrella, caparazon, o un
    // pisoton en los que no tienen animacion de achatado propia -- ver el
    // comentario de la clase de cada uno): reproduce Sfx::Kick una sola vez,
    // solo si killFalling() realmente arranco la secuencia esta vez (false
    // si ya estaba cayendo, evita que suene varias veces seguidas si algo lo
    // toca de nuevo mientras todavia esta en el aire). Investigado en el
    // disassembly original: es el mismo sonido/mecanismo ahi tambien
    // ("ShellOrBlockDefeat"), sin importar la causa. Template porque cada
    // clase de enemigo es un tipo distinto, todas comparten la firma
    // bool killFalling(bool).
    template <typename T>
    void killEnemyFalling(T& enemy, bool flyRight) {
        if (enemy.killFalling(flyRight)) audio_.play(Sfx::Kick);
    }
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
    // Cualquier forma de morir lo mata igual (ver HammerBro.h): pisoton,
    // estrella o fuego todos disparan la misma parabola de caida.
    void checkHammerBroCollision(Mario& character, float prevY);
    void checkFireballHammerBroCollision(Fireball& fireball);
    // El martillo en vuelo hace dano a cualquier personaje que toque (sin
    // pisoton, como la Pirana Plant); con estrella no lo mata -- no es un
    // enemigo, solo desaparece sin hacer dano (ver Hammer::reset).
    void checkHammerCollision(Mario& character, Hammer& hammer);
    // Cualquier forma de matarlo usa la misma parabola (ver Lakitu.h): a
    // diferencia de la Pirana Plant, aca el pisoton SI lo mata.
    void checkLakituCollision(Mario& character, float prevY);
    void checkFireballLakituCollision(Fireball& fireball);
    // Sin pisoton (ver Spiny.h): cualquier contacto hace dano, en Egg o en
    // Walking por igual, sin importar de que lado venga -- solo estrella o
    // fuego lo eliminan.
    void checkSpinyCollision(Mario& character, Spiny& spiny);
    void checkFireballSpinyCollision(Fireball& fireball, Spiny& spiny);
    // Si los dos estan caminando (no huevo en el aire, ver SpinyState) y se
    // cruzan, rebotan como si chocaran una pared -- mismo criterio que
    // checkGoombaKoopaBounce, no se matan entre si (Spiny no tiene
    // caparazon que pueda matar a otro enemigo).
    void checkSpinyBounce(Spiny& a, Spiny& b);
    // Sin pisoton (ver Blooper.h): cualquier contacto hace dano, solo
    // estrella o fuego lo matan.
    void checkBlooperCollision(Mario& character, Blooper& blooper);
    void checkFireballBlooperCollision(Fireball& fireball, Blooper& blooper);
    // Pisoton, estrella o fuego: cualquiera lo mata igual (ver CheepCheep.h),
    // en las dos variantes.
    void checkCheepCheepCollision(Mario& character, CheepCheep& cheepCheep, float prevY);
    void checkFireballCheepCheepCollision(Fireball& fireball, CheepCheep& cheepCheep);
    // Busca un slot libre en cheepCheepJumps_ y lo spawnea desde abajo del
    // borde de pantalla, cerca del personaje mas adelantado: adelante si
    // esta caminando, atras si esta corriendo (comportamiento real de
    // World 2-3/7-3/8-4 de SMB1, ver el comentario de CheepCheep.h) con una
    // distancia al azar (kCheepCheepJumpAheadOffsetMin/Max). No hace nada si
    // el pool esta lleno. Lo llaman tanto el timer automatico
    // (Game::update) como la tecla J de prueba.
    void spawnNextCheepCheepJump();
    // A diferencia del resto del elenco reciente (Blooper/Spiny/Pirana
    // Plant, sin pisoton), el Bill Bala SI se puede pisar como el elenco
    // clasico (ver BulletBill.h): pisoton, estrella o fuego lo matan igual.
    void checkBulletBillCollision(Mario& character, BulletBill& bulletBill, float prevY);
    void checkFireballBulletBillCollision(Fireball& fireball, BulletBill& bulletBill);
    // Unico enemigo del elenco sin ningun metodo de muerte (ver Podoboo.h):
    // cualquier contacto mientras esta afuera de la lava hace dano, sin
    // pisoton ni excepcion por estrella/fuego (la estrella sigue protegiendo
    // a Mario/Luigi del dano como siempre, pero el Podoboo no desaparece).
    void checkPodobooCollision(Mario& character, Podoboo& podoboo);
    // La bola explota igual al tocarlo (mismo feedback que contra Buzzy
    // Beetle), pero no le hace nada.
    void checkFireballPodobooCollision(Fireball& fireball, Podoboo& podoboo);
    // Sin pisoton, ni siquiera la estrella lo derrota (ver Bowser.h): la
    // estrella sigue protegiendo del dano como siempre, pero a Bowser no le
    // pasa nada -- solo killByFireball()/killByAxe() (ver mas abajo) lo
    // matan de verdad.
    void checkBowserCollision(Mario& character);
    // A diferencia de checkFireballXCollision del resto del elenco: SI
    // cuenta para derrotarlo (Bowser::killByFireball, cinco seguidas), en
    // vez de matarlo de una.
    void checkFireballBowserCollision(Fireball& fireball);
    // El fuego que escupe: mismo criterio que Hammer/checkHammerCollision,
    // sin pisoton y sigue de largo (no desaparece al tocar).
    void checkBowserFireCollision(Mario& character, BowserFire& fire);
    // Tocar el hacha derrota a Bowser de una sola vez (ver Axe.h) y hace
    // desaparecer el hacha misma.
    void checkAxeCollision(Mario& character, Axe& axe);
    // Bandera de meta (seccion 5.3, ver Flagpole.h): si el personaje toca el
    // mastil lo agarra (Mario::startFlagpoleGrab), suena flagpole.wav, sale
    // el puntaje segun la altura del agarre y la bandera empieza a bajar.
    void checkFlagpoleGrab(Mario& character);
    // Avanza la bandera, suelta a los que esperan abajo cuando ya bajo, y
    // arranca el cierre del nivel (levelClearTimer_) cuando el primero
    // entra al castillo.
    void updateFlagpoleSequence(float dt);

    // Puntaje/monedas/timer (seccion 6) y el HUD clasico que los muestra
    // (renderHud: MARIO + puntaje, monedas, WORLD + nombre del nivel, TIME).
    void addScore(int points) { score_ += points; }
    // Suma una moneda (+200 puntos, como el original) y cada 100 da una
    // vida extra -- unico lugar donde se cuentan, sea cual sea la moneda
    // (la del piso, la que salta de un bloque o la parada arriba de un
    // ladrillo).
    void collectCoin();
    // Vuelve el timer al tiempo limite del nivel (al empezarlo, o al
    // reaparecer en el checkpoint despues de que mueren todos).
    void resetLevelTimer();
    // Descuenta el tiempo en juego normal: al llegar a 100 avisa con
    // hurryup.wav, y en 0 mueren todos (Mario::killInstantly). Se frena en
    // cuanto alguien agarra la bandera.
    void updateLevelTimer(float dt);
    void renderHud();
    // Tuberia (seccion 5.3): si el personaje esta parado (grounded) justo
    // arriba del TOPE de una tuberia (Tilemap::isPipeTopTile) y aprieta
    // abajo, lo teletransporta al destino configurado en kPipeWarps -- sin
    // animacion de "hundirse" (simplificacion), aparece de una en el
    // destino. input es la de ESE personaje (input1_/input2_), no la
    // recibe Mario/Luigi (no le hace falta saber de tuberias).
    void checkPipeWarp(Mario& character, Input& input);
    // Ladrillo rompible (seccion 5.3): si Mario/Luigi saltando chocan de
    // cabeza contra un tile 'B' (ver Mario::headBumpedThisFrame), grande o
    // fuego lo rompe (breakBrickBlock); chico solo lo sacude sin romperlo
    // (bumpBrickBlock) -- moveAndCollide() ya se encarga de frenarle el
    // salto, esta funcion decide cual de las dos reacciones toca.
    void checkBrickBlockHit(Mario& character);
    // Lo rompe de verdad: saca el tile y tira 4 BrickDebris desde el pool.
    // El golpe en si (matar al enemigo de encima, si hay) es comun a este
    // caso y al de bumpBrickBlock, ver killEnemyOnTopOfTile.
    void breakBrickBlock(int col, int row);
    // No lo rompe: arranca la animacion corta de "sacudida" (sube unos px y
    // vuelve a bajar, ver blockBumps_/kBumpDuration) sin sacar el tile.
    void bumpBrickBlock(int col, int row);
    // Si el ladrillo (col, row) tiene una moneda parada arriba
    // (Tilemap::hasCoinOnTop, tile 'C'), la agarra: suma al contador y la
    // hace saltar con la animacion de BlockCoin, igual que una moneda de
    // bloque de interrogacion -- se llama sea que el ladrillo se rompio o
    // solo se sacudio, da igual (pedido del usuario).
    void collectTopCoinIfAny(int col, int row);
    // Comun a romper y sacudir (en el original, CUALQUIER golpe a un bloque
    // -- se rompa o no -- mata al enemigo que este parado justo encima):
    // mata a cualquier enemigo de prueba parado justo encima del tile
    // (col, row), con la misma parabola que usa el fuego/la estrella.
    void killEnemyOnTopOfTile(int col, int row);
    // Arranca (o reinicia si ya estaba en curso) la sacudida visual de
    // blockBumps_ para (col, row) -- extraido de bumpBrickBlock() para que
    // el bloque de interrogacion/dispensador lo pueda reusar sin duplicar la
    // logica del pool. Devuelve el slot usado (o nullptr si el pool estaba
    // lleno) para que checkQuestionBlockHit le pueda dejar cargado el item
    // pendiente (ver hasPendingItem en BlockBump).
    BlockBump* startBlockBump(int col, int row);

    // Bloque de interrogacion (seccion 5.3, tambien cubre el dispensador
    // 'D'): mismo mecanismo de deteccion que el ladrillo
    // (Mario::headBumpedThisFrame), se sacude y tira el item que le
    // corresponde a esa celda (ver kQuestionBlockItems/kDispenserBlockItems)
    // la primera vez que lo golpean, quedando "usado" para siempre
    // (Tilemap::markQuestionUsed/markDispenserUsed) -- un golpe mas tarde no
    // hace nada, ni se sacude. El item recien aparece (spawnItemFromQuestionBlock)
    // cuando termina la sacudida, no al toque -- ver hasPendingItem en BlockBump.
    void checkQuestionBlockHit(Mario& character);
    // Que item le toca a la celda (col, row) segun kQuestionBlockItems, y lo
    // hace aparecer saliendo del bloque (arriba de el, ver el comentario de
    // la clase) usando la instancia de prueba correspondiente
    // (mushroom_/fireFlower_/star_/coin_) -- no hay pool, un item por bloque
    // alcanza para este escenario de prueba. movingRight: hacia donde arranca
    // movi/endose Mushroom/Star (FireFlower/BlockCoin no se mueven, lo
    // ignoran) -- estilo Super Mario Bros. 3 en vez del original de 1985
    // (que siempre iba a la derecha, sin excepcion): sale hacia el lado
    // OPUESTO de donde vino el golpe, lo decide checkQuestionBlockHit segun
    // de que lado del bloque esta el personaje que lo golpeo.
    void spawnItemFromQuestionBlock(int col, int row, bool movingRight);
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
    // Bajo el agua usa una invulnerabilidad post-respawn mas larga que en
    // tierra (ver kUnderwaterRespawnInvincibleDuration en Game.cpp) -- un
    // solo lugar para este criterio en vez de repetirlo en las dos
    // funciones de respawn de arriba.
    void grantRespawnInvincibilityForWorld(Mario& character);
    // Recalcula cameraX_ para que siga a Mario (1P) o al punto medio entre
    // Mario y Luigi (2P, sin contar al que esta cayendo muerto), sin dejar
    // ver mas alla de los bordes del nivel (se clampea a [0, ancho del
    // nivel - pantalla] -- 0 si el nivel entero entra en una pantalla, como
    // los escenarios viejos que no scrollean). Tambien actualiza
    // cameraScrollSpeed_ (por eso necesita dt). Ver el comentario de
    // cameraX_/cameraScrollSpeed_.
    void updateCamera(float dt);
    // 2P: los bordes de la pantalla son paredes para los dos, asi ninguno
    // puede alejarse tanto del otro que la camara no los contenga -- el que
    // va adelante queda frenado en el borde derecho hasta que el otro lo
    // alcance (mismo criterio que el co-op de los Mario modernos).
    void keepCharactersOnScreen();
    // true si una caja de ese tamano en (x, y) no toca ningun tile solido
    // ni se sale del nivel por los costados.
    bool isAreaFree(float x, float y, int width, int height) const;

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    bool running_ = false;
    bool showDebugOverlay_ = true;
    int currentFps_ = 0;
    AppState appState_ = AppState::ScenarioSelect;
    TestScenario scenario_ = TestScenario::Items;
    // Opcion marcada con la flecha en el selector (indice en el orden de
    // TestScenario). Se mueve con cruceta/stick o flechas/W/S y se confirma
    // con A/Start o Enter/Espacio; los numeros siguen eligiendo directo.
    int menuSelection_ = 0;
    // El stick genera un evento por cada cambio minimo de eje: esto deja
    // pasar un solo paso de menu por cada inclinacion, hasta que vuelve al
    // centro.
    bool menuStickHeld_ = false;
    WorldType worldType_ = WorldType::Overworld;
    bool characterCollisionEnabled_ = true;
    // Desplazamiento horizontal de camara en px de nivel (seccion 3): se le
    // resta a la X de todo lo que se dibuja (ver render() y
    // updateCamera()), la logica de juego (fisica, colisiones) sigue
    // trabajando siempre en coordenadas de nivel absolutas, esto es
    // puramente de dibujado.
    float cameraX_ = 0.0f;
    // Cuanto se movio cameraX_ en el ultimo cuadro, en px/s (ver
    // updateCamera()) -- el original de 1985 engancha la velocidad del
    // Lakitu a esto (ScrollAmount en la disassembly, ver el comentario de
    // Lakitu.h), no a la posicion cruda del personaje: mientras la camara
    // esta scrolleando de verdad (el personaje ya empujo mas alla del
    // centro/borde del nivel) el Lakitu acelera, si no, va lento aunque el
    // personaje se mueva (todavia no llego a mover la camara).
    float cameraScrollSpeed_ = 0.0f;
    // Pide que el proximo updateCamera() salte directo al objetivo en vez
    // de acercarse de a poco (2P): al empezar un nivel, al pasar por una
    // tuberia o al volver al checkpoint, donde la camara anterior ya no
    // tiene nada que ver con donde estan los personajes.
    bool cameraSnapPending_ = true;
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

    // Hongo de prueba: sale de su bloque de interrogacion (ver
    // checkQuestionBlockHit), F11 lo repone igual (sin bloque, cae directo
    // en su posicion de prueba de siempre) por si hace falta probarlo suelto.
    SpriteSheet mushroomSprite_;
    Mushroom mushroom_;

    // Variante 1-UP del hongo (misma entidad Mushroom, ver
    // Mushroom::isOneUp): solo cambia el sprite (verde en vez de naranja,
    // recoloreado a partir de mushroomSprite_) y el efecto al agarrarlo
    // (suma una vida en vez de crecer, ver checkMushroomPickup).
    SpriteSheet oneUpSprite_;

    // Flor de fuego de prueba, mismo criterio que el hongo (F12 la repone).
    SpriteSheet fireFlowerSprite_;
    FireFlower fireFlower_;

    // Estrella de prueba, mismo criterio que el hongo (F4 la repone).
    SpriteSheet starSprite_;
    StarItem star_;

    // Moneda de prueba: a diferencia de los otros tres, esta NO sale de
    // ningun bloque (ver el comentario de spawnItemFromQuestionBlock) --
    // queda quieta puesta en el piso del escenario de Items, se agarra
    // caminando. Tecla C la repone en el mismo lugar.
    SpriteSheet coinSprite_;
    Coin coin_;

    // Bloques de interrogacion de prueba (seccion 5.3, escenario de Items):
    // uno por item de la fase inicial (ver QuestionBlockSpawn/kQuestionBlockItems
    // en Game.cpp). Animacion compartida por todos los bloques sin usar del
    // nivel (mismo ciclo, sincronizados, como en el original) en vez de un
    // timer por celda. itemEmerge_ maneja el "salir del bloque" de
    // Mushroom/FireFlower/Star (ver el comentario de ItemEmerge).
    SpriteSheet questionBlockSprite_;
    float questionBlockAnimTimer_ = 0.0f;
    int questionBlockAnimFrame_ = 0;
    ItemEmerge itemEmerge_;

    // Animacion (giro de 3 frames) compartida por todas las monedas paradas
    // arriba de un ladrillo (Tilemap::hasCoinOnTop, tile 'C') sin agarrar
    // todavia -- mismo criterio que questionBlockAnimTimer_/frame_ de
    // arriba, reusa coinSprite_ (la misma moneda del piso, Coin.h) en vez de
    // un sprite nuevo.
    float brickCoinAnimTimer_ = 0.0f;
    int brickCoinAnimFrame_ = 0;

    // La moneda que salta de un bloque (BlockCoin.h, distinta de Coin.h de
    // arriba -- ver el comentario de spawnItemFromQuestionBlock): tiene su
    // propio salto, no pasa por itemEmerge_. coinCount_ es el contador de
    // monedas del HUD (seccion 6, ver collectCoin: cada 100 da una vida).
    // livesCount_ es la otra pieza minima de la
    // seccion 6, sumada por checkMushroomPickup cuando el hongo agarrado es
    // el 1-UP (ver BlockItem::OneUp).
    SpriteSheet blockCoinSprite_;
    BlockCoin blockCoin_;
    int coinCount_ = 0;
    int livesCount_ = 0;
    int score_ = 0;
    // Tiempo restante en unidades del timer clasico (cada unidad son 24
    // cuadros del original, ver kTimeUnitSeconds en Game.cpp): el HUD
    // muestra la parte entera redondeada para arriba.
    float timeRemaining_ = 0.0f;
    bool hurryUpPlayed_ = false;
    // Fin de nivel: el tiempo que sobro se pasa a puntos (50 por unidad)
    // antes del cartel de nivel completo, como en el original.
    bool timeTallying_ = false;
    float tallySoundTimer_ = 0.0f;

    // Tuberia (seccion 5.3): sprite propio, 2 columnas (0=tope, 1=cuerpo) x
    // una fila por WorldType, cada celda de 32x16 (dos tiles de ancho de una
    // sola vez, ver Tilemap::isPipeTile). checkPipeWarp es quien decide
    // cuando entra por ella (ver el comentario en Game.cpp).
    SpriteSheet pipeSprite_;

    // Efectos de sonido (seccion 8.2): ver SoundManager.h para la API y el
    // comentario en Game::init() (donde se cargan) para el mapeo
    // evento->archivo.
    SoundManager audio_;

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

    // Hammer Bro de prueba (seccion 5.2, quinto enemigo): escenario de
    // prueba propio (TestScenario::HammerBro, test_hammerbro.json) en vez
    // de compartir el de Enemigos -- patrulla una plataforma angosta y
    // flotante, mas facil de ver aislado que mezclado con el resto del
    // elenco. El martillo que tira es una entidad separada (Hammer) para
    // poder animar el giro independiente del estado del Bro, ver
    // HammerBro::consumeThrowRequest(). Tecla H lo repone.
    SpriteSheet hammerBroSprite_;
    HammerBro hammerBro_;
    SpriteSheet hammerSprite_;
    std::array<Hammer, kMaxHammers> hammers_;

    // Lakitu de prueba (seccion 5.2, sexto enemigo): escenario propio
    // (TestScenario::LakituSpiny, test_lakitu.json) igual que el Hammer
    // Bro -- vuela siguiendo al personaje mas cercano y tira Spiny (Spiny.h,
    // septimo enemigo) que eclosionan al tocar el piso. Tecla L lo repone.
    SpriteSheet lakituSprite_;
    Lakitu lakitu_;
    // Los Spiny que tira (mismo criterio de pool que Hammer/hammers_): a
    // diferencia del martillo, quedan caminando indefinidamente hasta que
    // los maten, asi que el pool es mas grande (kMaxSpinies). Una sola
    // paleta (ver Spiny.h), su sprite se dibuja siempre con la fila 0.
    SpriteSheet spinySprite_;
    std::array<Spiny, kMaxSpinies> spinies_;

    // Blooper de prueba (seccion 5.2, octavo enemigo): escenario propio
    // (TestScenario::Blooper, test_blooper.json) con worldType_ arrancando
    // en Underwater (nivel de agua) -- flota sin piso, hundiendose despacio
    // entre chorros hacia el personaje mas cercano (ver Blooper.h). Tecla O
    // lo repone (B ya la usa Buzzy Beetle).
    SpriteSheet blooperSprite_;
    Blooper blooper_;

    // Cheep Cheep de prueba (seccion 5.2, noveno enemigo): escenario propio
    // (TestScenario::CheepCheep, test_cheepcheep.json) con dos instancias
    // Swimming (una por SwimPattern -- setVariant()/setSwimPattern() se
    // llaman una sola vez en init()) que comparten sprite con la Jumping
    // (una sola paleta, ver CheepCheep.h). Tecla N las repone.
    SpriteSheet cheepCheepSprite_;
    CheepCheep cheepCheepSwim_;     // SwimPattern::Straight
    CheepCheep cheepCheepSwimOsc_;  // SwimPattern::Oscillating

    // Cheep Cheep Jumping de prueba: escenario propio
    // (TestScenario::CheepCheepJump, test_cheepcheep_jump.json, largo con
    // scroll) en vez de compartir el de arriba -- a diferencia de la
    // Swimming necesita que el personaje camine (worldType_ Overworld, no
    // Underwater) para poder probar el cruce mientras avanza. Pool (mismo
    // criterio que spinies_/hammers_): cheepCheepJumpSpawnTimer_ hace que
    // vaya apareciendo una tras otra desde abajo del borde inferior de
    // pantalla, un poco por delante del personaje mas adelantado, mientras
    // este escenario este activo (ver Game::update). Tecla J fuerza una de
    // una si hay lugar en el pool, sin esperar al timer.
    std::array<CheepCheep, kMaxCheepCheepJumps> cheepCheepJumps_;
    float cheepCheepJumpSpawnTimer_ = 0.0f;
    // Deriva (izquierda/derecha) de la ultima que aparecio: se alterna
    // siempre en Game::spawnNextCheepCheepJump en vez de tirar una moneda
    // cada vez -- con moneda independiente, una racha de varias seguidas
    // para el mismo lado (estadisticamente normal pero se nota mal en la
    // practica) hacia parecer que "solo saltan para un lado".
    bool cheepCheepJumpLastDriftRight_ = false;

    // Bill Bala de prueba (enemigo nuevo, ver BulletBill.h -- surgio de una
    // confusion con el sprite de Cheep Cheep): escenario propio
    // (TestScenario::BulletBill, test_bulletbill.json), dos instancias
    // disparadas en direcciones opuestas para probar el cruce. Tecla I lo
    // repone (B/N/J ya estan tomadas).
    SpriteSheet bulletBillSprite_;
    BulletBill bulletBillA_;
    BulletBill bulletBillB_;

    // Podoboo de prueba (seccion 5.2, decimo enemigo): escenario propio
    // (TestScenario::Podoboo, test_podoboo.json) con worldType_ arrancando
    // en Castle (solo aparece en castillos en el original, ver Podoboo.h).
    // Una sola instancia fija (no un pool: el pozo de lava no se mueve ni se
    // agota, siempre el mismo). Tecla P lo repone.
    SpriteSheet podobooSprite_;
    Podoboo podoboo_;

    // Bowser de prueba (seccion 5.2, jefe final): escenario propio
    // (TestScenario::Bowser, test_bowser.json) con worldType_ arrancando en
    // Castle (solo aparece ahi en el original, ver Bowser.h). Solo escupe
    // fuego (a proposito no tira martillos, ver el comentario de Bowser.h):
    // bowserFires_ es su pool propio (mismo criterio que hammers_/spinies_).
    // El hacha (axe_) que lo derrota de una sola vez esta en el otro
    // extremo del nivel de prueba. Tecla Y repone Bowser y el hacha juntos.
    SpriteSheet bowserSprite_;
    Bowser bowser_;
    SpriteSheet bowserFireSprite_;
    std::array<BowserFire, kMaxBowserFires> bowserFires_;
    SpriteSheet axeSprite_;
    Axe axe_;

    // Bandera de meta: solo existe en los niveles que tienen tiles 'F' (ver
    // startScenario). levelClearTimer_ > 0 = nivel completado, se muestra
    // el cartel y al llegar a 0 vuelve al selector -- placeholder hasta que
    // exista el paso al nivel siguiente (seccion 6).
    SpriteSheet flagpoleSprite_;
    SpriteSheet flagSprite_;
    Flagpole flagpole_;
    float levelClearTimer_ = 0.0f;

    // Ladrillo rompible (seccion 5.3, tile 'B' en el JSON del nivel, ver
    // Tilemap::isBrickTile): grande/fuego lo rompe saltando de cabeza
    // (Game::checkBrickBlockHit), chico solo lo golpea sin romperlo. El
    // sprite del bloque en si lo dibuja el propio Tilemap::render(); estos
    // dos son solo para los pedazos que salen volando al romperse.
    SpriteSheet brickDebrisSprite_;
    std::array<BrickDebris, kMaxBrickDebris> brickDebris_;
    SpriteSheet brickBlockSprite_;
    std::array<BlockBump, kMaxBlockBumps> blockBumps_;

    // Pool de popups de puntaje (hongo/flor/estrella dan 1000, como en el
    // original; la moneda no muestra popup, solo suma al contador cuando
    // exista). Varios a la vez posibles si los dos jugadores agarran cosas
    // casi juntos.
    std::array<ScorePopup, 4> scorePopups_;
};
