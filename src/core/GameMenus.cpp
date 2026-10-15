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
#include "Game.h"
#include "../editor/LevelEditor.h"

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
enum TitleMenu { kMenuMain, kMenuPlay, kMenuExtras, kMenuEditor, kMenuPlus };
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

bool Game::enterEditor(EditorEntry entry) {
    if (!editor_) {
        editor_ = std::make_unique<LevelEditor>();
        char* base = SDL_GetPrefPath("SuperMarioClone", "SuperMarioClone");
        const std::string dir = base ? std::string(base) + "levels" : std::string("user_levels");
        SDL_free(base);
        if (!editor_->initEmbedded(window_, renderer_, dir)) {
            editor_.reset();
            audio_.play(Sfx::Bump);
            return false;
        }
    }
    if (entry == EditorEntry::New) editor_->startNew();
    if (entry == EditorEntry::Pick) editor_->openPicker();
    audio_.stopMusic();
    const LevelEditor::EmbeddedResult result = editor_->runEmbedded();
    input1_.clearPresses();
    input2_.clearPresses();
    if (result.kind != LevelEditor::EmbeddedResult::Kind::Play) return false;
    // PROBAR: se juega el nivel y al salir (o perder) se vuelve al editor.
    editorReturn_ = true;
    campaign_ = false;
    currentSlot_ = -1;
    startWithLevelFile(result.path, result.startCol, -1);
    return true;
}

