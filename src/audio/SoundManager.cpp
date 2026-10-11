#include "SoundManager.h"

#include <SDL.h>

SoundManager::~SoundManager() {
    shutdown();
}

void SoundManager::shutdown() {
    if (initialized_) Mix_HaltMusic();
    for (Mix_Music*& track : music_) {
        if (track != nullptr) {
            Mix_FreeMusic(track);
            track = nullptr;
        }
    }
    for (Mix_Chunk*& chunk : chunks_) {
        if (chunk != nullptr) {
            Mix_FreeChunk(chunk);
            chunk = nullptr;
        }
    }
    if (initialized_) {
        Mix_CloseAudio();
        initialized_ = false;
    }
}

bool SoundManager::init() {
    // 44.1kHz estereo, mismo formato que traen los .wav (seccion 8.2): con
    // 512 de buffer alcanza para SFX cortos sin notarse el delay.
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 512) != 0) {
        SDL_Log("Mix_OpenAudio fallo: %s", Mix_GetError());
        return false;
    }
    // Un canal FIJO por evento (ver play()) en vez de dejar que SDL_mixer
    // elija cualquiera libre: asi, si el mismo evento se dispara de nuevo
    // mientras el anterior todavia esta sonando (ej. un caparazon matando
    // a dos enemigos casi en el mismo instante), el segundo PISA al primero
    // en su mismo canal en vez de sumarse en uno nuevo -- sin esto sonaban
    // superpuestos/como eco, cosa que no pasa en el original (ahi cada
    // sonido tiene un solo canal de sonido cuadrado, nunca se puede solapar
    // con si mismo).
    Mix_AllocateChannels(static_cast<int>(Sfx::Count));
    initialized_ = true;
    return true;
}

bool SoundManager::load(Sfx id, const std::string& path) {
    Mix_Chunk* chunk = Mix_LoadWAV(path.c_str());
    if (chunk == nullptr) {
        SDL_Log("No se pudo cargar el sonido %s: %s", path.c_str(), Mix_GetError());
        return false;
    }
    chunks_[static_cast<size_t>(id)] = chunk;
    return true;
}

void SoundManager::play(Sfx id) const {
    Mix_Chunk* chunk = chunks_[static_cast<size_t>(id)];
    if (chunk == nullptr) return;
    // Canal fijo = el propio id (ver el comentario en init()): corta el
    // anterior si todavia estaba sonando en vez de solaparse con el mismo.
    Mix_PlayChannel(static_cast<int>(id), chunk, 0);
}

void SoundManager::setVolume(int volume) {
    if (!initialized_) return;
    Mix_Volume(-1, volume);  // -1 = todos los canales (uno por efecto)
}

bool SoundManager::loadMusic(Music id, const std::string& path) {
    if (!initialized_) return false;
    Mix_Music* track = Mix_LoadMUS(path.c_str());
    if (track == nullptr) {
        SDL_Log("No se pudo cargar la musica %s: %s", path.c_str(), Mix_GetError());
        return false;
    }
    music_[static_cast<size_t>(id)] = track;
    return true;
}

void SoundManager::playMusic(Music id, bool loop) {
    if (!initialized_ || id == Music::None) return;
    if (id == currentMusic_ && Mix_PlayingMusic()) return;
    Mix_Music* track = music_[static_cast<size_t>(id)];
    currentMusic_ = id;
    if (track == nullptr) return;
    Mix_PlayMusic(track, loop ? -1 : 0);
}

void SoundManager::stopMusic() {
    if (!initialized_) return;
    Mix_HaltMusic();
    // Si estaba en pausa (menu de pausa), que el proximo tema no arranque
    // pausado.
    Mix_ResumeMusic();
    currentMusic_ = Music::None;
}

void SoundManager::pauseMusic() {
    if (initialized_) Mix_PauseMusic();
}

void SoundManager::resumeMusic() {
    if (initialized_) Mix_ResumeMusic();
}

Music SoundManager::currentMusic() const {
    if (!initialized_ || !Mix_PlayingMusic()) return Music::None;
    return currentMusic_;
}

void SoundManager::setMusicVolume(int volume) {
    if (initialized_) Mix_VolumeMusic(volume);
}
