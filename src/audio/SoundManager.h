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
    HurryUp,
    TimeTally,
    GameOver,
    Pause,
    Vine,
    Count,
};

// Musica (seccion 8.2: OGG en loop), assets/sound/music/*.ogg generados con
// tools/asset_scripts/gen_music.py. Los temas que se repiten traen las
// etiquetas LOOPSTART/LOOPLENGTH que SDL_mixer respeta: la intro suena una
// vez y despues repite exacto el ciclo. Los *Fast son la version acelerada
// de cuando queda poco tiempo.
enum class Music {
    Overworld,
    Underground,
    Underwater,
    Castle,
    Invincibility,
    OverworldFast,
    UndergroundFast,
    UnderwaterFast,
    CastleFast,
    InvincibilityFast,
    LevelComplete,
    CastleComplete,
    GameOver,
    IntoPipe,
    Ending,
    Count,
    None = Count,
};

// Envoltorio fino sobre SDL_mixer (RAII, mismo criterio que SpriteSheet):
// carga todos los efectos y la musica de una (ver Game::init), reproduce los
// efectos por id de evento con play() y un tema de musica a la vez con
// playMusic().
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
    // Volumen de todos los efectos, 0 (mudo) a MIX_MAX_VOLUME (128), ver
    // la opcion de volumen del menu de configuracion.
    void setVolume(int volume);

    bool loadMusic(Music id, const std::string& path);
    // loop: repite (respetando el punto de loop del archivo) o suena una vez.
    // No hace nada si ya esta sonando ese mismo tema.
    void playMusic(Music id, bool loop);
    void stopMusic();
    void pauseMusic();
    void resumeMusic();
    // El tema que se pidio por ultima vez (None si se freno o termino uno
    // que no repite).
    Music currentMusic() const;
    void setMusicVolume(int volume);  // 0 a MIX_MAX_VOLUME (128)

private:
    bool initialized_ = false;
    std::array<Mix_Chunk*, static_cast<size_t>(Sfx::Count)> chunks_{};
    std::array<Mix_Music*, static_cast<size_t>(Music::Count)> music_{};
    Music currentMusic_ = Music::None;
};