void Game::openTitle() {
    if (editorReturn_) {
        editorReturn_ = false;
        if (enterEditor(EditorEntry::Resume)) return;
    }
    saveTopScore();
    choosingCharacter_ = false;
    choosingLevel_ = false;
    choosingTwoMode_ = false;
    confirmingQuit_ = false;
    choosingSlot_ = false;
    confirmingOverwrite_ = false;
    slotActions_ = false;
    confirmingDelete_ = false;
    worldSelectFlow_ = false;
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

int Game::continueWorld() const {
    // Solo el GAME OVER de esta sesion (las guardadas se eligen de a una).
    return continueWorld_;
}

bool Game::anySaveSlot() const {
    for (const Settings::SaveSlot& slot : settings_.saves) {
        if (slot.world > 0) return true;
    }
    return false;
}

void Game::askSlotThen(std::function<void()> start) {
    // Partida nueva: en que espacio se guarda (arranca en el primero vacio).
    pendingStart_ = std::move(start);
    choosingSlot_ = true;
    slotForContinue_ = false;
    confirmingOverwrite_ = false;
    slotChoice_ = 0;
    for (int i = 0; i < Settings::kSaveSlots; ++i) {
        if (settings_.saves[static_cast<size_t>(i)].world == 0) {
            slotChoice_ = i;
            break;
        }
    }
    refreshSlotPreviews();
}

void Game::openContinueSlots() {
    choosingSlot_ = true;
    slotForContinue_ = true;
    confirmingOverwrite_ = false;
    slotChoice_ = 0;
    for (int i = 0; i < Settings::kSaveSlots; ++i) {
        if (settings_.saves[static_cast<size_t>(i)].world > 0) {
            slotChoice_ = i;
            break;
        }
    }
    refreshSlotPreviews();
}

void Game::writeCurrentSlot(int world, int level) {
    if (currentSlot_ < 0 || currentSlot_ >= Settings::kSaveSlots) return;
    Settings::SaveSlot& slot = settings_.saves[static_cast<size_t>(currentSlot_)];
    slot.world = world;
    slot.level = level;
    slot.mode = turnMode_ ? "turns" : twoPlayerMode_ ? "coop" : "solo";
    slot.luigi = playAsLuigi_;
    slot.secondQuest = secondQuestNext_;
    settings_.save();
}

void Game::handleTitleInput(MenuInput input) {
    if (confirmingDelete_) {
        if (input == MenuInput::Left || input == MenuInput::Right || input == MenuInput::Up ||
            input == MenuInput::Down) {
            deleteChoice_ = 1 - deleteChoice_;
            audio_.play(Sfx::TimeTally);
        } else if (input == MenuInput::Confirm && deleteChoice_ == 0) {
            // Borrada: el espacio queda vacio.
            confirmingDelete_ = false;
            slotActions_ = false;
            settings_.saves[static_cast<size_t>(slotChoice_)] = Settings::SaveSlot{};
            settings_.save();
            if (currentSlot_ == slotChoice_) currentSlot_ = -1;
            audio_.play(Sfx::Bump);
            if (!anySaveSlot()) {
                choosingSlot_ = false;   // no queda nada para continuar
            } else {
                openContinueSlots();
            }
        } else if (input == MenuInput::Confirm || input == MenuInput::Back) {
            confirmingDelete_ = false;
            audio_.play(Sfx::Bump);
        }
        return;
    }
    if (slotActions_) {
        if (input == MenuInput::Left || input == MenuInput::Right || input == MenuInput::Up ||
            input == MenuInput::Down) {
            slotActionChoice_ = 1 - slotActionChoice_;
            audio_.play(Sfx::TimeTally);
        } else if (input == MenuInput::Back) {
            slotActions_ = false;
            audio_.play(Sfx::Bump);
        } else if (input == MenuInput::Confirm) {
            if (slotActionChoice_ == 1) {
                confirmingDelete_ = true;
                deleteChoice_ = 1;   // NO: que no se borre sin querer
                audio_.play(Sfx::TimeTally);
                return;
            }
            slotActions_ = false;
            const Settings::SaveSlot slot = settings_.saves[static_cast<size_t>(slotChoice_)];
            choosingSlot_ = false;
            audio_.play(Sfx::Coin);
            currentSlot_ = slotChoice_;
            startWorld_ = slot.world;
            startLevel_ = slot.level;
            secondQuestNext_ = slot.secondQuest;
            if (slot.mode == "turns") {
                startTurnCampaign();
            } else {
                startCampaign(slot.mode == "coop", slot.luigi);
            }
        }
        return;
    }
    if (confirmingOverwrite_) {
        if (input == MenuInput::Left || input == MenuInput::Right || input == MenuInput::Up ||
            input == MenuInput::Down) {
            overwriteChoice_ = 1 - overwriteChoice_;
            audio_.play(Sfx::TimeTally);
        } else if (input == MenuInput::Confirm && overwriteChoice_ == 0) {
            confirmingOverwrite_ = false;
            choosingSlot_ = false;
            audio_.play(Sfx::Coin);
            currentSlot_ = slotChoice_;
            if (pendingStart_) pendingStart_();
            writeCurrentSlot(startWorld_, startLevel_);
        } else if (input == MenuInput::Confirm || input == MenuInput::Back) {
            confirmingOverwrite_ = false;
            audio_.play(Sfx::Bump);
        }
        return;
    }
    if (choosingSlot_) {
        const auto filled = [&](int i) { return settings_.saves[static_cast<size_t>(i)].world > 0; };
        if (input == MenuInput::Up || input == MenuInput::Down) {
            // Para CONTINUAR, solo los espacios con partida.
            const int delta = input == MenuInput::Up ? -1 : 1;
            int next = slotChoice_;
            for (int i = 0; i < Settings::kSaveSlots; ++i) {
                next = (next + delta + Settings::kSaveSlots) % Settings::kSaveSlots;
                if (!slotForContinue_ || filled(next)) break;
            }
            slotChoice_ = next;
            audio_.play(Sfx::TimeTally);
        } else if (input == MenuInput::Back) {
            choosingSlot_ = false;
            audio_.play(Sfx::Bump);
        } else if (input == MenuInput::Confirm) {
            if (slotForContinue_) {
                if (!filled(slotChoice_)) return;
                // CONTINUAR o BORRAR esta partida.
                slotActions_ = true;
                slotActionChoice_ = 0;
                audio_.play(Sfx::TimeTally);
            } else if (filled(slotChoice_)) {
                confirmingOverwrite_ = true;   // ya tiene una partida: preguntar
                overwriteChoice_ = 1;
                audio_.play(Sfx::TimeTally);
            } else {
                choosingSlot_ = false;
                audio_.play(Sfx::Coin);
                currentSlot_ = slotChoice_;
                if (pendingStart_) pendingStart_();
                writeCurrentSlot(startWorld_, startLevel_);
            }
        }
        return;
    }
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
            const bool together = twoModeChoice_ == 0;
            askSlotThen([this, together] {
                if (together) {
                    startCampaign(true);
                } else {
                    startTurnCampaign();
                }
            });
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
            worldSelectFlow_ = true;
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
            const bool luigi = characterChoice_ == 1;
            if (worldSelectFlow_) {
                // ELEGIR MUNDO: para probar, no se guarda en ningun espacio.
                worldSelectFlow_ = false;
                currentSlot_ = -1;
                startCampaign(false, luigi);
            } else {
                askSlotThen([this, luigi] { startCampaign(false, luigi); });
            }
        } else if (input == MenuInput::Back) {
            choosingCharacter_ = false;
            worldSelectFlow_ = false;
        }
        return;
    }
    // Atras: al menu de arriba (en el principal no hace nada).
    if (input == MenuInput::Back) {
        if (titleMenu_ == kMenuMain) return;
        titleMenu_ = (titleMenu_ == kMenuEditor || titleMenu_ == kMenuPlus) ? kMenuExtras
                     : titleMenu_ == kMenuExtras                            ? kMenuPlay
                                                                            : kMenuMain;
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
                                             extrasDisabled(settings_.gameCompleted, true));
            audio_.play(Sfx::TimeTally);
            return;
        }
        if (input != MenuInput::Confirm) return;
        if (extrasDisabled(settings_.gameCompleted, true)[extrasSelection_]) return;
        audio_.play(Sfx::Coin);
        switch (extrasSelection_) {
            case kExtrasWorldSelect:
                secondQuestNext_ = false;
                choosingLevel_ = true;
                break;
            case kExtrasNewGamePlus:
                // La segunda vuelta del original (los niveles mas dificiles):
                // 1 o 2 jugadores.
                titleMenu_ = kMenuPlus;
                plusSelection_ = 0;
                break;
            case kExtrasEditor:
                titleMenu_ = kMenuEditor;
                editorSelection_ = kEditorCreate;
                break;
            default: break;
        }
        return;
    }
    if (titleMenu_ == kMenuPlus) {
        if (input == MenuInput::Up || input == MenuInput::Down) {
            plusSelection_ = 1 - plusSelection_;
            audio_.play(Sfx::TimeTally);
            return;
        }
        if (input != MenuInput::Confirm) return;
        audio_.play(Sfx::Coin);
        secondQuestNext_ = true;
        startWorld_ = 1;
        startLevel_ = 1;
        if (plusSelection_ == 0) {
            choosingCharacter_ = true;
            characterChoice_ = playAsLuigi_ ? 1 : 0;
        } else {
            choosingTwoMode_ = true;
            twoModeChoice_ = 0;
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
        audio_.play(Sfx::Coin);
        enterEditor(editorSelection_ == kEditorSelect ? EditorEntry::Pick : EditorEntry::New);
        return;
    }
    // Submenu JUGAR.
    if (input == MenuInput::Up || input == MenuInput::Down) {
        titleSelection_ = stepSelection(titleSelection_, input == MenuInput::Up ? -1 : 1, kTitleCount,
                                        titleDisabled(continueWorld() > 0 || anySaveSlot()));
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
            startLevel_ = 1;
            if (continueWorld_ > 0) {
                // Despues de un GAME OVER en esta sesion (sigue en el mismo espacio).
                startWorld_ = continueWorld_;
                startCampaign(continueTwoPlayers_, continueAsLuigi_);
            } else if (anySaveSlot()) {
                openContinueSlots();
            }
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

void Game::startRumble(const Mario& character, RumbleKind kind) {
    if (!settings_.vibration) return;
    std::vector<RumbleStep> steps;
    switch (kind) {
        case RumbleKind::Item: {
            // powerup.wav: tres subidas de notas (0.03-0.26, 0.26-0.60,
            // 0.60-0.90 s); un pulso cada 40 ms que crece en cada subida.
            const float sweeps[4] = {0.03f, 0.26f, 0.60f, 0.90f};
            for (int s = 0; s < 3; ++s) {
                for (float t = sweeps[s]; t < sweeps[s + 1] - 0.01f; t += 0.04f) {
                    const float k = (t - sweeps[s]) / (sweeps[s + 1] - sweeps[s]);
                    steps.push_back({t, static_cast<Uint16>(0x1800 + k * 0x5800), 45});
                }
            }
            break;
        }
        case RumbleKind::OneUp: {
            // 1up.wav: seis notas.
            const float notes[6] = {0.04f, 0.16f, 0.30f, 0.42f, 0.56f, 0.70f};
            for (int i = 0; i < 6; ++i) steps.push_back({notes[i], static_cast<Uint16>(0x2800 + i * 0x0A00), 90});
            break;
        }
        case RumbleKind::Hurt:
            // pipepowerdown.wav: tres golpes que se apagan.
            for (float t : {0.01f, 0.28f, 0.55f}) {
                steps.push_back({t, 0xB000, 60});
                steps.push_back({t + 0.06f, 0x5000, 120});
            }
            break;
        case RumbleKind::Death:
            steps.push_back({0.0f, 0xC000, 500});
            break;
        case RumbleKind::BrickBreak:
            steps.push_back({0.0f, 0xD000, 140});
            break;
        case RumbleKind::BlockBump:
            steps.push_back({0.0f, 0x7000, 90});
            break;
    }
    const size_t p = &character == &luigi_ ? 1 : 0;
    rumbleSteps_[p] = std::move(steps);
    rumbleClock_[p] = 0.0f;
    rumbleNext_[p] = 0;
}

void Game::updateRumble(float dt) {
    // Golpeado o muerto: arranca el patron una vez, al empezar.
    const bool marioHurt = mario_.isInDamageSequence() || mario_.isInDeathSequence();
    const bool luigiHurt = twoPlayerMode_ && (luigi_.isInDamageSequence() || luigi_.isInDeathSequence());
    if (marioHurt && !marioWasHurt_) {
        startRumble(mario_, mario_.isInDeathSequence() ? RumbleKind::Death : RumbleKind::Hurt);
    }
    if (luigiHurt && !luigiWasHurt_) {
        startRumble(luigi_, luigi_.isInDeathSequence() ? RumbleKind::Death : RumbleKind::Hurt);
    }
    marioWasHurt_ = marioHurt;
    luigiWasHurt_ = luigiHurt;
    // Deslizandose por el mastil: vibracion pareja mientras baja.
    for (size_t p = 0; p < 2; ++p) {
        const Mario& character = p == 1 ? luigi_ : mario_;
        if (p == 1 && !twoPlayerMode_) continue;
        if (!character.isSlidingFlagpole()) {
            flagRumbleTimer_[p] = 0.0f;
            continue;
        }
        flagRumbleTimer_[p] -= dt;
        if (flagRumbleTimer_[p] <= 0.0f) {
            flagRumbleTimer_[p] = 0.1f;
            if (settings_.vibration) (p == 1 ? input2_ : input1_).rumble(0x7000, 200);
        }
    }
    // Los pulsos de cada jugador, a su tiempo.
    for (size_t p = 0; p < 2; ++p) {
        auto& steps = rumbleSteps_[p];
        if (rumbleNext_[p] >= steps.size()) continue;
        rumbleClock_[p] += dt;
        Input& input = p == 1 ? input2_ : input1_;
        while (rumbleNext_[p] < steps.size() && steps[rumbleNext_[p]].at <= rumbleClock_[p]) {
            const RumbleStep& step = steps[rumbleNext_[p]++];
            if (settings_.vibration) input.rumble(step.strength, step.ms);
        }
    }
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
    // Termino el castillo de un mundo (x-4): se guarda la partida para
    // seguir desde el mundo siguiente (CONTINUAR, tambien despues de cerrar
    // el juego).
    // Paso el nivel: se guarda desde el siguiente.
    if (std::filesystem::exists(next)) writeCurrentSlot(world, level);
    if (!std::filesystem::exists(next)) {
        // Despues del 8-4 (WorldSelectEnableFlag del original): queda
        // desbloqueado lo de EXTRAS (PARTIDA + es la segunda vuelta).
        if (campaignWorld_ >= 8) {
            settings_.gameCompleted = true;
            if (currentSlot_ >= 0) settings_.saves[static_cast<size_t>(currentSlot_)].world = 0;
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

void Game::refreshSlotPreviews() {
    for (size_t i = 0; i < slotPreviews_.size(); ++i) {
        if (slotPreviews_[i]) SDL_DestroyTexture(slotPreviews_[i]);
        slotPreviews_[i] = nullptr;
        const int world = settings_.saves[i].world;
        if (world > 0) slotPreviews_[i] = renderLevelPreview(campaignLevelPath(world, settings_.saves[i].level));
    }
}

void Game::renderSlotPicker() {
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 240);
    SDL_RenderFillRect(renderer_, nullptr);
    drawCentered(12, tr(Txt::ChooseSlot), 2, kMenuWhite);
    const int boxW = 240;
    const int boxX = (gLogicalWidth - boxW) / 2;
    for (int i = 0; i < Settings::kSaveSlots; ++i) {
        const Settings::SaveSlot& slot = settings_.saves[static_cast<size_t>(i)];
        const int y = 32 + i * 68;
        const SDL_Rect box{boxX, y, boxW, 62};
        const bool selected = i == slotChoice_;
        const bool usable = !slotForContinue_ || slot.world > 0;
        SDL_SetRenderDrawColor(renderer_, 20, 20, 40, 255);
        SDL_RenderFillRect(renderer_, &box);
        const SDL_Color border = selected ? kMenuYellow : SDL_Color{90, 90, 110, 255};
        SDL_SetRenderDrawColor(renderer_, border.r, border.g, border.b, 255);
        SDL_RenderDrawRect(renderer_, &box);
        // Vista previa: la franja de abajo de la primera pantalla (piso).
        const SDL_Rect thumb{boxX + 4, y + 4, 96, 54};
        if (slotPreviews_[static_cast<size_t>(i)]) {
            int previewH = 0;
            SDL_QueryTexture(slotPreviews_[static_cast<size_t>(i)], nullptr, nullptr, nullptr, &previewH);
            const SDL_Rect source{0, std::max(0, previewH - 144), 256, std::min(144, previewH)};
            SDL_RenderCopy(renderer_, slotPreviews_[static_cast<size_t>(i)], &source, &thumb);
        } else {
            SDL_SetRenderDrawColor(renderer_, 40, 40, 60, 255);
            SDL_RenderFillRect(renderer_, &thumb);
        }
        SDL_SetRenderDrawColor(renderer_, 90, 90, 110, 255);
        SDL_RenderDrawRect(renderer_, &thumb);
        const SDL_Color text = usable ? kMenuWhite : kMenuGray;
        const int tx = boxX + 108;
        DebugFont::drawText(renderer_, tx, y + 6, std::string(tr(Txt::SlotLabel)) + " " + std::to_string(i + 1), 2,
                            selected ? kMenuYellow : text);
        if (slot.world <= 0) {
            DebugFont::drawText(renderer_, tx, y + 24, tr(Txt::SlotEmpty), 2, text);
            continue;
        }
        DebugFont::drawText(renderer_, tx, y + 20, std::string(tr(Txt::HudWorld)) + " " + std::to_string(slot.world) + "-" + std::to_string(slot.level),
                            2, text);
        const Txt mode = slot.mode == "coop" ? Txt::ModeCoop : slot.mode == "turns" ? Txt::ModeTurns : Txt::OnePlayer;
        std::string line = tr(mode);
        if (slot.mode == "solo" && slot.luigi) line = "LUIGI";
        DebugFont::drawText(renderer_, tx, y + 34, line, 2, text);
        if (slot.secondQuest) DebugFont::drawText(renderer_, tx, y + 48, tr(Txt::NewGamePlus), 2, kMenuYellow);
    }
    if (slotActions_ && !confirmingDelete_) {
        const SDL_Rect box{(gLogicalWidth - 200) / 2, 96, 200, 44};
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 245);
        SDL_RenderFillRect(renderer_, &box);
        SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer_, &box);
        const int continueX = gLogicalWidth / 2 - 84;
        const int deleteX = gLogicalWidth / 2 + 24;
        DebugFont::drawText(renderer_, continueX, 114, tr(Txt::Continue), 2, kMenuWhite);
        DebugFont::drawText(renderer_, deleteX, 114, tr(Txt::DeleteOption), 2, kMenuWhite);
        titleCursorSprite_.draw(renderer_, 0, 0, (slotActionChoice_ == 0 ? continueX : deleteX) - 12, 114, false);
    }
    if (confirmingDelete_) {
        const SDL_Rect box{(gLogicalWidth - 200) / 2, 90, 200, 62};
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 245);
        SDL_RenderFillRect(renderer_, &box);
        SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer_, &box);
        drawCentered(98, tr(Txt::DeleteOption), 2, kMenuWhite);
        drawCentered(110, tr(Txt::Overwrite2), 2, kMenuWhite);
        const int yesX = gLogicalWidth / 2 - 48;
        const int noX = gLogicalWidth / 2 + 24;
        DebugFont::drawText(renderer_, yesX, 132, tr(Txt::Yes), 2, kMenuWhite);
        DebugFont::drawText(renderer_, noX, 132, tr(Txt::No), 2, kMenuWhite);
        titleCursorSprite_.draw(renderer_, 0, 0, (deleteChoice_ == 0 ? yesX : noX) - 12, 132, false);
    }
    if (confirmingOverwrite_) {
        const SDL_Rect box{(gLogicalWidth - 200) / 2, 90, 200, 62};
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 245);
        SDL_RenderFillRect(renderer_, &box);
        SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer_, &box);
        drawCentered(98, tr(Txt::Overwrite1), 2, kMenuWhite);
        drawCentered(110, tr(Txt::Overwrite2), 2, kMenuWhite);
        const int yesX = gLogicalWidth / 2 - 48;
        const int noX = gLogicalWidth / 2 + 24;
        DebugFont::drawText(renderer_, yesX, 132, tr(Txt::Yes), 2, kMenuWhite);
        DebugFont::drawText(renderer_, noX, 132, tr(Txt::No), 2, kMenuWhite);
        titleCursorSprite_.draw(renderer_, 0, 0, (overwriteChoice_ == 0 ? yesX : noX) - 12, 132, false);
    }
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
            disabled = titleDisabled(continueWorld() > 0 || anySaveSlot());
            selection = titleSelection_;
        } else if (titleMenu_ == kMenuExtras) {
            ids = {Txt::WorldSelect, Txt::NewGamePlus, Txt::LevelEditor, Txt::ExtraOptions};
            disabled = extrasDisabled(settings_.gameCompleted, true);
            selection = extrasSelection_;
        } else if (titleMenu_ == kMenuPlus) {
            ids = {Txt::OnePlayer, Txt::TwoPlayers};
            disabled.assign(ids.size(), false);
            selection = plusSelection_;
        } else if (titleMenu_ == kMenuEditor) {
            ids = {Txt::EditorCreate, Txt::EditorSelect};
            disabled.assign(ids.size(), false);
            selection = editorSelection_;
        } else {
            ids = {Txt::Play, Txt::Options, Txt::Credits, Txt::Quit};
            disabled.assign(ids.size(), false);
            selection = mainSelection_;
        }
        // CONTINUAR dice desde donde sigue (ej. "CONTINUAR 3-1").
        std::vector<std::string> labels;
        for (Txt id : ids) {
            std::string label = tr(id);
            if (id == Txt::Continue && continueWorld() > 0) label += " " + std::to_string(continueWorld()) + "-1";
            labels.push_back(label);
        }
        int widest = 0;
        for (const std::string& label : labels) widest = std::max(widest, static_cast<int>(label.size()) * 8);
        const int x = (gLogicalWidth - widest) / 2;
        const int step = 14;
        for (int i = 0; i < static_cast<int>(ids.size()); ++i) {
            const int y = 134 + i * step;
            if (i == selection) titleCursorSprite_.draw(renderer_, 0, 0, x - 12, y, false);
            DebugFont::drawText(renderer_, x, y, labels[static_cast<size_t>(i)], 2,
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
    if (choosingSlot_) renderSlotPicker();
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
