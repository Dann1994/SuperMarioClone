#include "Game.h"

#include <SDL_image.h>

#include <algorithm>
#include <cstdio>

#include "../render/DebugFont.h"

namespace {
constexpr float kFixedDt = 1.0f / 60.0f;
constexpr float kMaxFrameTime = 0.25f;  // evita "spiral of death" si hay un frame muy largo

// Punto de aparicion/reaparicion (al morir chico) de cada personaje.
constexpr float kMarioSpawnX = 32.0f;
constexpr float kMarioSpawnY = 64.0f;
constexpr float kLuigiSpawnX = 56.0f;
constexpr float kLuigiSpawnY = 64.0f;

// Cicla chico -> grande -> fuego -> chico. Herramienta de debug (seccion 11)
// para probar los tres estados de Mario antes de tener los power-ups reales
// (hongo, flor de fuego) que los disparan en el juego.
MarioSize nextSize(MarioSize size) {
    switch (size) {
        case MarioSize::Small: return MarioSize::Big;
        case MarioSize::Big: return MarioSize::Fire;
        case MarioSize::Fire: default: return MarioSize::Small;
    }
}

// Constantes de la colision Mario<->Luigi (prototipo, sin base documentada
// en el juego original -- este mismo no tiene multiplayer simultaneo). Se
// ajustan a ojo, como el resto de la fisica del proyecto.
constexpr float kMoveThreshold = 15.0f;       // px/s de velocidad de acercamiento, minimo para contar como choque
constexpr float kMinCollisionSpeed = 45.0f;   // px/s, piso de velocidad de choque usado para el impulso (ver mas abajo)
constexpr float kStompBounce = -180.0f;       // px/s, pisada normal (mas chica que el salto: -260)
constexpr float kStompBounceBoosted = -260.0f; // px/s, pisada con Jump sostenido: igual a un salto completo
constexpr float kStompSidewaysSpeed = 75.0f;  // px/s, empuje lateral para no rebotar en el mismo lugar
constexpr float kStompCooldown = 0.3f;        // s, evita un segundo rebote encadenado

// Peso (masa) relativo para el choque horizontal: chico = referencia (1x),
// grande y fuego (mismo hitbox de 32px) pesan mas.
constexpr float kSmallWeight = 1.0f;
constexpr float kBigWeight = 1.8f;
// Cuanto rebotan al chocar (0 = quedan pegados sin rebote, 1 = elastico
// perfecto). Ver resolveCharacterCollision: la velocidad de cada uno despues
// del choque sale de un impulso de colision real (conservacion de momento
// con restitucion), usando la velocidad y el peso de cada personaje en ese
// instante -- no un valor fijo. Asi, tamano Y velocidad (por ejemplo correr)
// afectan el resultado solos, sin casos especiales para "esta corriendo".
constexpr float kCollisionRestitution = 0.7f;

float weightOf(MarioSize size) {
    return size == MarioSize::Small ? kSmallWeight : kBigWeight;
}
}  // namespace

