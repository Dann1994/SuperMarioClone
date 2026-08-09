#include "Input.h"

#include <cstring>

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

}  // namespace

Input::Input(ControlProfile profile)
    : bindings_(profile == ControlProfile::Player1 ? kBindingsPlayer1 : kBindingsPlayer2) {}

void Input::beginFrame() {
    prevState_ = currState_;

    SDL_PumpEvents();
    const Uint8* state = SDL_GetKeyboardState(nullptr);
    std::memcpy(currState_.data(), state, currState_.size());

    for (int i = 0; i < static_cast<int>(Action::Count); ++i) {
        const Action action = static_cast<Action>(i);
        if (anyScancodeDown(currState_, action) && !anyScancodeDown(prevState_, action)) {
            latchedPress_[i] = true;
        }
    }
}

bool Input::anyScancodeDown(const std::array<Uint8, SDL_NUM_SCANCODES>& state, Action action) const {
    const auto& keys = bindings_[static_cast<int>(action)];
    return state[keys[0]] || state[keys[1]];
}

bool Input::down(Action action) const {
    return anyScancodeDown(currState_, action);
}

bool Input::consumePress(Action action) {
    bool& latch = latchedPress_[static_cast<size_t>(action)];
    const bool value = latch;
    latch = false;
    return value;
}
