#pragma once

#include <SDL_mixer.h>

#include <array>
#include <string>

// Efectos de sonido (seccion 8.2 de la especificacion, formato WAV). Cada
// valor es un EVENTO del juego, no un archivo -- varios eventos distintos
// comparten el mismo .wav cuando asi es en el original (misma logica que
// alla: reusan el mismo canal/muestra porque nunca pasan al mismo tiempo).
// Ver el comentario en Game::init() (donde se cargan) para el mapeo
// evento->archivo real y por que se eligio cada uno.
enum class Sfx {
    Jump,
    JumpSmall,
    Coin,
    Bump,
    Brick,
    Item,
    PowerUp,
    PowerDown,
    Pipe,
    StompKill,
    Kick,
    Death,
    OneUp,
    BowserFall,
    Fireball,
    BowserFire,
    BillFirework,
    Flagpole,
    Count,
};

// Envoltorio fino sobre SDL_mixer (RAII, mismo criterio que SpriteSheet):
// carga todos los efectos de una (ver Game::init) y los reproduce por id de
// evento con play(). Sin musica todavia (seccion 8.2 pide OGG en loop, no
// hay ninguna pista pedida/provista todavia).
class SoundManager {
public:
    ~SoundManager();
    SoundManager() = default;
    SoundManager(const SoundManager&) = delete;
    SoundManager& operator=(const SoundManager&) = delete;

    // Abre el dispositivo de audio (Mix_OpenAudio). Llamar una sola vez,
    // antes de load(), despues de SDL_Init(SDL_INIT_AUDIO).
    bool init();
    // Libera todo y cierra el dispositivo (Mix_CloseAudio) -- Game::shutdown()
    // la llama a mano ANTES de SDL_Quit() (el orden importa: si no, SDL_Quit
    // cierra el subsistema de audio con el mixer todavia abierto encima). El
    // destructor tambien la llama, por si el objeto se destruye sin pasar
    // por Game::shutdown() (ej. si init() nunca se completo del todo).
    void shutdown();
    bool load(Sfx id, const std::string& path);
    // No hace nada si ese efecto no se pudo cargar (falta el archivo, por
    // ejemplo) -- no es motivo para frenar el juego, solo se pierde el sonido.
    void play(Sfx id) const;

private:
    bool initialized_ = false;
    std::array<Mix_Chunk*, static_cast<size_t>(Sfx::Count)> chunks_{};
};