bool Game::init() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        SDL_Log("SDL_Init fallo: %s", SDL_GetError());
        return false;
    }

    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        SDL_Log("IMG_Init fallo: %s", IMG_GetError());
        return false;
    }

    window_ = SDL_CreateWindow(
        "Super Mario Clone",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        kLogicalWidth * 3, kLogicalHeight * 3,
        SDL_WINDOW_RESIZABLE);
    if (!window_) {
        SDL_Log("SDL_CreateWindow fallo: %s", SDL_GetError());
        return false;
    }

    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer_) {
        SDL_Log("SDL_CreateRenderer fallo: %s", SDL_GetError());
        return false;
    }

    SDL_RenderSetLogicalSize(renderer_, kLogicalWidth, kLogicalHeight);
    SDL_RenderSetIntegerScale(renderer_, SDL_TRUE);

    if (!level_.loadFromFile("levels/world1/1-1.json")) {
        return false;
    }
    if (!mario_.loadSprite(renderer_, "assets/sprites/mario_small.png",
                            "assets/sprites/mario_big.png", "assets/sprites/mario_fire.png")) {
        return false;
    }
    if (!luigi_.loadSprite(renderer_, "assets/sprites/luigi_small.png",
                            "assets/sprites/luigi_big.png", "assets/sprites/luigi_fire.png")) {
        return false;
    }
    // Paleta de estrella: por ahora solo la variante "overworld" (falta el
    // concepto de tipo de mundo para elegir entre overworld/castle/
    // underground/underwater).
    if (!mario_.loadStarSprite(renderer_, "assets/sprites/mario_small_star.png",
                                "assets/sprites/mario_big_star.png", "assets/sprites/mario_fire_star.png")) {
        return false;
    }
    if (!luigi_.loadStarSprite(renderer_, "assets/sprites/luigi_small_star.png",
                                "assets/sprites/luigi_big_star.png", "assets/sprites/luigi_fire_star.png")) {
        return false;
    }
    if (!fireballSprite_.loadFromFile(renderer_, "assets/sprites/fireball.png", Fireball::kSize, Fireball::kSize)) {
        return false;
    }
    if (!fireballExplosionSprite_.loadFromFile(renderer_, "assets/sprites/fireball_explosion.png", 16, 16)) {
        return false;
    }
    if (!mushroomSprite_.loadFromFile(renderer_, "assets/sprites/mushroom.png", Mushroom::kSize, Mushroom::kSize)) {
        return false;
    }
    if (!fireFlowerSprite_.loadFromFile(renderer_, "assets/sprites/fire_flower.png", FireFlower::kSize, FireFlower::kSize)) {
        return false;
    }
    if (!starSprite_.loadFromFile(renderer_, "assets/sprites/star_item.png", StarItem::kSize, StarItem::kSize)) {
        return false;
    }
    if (!coinSprite_.loadFromFile(renderer_, "assets/sprites/coin.png", Coin::kSize, Coin::kSize)) {
        return false;
    }
    mario_.spawn(kMarioSpawnX, kMarioSpawnY);
    luigi_.spawn(kLuigiSpawnX, kLuigiSpawnY);
    mushroom_.spawn(100.0f, 208.0f, true);
    fireFlower_.spawn(130.0f, 208.0f);
    star_.spawn(160.0f, 208.0f, true);
    coin_.spawn(190.0f, 208.0f);

    running_ = true;
    return true;
}

void Game::processEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            running_ = false;
        } else if (event.type == SDL_KEYDOWN && !event.key.repeat) {
            if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                running_ = false;
            } else if (event.key.keysym.scancode == SDL_SCANCODE_F3) {
                showDebugOverlay_ = !showDebugOverlay_;
            } else if (event.key.keysym.scancode == SDL_SCANCODE_F4) {
                star_.spawn(160.0f, 208.0f, true);
            } else if (event.key.keysym.scancode == SDL_SCANCODE_F1) {
                mario_.setSize(nextSize(mario_.size()));
            } else if (event.key.keysym.scancode == SDL_SCANCODE_F2) {
                luigi_.setSize(nextSize(luigi_.size()));
            } else if (event.key.keysym.scancode == SDL_SCANCODE_F5) {
                mario_.setInvincible(!mario_.isInvincible());
            } else if (event.key.keysym.scancode == SDL_SCANCODE_F6) {
                luigi_.setInvincible(!luigi_.isInvincible());
            } else if (event.key.keysym.scancode == SDL_SCANCODE_F7) {
                worldType_ = nextWorldType(worldType_);
                mario_.setWorldType(worldType_);
                luigi_.setWorldType(worldType_);
            } else if (event.key.keysym.scancode == SDL_SCANCODE_F8) {
                characterCollisionEnabled_ = !characterCollisionEnabled_;
            } else if (event.key.keysym.scancode == SDL_SCANCODE_F9) {
                mario_.takeDamage();
            } else if (event.key.keysym.scancode == SDL_SCANCODE_F10) {
                luigi_.takeDamage();
            } else if (event.key.keysym.scancode == SDL_SCANCODE_F11) {
                mushroom_.spawn(100.0f, 208.0f, true);
            } else if (event.key.keysym.scancode == SDL_SCANCODE_F12) {
                fireFlower_.spawn(130.0f, 208.0f);
            } else if (event.key.keysym.scancode == SDL_SCANCODE_C) {
                coin_.spawn(190.0f, 208.0f);
            } else if (event.key.keysym.scancode == SDL_SCANCODE_M) {
                twoPlayerMode_ = !twoPlayerMode_;
            }
        }
    }
}

