// Musica del juego (seccion 8.2): que tema suena en cada momento, como el
// original. Parte de la clase Game, en un archivo aparte igual que los menus.
#include "Game.h"

namespace {

// Duracion de hurryup.wav: la musica rapida arranca cuando termina el aviso.
constexpr float kHurryJingleDuration = 2.9f;

}  // namespace

void Game::loadMusic() {
    const std::string dir = "assets/sound/music/";
    const std::pair<Music, const char*> tracks[] = {
        {Music::Overworld, "overworld"},
        {Music::Underground, "underground"},
        {Music::Underwater, "underwater"},
        {Music::Castle, "castle"},
        {Music::Invincibility, "invincibility"},
        {Music::OverworldFast, "overworld_fast"},
        {Music::UndergroundFast, "underground_fast"},
        {Music::UnderwaterFast, "underwater_fast"},
        {Music::CastleFast, "castle_fast"},
        {Music::InvincibilityFast, "invincibility_fast"},
        {Music::LevelComplete, "level_complete"},
        {Music::CastleComplete, "castle_complete"},
        {Music::GameOver, "game_over"},
        {Music::IntoPipe, "into_pipe"},
        {Music::Ending, "ending"},
    };
    // Si falta algun archivo el juego sigue andando, sin ese tema.
    for (const auto& [id, name] : tracks) audio_.loadMusic(id, dir + name + ".ogg");
}

Music Game::levelMusic() const {
    // Con estrella suena la invencibilidad (cualquiera de los dos jugadores).
    const bool star = mario_.isInvincible() || (twoPlayerMode_ && luigi_.isInvincible());
    if (star) return hurryMusic_ ? Music::InvincibilityFast : Music::Invincibility;
    switch (worldType_) {
        case WorldType::Underground: return hurryMusic_ ? Music::UndergroundFast : Music::Underground;
        case WorldType::Underwater: return hurryMusic_ ? Music::UnderwaterFast : Music::Underwater;
        case WorldType::Castle: return hurryMusic_ ? Music::CastleFast : Music::Castle;
        case WorldType::Overworld:
        default: return hurryMusic_ ? Music::OverworldFast : Music::Overworld;
    }
}

void Game::resetLevelMusic() {
    audio_.stopMusic();
    hurryMusic_ = timeRemaining_ <= 100.0f;
    hurryJingleTimer_ = 0.0f;
    endJinglePlayed_ = false;
}

void Game::startHurryMusic() {
    // Como el original: se corta el tema, suena el aviso y despues vuelve
    // el mismo tema acelerado.
    hurryMusic_ = true;
    hurryJingleTimer_ = kHurryJingleDuration;
    audio_.stopMusic();
}

void Game::updateMusic(float dt) {
    // Castillo: silencio mientras cae el puente; despues, el tema de
    // castillo completo (una vez).
    if (bridgeCollapsing_) {
        audio_.stopMusic();
        return;
    }
    if (castleClearTimer_ > 0.0f || (levelEnding_ && !flagpole_.isLowering())) {
        if (!endJinglePlayed_ && !bridgeCollapsing_ && castleClearTimer_ > 0.0f) {
            endJinglePlayed_ = true;
            audio_.playMusic(Music::CastleComplete, false);
        }
        return;
    }
    // Bandera: silencio mientras baja (suena flagpole.wav), y cuando llega
    // abajo, nivel completo (una vez).
    if (levelEnding_) {
        if (!flagpole_.isFlagDown()) {
            audio_.stopMusic();
        } else if (!endJinglePlayed_) {
            endJinglePlayed_ = true;
            audio_.playMusic(Music::LevelComplete, false);
        }
        return;
    }
    // Murieron todos: se corta (suena death.wav).
    const bool everyoneDying =
        mario_.isInDeathSequence() && (!twoPlayerMode_ || luigi_.isInDeathSequence());
    if (everyoneDying) {
        audio_.stopMusic();
        return;
    }
    if (hurryJingleTimer_ > 0.0f) {
        hurryJingleTimer_ -= dt;
        return;
    }
    // Normal: el tema del tipo de mundo actual (cambia solo al pasar a un
    // area con otro mundo) o la estrella.
    audio_.playMusic(levelMusic(), true);
}

const char* Game::musicName(Music music) {
    switch (music) {
        case Music::Overworld: return "SUPERFICIE";
        case Music::Underground: return "SUBTERRANEO";
        case Music::Underwater: return "AGUA";
        case Music::Castle: return "CASTILLO";
        case Music::Invincibility: return "ESTRELLA";
        case Music::OverworldFast: return "SUPERFICIE RAPIDA";
        case Music::UndergroundFast: return "SUBTERRANEO RAPIDA";
        case Music::UnderwaterFast: return "AGUA RAPIDA";
        case Music::CastleFast: return "CASTILLO RAPIDA";
        case Music::InvincibilityFast: return "ESTRELLA RAPIDA";
        case Music::LevelComplete: return "NIVEL COMPLETO";
        case Music::CastleComplete: return "CASTILLO COMPLETO";
        case Music::GameOver: return "GAME OVER";
        case Music::IntoPipe: return "TUBERIA";
        case Music::Ending: return "FINAL";
        default: return "-";
    }
}
