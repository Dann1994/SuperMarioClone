// Menus del juego (seccion 10 de la especificacion): titulo con el menu
// principal, pausa, configuracion (+ controles), creditos y fin de la demo,
// y la partida que encadena los niveles. Parte de la clase Game, en un
// archivo aparte para no seguir agrandando Game.cpp.
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>

#include "../render/DebugFont.h"
#include "EditorLauncher.h"
#include "Game.h"

#include "../entities/Difficulty.h"

namespace {

const SDL_Color kMenuWhite{255, 255, 255, 255};
const SDL_Color kMenuYellow{252, 216, 88, 255};
const SDL_Color kMenuGray{120, 120, 140, 255};
const SDL_Color kMenuDim{180, 180, 200, 255};

// Opciones del menu principal, en orden.
// Menu principal (anidado): JUGAR abre el submenu de partidas.
enum MainItem { kMainPlay, kMainSettings, kMainCredits, kMainQuit, kMainCount };
// Submenu JUGAR.
enum TitleItem { kTitleOnePlayer, kTitleTwoPlayers, kTitleContinue, kTitleExtras, kTitleCount };
// Submenu EXTRAS.
enum ExtrasItem { kExtrasWorldSelect, kExtrasNewGamePlus, kExtrasEditor, kExtrasOptions, kExtrasCount };

// Que se puede elegir en EXTRAS: todo se desbloquea al terminar el juego una
// vez, salvo ELEGIR MUNDO (abierto mientras estamos en pruebas). OPCIONES
// EXTRA todavia no tiene nada adentro.
std::vector<bool> extrasDisabled(bool completed, bool editorAvailable) {
    std::vector<bool> disabled(kExtrasCount, false);
    disabled[kExtrasNewGamePlus] = !completed;
    disabled[kExtrasEditor] = !completed || !editorAvailable;
    disabled[kExtrasOptions] = true;
    return disabled;
}
// Submenu EDITOR DE NIVELES.
enum EditorItem { kEditorCreate, kEditorSelect, kEditorCount };
// Que menu del titulo se ve (titleMenu_).
enum TitleMenu { kMenuMain, kMenuPlay, kMenuExtras, kMenuEditor };
// Opciones del menu de pausa.
enum PauseItem { kPauseResume, kPauseRestart, kPauseOptions, kPauseQuit, kPauseCount };
// Opciones del menu de configuracion.
enum SettingsItem { kSetDisplay, kSetWindowSize, kSetAspect, kSetFps, kSetMaster, kSetMusic, kSetSfx,
                    kSetLanguage, kSetVibration, kSetPlayerBump, kSetControls, kSetBack, kSetCount };
// Filas del menu de controles: 5 acciones x 2 jugadores, 2 botones del
// mando, restaurar, volver.
constexpr int kActionCount = 6;
constexpr int kCtrlPadJump = kActionCount * 2;
constexpr int kCtrlPadRun = kCtrlPadJump + 1;
constexpr int kCtrlReset = kCtrlPadRun + 1;
constexpr int kCtrlBack = kCtrlReset + 1;
constexpr int kCtrlCount = kCtrlBack + 1;

// Indices de decorationSprites_ (mismo orden que kDecorations en Game.cpp).
constexpr int kDecoHillBig = 1;
constexpr int kDecoHillSmall = 2;
constexpr int kDecoBushBig = 3;

std::string upper(std::string text) {
    for (char& c : text) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return text;
}

Txt actionName(int action) {
    static const Txt kNames[kActionCount] = {Txt::Left, Txt::Right, Txt::Jump, Txt::Run, Txt::Down, Txt::ClimbUp};
    return kNames[action];
}

std::string volumeBar(int value) {
    // "[#####.....]" -- la fuente no tiene un bloque lleno, '#' alcanza.
    return "[" + std::string(static_cast<size_t>(value), '#') + std::string(static_cast<size_t>(10 - value), '.') + "]";
}

}  // namespace

// ============================================================ estado

bool Game::isMenuState() const {
    return appState_ == AppState::Title || appState_ == AppState::Paused || appState_ == AppState::Settings ||
           appState_ == AppState::Controls || appState_ == AppState::Credits || appState_ == AppState::DemoEnd;
}

void Game::saveTopScore() {
    if (score_ <= settings_.topScore) return;
    settings_.topScore = score_;
    settings_.save();
}

void Game::openTitle() {
    saveTopScore();
    choosingCharacter_ = false;
    choosingLevel_ = false;
    choosingTwoMode_ = false;
    confirmingQuit_ = false;
    titleMenu_ = 0;
    gPrimaryHardMode = false;   // modo prueba y niveles sueltos: siempre la primera vuelta
    audio_.stopMusic();
    cutscene_ = false;
    pipeTransitionTimer_ = 0.0f;
    input1_.setScripted(false);
    input2_.setScripted(false);
    appState_ = AppState::Title;
    campaign_ = false;
    waitingBinding_ = false;
}

void Game::openScenarioSelect() {
    setPlayAsLuigi(false);
    appState_ = AppState::ScenarioSelect;
    campaign_ = false;
}

void Game::openPause() {
    appState_ = AppState::Paused;
    pauseSelection_ = kPauseResume;
    audio_.pauseMusic();
    audio_.play(Sfx::Pause);
}

void Game::openSettings(AppState returnTo) {
    settingsReturnState_ = returnTo;
    settingsSelection_ = 0;
    appState_ = AppState::Settings;
}

// ============================================================ entrada

