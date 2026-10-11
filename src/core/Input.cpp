#include "Input.h"

#include <algorithm>
#include <cstdlib>
#include <vector>

namespace {

// Cada accion admite hasta dos scancodes. Player1 y Player2 no comparten
// ninguna tecla para poder jugar los dos en el mismo teclado a la vez.
const SDL_Scancode kBindingsPlayer1[static_cast<int>(Action::Count)][2] = {
    /* MoveLeft  */ {SDL_SCANCODE_A, SDL_SCANCODE_A},
    /* MoveRight */ {SDL_SCANCODE_D, SDL_SCANCODE_D},
    /* Jump      */ {SDL_SCANCODE_SPACE, SDL_SCANCODE_W},
    /* Run       */ {SDL_SCANCODE_LSHIFT, SDL_SCANCODE_LSHIFT},
    /* Down      */ {SDL_SCANCODE_S, SDL_SCANCODE_S},
};

const SDL_Scancode kBindingsPlayer2[static_cast<int>(Action::Count)][2] = {
    /* MoveLeft  */ {SDL_SCANCODE_LEFT, SDL_SCANCODE_LEFT},
    /* MoveRight */ {SDL_SCANCODE_RIGHT, SDL_SCANCODE_RIGHT},
    /* Jump      */ {SDL_SCANCODE_UP, SDL_SCANCODE_UP},
    /* Run       */ {SDL_SCANCODE_RSHIFT, SDL_SCANCODE_RSHIFT},
    /* Down      */ {SDL_SCANCODE_DOWN, SDL_SCANCODE_DOWN},
};

// Zona muerta del stick izquierdo (rango del eje: -32768..32767). Agacharse
// pide mas recorrido que caminar, para que inclinar el stick en diagonal al
// correr no lo haga agacharse sin querer.
constexpr Sint16 kStickMoveDeadzone = 12000;
constexpr Sint16 kStickDownDeadzone = 24000;

// Mandos abiertos, compartidos por todas las instancias de Input y en orden
// de conexion: el indice en este vector es el "slot" de cada jugador.
std::vector<SDL_GameController*> gControllers;
int gLastJoystickCount = -1;

// Cierra los mandos desconectados y abre los nuevos. SDL_NumJoysticks()
// refleja los hotplug procesados por el SDL_PumpEvents/SDL_PollEvent mas
// reciente, asi que alcanza con compararlo contra la ultima pasada para no
// recorrer la lista de dispositivos en cada frame.
void refreshControllers() {
    gControllers.erase(
        std::remove_if(gControllers.begin(), gControllers.end(),
                       [](SDL_GameController* controller) {
                           if (SDL_GameControllerGetAttached(controller)) return false;
                           SDL_GameControllerClose(controller);
                           return true;
                       }),
        gControllers.end());

    const int count = SDL_NumJoysticks();
    if (count == gLastJoystickCount) return;
    gLastJoystickCount = count;

    for (int i = 0; i < count; ++i) {
        if (!SDL_IsGameController(i)) continue;
        const SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(i);
        const bool alreadyOpen = std::any_of(
            gControllers.begin(), gControllers.end(), [id](SDL_GameController* controller) {
                return SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller)) == id;
            });
        if (alreadyOpen) continue;

        if (SDL_GameController* controller = SDL_GameControllerOpen(i)) {
            SDL_Log("Mando conectado: %s", SDL_GameControllerName(controller));
            gControllers.push_back(controller);
        }
    }
}

}  // namespace

Input::Input(ControlProfile profile)
    : profile_(profile), controllerSlot_(profile == ControlProfile::Player1 ? 0 : 1) {
    resetBindings();
}

void Input::resetBindings() {
    const auto& defaults = profile_ == ControlProfile::Player1 ? kBindingsPlayer1 : kBindingsPlayer2;
    for (size_t i = 0; i < bindings_.size(); ++i) bindings_[i] = {defaults[i][0], defaults[i][1]};
    padJumpButton_ = SDL_CONTROLLER_BUTTON_A;
    padRunButton_ = SDL_CONTROLLER_BUTTON_X;
}

void Input::setKeyBinding(Action action, SDL_Scancode key) {
    bindings_[static_cast<size_t>(action)] = {key, key};
}

void Input::setPadButtons(SDL_GameControllerButton jump, SDL_GameControllerButton run) {
    padJumpButton_ = jump;
    padRunButton_ = run;
}

void Input::rumble(Uint16 strength, Uint32 durationMs) {
    if (controllerSlot_ >= static_cast<int>(gControllers.size())) return;
    SDL_GameControllerRumble(gControllers[controllerSlot_], strength, strength, durationMs);
}

void Input::beginFrame() {
    prevState_ = currState_;

    SDL_PumpEvents();
    refreshControllers();

    currState_.fill(false);
    if (scripted_) {
        currState_ = scriptedState_;
    } else {
        readKeyboard(currState_);
        readController(currState_);
    }

    for (size_t i = 0; i < currState_.size(); ++i) {
        if (currState_[i] && !prevState_[i]) {
            latchedPress_[i] = true;
        }
    }
}

void Input::readKeyboard(ActionState& state) const {
    const Uint8* keyboard = SDL_GetKeyboardState(nullptr);
    for (size_t i = 0; i < state.size(); ++i) {
        const auto& keys = bindings_[i];
        state[i] = state[i] || keyboard[keys[0]] || keyboard[keys[1]];
    }
}

void Input::readController(ActionState& state) const {
    if (controllerSlot_ >= static_cast<int>(gControllers.size())) return;
    SDL_GameController* controller = gControllers[controllerSlot_];

    auto button = [controller](SDL_GameControllerButton b) {
        return SDL_GameControllerGetButton(controller, b) != 0;
    };
    const Sint16 stickX = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
    const Sint16 stickY = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);

    auto set = [&state](Action action, bool value) {
        bool& slot = state[static_cast<size_t>(action)];
        slot = slot || value;
    };
    set(Action::MoveLeft, button(SDL_CONTROLLER_BUTTON_DPAD_LEFT) || stickX < -kStickMoveDeadzone);
    set(Action::MoveRight, button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || stickX > kStickMoveDeadzone);
    set(Action::Down, button(SDL_CONTROLLER_BUTTON_DPAD_DOWN) ||
                          (stickY > kStickDownDeadzone && std::abs(stickY) > std::abs(stickX)));
    set(Action::Jump, button(padJumpButton_));
    set(Action::Run, button(padRunButton_));
}

bool Input::down(Action action) const {
    return currState_[static_cast<size_t>(action)];
}

bool Input::consumePress(Action action) {
    bool& latch = latchedPress_[static_cast<size_t>(action)];
    const bool value = latch;
    latch = false;
    return value;
}

void Input::clearPresses() {
    latchedPress_.fill(false);
}

void Input::setScripted(bool active, std::initializer_list<Action> held) {
    scripted_ = active;
    scriptedState_.fill(false);
    for (Action action : held) scriptedState_[static_cast<size_t>(action)] = true;
    latchedPress_.fill(false);
}
