#pragma once

#include <SDL.h>
#include <array>

enum class Action {
    MoveLeft,
    MoveRight,
    Jump,
    Run,
    Down,
    Count
};

// Perfil de control: que set de teclas usa esta instancia de Input. Pensado
// para el co-op local de a dos en el mismo teclado; el remapeo real por
// usuario (seccion 9 de la especificacion) es un paso posterior.
enum class ControlProfile {
    Player1,
    Player2,
};

// Estado de teclado abstraido en acciones logicas, con su propio set de
// teclas segun el ControlProfile.
//
// consumePress() usa un latch: una vez detectado el flanco de "apretar" queda
// guardado hasta que alguien lo consume, en vez de perderse si esa pasada de
// beginFrame() no coincide con un paso de fisica de tamano fijo (el juego
// corre la logica en pasos fijos de 1/60s, que no siempre calzan 1 a 1 con
// cada frame real, sobre todo en monitores de mas de 60Hz).
class Input {
public:
    explicit Input(ControlProfile profile = ControlProfile::Player1);

    void beginFrame();

    bool down(Action action) const;
    bool consumePress(Action action);

private:
    bool anyScancodeDown(const std::array<Uint8, SDL_NUM_SCANCODES>& state, Action action) const;

    const SDL_Scancode (*bindings_)[2];
    std::array<Uint8, SDL_NUM_SCANCODES> prevState_{};
    std::array<Uint8, SDL_NUM_SCANCODES> currState_{};
    std::array<bool, static_cast<size_t>(Action::Count)> latchedPress_{};
};