Game::MenuInput Game::menuInputFromEvent(const SDL_Event& event) {
    if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.scancode) {
            case SDL_SCANCODE_UP: case SDL_SCANCODE_W: return MenuInput::Up;
            case SDL_SCANCODE_DOWN: case SDL_SCANCODE_S: return MenuInput::Down;
            case SDL_SCANCODE_LEFT: case SDL_SCANCODE_A: return MenuInput::Left;
            case SDL_SCANCODE_RIGHT: case SDL_SCANCODE_D: return MenuInput::Right;
            case SDL_SCANCODE_RETURN: case SDL_SCANCODE_SPACE: case SDL_SCANCODE_KP_ENTER:
                return event.key.repeat ? MenuInput::None : MenuInput::Confirm;
            case SDL_SCANCODE_ESCAPE: case SDL_SCANCODE_BACKSPACE:
                return event.key.repeat ? MenuInput::None : MenuInput::Back;
            default: return MenuInput::None;
        }
    }
    if (event.type == SDL_CONTROLLERBUTTONDOWN) {
        switch (event.cbutton.button) {
            case SDL_CONTROLLER_BUTTON_DPAD_UP: return MenuInput::Up;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return MenuInput::Down;
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return MenuInput::Left;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return MenuInput::Right;
            case SDL_CONTROLLER_BUTTON_A: return MenuInput::Confirm;
            case SDL_CONTROLLER_BUTTON_B: case SDL_CONTROLLER_BUTTON_BACK: return MenuInput::Back;
            // Start: confirma en los menus, y en la pausa la cierra (Resume
            // es la primera opcion y la que queda marcada al abrirla).
            case SDL_CONTROLLER_BUTTON_START:
                return appState_ == AppState::Paused ? MenuInput::Back : MenuInput::Confirm;
            default: return MenuInput::None;
        }
    }
    if (event.type == SDL_CONTROLLERAXISMOTION) {
        // El stick genera un evento por cada cambio minimo: un paso por
        // inclinacion, hasta que vuelve al centro (igual que en el selector).
        constexpr int kPress = 24000;
        constexpr int kRelease = 8000;
        const int value = event.caxis.value;
        if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
            if (std::abs(value) < kRelease) menuStickHeld_ = false;
            if (std::abs(value) > kPress && !menuStickHeld_) {
                menuStickHeld_ = true;
                return value < 0 ? MenuInput::Up : MenuInput::Down;
            }
        } else if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX) {
            if (std::abs(value) < kRelease) menuStickHeldX_ = false;
            if (std::abs(value) > kPress && !menuStickHeldX_) {
                menuStickHeldX_ = true;
                return value < 0 ? MenuInput::Left : MenuInput::Right;
            }
        }
    }
    return MenuInput::None;
}

void Game::handleMenuEvent(const SDL_Event& event) {
    // Reasignando un control: la proxima tecla/boton es el nuevo.
    if (waitingBinding_) {
        const bool padRow = controlsSelection_ == kCtrlPadJump || controlsSelection_ == kCtrlPadRun;
        if (event.type == SDL_KEYDOWN && !event.key.repeat) {
            const SDL_Scancode key = event.key.keysym.scancode;
            if (key == SDL_SCANCODE_ESCAPE) {
                waitingBinding_ = false;
                return;
            }
            if (!padRow) {
                const int player = controlsSelection_ / kActionCount;
                const int action = controlsSelection_ % kActionCount;
                settings_.keys[player][action] = static_cast<int>(key);
                waitingBinding_ = false;
                applySettings();
                audio_.play(Sfx::Coin);
            }
        } else if (event.type == SDL_CONTROLLERBUTTONDOWN && padRow) {
            if (controlsSelection_ == kCtrlPadJump) settings_.padJumpButton = event.cbutton.button;
            if (controlsSelection_ == kCtrlPadRun) settings_.padRunButton = event.cbutton.button;
            waitingBinding_ = false;
            applySettings();
            audio_.play(Sfx::Coin);
        }
        return;
    }

    const MenuInput input = menuInputFromEvent(event);
    if (input == MenuInput::None) return;
    switch (appState_) {
        case AppState::Title: handleTitleInput(input); break;
        case AppState::Paused: handlePauseInput(input); break;
        case AppState::Settings: handleSettingsInput(input); break;
        case AppState::Controls: handleControlsInput(input); break;
        case AppState::Credits:
            if (input == MenuInput::Confirm || input == MenuInput::Back) openTitle();
            break;
        case AppState::DemoEnd:
            if (input == MenuInput::Confirm || input == MenuInput::Back) openTitle();
            break;
        default: break;
    }
}

namespace {

// Mueve la seleccion salteando las opciones deshabilitadas.
int stepSelection(int current, int delta, int count, const std::vector<bool>& disabled) {
    int next = current;
    for (int i = 0; i < count; ++i) {
        next = (next + delta + count) % count;
        if (next >= static_cast<int>(disabled.size()) || !disabled[next]) return next;
    }
    return current;
}

std::vector<bool> titleDisabled(bool canContinue) {
    // CONTINUAR solo despues de un GAME OVER en la partida.
    std::vector<bool> disabled(kTitleCount, false);
    disabled[kTitleContinue] = !canContinue;
    return disabled;
}

}  // namespace