void Game::trySpawnFireball(Mario& shooter, std::array<Fireball, kMaxFireballsPerPlayer>& pool) {
    if (!shooter.consumeThrowRequest()) return;

    for (Fireball& fireball : pool) {
        if (fireball.isAlive()) continue;
        const bool right = shooter.facingRight();
        const float spawnX = right ? shooter.x() + 12.0f : shooter.x() - Fireball::kSize;
        const float spawnY = shooter.y() + shooter.height() * 0.4f;
        fireball.spawn(spawnX, spawnY, right);
        shooter.playThrowAnimation();
        return;
    }
    // Los dos slots estan ocupados: como en el original, no se tira una tercera
    // (y no se reproduce la animacion de tirar, ya que no se tiro nada).
}

void Game::resolveCharacterCollision() {
    if (!characterCollisionEnabled_) return;

    const float ax = mario_.x(), ay = mario_.y();
    const float aw = static_cast<float>(mario_.width()), ah = static_cast<float>(mario_.height());
    const float bx = luigi_.x(), by = luigi_.y();
    const float bw = static_cast<float>(luigi_.width()), bh = static_cast<float>(luigi_.height());

    const bool overlapX = ax < bx + bw && ax + aw > bx;
    const bool overlapY = ay < by + bh && ay + ah > by;
    if (!overlapX || !overlapY) return;

    const float overlapLeft = (bx + bw) - ax;   // A entra por la izquierda de B
    const float overlapRight = (ax + aw) - bx;  // A entra por la derecha de B (B esta a la izq de A)
    const float overlapTop = (by + bh) - ay;    // A entra por arriba de B
    const float overlapBottom = (ay + ah) - by; // A entra por abajo de B

    const float overlapXAmount = std::min(overlapLeft, overlapRight);
    const float overlapYAmount = std::min(overlapTop, overlapBottom);

    if (overlapYAmount < overlapXAmount) {
        // Colision vertical: alguien esta arriba del otro. Solo cuenta como
        // "pisada" si el de arriba esta cayendo (si no, es un roce lateral
        // con distinta altura de pie, no un salto encima).
        const bool marioOnTop = ay < by;
        Mario& top = marioOnTop ? mario_ : luigi_;
        Mario& bottom = marioOnTop ? luigi_ : mario_;
        Input& topInput = marioOnTop ? input1_ : input2_;

        if (top.velocityY() > 20.0f) {
            top.setY(bottom.y() - top.height());
            if (top.canStompBounce()) {
                const bool boosted = topInput.down(Action::Jump);
                top.setVelocityY(boosted ? kStompBounceBoosted : kStompBounce);
                // Empuja hacia el lado que mira para que no quede rebotando
                // en el mismo lugar, y bloquea un segundo rebote encadenado
                // si cae de nuevo antes de separarse del todo.
                top.setVelocityX(top.facingRight() ? kStompSidewaysSpeed : -kStompSidewaysSpeed);
                top.setStompCooldown(kStompCooldown);
            } else {
                top.setVelocityY(0.0f);
            }
        }
        return;
    }

    // Colision horizontal: separar segun peso y, si se estan acercando,
    // rebotar con un impulso de colision real (momento + restitucion).
    const bool marioLeft = ax < bx;
    Mario& left = marioLeft ? mario_ : luigi_;
    Mario& right = marioLeft ? luigi_ : mario_;

    const float leftWeight = weightOf(left.size());
    const float rightWeight = weightOf(right.size());
    const float totalWeight = leftWeight + rightWeight;

    // El mas pesado cede menos terreno al separarse de la superposicion.
    left.nudgeX(-overlapXAmount * (rightWeight / totalWeight));
    right.nudgeX(overlapXAmount * (leftWeight / totalWeight));

    const float leftV = left.velocityX();
    const float rightV = right.velocityX();
    const float closingSpeed = leftV - rightV;  // > 0 si se estan acercando

    if (closingSpeed > kMoveThreshold) {
        // Impulso de colision 1D con restitucion (formula estandar de
        // conservacion de momento). Con la misma velocidad y peso para los
        // dos, ambos rebotan igual; el mas pesado y/o mas rapido desplaza
        // mas al otro y se desvia menos el mismo. La animacion de "patinar"
        // se fuerza explicitamente (no depende de que sigan apretando la
        // direccion, ni de si estan nadando).
        //
        // La velocidad de choque real tiene un piso (kMinCollisionSpeed):
        // sin esto, si uno mantiene la tecla apretado contra el otro sin
        // acelerar mucho, el impulso sale tan chico que quedan "pegados"
        // resbalando de a poco en vez de separarse con un rebote que se note.
        const float effectiveClosingSpeed = std::max(closingSpeed, kMinCollisionSpeed);
        const float impulse = -(1.0f + kCollisionRestitution) * effectiveClosingSpeed / (1.0f / leftWeight + 1.0f / rightWeight);
        left.setVelocityX(leftV + impulse / leftWeight);
        right.setVelocityX(rightV - impulse / rightWeight);
        left.setFacingRight(true);
        right.setFacingRight(false);
        left.playBumpAnimation();
        right.playBumpAnimation();
    }
    // Si no se estan acercando lo suficiente, ya quedaron separados por el
    // nudgeX de arriba, sin rebote.
}

