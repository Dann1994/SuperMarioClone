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

namespace {

const SDL_Color kMenuWhite{255, 255, 255, 255};
const SDL_Color kMenuYellow{252, 216, 88, 255};
const SDL_Color kMenuGray{120, 120, 140, 255};
const SDL_Color kMenuDim{180, 180, 200, 255};

// Opciones del menu principal, en orden.
enum TitleItem { kTitleOnePlayer, kTitleTwoPlayers, kTitleContinue, kTitleWorldSelect, kTitleOptions,
                 kTitleTestMode, kTitleCredits, kTitleQuit, kTitleCount };
// Opciones del menu de pausa.
enum PauseItem { kPauseResume, kPauseRestart, kPauseOptions, kPauseQuit, kPauseCount };
// Opciones del menu de configuracion.
enum SettingsItem { kSetDisplay, kSetWindowSize, kSetAspect, kSetFps, kSetMaster, kSetMusic, kSetSfx,
                    kSetLanguage, kSetVibration, kSetControls, kSetBack, kSetCount };
// Filas del menu de controles: 5 acciones x 2 jugadores, 2 botones del
// mando, restaurar, volver.
constexpr int kActionCount = 5;
constexpr int kCtrlPadJump = kActionCount * 2;
constexpr int kCtrlPadRun = kCtrlPadJump + 1;
constexpr int kCtrlReset = kCtrlPadRun + 1;
constexpr int kCtrlBack = kCtrlReset + 1;
constexpr int kCtrlCount = kCtrlBack + 1;

// Indices de decorationSprites_ (mismo orden que kDecorations en Game.cpp).
constexpr int kDecoHillBig = 1;
constexpr int kDecoHillSmall = 2;
constexpr int kDecoBushBig = 3;
constexpr int kDecoCloudBig = 5;
constexpr int kDecoCloudSmall = 6;

std::string upper(std::string text) {
    for (char& c : text) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return text;
}

Txt actionName(int action) {
    static const Txt kNames[kActionCount] = {Txt::Left, Txt::Right, Txt::Jump, Txt::Run, Txt::Down};
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

void Game::openTitle() {
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

std::vector<bool> titleDisabled() {
    // Continuar y seleccionar mundo necesitan el guardado por mundo (seccion
    // 6), que todavia no existe.
    std::vector<bool> disabled(kTitleCount, false);
    disabled[kTitleContinue] = true;
    disabled[kTitleWorldSelect] = true;
    return disabled;
}

}  // namespace

void Game::handleTitleInput(MenuInput input) {
    if (input == MenuInput::Up || input == MenuInput::Down) {
        titleSelection_ = stepSelection(titleSelection_, input == MenuInput::Up ? -1 : 1, kTitleCount, titleDisabled());
        audio_.play(Sfx::TimeTally);
        return;
    }
    if (input != MenuInput::Confirm) return;
    audio_.play(Sfx::Coin);
    switch (titleSelection_) {
        case kTitleOnePlayer: startCampaign(false); break;
        case kTitleTwoPlayers: startCampaign(true); break;
        case kTitleOptions: openSettings(AppState::Title); break;
        case kTitleTestMode: openScenarioSelect(); break;
        case kTitleCredits: appState_ = AppState::Credits; break;
        case kTitleQuit: running_ = false; break;
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

void Game::startCampaign(bool twoPlayers) {
    twoPlayerMode_ = twoPlayers;
    campaignWorld_ = 1;
    campaignLevel_ = 1;
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
    // Fondo: un pedazo de 1-1 (cielo, piso, colinas, arbustos, nubes).
    const int worldRow = static_cast<int>(WorldType::Overworld);
    const SDL_Color sky = backgroundColorFor(WorldType::Overworld);
    SDL_SetRenderDrawColor(renderer_, sky.r, sky.g, sky.b, 255);
    SDL_RenderFillRect(renderer_, nullptr);
    for (int x = 0; x < gLogicalWidth; x += 16) {
        terrainSprite_.draw(renderer_, worldRow, 0, x, 208, false);
        terrainSprite_.draw(renderer_, worldRow, 0, x, 224, false);
    }
    decorationSprites_[kDecoHillBig].draw(renderer_, 0, 0, 0, 160, false);
    decorationSprites_[kDecoHillSmall].draw(renderer_, 0, 0, gLogicalWidth - 96, 176, false);
    decorationSprites_[kDecoBushBig].draw(renderer_, 0, 0, gLogicalWidth / 2 - 24, 192, false);
    decorationSprites_[kDecoCloudBig].draw(renderer_, 0, 0, 24, 18, false);
    decorationSprites_[kDecoCloudSmall].draw(renderer_, 0, 0, gLogicalWidth - 72, 30, false);

    // Cartel del titulo.
    const int boxW = 200;
    const SDL_Rect box{(gLogicalWidth - boxW) / 2, 22, boxW, 48};
    SDL_SetRenderDrawColor(renderer_, 156, 74, 0, 255);
    SDL_RenderFillRect(renderer_, &box);
    SDL_SetRenderDrawColor(renderer_, 252, 188, 176, 255);
    SDL_RenderDrawRect(renderer_, &box);
    drawCentered(28, "SUPER MARIO", 4, SDL_Color{252, 188, 176, 255});
    drawCentered(52, "CLONE", 3, kMenuWhite);

    std::vector<std::string> items;
    for (Txt id : {Txt::OnePlayer, Txt::TwoPlayers, Txt::Continue, Txt::WorldSelect, Txt::Options, Txt::TestMode,
                   Txt::Credits, Txt::Quit}) {
        items.push_back(tr(id));
    }
    std::vector<bool> disabled = titleDisabled();
    drawMenuList((gLogicalWidth - 17 * 8) / 2 + 8, 82, 13, items, titleSelection_, disabled);
    if (titleSelection_ == kTitleContinue || titleSelection_ == kTitleWorldSelect) {
        drawCentered(186, tr(Txt::ComingSoon), 1, kMenuWhite);
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
    labels.push_back(tr(Txt::Controls));
    values.push_back(">");
    labels.push_back(tr(Txt::Back));
    values.push_back("");

    // Dos columnas: nombre a la izquierda, valor a la derecha (escala 1 para
    // que entre en 4:3 tambien).
    const int left = gLogicalWidth / 2 - 110;
    const int right = gLogicalWidth / 2 + 30;
    for (int i = 0; i < kSetCount; ++i) {
        const int y = 34 + i * 15;
        const bool selected = i == settingsSelection_;
        const SDL_Color color = selected ? kMenuWhite : kMenuYellow;
        if (selected) DebugFont::drawText(renderer_, left - 10, y, ">", 2, kMenuWhite);
        DebugFont::drawText(renderer_, left, y + 2, labels[i], 1, color);
        DebugFont::drawText(renderer_, right, y + 2, values[i], 1, color);
    }
    drawCentered(210, tr(Txt::ValueHint), 1, kMenuDim);
    drawCentered(220, tr(Txt::MenuHint), 1, kMenuDim);
}

void Game::renderControls() {
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderFillRect(renderer_, nullptr);
    drawCentered(8, tr(Txt::Controls), 2, kMenuWhite);

    const int left = gLogicalWidth / 2 - 110;
    const int right = gLogicalWidth / 2 + 20;
    int y = 26;
    auto row = [&](int index, const std::string& label, const std::string& value) {
        const bool selected = index == controlsSelection_;
        const SDL_Color color = selected ? kMenuWhite : kMenuYellow;
        if (selected) DebugFont::drawText(renderer_, left - 10, y - 1, ">", 1, kMenuWhite);
        DebugFont::drawText(renderer_, left, y, label, 1, color);
        std::string shown = value;
        if (selected && waitingBinding_) shown = "...";
        DebugFont::drawText(renderer_, right, y, shown, 1, color);
        y += 9;
    };
    for (int player = 0; player < 2; ++player) {
        DebugFont::drawText(renderer_, left, y, tr(player == 0 ? Txt::Player1 : Txt::Player2), 1, kMenuDim);
        y += 10;
        const Input& input = player == 0 ? input1_ : input2_;
        for (int action = 0; action < kActionCount; ++action) {
            const SDL_Scancode key = input.keyBinding(static_cast<Action>(action));
            row(player * kActionCount + action, std::string("  ") + tr(actionName(action)), upper(SDL_GetScancodeName(key)));
        }
        y += 4;
    }
    DebugFont::drawText(renderer_, left, y, tr(Txt::Gamepad), 1, kMenuDim);
    y += 10;
    const char* jump = SDL_GameControllerGetStringForButton(input1_.padJumpButton());
    const char* run = SDL_GameControllerGetStringForButton(input1_.padRunButton());
    row(kCtrlPadJump, std::string("  ") + tr(Txt::Jump), upper(jump ? jump : "?"));
    row(kCtrlPadRun, std::string("  ") + tr(Txt::Run), upper(run ? run : "?"));
    y += 4;
    row(kCtrlReset, tr(Txt::ResetDefaults), "");
    row(kCtrlBack, tr(Txt::Back), "");

    const bool padRow = controlsSelection_ == kCtrlPadJump || controlsSelection_ == kCtrlPadRun;
    if (waitingBinding_) {
        drawCentered(222, tr(padRow ? Txt::PressButton : Txt::PressKey), 1, kMenuWhite);
    } else {
        drawCentered(222, tr(Txt::BindHint), 1, kMenuDim);
    }
}

void Game::renderCredits() {
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderFillRect(renderer_, nullptr);
    drawCentered(20, "SUPER MARIO CLONE", 3, kMenuWhite);
    const char* const lines[] = {
        "CLON EXPERIMENTAL DE SUPER MARIO BROS.",
        "(NINTENDO, 1985), SIN FINES DE LUCRO",
        "",
        "DISENO Y DESARROLLO: DANIEL VERGARA",
        "PROGRAMACION ASISTIDA POR CLAUDE (ANTHROPIC)",
        "",
        "SPRITES: HOJAS DE REFERENCIA DE SUPERJUSTINBROS",
        "SONIDOS: EFECTOS ORIGINALES DE SUPER MARIO BROS.",
        "",
        "C++ / SDL2",
    };
    int y = 60;
    for (const char* line : lines) {
        drawCentered(y, line, 1, kMenuYellow);
        y += 11;
    }
    drawCentered(220, tr(Txt::MenuHint), 1, kMenuDim);
}

void Game::renderDemoEnd() {
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderFillRect(renderer_, nullptr);
    renderHud();
    drawCentered(90, tr(Txt::EndOfDemo), 3, kMenuWhite);
    drawCentered(130, tr(Txt::ThanksForPlaying), 2, kMenuYellow);
    drawCentered(220, tr(Txt::MenuHint), 1, kMenuDim);
}