void Game::handleTitleInput(MenuInput input) {
    if (confirmingQuit_) {
        // "Esta seguro que quiere cerrar el juego?": SI / NO (arranca en NO).
        if (input == MenuInput::Left || input == MenuInput::Right || input == MenuInput::Up ||
            input == MenuInput::Down) {
            quitChoice_ = 1 - quitChoice_;
            audio_.play(Sfx::TimeTally);
        } else if (input == MenuInput::Confirm) {
            if (quitChoice_ == 0) {
                running_ = false;
            } else {
                confirmingQuit_ = false;
                audio_.play(Sfx::Bump);
            }
        } else if (input == MenuInput::Back) {
            confirmingQuit_ = false;
            audio_.play(Sfx::Bump);
        }
        return;
    }
    if (choosingTwoMode_) {
        // 2 jugadores: a la vez (los dos en pantalla) o por turnos (el original).
        if (input == MenuInput::Left || input == MenuInput::Right || input == MenuInput::Up ||
            input == MenuInput::Down) {
            twoModeChoice_ = 1 - twoModeChoice_;
            audio_.play(Sfx::TimeTally);
        } else if (input == MenuInput::Confirm) {
            choosingTwoMode_ = false;
            audio_.play(Sfx::Coin);
            if (twoModeChoice_ == 0) {
                startCampaign(true);
            } else {
                startTurnCampaign();
            }
        } else if (input == MenuInput::Back) {
            choosingTwoMode_ = false;
        }
        return;
    }
    if (choosingLevel_) {
        // Arriba/abajo cambia de mundo, izquierda/derecha de nivel (pasa al
        // mundo de al lado al dar la vuelta).
        int index = (startWorld_ - 1) * 4 + (startLevel_ - 1);
        if (input == MenuInput::Up) index -= 4;
        if (input == MenuInput::Down) index += 4;
        if (input == MenuInput::Left) index -= 1;
        if (input == MenuInput::Right) index += 1;
        if (input == MenuInput::Up || input == MenuInput::Down || input == MenuInput::Left ||
            input == MenuInput::Right) {
            index = (index % 32 + 32) % 32;
            startWorld_ = index / 4 + 1;
            startLevel_ = index % 4 + 1;
            audio_.play(Sfx::TimeTally);
        } else if (input == MenuInput::Confirm) {
            choosingLevel_ = false;
            choosingCharacter_ = true;
            characterChoice_ = playAsLuigi_ ? 1 : 0;
            audio_.play(Sfx::Coin);
        } else if (input == MenuInput::Back) {
            choosingLevel_ = false;
        }
        return;
    }
    if (choosingCharacter_) {
        // 1 jugador: elegir Mario o Luigi.
        if (input == MenuInput::Left || input == MenuInput::Right || input == MenuInput::Up ||
            input == MenuInput::Down) {
            characterChoice_ = 1 - characterChoice_;
            audio_.play(Sfx::TimeTally);
        } else if (input == MenuInput::Confirm) {
            choosingCharacter_ = false;
            audio_.play(Sfx::Coin);
            startCampaign(false, characterChoice_ == 1);
        } else if (input == MenuInput::Back) {
            choosingCharacter_ = false;
        }
        return;
    }
    // Atras: al menu de arriba (en el principal no hace nada).
    if (input == MenuInput::Back) {
        if (titleMenu_ == kMenuMain) return;
        titleMenu_ = titleMenu_ == kMenuEditor ? kMenuExtras : titleMenu_ == kMenuExtras ? kMenuPlay : kMenuMain;
        audio_.play(Sfx::Bump);
        return;
    }
    if (titleMenu_ == kMenuMain) {
        // Menu principal: JUGAR, CONFIGURACION, CREDITOS, SALIR.
        if (input == MenuInput::Up || input == MenuInput::Down) {
            mainSelection_ = (mainSelection_ + (input == MenuInput::Up ? kMainCount - 1 : 1)) % kMainCount;
            audio_.play(Sfx::TimeTally);
            return;
        }
        if (input != MenuInput::Confirm) return;
        audio_.play(Sfx::Coin);
        switch (mainSelection_) {
            case kMainPlay:
                titleMenu_ = kMenuPlay;
                titleSelection_ = kTitleOnePlayer;
                break;
            case kMainSettings: openSettings(AppState::Title); break;
            case kMainCredits: appState_ = AppState::Credits; break;
            case kMainQuit:
                confirmingQuit_ = true;
                quitChoice_ = 1;   // NO: que no se cierre sin querer
                break;
            default: break;
        }
        return;
    }
    if (titleMenu_ == kMenuExtras) {
        // EXTRAS: ELEGIR MUNDO, EDITOR DE NIVELES (gris si no hay editor).
        if (input == MenuInput::Up || input == MenuInput::Down) {
            extrasSelection_ = stepSelection(extrasSelection_, input == MenuInput::Up ? -1 : 1, kExtrasCount,
                                             extrasDisabled(settings_.gameCompleted, EditorLauncher::available()));
            audio_.play(Sfx::TimeTally);
            return;
        }
        if (input != MenuInput::Confirm) return;
        if (extrasDisabled(settings_.gameCompleted, EditorLauncher::available())[extrasSelection_]) return;
        audio_.play(Sfx::Coin);
        switch (extrasSelection_) {
            case kExtrasWorldSelect:
                secondQuestNext_ = false;
                choosingLevel_ = true;
                break;
            case kExtrasNewGamePlus:
                // La segunda vuelta del original: los niveles mas dificiles.
                secondQuestNext_ = true;
                startWorld_ = 1;
                startLevel_ = 1;
                choosingCharacter_ = true;
                characterChoice_ = playAsLuigi_ ? 1 : 0;
                break;
            case kExtrasEditor:
                titleMenu_ = kMenuEditor;
                editorSelection_ = kEditorCreate;
                break;
            default: break;
        }
        return;
    }
    if (titleMenu_ == kMenuEditor) {
        // EDITOR DE NIVELES: CREAR (nivel nuevo) o SELECCIONAR (la lista).
        if (input == MenuInput::Up || input == MenuInput::Down) {
            editorSelection_ = (editorSelection_ + 1) % kEditorCount;
            audio_.play(Sfx::TimeTally);
            return;
        }
        if (input != MenuInput::Confirm) return;
        if (EditorLauncher::launch(editorSelection_ == kEditorSelect)) {
            audio_.play(Sfx::Coin);
            // El juego se minimiza mientras se edita y vuelve al cerrar el editor.
            editorLaunched_ = true;
            SDL_MinimizeWindow(window_);
        } else {
            audio_.play(Sfx::Bump);
        }
        return;
    }
    // Submenu JUGAR.
    if (input == MenuInput::Up || input == MenuInput::Down) {
        titleSelection_ = stepSelection(titleSelection_, input == MenuInput::Up ? -1 : 1, kTitleCount, titleDisabled(continueWorld_ > 0));
        audio_.play(Sfx::TimeTally);
        return;
    }
    if (input != MenuInput::Confirm) return;
    audio_.play(Sfx::Coin);
    switch (titleSelection_) {
        case kTitleOnePlayer:
            secondQuestNext_ = false;
            startWorld_ = 1;
            startLevel_ = 1;
            choosingCharacter_ = true;
            characterChoice_ = playAsLuigi_ ? 1 : 0;
            break;
        case kTitleTwoPlayers:
            secondQuestNext_ = false;
            startWorld_ = 1;
            startLevel_ = 1;
            choosingTwoMode_ = true;
            twoModeChoice_ = 0;
            break;
        case kTitleContinue:
            if (continueWorld_ <= 0) break;
            startWorld_ = continueWorld_;
            startLevel_ = 1;
            startCampaign(continueTwoPlayers_, continueAsLuigi_);
            break;
        case kTitleExtras:
            titleMenu_ = kMenuExtras;
            extrasSelection_ = kExtrasWorldSelect;
            break;
        default: break;
    }
}