void Game::checkDamageHazard(Mario& character) {
    const SDL_Rect box{static_cast<int>(character.x()), static_cast<int>(character.y()),
                        character.width(), character.height()};
    const bool overlap = box.x < damageHazard_.x + damageHazard_.w && box.x + box.w > damageHazard_.x &&
                          box.y < damageHazard_.y + damageHazard_.h && box.y + box.h > damageHazard_.y;
    if (!overlap) return;

    if (character.size() == MarioSize::Small) {
        character.startDeath();
    } else {
        character.takeDamage();
    }
}

void Game::checkMushroomPickup(Mario& character) {
    if (!mushroom_.isAlive()) return;

    const bool overlap =
        character.x() < mushroom_.x() + Mushroom::kSize && character.x() + character.width() > mushroom_.x() &&
        character.y() < mushroom_.y() + Mushroom::kSize && character.y() + character.height() > mushroom_.y();
    if (!overlap) return;

    if (character.size() == MarioSize::Small) {
        character.startGrow();
    }
    spawnScorePopup(mushroom_.x(), mushroom_.y(), "1000");
    mushroom_.collect();
}

void Game::checkFireFlowerPickup(Mario& character) {
    if (!fireFlower_.isAlive()) return;

    const bool overlap =
        character.x() < fireFlower_.x() + FireFlower::kSize && character.x() + character.width() > fireFlower_.x() &&
        character.y() < fireFlower_.y() + FireFlower::kSize && character.y() + character.height() > fireFlower_.y();
    if (!overlap) return;

    if (character.size() == MarioSize::Small) {
        // En este clon, si la agarra chico se comporta como el hongo (crece
        // a grande nomas, con la misma animacion) en vez de pasar directo a
        // fuego como en el juego original.
        character.startGrow(MarioSize::Big);
    } else if (character.size() == MarioSize::Big) {
        // Misma secuencia de parpadeo, pero sin cambio de altura (grande y
        // fuego miden igual): parpadea entre la paleta normal y la de fuego.
        character.startGrow(MarioSize::Fire);
    }
    spawnScorePopup(fireFlower_.x(), fireFlower_.y(), "1000");
    fireFlower_.collect();
}

void Game::checkStarPickup(Mario& character) {
    if (!star_.isAlive()) return;

    const bool overlap =
        character.x() < star_.x() + StarItem::kSize && character.x() + character.width() > star_.x() &&
        character.y() < star_.y() + StarItem::kSize && character.y() + character.height() > star_.y();
    if (!overlap) return;

    character.setInvincible(true);
    spawnScorePopup(star_.x(), star_.y(), "1000");
    star_.collect();
}

void Game::checkCoinPickup(Mario& character) {
    if (!coin_.isAlive()) return;

    const bool overlap =
        character.x() < coin_.x() + Coin::kSize && character.x() + character.width() > coin_.x() &&
        character.y() < coin_.y() + Coin::kSize && character.y() + character.height() > coin_.y();
    if (!overlap) return;

    // Sin contador de monedas/vidas todavia (seccion 6): por ahora solo se
    // recolecta (sin popup de puntaje, la moneda no lo tiene en el original).
    coin_.collect();
}

