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

// Estado de teclado + mando abstraido en acciones logicas, con su propio set
// de teclas segun el ControlProfile.
//
// Mando: se detectan solos al conectarse/desconectarse (hotplug), en orden de
// conexion -- el primero es de Player1, el segundo de Player2. Se leen con la
// API SDL_GameController, que normaliza cualquier mando conocido al layout de
// Xbox: A salta, X corre/tira fuego, cruceta o stick izquierdo para moverse y
// agacharse. El teclado sigue andando a la par del mando.
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
    // Descarta las apretadas pendientes: al confirmar un menu con A/Espacio
    // (que tambien son "saltar") el personaje no tiene que arrancar saltando.
    void clearPresses();

    // Reasignar controles (menu de configuracion, seccion 10.2): tecla de
    // cada accion (reemplaza las dos de fabrica) y botones del mando para
    // saltar/correr (los de moverse son siempre cruceta/stick).
    SDL_Scancode keyBinding(Action action) const { return bindings_[static_cast<size_t>(action)][0]; }
    void setKeyBinding(Action action, SDL_Scancode key);
    SDL_GameControllerButton padJumpButton() const { return padJumpButton_; }
    SDL_GameControllerButton padRunButton() const { return padRunButton_; }
    void setPadButtons(SDL_GameControllerButton jump, SDL_GameControllerButton run);
    void resetBindings();
    // Vibracion del mando de este jugador (si tiene uno conectado).
    void rumble(Uint16 strength, Uint32 durationMs);

private:
    using ActionState = std::array<bool, static_cast<size_t>(Action::Count)>;

    void readKeyboard(ActionState& state) const;
    void readController(ActionState& state) const;

    using Bindings = std::array<std::array<SDL_Scancode, 2>, static_cast<size_t>(Action::Count)>;
    ControlProfile profile_;
    Bindings bindings_{};
    SDL_GameControllerButton padJumpButton_ = SDL_CONTROLLER_BUTTON_A;
    SDL_GameControllerButton padRunButton_ = SDL_CONTROLLER_BUTTON_X;
    int controllerSlot_;
    ActionState prevState_{};
    ActionState currState_{};
    ActionState latchedPress_{};
};