void Game::handlePauseInput(MenuInput input) {
    if (input == MenuInput::Up || input == MenuInput::Down) {
        pauseSelection_ = (pauseSelection_ + (input == MenuInput::Up ? kPauseCount - 1 : 1)) % kPauseCount;
        audio_.play(Sfx::TimeTally);
        return;
    }
    if (input == MenuInput::Back) {
        appState_ = AppState::Playing;
        audio_.resumeMusic();
        audio_.play(Sfx::Pause);
        return;
    }
    if (input != MenuInput::Confirm) return;
    switch (pauseSelection_) {
        case kPauseResume:
            appState_ = AppState::Playing;
            audio_.resumeMusic();
            audio_.play(Sfx::Pause);
            break;
        case kPauseRestart:
            // Reinicia el nivel desde el principio, sin perder vidas.
            audio_.play(Sfx::Coin);
            checkpointCol_ = -1;
            loadScenario(scenario_);
            break;
        case kPauseOptions:
            audio_.play(Sfx::Coin);
            openSettings(AppState::Paused);
            break;
        case kPauseQuit:
            audio_.play(Sfx::Coin);
            openTitle();
            break;
        default: break;
    }
    // Lo apretado en el menu no cuenta al volver a jugar.
    input1_.clearPresses();
    input2_.clearPresses();
}

void Game::changeSetting(int index, int direction) {
    switch (index) {
        case kSetDisplay: settings_.fullscreen = !settings_.fullscreen; break;
        case kSetWindowSize: settings_.windowScale = (settings_.windowScale + direction + 7) % 7; break;  // AUTO,1..6
        case kSetAspect: settings_.widescreen = !settings_.widescreen; break;
        case kSetFps: settings_.showFps = !settings_.showFps; break;
        case kSetMaster: settings_.masterVolume = std::clamp(settings_.masterVolume + direction, 0, 10); break;
        case kSetMusic: settings_.musicVolume = std::clamp(settings_.musicVolume + direction, 0, 10); break;
        case kSetSfx: settings_.sfxVolume = std::clamp(settings_.sfxVolume + direction, 0, 10); break;
        case kSetLanguage: settings_.english = !settings_.english; break;
        case kSetVibration: settings_.vibration = !settings_.vibration; break;
        case kSetPlayerBump: settings_.playerCollision = !settings_.playerCollision; break;
        default: return;
    }
    applySettings();
    if (index == kSetVibration && settings_.vibration) input1_.rumble(0x8000, 200);  // para sentir como queda
}

void Game::handleSettingsInput(MenuInput input) {
    if (input == MenuInput::Up || input == MenuInput::Down) {
        settingsSelection_ = (settingsSelection_ + (input == MenuInput::Up ? kSetCount - 1 : 1)) % kSetCount;
        audio_.play(Sfx::TimeTally);
        return;
    }
    if (input == MenuInput::Back || (input == MenuInput::Confirm && settingsSelection_ == kSetBack)) {
        settings_.save();
        audio_.play(Sfx::Bump);
        appState_ = settingsReturnState_;
        return;
    }
    if (input == MenuInput::Confirm && settingsSelection_ == kSetControls) {
        audio_.play(Sfx::Coin);
        controlsSelection_ = 0;
        appState_ = AppState::Controls;
        return;
    }
    if (input == MenuInput::Left || input == MenuInput::Right || input == MenuInput::Confirm) {
        changeSetting(settingsSelection_, input == MenuInput::Left ? -1 : 1);
        audio_.play(Sfx::Coin);
    }
}

void Game::handleControlsInput(MenuInput input) {
    if (input == MenuInput::Up || input == MenuInput::Down) {
        controlsSelection_ = (controlsSelection_ + (input == MenuInput::Up ? kCtrlCount - 1 : 1)) % kCtrlCount;
        audio_.play(Sfx::TimeTally);
        return;
    }
    if (input == MenuInput::Back || (input == MenuInput::Confirm && controlsSelection_ == kCtrlBack)) {
        settings_.save();
        audio_.play(Sfx::Bump);
        appState_ = AppState::Settings;
        return;
    }
    if (input != MenuInput::Confirm) return;
    audio_.play(Sfx::Coin);
    if (controlsSelection_ == kCtrlReset) {
        for (auto& player : settings_.keys) player.fill(-1);
        settings_.padJumpButton = -1;
        settings_.padRunButton = -1;
        applySettings();
        return;
    }
    waitingBinding_ = true;
}

// ============================================================ configuracion