void Game::spawnScorePopup(float x, float y, const std::string& text) {
    for (ScorePopup& popup : scorePopups_) {
        if (popup.isAlive()) continue;
        popup.spawn(x, y, text);
        return;
    }
    // Sin slots libres: se pierde este popup, no es grave (solo visual).
}

SDL_Point Game::findRespawnPoint(float aroundX, int characterHeight) const {
    const int ts = level_.tileSize();
    const int aroundCol = static_cast<int>(aroundX) / ts;
    const int widthTiles = level_.widthTiles();

    for (int radius = 0; radius <= widthTiles; ++radius) {
        for (int dir = 0; dir < 2; ++dir) {
            if (radius == 0 && dir == 1) continue;  // radio 0 es la misma columna en los dos lados
            const int col = dir == 0 ? aroundCol + radius : aroundCol - radius;
            if (col < 0 || col >= widthTiles) continue;
            for (int row = 0; row < level_.heightTiles(); ++row) {
                if (!level_.isSolidTile(col, row)) continue;
                // Primer piso solido de arriba hacia abajo en esa columna:
                // si no hay ninguno, la columna es un pozo y se descarta,
                // se sigue buscando hacia el costado.
                const SDL_Rect spawnBox{col * ts, row * ts - characterHeight, ts, characterHeight};
                const bool onHazard = spawnBox.x < damageHazard_.x + damageHazard_.w &&
                                       spawnBox.x + spawnBox.w > damageHazard_.x &&
                                       spawnBox.y < damageHazard_.y + damageHazard_.h &&
                                       spawnBox.y + spawnBox.h > damageHazard_.y;
                if (onHazard) break;  // esta columna no sirve, no sigas bajando en ella: probá la que sigue
                return SDL_Point{spawnBox.x, spawnBox.y};
            }
        }
    }
    // No deberia pasar (implicaria un nivel sin piso en ningun lado).
    return SDL_Point{static_cast<int>(kMarioSpawnX), static_cast<int>(kMarioSpawnY)};
}

void Game::respawnNearestPlatform(Mario& character) {
    // Preferentemente en el centro de la pantalla, no en donde murio.
    const SDL_Point point = findRespawnPoint(kLogicalWidth / 2.0f, character.height());
    character.spawn(static_cast<float>(point.x), static_cast<float>(point.y));
    character.grantRespawnInvincibility();
}

void Game::respawnAtCheckpoint(Mario& character, float spawnX, float spawnY) {
    // Placeholder hasta que exista un sistema de checkpoints real (seccion
    // 6): vuelve al spawn original del nivel.
    character.spawn(spawnX, spawnY);
    character.grantRespawnInvincibility();
}

void Game::update(float dt) {
    // Congelado (secuencias de danio/crecer, o alguien que "muere del
    // todo"): todo lo demas se pausa. En 2P eso es cuando mueren los dos a
    // la vez; en 1P, Mario es el unico jugador, asi que su muerte sola ya
    // cuenta como "murieron todos" y tambien congela.
    const bool damageOrGrowFrozen = mario_.isInDamageSequence() || mario_.isInGrowSequence() ||
        (twoPlayerMode_ && (luigi_.isInDamageSequence() || luigi_.isInGrowSequence()));
    const bool everyoneDying = twoPlayerMode_
        ? (mario_.isInDeathSequence() && luigi_.isInDeathSequence())
        : mario_.isInDeathSequence();

    if (damageOrGrowFrozen || everyoneDying) {
        mario_.updateDamageSequence(dt);
        mario_.updateGrowSequence(dt);
        mario_.updateDeathSequence(dt);
        if (mario_.consumeDeathFinished()) {
            respawnAtCheckpoint(mario_, kMarioSpawnX, kMarioSpawnY);
        }
        if (twoPlayerMode_) {
            luigi_.updateDamageSequence(dt);
            luigi_.updateGrowSequence(dt);
            luigi_.updateDeathSequence(dt);
            if (luigi_.consumeDeathFinished()) {
                respawnAtCheckpoint(luigi_, kLuigiSpawnX, kLuigiSpawnY);
            }
        }
        return;
    }

    mario_.update(dt, input1_, level_);
    // Solo se llega aca muriendo en 2P con el otro todavia vivo (en 1P o en
    // "murieron los dos" ya se atendio arriba): reaparece en la plataforma
    // mas cercana en vez del spawn del nivel, sin pausar nada.
    if (mario_.consumeDeathFinished()) {
        respawnNearestPlatform(mario_);
    }

    if (twoPlayerMode_) {
        luigi_.update(dt, input2_, level_);
        if (luigi_.consumeDeathFinished()) {
            respawnNearestPlatform(luigi_);
        }
        resolveCharacterCollision();
    }

    checkDamageHazard(mario_);
    if (twoPlayerMode_) checkDamageHazard(luigi_);

    mushroom_.update(dt, level_, kLogicalWidth);
    checkMushroomPickup(mario_);
    if (twoPlayerMode_) checkMushroomPickup(luigi_);

    fireFlower_.update(dt);
    checkFireFlowerPickup(mario_);
    if (twoPlayerMode_) checkFireFlowerPickup(luigi_);

    star_.update(dt, level_, kLogicalWidth);
    checkStarPickup(mario_);
    if (twoPlayerMode_) checkStarPickup(luigi_);

    coin_.update(dt);
    checkCoinPickup(mario_);
    if (twoPlayerMode_) checkCoinPickup(luigi_);

    for (ScorePopup& popup : scorePopups_) popup.update(dt);

    trySpawnFireball(mario_, marioFireballs_);
    for (Fireball& fireball : marioFireballs_) fireball.update(dt, level_, kLogicalWidth);

    if (twoPlayerMode_) {
        trySpawnFireball(luigi_, luigiFireballs_);
        for (Fireball& fireball : luigiFireballs_) fireball.update(dt, level_, kLogicalWidth);
    }
}