void Game::applyWindowSize() {
    const bool fullscreen = (SDL_GetWindowFlags(window_) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
    if (fullscreen) return;
    int scale = settings_.windowScale;
    if (scale <= 0) {
        // Automatico: el multiplo entero mas grande que entre en ~85% del escritorio.
        SDL_Rect usable;
        scale = 3;
        if (SDL_GetDisplayUsableBounds(0, &usable) == 0) {
            scale = std::max(1, std::min(static_cast<int>(usable.w * 0.85f) / gLogicalWidth,
                                         static_cast<int>(usable.h * 0.85f) / kLogicalHeight));
        }
    }
    SDL_SetWindowSize(window_, gLogicalWidth * scale, kLogicalHeight * scale);
    SDL_SetWindowPosition(window_, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
}

void Game::applySettings() {
    setEnglish(settings_.english);
    characterCollisionEnabled_ = settings_.playerCollision;

    // Proporcion: cambia el ancho logico (la camara se reacomoda sola).
    const int width = settings_.widescreen ? kWideLogicalWidth : kClassicWidth;
    const bool widthChanged = width != gLogicalWidth;
    gLogicalWidth = width;
    SDL_RenderSetLogicalSize(renderer_, gLogicalWidth, kLogicalHeight);
    cameraSnapPending_ = true;

    const bool fullscreen = (SDL_GetWindowFlags(window_) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
    if (fullscreen != settings_.fullscreen) toggleFullscreen();

    // El tamano de ventana solo se toca si cambio algo que lo afecta (no
    // pisar una ventana que el jugador agrando a mano cada vez que se abre
    // el menu).
    static int lastScale = -1;
    if (widthChanged || lastScale != settings_.windowScale) {
        if (lastScale != -1 || settings_.windowScale > 0 || widthChanged) applyWindowSize();
        lastScale = settings_.windowScale;
    }

    audio_.setVolume(MIX_MAX_VOLUME * settings_.masterVolume * settings_.sfxVolume / 100);
    audio_.setMusicVolume(MIX_MAX_VOLUME * settings_.masterVolume * settings_.musicVolume / 100);

    // Controles: los de fabrica y encima los reasignados.
    for (int player = 0; player < 2; ++player) {
        Input& input = player == 0 ? input1_ : input2_;
        input.resetBindings();
        for (int action = 0; action < kActionCount; ++action) {
            const int key = settings_.keys[player][action];
            if (key >= 0) input.setKeyBinding(static_cast<Action>(action), static_cast<SDL_Scancode>(key));
        }
        const auto jump = settings_.padJumpButton >= 0 ? static_cast<SDL_GameControllerButton>(settings_.padJumpButton)
                                                       : SDL_CONTROLLER_BUTTON_A;
        const auto run = settings_.padRunButton >= 0 ? static_cast<SDL_GameControllerButton>(settings_.padRunButton)
                                                     : SDL_CONTROLLER_BUTTON_X;
        input.setPadButtons(jump, run);
    }
}

void Game::stompRumble(const Mario& character) {
    // Pisar un enemigo: una vibracion leve y corta en el mando de quien piso.
    if (!settings_.vibration) return;
    (&character == &luigi_ ? input2_ : input1_).rumble(kStompRumbleStrength, kStompRumbleMs);
}

void Game::updateRumble() {
    const bool marioHurt = mario_.isInDamageSequence() || mario_.isInDeathSequence();
    const bool luigiHurt = twoPlayerMode_ && (luigi_.isInDamageSequence() || luigi_.isInDeathSequence());
    if (settings_.vibration) {
        if (marioHurt && !marioWasHurt_) input1_.rumble(0xC000, mario_.isInDeathSequence() ? 500 : 250);
        if (luigiHurt && !luigiWasHurt_) input2_.rumble(0xC000, luigi_.isInDeathSequence() ? 500 : 250);
    }
    marioWasHurt_ = marioHurt;
    luigiWasHurt_ = luigiHurt;
}

// ============================================================ partida

std::string Game::campaignLevelPath(int world, int level) {
    char path[64];
    std::snprintf(path, sizeof(path), "levels/world%d/%d-%d.json", world, world, level);
    return path;
}

void Game::setPlayAsLuigi(bool luigi) {
    playAsLuigi_ = luigi;
    if (luigi != appearanceSwapped_) {
        mario_.swapAppearance(luigi_);
        appearanceSwapped_ = luigi;
    }
}

void Game::startTurnCampaign() {
    startCampaign(false, false);
    turnMode_ = true;
    TurnState fresh;
    fresh.lives = livesCount_;
    fresh.world = campaignWorld_;
    fresh.level = campaignLevel_;
    turnStates_ = {fresh, fresh};
    turnPlayer_ = 0;
}

void Game::startCampaign(bool twoPlayers, bool asLuigi) {
    turnMode_ = false;
    if (inputsSwapped_) {
        std::swap(input1_, input2_);
        inputsSwapped_ = false;
    }
    gPrimaryHardMode = secondQuestNext_;
    hidden1UpFlag_ = true;
    continueWorld_ = 0;   // una partida nueva (o la continuada) gasta el CONTINUAR
    setPlayAsLuigi(!twoPlayers && asLuigi);
    twoPlayerMode_ = twoPlayers;
    campaignWorld_ = startWorld_;
    campaignLevel_ = startLevel_;
    customLevelPath_ = campaignLevelPath(campaignWorld_, campaignLevel_);
    customStartCol_ = -1;
    startScenario(TestScenario::CustomLevel);
    campaign_ = true;
}

void Game::onLevelCleared() {
    if (!campaign_) {
        // Modo prueba vuelve a su selector; un nivel suelto (--level, el
        // editor), al menu principal.
        if (scenario_ == TestScenario::CustomLevel) {
            openTitle();
        } else {
            openScenarioSelect();
        }
        return;
    }
    // Termino un x-3 con suficientes monedas: el 1-UP escondido del x-1
    // siguiente va a estar (Hidden1UpCoinAmts).
    static const int kHidden1UpCoins[8] = {21, 35, 22, 27, 23, 24, 35, 99};
    if (campaignLevel_ == 3 && campaignWorld_ >= 1 && campaignWorld_ <= 8 &&
        coinTallyForOneUps_ >= kHidden1UpCoins[campaignWorld_ - 1]) {
        hidden1UpFlag_ = true;
    }
    // Siguiente nivel del mundo (4 por mundo, como el original), o el
    // primero del mundo siguiente. Si no existe todavia: fin de la demo.
    int world = campaignWorld_;
    int level = campaignLevel_ + 1;
    if (level > 4) {
        ++world;
        level = 1;
    }
    const std::string next = campaignLevelPath(world, level);
    if (!std::filesystem::exists(next)) {
        // Despues del 8-4 (WorldSelectEnableFlag del original): queda
        // desbloqueado lo de EXTRAS (PARTIDA + es la segunda vuelta).
        if (campaignWorld_ >= 8 && !settings_.gameCompleted) {
            settings_.gameCompleted = true;
            settings_.save();
        }
        appState_ = AppState::DemoEnd;
        audio_.playMusic(Music::Ending, false);
        return;
    }
    campaignWorld_ = world;
    campaignLevel_ = level;
    customLevelPath_ = next;
    checkpointCol_ = -1;
    loadScenario(TestScenario::CustomLevel);  // conserva puntaje, monedas y vidas
}

// ============================================================ dibujo

void Game::drawCenteredFit(int y, const std::string& text, SDL_Color color) {
    // Con la fuente del juego; si no entra en un renglon, en dos (cortando
    // en el espacio mas cercano a la mitad).
    if (static_cast<int>(text.size()) * 8 <= gLogicalWidth - 8) {
        drawCentered(y, text, 2, color);
        return;
    }
    size_t cut = std::string::npos;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] != ' ') continue;
        const auto dist = [&](size_t p) { return p > text.size() / 2 ? p - text.size() / 2 : text.size() / 2 - p; };
        if (cut == std::string::npos || dist(i) < dist(cut)) cut = i;
    }
    if (cut == std::string::npos) {
        drawCentered(y, text, 2, color);
        return;
    }
    std::string first = text.substr(0, cut);
    std::string second = text.substr(cut + 1);
    while (!second.empty() && second.front() == ' ') second.erase(second.begin());
    while (!first.empty() && first.back() == ' ') first.pop_back();
    drawCentered(y - 5, first, 2, color);
    drawCentered(y + 5, second, 2, color);
}

void Game::drawCentered(int y, const std::string& text, int scale, SDL_Color color) {
    const int width = static_cast<int>(text.size()) * 4 * scale;
    DebugFont::drawText(renderer_, (gLogicalWidth - width) / 2, y, text, scale, color);
}

void Game::drawMenuList(int x, int y, int step, const std::vector<std::string>& items, int selection,
                        const std::vector<bool>& disabled) {
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        const bool off = i < static_cast<int>(disabled.size()) && disabled[i];
        const bool selected = i == selection;
        if (selected) DebugFont::drawText(renderer_, x - 12, y + i * step, ">", 2, kMenuWhite);
        DebugFont::drawText(renderer_, x, y + i * step, items[i], 2,
                            off ? kMenuGray : selected ? kMenuWhite : kMenuYellow);
    }
}

void Game::renderFpsCorner() {
    if (!settings_.showFps) return;
    char text[16];
    std::snprintf(text, sizeof(text), "FPS %d", currentFps_);
    DebugFont::drawText(renderer_, gLogicalWidth - static_cast<int>(std::strlen(text)) * 4 - 3, 3, text, 1,
                        kMenuWhite);
}

void Game::renderTitle() {
    // Como la pantalla de titulo original (ver gen_title.py): el HUD
    // arriba, el cartel, el (c), las opciones con el hongo de cursor y el
    // record (TOP-), sobre el principio del 1-1.
    const int worldRow = static_cast<int>(WorldType::Overworld);
    const SDL_Color sky = backgroundColorFor(WorldType::Overworld);
    SDL_SetRenderDrawColor(renderer_, sky.r, sky.g, sky.b, 255);
    SDL_RenderFillRect(renderer_, nullptr);
    for (int x = 0; x < gLogicalWidth; x += 16) {
        terrainSprite_.draw(renderer_, worldRow, 0, x, 208, false);
        terrainSprite_.draw(renderer_, worldRow, 0, x, 224, false);
    }
    decorationSprites_[kDecoHillBig].draw(renderer_, 0, 0, 0, 160, false);
    decorationSprites_[kDecoBushBig].draw(renderer_, 0, 0, gLogicalWidth - 96, 192, false);
    renderHud();

    const int logoX = (gLogicalWidth - 176) / 2;
    titleLogoSprite_.draw(renderer_, 0, 0, logoX, 32, false);
    const SDL_Color pink{252, 188, 176, 255};
    DebugFont::drawText(renderer_, logoX + 176 - 14 * 8, 122, "@1985 NINTENDO", 2, pink);

    // Menu anidado en una columna: el principal (JUGAR, CONFIGURACION,
    // CREDITOS, SALIR), JUGAR, EXTRAS o EDITOR DE NIVELES.
    {
        std::vector<Txt> ids;
        std::vector<bool> disabled;
        int selection = 0;
        if (titleMenu_ == kMenuPlay) {
            ids = {Txt::OnePlayer, Txt::TwoPlayers, Txt::Continue, Txt::Extras};
            disabled = titleDisabled(continueWorld_ > 0);
            selection = titleSelection_;
        } else if (titleMenu_ == kMenuExtras) {
            ids = {Txt::WorldSelect, Txt::NewGamePlus, Txt::LevelEditor, Txt::ExtraOptions};
            disabled = extrasDisabled(settings_.gameCompleted, EditorLauncher::available());
            selection = extrasSelection_;
        } else if (titleMenu_ == kMenuEditor) {
            ids = {Txt::EditorCreate, Txt::EditorSelect};
            disabled.assign(ids.size(), false);
            selection = editorSelection_;
        } else {
            ids = {Txt::Play, Txt::Options, Txt::Credits, Txt::Quit};
            disabled.assign(ids.size(), false);
            selection = mainSelection_;
        }
        int widest = 0;
        for (Txt id : ids) widest = std::max(widest, static_cast<int>(std::string(tr(id)).size()) * 8);
        const int x = (gLogicalWidth - widest) / 2;
        const int step = 14;
        for (int i = 0; i < static_cast<int>(ids.size()); ++i) {
            const int y = 134 + i * step;
            if (i == selection) titleCursorSprite_.draw(renderer_, 0, 0, x - 12, y, false);
            DebugFont::drawText(renderer_, x, y, tr(ids[static_cast<size_t>(i)]), 2,
                                disabled[static_cast<size_t>(i)] ? kMenuGray : kMenuWhite);
        }
    }

    if (secondQuestNext_) {
        DebugFont::drawText(renderer_, logoX, 122, tr(Txt::SecondQuest), 2, pink);
    }
    char top[24];
    std::snprintf(top, sizeof(top), "TOP- %06d", std::min(std::max(settings_.topScore, score_), 999999));
    drawCentered(196, top, 2, kMenuWhite);

    // Los personajes parados en el piso, como el original: Mario siempre,
    // Luigi tambien si esta marcada la opcion de 2 jugadores. Eligiendo
    // personaje para 1 jugador, solo el elegido.
    const int heroX = 40;
    const int heroY = 208 - 16;
    if (choosingCharacter_) {
        (characterChoice_ == 1 ? introLuigiSprite_ : introMarioSprite_).draw(renderer_, 0, 0, heroX, heroY, false);
    } else {
        introMarioSprite_.draw(renderer_, 0, 0, heroX, heroY, false);
        if (titleMenu_ == kMenuPlay && titleSelection_ == kTitleTwoPlayers) {
            introLuigiSprite_.draw(renderer_, 0, 0, heroX + 20, heroY, false);
        }
    }

    if (choosingTwoMode_) {
        const int boxW = 168;
        const SDL_Rect box{(gLogicalWidth - boxW) / 2, 132, boxW, 60};
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 230);
        SDL_RenderFillRect(renderer_, &box);
        SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer_, &box);
        drawCentered(140, tr(Txt::TwoPlayerMode), 2, kMenuWhite);
        const int optionX = box.x + 28;
        DebugFont::drawText(renderer_, optionX, 160, tr(Txt::Simultaneous), 2, kMenuWhite);
        DebugFont::drawText(renderer_, optionX, 174, tr(Txt::TakeTurns), 2, kMenuWhite);
        titleCursorSprite_.draw(renderer_, 0, 0, optionX - 12, twoModeChoice_ == 0 ? 160 : 174, false);
    }

    if (choosingLevel_) {
        const int boxW = 168;
        const SDL_Rect box{(gLogicalWidth - boxW) / 2, 132, boxW, 60};
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 230);
        SDL_RenderFillRect(renderer_, &box);
        SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer_, &box);
        drawCentered(140, tr(Txt::ChooseLevel), 2, kMenuWhite);
        char label[16];
        std::snprintf(label, sizeof(label), "< %d-%d >", startWorld_, startLevel_);
        drawCentered(162, label, 2, kMenuWhite);
    }

    if (confirmingQuit_) {
        const int boxW = 200;
        const SDL_Rect box{(gLogicalWidth - boxW) / 2, 128, boxW, 66};
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 230);
        SDL_RenderFillRect(renderer_, &box);
        SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer_, &box);
        drawCentered(136, tr(Txt::QuitConfirm1), 2, kMenuWhite);
        drawCentered(148, tr(Txt::QuitConfirm2), 2, kMenuWhite);
        const int yesX = gLogicalWidth / 2 - 48;
        const int noX = gLogicalWidth / 2 + 24;
        DebugFont::drawText(renderer_, yesX, 172, tr(Txt::Yes), 2, kMenuWhite);
        DebugFont::drawText(renderer_, noX, 172, tr(Txt::No), 2, kMenuWhite);
        titleCursorSprite_.draw(renderer_, 0, 0, (quitChoice_ == 0 ? yesX : noX) - 12, 172, false);
    }

    if (choosingCharacter_) {
        // Cartel para elegir con quien jugar 1 jugador.
        const int boxW = 168;
        const SDL_Rect box{(gLogicalWidth - boxW) / 2, 132, boxW, 60};
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 230);
        SDL_RenderFillRect(renderer_, &box);
        SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer_, &box);
        drawCentered(140, tr(Txt::ChooseCharacter), 2, kMenuWhite);
        const int marioX = gLogicalWidth / 2 - 60;
        const int luigiX = gLogicalWidth / 2 + 12;
        introMarioSprite_.draw(renderer_, 0, 0, marioX + 16, 158, false);
        introLuigiSprite_.draw(renderer_, 0, 0, luigiX + 16, 158, false);
        DebugFont::drawText(renderer_, marioX, 178, "MARIO", 2, SDL_Color{248, 56, 0, 255});
        DebugFont::drawText(renderer_, luigiX, 178, "LUIGI", 2, SDL_Color{0, 200, 0, 255});
        titleCursorSprite_.draw(renderer_, 0, 0, (characterChoice_ == 0 ? marioX : luigiX) - 12, 178, false);
    }
}