void Game::render() {
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);

    level_.render(renderer_, backgroundColorFor(worldType_));

    // Placeholder de peligro (ver comentario junto a damageHazard_).
    SDL_SetRenderDrawColor(renderer_, 230, 20, 200, 255);
    SDL_RenderFillRect(renderer_, &damageHazard_);

    const int worldRow = static_cast<int>(worldType_);
    mushroom_.render(renderer_, mushroomSprite_, worldRow);
    fireFlower_.render(renderer_, fireFlowerSprite_, worldRow);
    star_.render(renderer_, starSprite_, worldRow);
    coin_.render(renderer_, coinSprite_, worldRow);
    mario_.render(renderer_);
    for (const Fireball& fireball : marioFireballs_) fireball.render(renderer_, fireballSprite_, fireballExplosionSprite_);
    if (twoPlayerMode_) {
        luigi_.render(renderer_);
        for (const Fireball& fireball : luigiFireballs_) fireball.render(renderer_, fireballSprite_, fireballExplosionSprite_);
    }

    for (const ScorePopup& popup : scorePopups_) popup.render(renderer_);

    if (showDebugOverlay_) {
        char fpsText[32];
        std::snprintf(fpsText, sizeof(fpsText), "FPS:%3d", currentFps_);
        DebugFont::drawText(renderer_, 4, 4, fpsText, 2, SDL_Color{255, 255, 255, 255});
    }

    SDL_RenderPresent(renderer_);
}

void Game::run() {
    Uint64 prevCounter = SDL_GetPerformanceCounter();
    const double frequency = static_cast<double>(SDL_GetPerformanceFrequency());
    double accumulator = 0.0;

    Uint32 fpsWindowStart = SDL_GetTicks();
    int fpsFrameCount = 0;

    while (running_) {
        const Uint64 now = SDL_GetPerformanceCounter();
        float frameTime = static_cast<float>((now - prevCounter) / frequency);
        prevCounter = now;
        if (frameTime > kMaxFrameTime) {
            frameTime = kMaxFrameTime;
        }
        accumulator += frameTime;

        input1_.beginFrame();
        input2_.beginFrame();
        processEvents();

        while (accumulator >= kFixedDt) {
            update(kFixedDt);
            accumulator -= kFixedDt;
        }

        render();

        ++fpsFrameCount;
        const Uint32 nowTicks = SDL_GetTicks();
        if (nowTicks - fpsWindowStart >= 500) {
            currentFps_ = static_cast<int>(fpsFrameCount * 1000.0 / (nowTicks - fpsWindowStart));
            fpsFrameCount = 0;
            fpsWindowStart = nowTicks;
        }
    }
}

void Game::shutdown() {
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
    IMG_Quit();
    SDL_Quit();
}