void Game::renderPauseMenu() {
    // Velo oscuro sobre el juego congelado + caja con las opciones.
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 150);
    SDL_RenderFillRect(renderer_, nullptr);
    const SDL_Rect box{(gLogicalWidth - 180) / 2, 70, 180, 100};
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 220);
    SDL_RenderFillRect(renderer_, &box);
    SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer_, &box);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
    drawCentered(78, tr(Txt::Paused), 2, kMenuWhite);
    std::vector<std::string> items{tr(Txt::Resume), tr(Txt::RestartLevel), tr(Txt::Options), tr(Txt::QuitToMenu)};
    drawMenuList(box.x + 24, 100, 15, items, pauseSelection_);
}

void Game::renderSettings() {
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderFillRect(renderer_, nullptr);
    drawCentered(12, tr(Txt::Options), 2, kMenuWhite);

    std::vector<std::string> labels;
    std::vector<std::string> values;
    auto onOff = [](bool value) { return std::string(tr(value ? Txt::On : Txt::Off)); };
    labels.push_back(tr(Txt::Display));
    values.push_back(tr(settings_.fullscreen ? Txt::Fullscreen : Txt::Windowed));
    labels.push_back(tr(Txt::WindowSize));
    values.push_back(settings_.windowScale == 0 ? std::string(tr(Txt::Auto)) : "x" + std::to_string(settings_.windowScale));
    labels.push_back(tr(Txt::AspectRatio));
    values.push_back(settings_.widescreen ? "16:9" : "4:3");
    labels.push_back(tr(Txt::ShowFps));
    values.push_back(onOff(settings_.showFps));
    labels.push_back(tr(Txt::MasterVolume));
    values.push_back(volumeBar(settings_.masterVolume));
    labels.push_back(tr(Txt::MusicVolume));
    values.push_back(volumeBar(settings_.musicVolume));
    labels.push_back(tr(Txt::SfxVolume));
    values.push_back(volumeBar(settings_.sfxVolume));
    labels.push_back(tr(Txt::Language));
    values.push_back(tr(Txt::LanguageName));
    labels.push_back(tr(Txt::Vibration));
    values.push_back(onOff(settings_.vibration));
    labels.push_back(tr(Txt::PlayerBump));
    values.push_back(onOff(settings_.playerCollision));
    labels.push_back(tr(Txt::Controls));
    values.push_back(">");
    labels.push_back(tr(Txt::Back));
    values.push_back("");

    // Dos columnas con la fuente del juego: nombre a la izquierda, valor
    // alineado a la derecha.
    const int left = 22;
    const int rightEdge = gLogicalWidth - 12;
    for (int i = 0; i < kSetCount; ++i) {
        const int y = 32 + i * 15;
        const bool selected = i == settingsSelection_;
        const SDL_Color color = selected ? kMenuWhite : kMenuYellow;
        if (selected) titleCursorSprite_.draw(renderer_, 0, 0, left - 14, y, false);
        DebugFont::drawText(renderer_, left, y, labels[i], 2, color);
        const int valueWidth = static_cast<int>(values[i].size()) * 8;
        DebugFont::drawText(renderer_, rightEdge - valueWidth, y, values[i], 2, color);
    }
    drawCenteredFit(200, tr(Txt::ValueHint), kMenuDim);
    drawCenteredFit(220, tr(Txt::MenuHint), kMenuDim);
}

void Game::renderControls() {
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderFillRect(renderer_, nullptr);
    drawCentered(6, tr(Txt::Controls), 2, kMenuWhite);

    // Con la fuente del juego: nombre a la izquierda, tecla a la derecha.
    const int left = 22;
    const int rightEdge = gLogicalWidth - 12;
    int y = 22;
    auto row = [&](int index, const std::string& label, const std::string& value) {
        const bool selected = index == controlsSelection_;
        const SDL_Color color = selected ? kMenuWhite : kMenuYellow;
        if (selected) titleCursorSprite_.draw(renderer_, 0, 0, left - 14, y, false);
        DebugFont::drawText(renderer_, left, y, label, 2, color);
        std::string shown = value;
        if (selected && waitingBinding_) shown = "...";
        DebugFont::drawText(renderer_, rightEdge - static_cast<int>(shown.size()) * 8, y, shown, 2, color);
        y += 10;
    };
    for (int player = 0; player < 2; ++player) {
        DebugFont::drawText(renderer_, left, y, tr(player == 0 ? Txt::Player1 : Txt::Player2), 2, kMenuDim);
        y += 10;
        const Input& input = player == 0 ? input1_ : input2_;
        for (int action = 0; action < kActionCount; ++action) {
            const SDL_Scancode key = input.keyBinding(static_cast<Action>(action));
            row(player * kActionCount + action, std::string("  ") + tr(actionName(action)), upper(SDL_GetScancodeName(key)));
        }
        y += 2;
    }
    DebugFont::drawText(renderer_, left, y, tr(Txt::Gamepad), 2, kMenuDim);
    y += 10;
    const char* jump = SDL_GameControllerGetStringForButton(input1_.padJumpButton());
    const char* run = SDL_GameControllerGetStringForButton(input1_.padRunButton());
    row(kCtrlPadJump, std::string("  ") + tr(Txt::Jump), upper(jump ? jump : "?"));
    row(kCtrlPadRun, std::string("  ") + tr(Txt::Run), upper(run ? run : "?"));
    y += 2;
    row(kCtrlReset, tr(Txt::ResetDefaults), "");
    row(kCtrlBack, tr(Txt::Back), "");

    const bool padRow = controlsSelection_ == kCtrlPadJump || controlsSelection_ == kCtrlPadRun;
    if (waitingBinding_) {
        drawCenteredFit(222, tr(padRow ? Txt::PressButton : Txt::PressKey), kMenuWhite);
    } else {
        drawCenteredFit(222, tr(Txt::BindHint), kMenuDim);
    }
}

void Game::renderCredits() {
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderFillRect(renderer_, nullptr);
    // Con la fuente del juego (escala 2, 8 px por letra): renglones de
    // hasta 30 letras para que entren tambien en la pantalla clasica.
    drawCentered(16, "SUPER MARIO CLONE", 2, kMenuWhite);
    const char* const lines[] = {
        "CLON EXPERIMENTAL DE",
        "SUPER MARIO BROS. (NINTENDO,",
        "1985), SIN FINES DE LUCRO",
        "",
        "DISENO Y DESARROLLO",
        "DANIEL VERGARA",
        "",
        "PROGRAMACION ASISTIDA POR",
        "CLAUDE (ANTHROPIC)",
        "",
        "SPRITES: SUPERJUSTINBROS",
        "SONIDOS: SUPER MARIO BROS.",
        "",
        "C++ / SDL2",
    };
    int y = 40;
    for (const char* line : lines) {
        drawCentered(y, line, 2, kMenuYellow);
        y += 12;
    }
    drawCentered(220, tr(Txt::MenuHint), 1, kMenuDim);
}

void Game::renderDemoEnd() {
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderFillRect(renderer_, nullptr);
    renderHud();
    // Despues del 8-4 es el final de verdad, no el de la demo.
    if (campaignWorld_ == 8 && campaignLevel_ == 4) {
        drawCentered(70, tr(Txt::QuestOver1), 2, kMenuWhite);
        drawCentered(95, tr(Txt::QuestOver2), 2, kMenuWhite);
        drawCentered(140, tr(Txt::TheEnd), 3, kMenuYellow);
        drawCentered(220, tr(Txt::MenuHint), 1, kMenuDim);
        return;
    }
    drawCentered(90, tr(Txt::EndOfDemo), 3, kMenuWhite);
    drawCentered(130, tr(Txt::ThanksForPlaying), 2, kMenuYellow);
    drawCentered(220, tr(Txt::MenuHint), 1, kMenuDim);
}
