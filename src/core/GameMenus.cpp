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
#include <iterator>

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
enum MainItem { kMainPlay, kMainSettings, kMainScores, kMainQuit, kMainCount };
// Submenu JUGAR.
enum TitleItem { kTitleOnePlayer, kTitleTwoPlayers, kTitleExtras, kTitleCount };
// Submenu EXTRAS.
enum ExtrasItem { kExtrasWorldSelect, kExtrasNewGamePlus, kExtrasEditor, kExtrasJukebox, kExtrasAchievements,
                  kExtrasShop, kExtrasMyExtras, kExtrasCount };

// Que se puede elegir en EXTRAS: PARTIDA + y el editor, cuando los
// desbloquea su logro; la ROCOLA, comprada; ELEGIR MUNDO, al pasar el primer
// nivel (y ahi solo los niveles ya pasados); LOGROS, TIENDA y MIS EXTRAS,
// desde el principio.
std::vector<bool> extrasDisabled(const Settings& settings) {
    std::vector<bool> disabled(kExtrasCount, false);
    disabled[kExtrasWorldSelect] = settings.clearedLevels.empty();
    disabled[kExtrasNewGamePlus] = !settings.extraUnlocked("new_game_plus");
    disabled[kExtrasEditor] = !settings.extraUnlocked("level_editor");
    disabled[kExtrasJukebox] = !settings.extraOwned("jukebox");
    return disabled;
}

// ---------------------------------------------------------------- TIENDA
// Lo que se desbloquea con los logros. Para agregar algo (un extra o un
// cosmetico): una fila aca (id unico, nombres, que hace y precio), un logro
// que lo desbloquee en kAchievements y, si hace falta, su efecto en el juego
// (extraOwned + extraEnabled del id) y su icono en drawExtraIcon.
// menuOption: habilita una opcion del menu EXTRAS (no se activa ni
// desactiva). price 0: no se compra, queda habilitado con el logro (no
// aparece en la TIENDA). item: el que da en la pausa (ITEMS, en PARTIDA +).
enum class ExtraItem { None, Mushroom, FireFlower, Star, InfiniteStar, HammerSuit, BoomerangSuit };
struct ExtraContent {
    const char* id;
    const char* nameEs;
    const char* nameEn;
    const char* descEs;   // que hace (la TIENDA)
    const char* descEn;
    bool menuOption = false;
    ExtraItem item = ExtraItem::None;
    int price = 0;
};
constexpr ExtraContent kExtraContent[] = {
    {"new_game_plus", "PARTIDA +", "NEW GAME +", "LA SEGUNDA VUELTA, MAS DIFICIL", "THE HARDER SECOND QUEST", true},
    {"level_editor", "EDITOR DE NIVELES", "LEVEL EDITOR", "ARMA Y JUGA TUS NIVELES", "BUILD AND PLAY YOUR OWN LEVELS",
     true},
    {"jukebox", "ROCOLA", "JUKEBOX", "ESCUCHA LAS MUSICAS Y LOS SONIDOS", "LISTEN TO THE MUSIC AND SOUNDS", true,
     ExtraItem::None, 1000},
    {"luigi_lost_levels", "LUIGI DE LOST LEVELS", "LOST LEVELS LUIGI", "LUIGI SALTA MAS ALTO Y PATINA MAS",
     "LUIGI JUMPS HIGHER AND SLIDES MORE", false, ExtraItem::None, 1500},
    {"item_mushroom", "ITEM: SUPER CHAMPIÑON", "ITEM: SUPER MUSHROOM", "UN CHAMPIÑON DESDE LA PAUSA (PARTIDA +)",
     "A MUSHROOM FROM THE PAUSE MENU (NEW GAME +)", false, ExtraItem::Mushroom, 1500},
    {"item_flower", "ITEM: FLOR DE FUEGO", "ITEM: FIRE FLOWER", "UNA FLOR DE FUEGO DESDE LA PAUSA (PARTIDA +)",
     "A FIRE FLOWER FROM THE PAUSE MENU (NEW GAME +)", false, ExtraItem::FireFlower, 2000},
    {"level_hammer_flower", "FLOR MARTILLO (NIVELES)", "HAMMER FLOWER (LEVELS)",
     "10% DE QUE SALGA EN VEZ DE LA FLOR DE FUEGO", "10% CHANCE INSTEAD OF A FIRE FLOWER", false, ExtraItem::None,
     2000},
    {"level_boomerang_flower", "FLOR BUMERAN (NIVELES)", "BOOMERANG FLOWER (LEVELS)",
     "10% DE QUE SALGA EN VEZ DE LA FLOR DE FUEGO", "10% CHANCE INSTEAD OF A FIRE FLOWER", false, ExtraItem::None,
     2000},
    {"item_hammer", "ITEM: FLOR MARTILLO", "ITEM: HAMMER FLOWER", "EL TRAJE DE HAMMER BRO DESDE LA PAUSA (PARTIDA +)",
     "THE HAMMER SUIT FROM THE PAUSE MENU (NEW GAME +)", false, ExtraItem::HammerSuit, 2500},
    {"item_boomerang", "ITEM: FLOR BUMERAN", "ITEM: BOOMERANG FLOWER",
     "EL TRAJE DE BOOMERANG BRO DESDE LA PAUSA (PARTIDA +)", "THE BOOMERANG SUIT FROM THE PAUSE MENU (NEW GAME +)",
     false, ExtraItem::BoomerangSuit, 2500},
    {"item_star", "ITEM: ESTRELLA", "ITEM: STAR", "UNA ESTRELLA DESDE LA PAUSA (PARTIDA +)",
     "A STAR FROM THE PAUSE MENU (NEW GAME +)", false, ExtraItem::Star, 3000},
    {"infinite_lives", "VIDAS INFINITAS", "INFINITE LIVES", "EN LAS PARTIDAS NO SE PIERDEN VIDAS",
     "YOU NEVER LOSE LIVES", false, ExtraItem::None, 4000},
    {"infinite_time", "TIEMPO INFINITO", "ENDLESS TIME", "EL RELOJ DEL NIVEL NO BAJA", "THE LEVEL TIMER STOPS", false,
     ExtraItem::None, 4000},
    {"item_infinite_star", "ITEM: ESTRELLA INFINITA", "ITEM: ENDLESS STAR",
     "LA ESTRELLA DURA TODO EL NIVEL, DESDE LA PAUSA (PARTIDA +)",
     "THE STAR LASTS THE WHOLE LEVEL, FROM THE PAUSE MENU (NEW GAME +)", false, ExtraItem::InfiniteStar, 5000},
};
constexpr int kExtraContentCount = static_cast<int>(std::size(kExtraContent));

const ExtraContent* findExtra(const std::string& id) {
    for (const ExtraContent& item : kExtraContent) {
        if (id == item.id) return &item;
    }
    return nullptr;
}

// ---------------------------------------------------------------- LOGROS
// Cada logro da monedas (segun lo dificil) y algunos desbloquean un extra
// de la TIENDA (unlocks). icon: su lugar en achievements.png
// (gen_achievement_icons.py). Los de avance (progress) se cumplen solos al
// llegar a la meta; el resto los cumple el juego (completeAchievement).
// Solo cuentan las partidas (no los niveles creados ni las pruebas).
enum class ExtraProgress { None, FlawlessLevels, HammerBroStomps, HammerBroStompsPlus, HiddenOneUps, TotalCoins,
                           TotalEnemies };
struct Achievement {
    const char* id;
    const char* nameEs;
    const char* nameEn;
    const char* condEs;
    const char* condEn;
    int reward;                 // monedas
    const char* unlocks;        // id de kExtraContent, o nullptr
    int icon;
    ExtraProgress progress = ExtraProgress::None;
    int target = 0;
};
constexpr int kCampaignLevels = 32;   // 1-1 ... 8-4
constexpr Achievement kAchievements[] = {
    {"first_step", "PRIMER PASO", "FIRST STEP", "PASA EL 1-1", "CLEAR 1-1", 100, nullptr, 0},
    {"first_castle", "PRIMER CASTILLO", "FIRST CASTLE", "PASA EL 1-4", "CLEAR 1-4", 200, nullptr, 1},
    {"warp", "ATAJO", "SHORTCUT", "USA UNA ZONA DE WARP", "USE A WARP ZONE", 100, nullptr, 2},
    {"flag_top", "BANDERA ARRIBA", "TOP OF THE FLAG", "AGARRA LA BANDERA EN LO MAS ALTO", "GRAB THE TOP OF THE FLAGPOLE",
     200, nullptr, 3},
    {"bowser_fire", "FUEGO CONTRA FUEGO", "FIGHT FIRE WITH FIRE", "DERROTA A BOWSER CON BOLAS DE FUEGO",
     "DEFEAT BOWSER WITH FIREBALLS", 300, nullptr, 4},
    {"collector", "COLECCIONISTA", "COLLECTOR", "JUNTA 1000 MONEDAS EN TOTAL", "COLLECT 1000 COINS IN TOTAL", 300,
     nullptr, 5, ExtraProgress::TotalCoins, 1000},
    {"big_collector", "GRAN COLECCIONISTA", "BIG COLLECTOR", "JUNTA 5000 MONEDAS EN TOTAL",
     "COLLECT 5000 COINS IN TOTAL", 800, "jukebox", 6, ExtraProgress::TotalCoins, 5000},
    {"hunter", "CAZADOR", "HUNTER", "ELIMINA 100 ENEMIGOS EN TOTAL", "DEFEAT 100 ENEMIES IN TOTAL", 300, nullptr, 7,
     ExtraProgress::TotalEnemies, 100},
    {"exterminator", "EXTERMINADOR", "EXTERMINATOR", "ELIMINA 500 ENEMIGOS EN TOTAL", "DEFEAT 500 ENEMIES IN TOTAL", 800,
     nullptr, 8, ExtraProgress::TotalEnemies, 500},
    {"teamwork", "TRABAJO EN EQUIPO", "TEAMWORK", "COMPLETA UN MUNDO ENTERO DE A 2 A LA VEZ",
     "CLEAR A WHOLE WORLD WITH 2 PLAYERS TOGETHER", 500, nullptr, 9},
    {"hammer_stomp", "MARTILLAZO", "HAMMER TIME", "PISA A 10 HAMMER BROS", "STOMP 10 HAMMER BROS", 500, "item_hammer", 10,
     ExtraProgress::HammerBroStomps, 10},
    {"hammer_stomp_plus", "MARTILLAZO +", "HAMMER TIME +", "PISA A 10 HAMMER BROS EN PARTIDA +",
     "STOMP 10 HAMMER BROS IN NEW GAME +", 800, "item_boomerang", 11, ExtraProgress::HammerBroStompsPlus, 10},
    {"beat_game", "FIN DEL JUEGO", "THE END", "COMPLETA EL JUEGO", "BEAT THE GAME", 1000, "new_game_plus", 12},
    {"no_skip", "SIN ATAJOS", "NO SHORTCUTS", "COMPLETA EL JUEGO SIN SALTEAR NIVELES",
     "BEAT THE GAME WITHOUT SKIPPING LEVELS", 1000, "level_editor", 13},
    {"green_brother", "HERMANO VERDE", "GREEN BROTHER", "COMPLETA EL JUEGO CON LUIGI (1 JUGADOR)",
     "BEAT THE GAME AS LUIGI (1 PLAYER)", 1000, "luigi_lost_levels", 14},
    {"no_mushroom", "SIN CHAMPIÑONES", "NO MUSHROOMS", "COMPLETA EL JUEGO SIN AGARRAR NI UN SUPER CHAMPIÑON",
     "BEAT THE GAME WITHOUT GETTING A SINGLE SUPER MUSHROOM", 1500, "item_mushroom", 15},
    {"no_flower", "SIN FLORES", "NO FLOWERS", "COMPLETA EL JUEGO SIN AGARRAR NI UNA FLOR DE FUEGO",
     "BEAT THE GAME WITHOUT GETTING A SINGLE FIRE FLOWER", 1500, "item_flower", 16},
    {"hammer_hunter", "CAZADOR DE HAMMER BROS", "HAMMER BRO HUNTER",
     "COMPLETA UNA PARTIDA ELIMINANDO A TODOS LOS HAMMER BROS", "BEAT A GAME DEFEATING EVERY HAMMER BRO", 1500,
     "level_hammer_flower", 17},
    {"life_finder", "BUSCAVIDAS", "LIFE FINDER", "ENCUENTRA TODAS LAS VIDAS OCULTAS DEL JUEGO",
     "FIND EVERY HIDDEN 1-UP IN THE GAME", 1500, "infinite_lives", 18, ExtraProgress::HiddenOneUps, 0},
    {"beat_plus", "FIN DE PARTIDA +", "NEW GAME + CLEARED", "COMPLETA PARTIDA +", "BEAT NEW GAME +", 2000, nullptr, 19},
    {"untouchable", "INTOCABLE", "UNTOUCHABLE", "PASA LOS 32 NIVELES SIN RECIBIR DAÑO (EN TOTAL)",
     "CLEAR ALL 32 LEVELS WITHOUT TAKING DAMAGE (IN TOTAL)", 2000, "item_star", 20, ExtraProgress::FlawlessLevels,
     kCampaignLevels},
    {"hammer_hunter_plus", "CAZADOR DE HAMMER BROS +", "HAMMER BRO HUNTER +",
     "COMPLETA PARTIDA + ELIMINANDO A TODOS LOS HAMMER BROS", "BEAT NEW GAME + DEFEATING EVERY HAMMER BRO", 2000,
     "level_boomerang_flower", 21},
    {"speedrun", "CONTRARRELOJ", "SPEEDRUN", "COMPLETA EL JUEGO EN MENOS DE 25 MINUTOS SIN SALTEAR NIVELES",
     "BEAT THE GAME IN UNDER 25 MINUTES WITHOUT SKIPPING LEVELS", 2000, "infinite_time", 22},
    {"perfect", "PERFECTO", "PERFECT", "COMPLETA EL JUEGO SIN MORIR, SIN RECIBIR DAÑO Y SIN AGARRAR ESTRELLAS",
     "BEAT THE GAME WITHOUT DYING, TAKING DAMAGE OR GETTING A STAR", 3000, "item_infinite_star", 23},
};
constexpr int kAchievementCount = static_cast<int>(std::size(kAchievements));

const Achievement* findAchievement(const std::string& id) {
    for (const Achievement& achievement : kAchievements) {
        if (id == achievement.id) return &achievement;
    }
    return nullptr;
}

// El logro que desbloquea un extra (para la TIENDA: "se desbloquea con...").
const Achievement* achievementUnlocking(const std::string& extraId) {
    for (const Achievement& achievement : kAchievements) {
        if (achievement.unlocks && extraId == achievement.unlocks) return &achievement;
    }
    return nullptr;
}

int gHiddenOneUpTotal = 0;   // los 1-UP de los bloques de los 32 niveles (Game::init)

int extraTarget(const Achievement& achievement) {
    return achievement.progress == ExtraProgress::HiddenOneUps ? gHiddenOneUpTotal : achievement.target;
}

int extraProgressValue(const Settings& settings, ExtraProgress progress) {
    switch (progress) {
        case ExtraProgress::FlawlessLevels: return static_cast<int>(settings.flawlessLevels.size());
        case ExtraProgress::HammerBroStomps: return settings.hammerBroStomps;
        case ExtraProgress::HammerBroStompsPlus: return settings.hammerBroStompsPlus;
        case ExtraProgress::HiddenOneUps: return static_cast<int>(settings.foundOneUps.size());
        case ExtraProgress::TotalCoins: return settings.totalCoins;
        case ExtraProgress::TotalEnemies: return settings.totalEnemies;
        case ExtraProgress::None: break;
    }
    return 0;
}

// Los extras que estan a la venta (con precio) y los comprados que se
// activan o desactivan (MIS EXTRAS), en el orden de kExtraContent.
std::vector<int> shopExtras() {
    std::vector<int> list;
    for (int i = 0; i < kExtraContentCount; ++i) {
        if (kExtraContent[i].price > 0) list.push_back(i);
    }
    return list;
}

std::vector<int> toggleableExtras(const Settings& settings) {
    std::vector<int> list;
    for (int i = 0; i < kExtraContentCount; ++i) {
        if (!kExtraContent[i].menuOption && settings.extraOwned(kExtraContent[i].id)) list.push_back(i);
    }
    return list;
}

// Parte un texto en renglones de hasta maxChars letras (por palabras).
std::vector<std::string> wrapText(const std::string& text, size_t maxChars) {
    std::vector<std::string> lines;
    std::string line;
    size_t start = 0;
    while (start <= text.size()) {
        size_t end = text.find(' ', start);
        if (end == std::string::npos) end = text.size();
        const std::string word = text.substr(start, end - start);
        if (!line.empty() && static_cast<size_t>(DebugFont::length(line) + 1 + DebugFont::length(word)) > maxChars) {
            lines.push_back(line);
            line.clear();
        }
        line += (line.empty() ? "" : " ") + word;
        start = end + 1;
    }
    if (!line.empty()) lines.push_back(line);
    return lines;
}
// Submenu EDITOR DE NIVELES.
enum EditorItem { kEditorCreate, kEditorSelect, kEditorCount };
// Que menu del titulo se ve (titleMenu_).
enum TitleMenu { kMenuMain, kMenuPlay, kMenuExtras, kMenuEditor, kMenuPlus };
// Opciones del menu de pausa.
enum PauseItem { kPauseResume, kPauseRestart, kPauseItems, kPauseOptions, kPauseQuit, kPauseCount };
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
           appState_ == AppState::Controls || appState_ == AppState::Credits || appState_ == AppState::DemoEnd ||
           appState_ == AppState::Summary || appState_ == AppState::HighScores;
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
    // Saliendo de una partida a mitad de nivel: el tiempo jugado queda en su
    // espacio (el nivel, el de antes), y la alcancia.
    storeSlotPlayTime();
    settings_.save();
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
    replacingSlot_ = false;
    confirmingOverwrite_ = false;
    slotActions_ = false;
    confirmingDelete_ = false;
    worldSelectFlow_ = false;
    extraContentOpen_ = false;
    confirmingPurchase_ = false;
    jukeboxOpen_ = false;
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
    pausedBy_ = 0;   // teclado: el jugador 1 (el mando lo cambia al pausar)
    pauseItemsOpen_ = false;
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

    // Nombre para la tabla: las letras del teclado se escriben directo.
    if (appState_ == AppState::Summary && summaryStage_ == 1 && handleNameTyping(event)) return;
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
            if (input == MenuInput::Confirm || input == MenuInput::Back) {
                if (summaryPending_) {
                    summaryPending_ = false;
                    openSummary();
                } else {
                    openTitle();
                }
            }
            break;
        case AppState::Summary: handleSummaryInput(input); break;
        case AppState::HighScores:
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

}  // namespace

bool Game::anySaveSlot() const {
    for (const Settings::SaveSlot& slot : settings_.saves) {
        if (slot.world > 0) return true;
    }
    return false;
}

void Game::askSlotThen(std::function<void()> start) {
    // PARTIDAS: arriba PARTIDA NUEVA (la que se acaba de armar, start) y
    // abajo las guardadas para CONTINUAR o BORRAR.
    pendingStart_ = std::move(start);
    choosingSlot_ = true;
    replacingSlot_ = false;
    confirmingOverwrite_ = false;
    slotChoice_ = -1;
    refreshSlotPreviews();
}

void Game::checkExtraProgress() {
    // Los logros de avance (cantidad o niveles): llegaron a la meta.
    for (const Achievement& achievement : kAchievements) {
        if (achievement.progress == ExtraProgress::None || extraTarget(achievement) <= 0) continue;
        if (extraProgressValue(settings_, achievement.progress) >= extraTarget(achievement)) {
            completeAchievement(achievement.id);
        }
    }
}

void Game::completeAchievement(const std::string& id) {
    // Una sola vez: las monedas a la alcancia, el extra que desbloquea (a la
    // TIENDA, o habilitado si no tiene precio) y el aviso.
    const Achievement* achievement = findAchievement(id);
    if (!achievement || settings_.achievementDone(id)) return;
    settings_.achievements.push_back(id);
    settings_.coinBank = std::min(Settings::kMaxCoinBank, settings_.coinBank + achievement->reward);
    if (achievement->unlocks && !settings_.extraUnlocked(achievement->unlocks)) {
        settings_.unlockedExtras.push_back(achievement->unlocks);
    }
    settings_.save();
    unlockToasts_.push_back(id);
}

void Game::awardCompletionAchievements() {
    // Se termino el juego (despues del 8-4): los logros de "completar...".
    completeAchievement(gPrimaryHardMode ? "beat_plus" : "beat_game");
    if (campaignNoSkip_) completeAchievement("no_skip");
    if (campaignNoDamage_ && campaignNoStar_) completeAchievement("perfect");
    if (campaignNoMushroom_) completeAchievement("no_mushroom");
    if (campaignNoFlower_) completeAchievement("no_flower");
    if (campaignAllHammerBros_) completeAchievement(gPrimaryHardMode ? "hammer_hunter_plus" : "hammer_hunter");
    if (campaignNoSkip_ && campaignTime_ < kInfiniteTimeLimit) completeAchievement("speedrun");
    if (campaignOnlyLuigi_) completeAchievement("green_brother");
}

void Game::drawExtraIcon(const std::string& extraId, int x, int y) {
    // El icono de cada extra (TIENDA y MIS EXTRAS): el sprite de su item o lo
    // que mejor lo representa.
    const Uint32 now = SDL_GetTicks();
    const ExtraContent* item = findExtra(extraId);
    if (extraId == "new_game_plus") {
        introMarioSprite_.draw(renderer_, 3, 0, x, y, false);   // Mario chico saltando
    } else if (extraId == "level_hammer_flower" || (item && item->item == ExtraItem::HammerSuit)) {
        hammerFlowerSprite_.draw(renderer_, 0, 0, x, y - 2, false);
    } else if (extraId == "level_boomerang_flower" || (item && item->item == ExtraItem::BoomerangSuit)) {
        boomerangFlowerSprite_.draw(renderer_, 0, 0, x - 2, y - 2, false);
    } else if (extraId == "infinite_lives") {
        oneUpSprite_.draw(renderer_, 0, 0, x, y, false);
    } else if (extraId == "infinite_time") {
        iconClockSprite_.draw(renderer_, 0, 0, x, y, false);
    } else if (extraId == "jukebox") {
        iconNoteSprite_.draw(renderer_, 0, 0, x, y, false);
    } else if (extraId == "luigi_lost_levels") {
        introLuigiSprite_.draw(renderer_, 0, 0, x, y, false);
    } else if (extraId == "level_editor") {
        questionBlockSprite_.draw(renderer_, 0, 0, x, y, false);
    } else if (item) {
        switch (item->item) {
            case ExtraItem::Mushroom: mushroomSprite_.draw(renderer_, 0, 0, x, y, false); break;
            case ExtraItem::FireFlower: fireFlowerSprite_.draw(renderer_, 0, 0, x, y, false); break;
            case ExtraItem::InfiniteStar: starSprite_.draw(renderer_, 0, static_cast<int>(now / 70) % 4, x, y, false); break;
            default: starSprite_.draw(renderer_, 0, 2, x, y, false); break;
        }
    }
}

void Game::renderUnlockToast() {
    // Logro cumplido: un cuadro abajo al centro que sube, queda y baja, con
    // el icono del logro, "LOGRO CUMPLIDO!", su nombre, las monedas y, si
    // desbloquea algo, "NUEVO EN LA TIENDA: ...". Si se cumplieron varios a
    // la vez, uno detras del otro. Suena el de la vida.
    if (unlockToasts_.empty()) return;
    constexpr Uint32 kSlide = 300;   // ms subiendo / bajando
    constexpr Uint32 kTotal = 4200;  // ms en pantalla
    const Uint32 now = SDL_GetTicks();
    if (unlockToastStart_ == 0) {
        unlockToastStart_ = now;
        audio_.play(Sfx::OneUp);
    }
    const Uint32 elapsed = now - unlockToastStart_;
    const Achievement* achievement = findAchievement(unlockToasts_.front());
    if (elapsed >= kTotal || !achievement) {
        unlockToasts_.erase(unlockToasts_.begin());
        unlockToastStart_ = 0;
        return;   // el siguiente arranca en el proximo cuadro
    }
    float shown = 1.0f;
    if (elapsed < kSlide) shown = static_cast<float>(elapsed) / kSlide;
    if (elapsed > kTotal - kSlide) shown = static_cast<float>(kTotal - elapsed) / kSlide;
    const ExtraContent* unlocked = achievement->unlocks ? findExtra(achievement->unlocks) : nullptr;
    constexpr int kW = 256;
    const int kH = unlocked ? 56 : 44;
    const int x = (gLogicalWidth - kW) / 2;
    const int y = kLogicalHeight - static_cast<int>(static_cast<float>(kH + 8) * shown);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    // Caja oscura con borde verde (a lo logro de Xbox) y un circulo para el icono.
    const SDL_Rect box{x, y, kW, kH};
    SDL_SetRenderDrawColor(renderer_, 24, 24, 28, 235);
    SDL_RenderFillRect(renderer_, &box);
    SDL_SetRenderDrawColor(renderer_, 16, 124, 16, 255);
    SDL_RenderDrawRect(renderer_, &box);
    const SDL_Rect inner{x + 1, y + 1, kW - 2, kH - 2};
    SDL_RenderDrawRect(renderer_, &inner);
    const int cx = x + 20;
    const int cy = y + 22;
    for (int dy = -15; dy <= 15; ++dy) {
        const int half = static_cast<int>(std::sqrt(static_cast<float>(15 * 15 - dy * dy)));
        SDL_RenderDrawLine(renderer_, cx - half, cy + dy, cx + half, cy + dy);
    }
    achievementIconsSprite_.draw(renderer_, 0, achievement->icon, cx - 8, cy - 8, false);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
    DebugFont::drawText(renderer_, x + 40, y + 5, tr(Txt::ToastAchievement), 2, SDL_Color{120, 220, 120, 255});
    drawCoinAmount(x + kW - 8 - (8 + 8 + 8 * static_cast<int>(std::to_string(achievement->reward).size())), y + 5,
                   achievement->reward, kMenuYellow);
    DebugFont::drawText(renderer_, x + 40, y + 18, isEnglish() ? achievement->nameEn : achievement->nameEs, 2,
                        kMenuWhite);
    if (unlocked) {
        DebugFont::drawText(renderer_, x + 40, y + 32, tr(Txt::ToastNew), 2, kMenuDim);
        DebugFont::drawText(renderer_, x + 40, y + 43, isEnglish() ? unlocked->nameEn : unlocked->nameEs, 2,
                            kMenuYellow);
    }
}

void Game::storeSlotPlayTime() {
    // Se sale de la partida (al titulo o cerrando el juego): el tiempo
    // jugado queda en su espacio para seguir contando al continuarla.
    if (!campaign_ || currentSlot_ < 0 || currentSlot_ >= Settings::kSaveSlots) return;
    Settings::SaveSlot& slot = settings_.saves[static_cast<size_t>(currentSlot_)];
    if (slot.world > 0) slot.playTime = campaignTime_;
}

void Game::writeCurrentSlot(int world, int level) {
    if (currentSlot_ < 0 || currentSlot_ >= Settings::kSaveSlots) return;
    Settings::SaveSlot& slot = settings_.saves[static_cast<size_t>(currentSlot_)];
    slot.world = world;
    slot.level = level;
    slot.mode = turnMode_ ? "turns" : twoPlayerMode_ ? "coop" : "solo";
    slot.luigi = playAsLuigi_;
    slot.secondQuest = secondQuestNext_;
    slot.noDamage = campaignNoDamage_;
    slot.noSkip = campaignNoSkip_;
    slot.noStar = campaignNoStar_;
    slot.noMushroom = campaignNoMushroom_;
    slot.noFlower = campaignNoFlower_;
    slot.allHammerBros = campaignAllHammerBros_;
    slot.onlyLuigi = campaignOnlyLuigi_;
    slot.playTime = campaignTime_;
    slot.enemies = campaignEnemies_;
    slot.deathsMario = campaignDeaths_[0];
    slot.deathsLuigi = campaignDeaths_[1];
    slot.scoreMario = finalScore(0);
    slot.scoreLuigi = finalScore(1);
    settings_.save();
}

void Game::startNewGameInSlot(int slot) {
    choosingSlot_ = false;
    replacingSlot_ = false;
    audio_.play(Sfx::Coin);
    currentSlot_ = slot;
    if (pendingStart_) pendingStart_();
    writeCurrentSlot(startWorld_, startLevel_);
}

void Game::handleSlotPickerInput(MenuInput input) {
    const auto filled = [&](int i) { return settings_.saves[static_cast<size_t>(i)].world > 0; };
    if (input == MenuInput::Up || input == MenuInput::Down) {
        // PARTIDA NUEVA (-1) y las ocupadas; eligiendo cual reemplazar,
        // solo las ocupadas (estan todas).
        const int delta = input == MenuInput::Up ? -1 : 1;
        const int count = Settings::kSaveSlots + 1;
        int next = slotChoice_;
        for (int i = 0; i < count; ++i) {
            next = (next + 1 + delta + count) % count - 1;
            if (next == -1 ? !replacingSlot_ : filled(next)) break;
        }
        slotChoice_ = next;
        audio_.play(Sfx::TimeTally);
    } else if (input == MenuInput::Back) {
        if (replacingSlot_) {
            replacingSlot_ = false;
            slotChoice_ = -1;
        } else {
            choosingSlot_ = false;
        }
        audio_.play(Sfx::Bump);
    } else if (input == MenuInput::Confirm) {
        if (replacingSlot_) {
            confirmingOverwrite_ = true;   // "SOBRESCRIBIR ESTA PARTIDA?"
            overwriteChoice_ = 1;
            audio_.play(Sfx::TimeTally);
        } else if (slotChoice_ >= 0) {
            // CONTINUAR o BORRAR esta partida.
            slotActions_ = true;
            slotActionChoice_ = 0;
            audio_.play(Sfx::TimeTally);
        } else {
            // PARTIDA NUEVA: en el primer espacio vacio; si no hay, se elige
            // cual reemplazar.
            for (int i = 0; i < Settings::kSaveSlots; ++i) {
                if (filled(i)) continue;
                startNewGameInSlot(i);
                return;
            }
            replacingSlot_ = true;
            slotChoice_ = 0;
            audio_.play(Sfx::TimeTally);
        }
    }
}

void Game::handleExtrasScreenInput(MenuInput input) {
    if (confirmingPurchase_) {
        // "COMPRAR POR N MONEDAS?": SI / NO (arranca en NO).
        if (input == MenuInput::Left || input == MenuInput::Right || input == MenuInput::Up ||
            input == MenuInput::Down) {
            purchaseChoice_ = 1 - purchaseChoice_;
            audio_.play(Sfx::TimeTally);
        } else if (input == MenuInput::Confirm && purchaseChoice_ == 0) {
            const ExtraContent& content = kExtraContent[shopExtras()[static_cast<size_t>(extraContentSelection_)]];
            confirmingPurchase_ = false;
            if (settings_.coinBank >= content.price) {
                settings_.coinBank -= content.price;
                settings_.purchasedExtras.push_back(content.id);
                if (!settings_.extraEnabled(content.id)) settings_.enabledExtras.push_back(content.id);   // ya activado
                settings_.save();
                audio_.play(Sfx::OneUp);
            }
        } else if (input == MenuInput::Confirm || input == MenuInput::Back) {
            confirmingPurchase_ = false;
            audio_.play(Sfx::Bump);
        }
        return;
    }
    const std::vector<int> shop = shopExtras();
    const std::vector<int> mine = toggleableExtras(settings_);
    const int count = extrasScreen_ == 0 ? kAchievementCount
                      : extrasScreen_ == 1 ? static_cast<int>(shop.size())
                                           : static_cast<int>(mine.size());
    if (input == MenuInput::Back) {
        extraContentOpen_ = false;
        audio_.play(Sfx::Bump);
        return;
    }
    if ((input == MenuInput::Up || input == MenuInput::Down) && count > 0) {
        extraContentSelection_ = (extraContentSelection_ + (input == MenuInput::Up ? count - 1 : 1)) % count;
        audio_.play(Sfx::TimeTally);
        return;
    }
    if (input != MenuInput::Confirm || count == 0) return;
    if (extrasScreen_ == 1) {
        // TIENDA: desbloqueado y sin comprar, con monedas suficientes.
        const ExtraContent& content = kExtraContent[shop[static_cast<size_t>(extraContentSelection_)]];
        if (settings_.extraUnlocked(content.id) && !settings_.extraPurchased(content.id) &&
            settings_.coinBank >= content.price) {
            confirmingPurchase_ = true;
            purchaseChoice_ = 1;   // NO: que no se compre sin querer
            audio_.play(Sfx::TimeTally);
        } else {
            audio_.play(Sfx::Bump);
        }
    } else if (extrasScreen_ == 2) {
        // MIS EXTRAS: activar o desactivar.
        const std::string id = kExtraContent[mine[static_cast<size_t>(extraContentSelection_)]].id;
        auto& enabled = settings_.enabledExtras;
        const auto it = std::find(enabled.begin(), enabled.end(), id);
        if (it != enabled.end()) {
            enabled.erase(it);
        } else {
            enabled.push_back(id);
        }
        settings_.save();
        audio_.play(Sfx::Coin);
    }
}

void Game::handleTitleInput(MenuInput input) {
    if (jukeboxOpen_) {
        handleJukeboxInput(input);
        return;
    }
    if (extraContentOpen_) {
        handleExtrasScreenInput(input);
        return;
    }
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
            slotChoice_ = -1;
            refreshSlotPreviews();
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
            // Con los jugadores que se acaban de elegir (1 jugador con Mario
            // o Luigi, 2 a la vez o por turnos), desde donde iba esa partida.
            if (pendingStart_) pendingStart_();
            campaignNoDamage_ = slot.noDamage;
            campaignNoSkip_ = slot.noSkip;
            campaignNoStar_ = slot.noStar;
            campaignNoMushroom_ = slot.noMushroom;
            campaignNoFlower_ = slot.noFlower;
            campaignAllHammerBros_ = slot.allHammerBros;
            // Con Luigi, si se sigue con Luigi solo.
            campaignOnlyLuigi_ = slot.onlyLuigi && playAsLuigi_ && !twoPlayerMode_ && !turnMode_;
            campaignTime_ = slot.playTime;   // sigue desde donde quedo
            campaignEnemies_ = slot.enemies;
            campaignDeaths_ = {slot.deathsMario, slot.deathsLuigi};
            // El puntaje con el que iba (por turnos, el de cada uno).
            if (turnMode_) {
                turnStates_[0].score = slot.scoreMario;
                turnStates_[1].score = slot.scoreLuigi;
                score_ = turnStates_[static_cast<size_t>(turnPlayer_)].score;
                playerScore_[0] = score_;
            } else if (twoPlayerMode_) {
                playerScore_ = {slot.scoreMario, slot.scoreLuigi};
                score_ = slot.scoreMario + slot.scoreLuigi;
            } else {
                score_ = slot.scoreMario + slot.scoreLuigi;
                playerScore_[0] = score_;
            }
            writeCurrentSlot(startWorld_, startLevel_);   // guarda el modo nuevo
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
            startNewGameInSlot(slotChoice_);
        } else if (input == MenuInput::Confirm || input == MenuInput::Back) {
            confirmingOverwrite_ = false;
            audio_.play(Sfx::Bump);
        }
        return;
    }
    if (choosingSlot_) {
        handleSlotPickerInput(input);
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
            // Solo los niveles ya pasados: se saltean los otros en la misma
            // direccion (de a uno, o de a mundo con arriba/abajo).
            const int step = (input == MenuInput::Up || input == MenuInput::Left) ? -1 : 1;
            const int start = (startWorld_ - 1) * 4 + (startLevel_ - 1);
            index = (index % 32 + 32) % 32;
            for (int tries = 0; tries < 32 && !levelCleared(index / 4 + 1, index % 4 + 1); ++tries) {
                index = ((index + step) % 32 + 32) % 32;
            }
            if (!levelCleared(index / 4 + 1, index % 4 + 1)) index = start;
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
                campaignNoDamage_ = false;   // ELEGIR MUNDO no cuenta para logros
                campaignNoSkip_ = false;
                campaignNoStar_ = false;
                campaignNoMushroom_ = false;
                campaignNoFlower_ = false;
                campaignAllHammerBros_ = false;
                campaignOnlyLuigi_ = false;
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
            case kMainScores:
                highlightScore_ = -1;
                appState_ = AppState::HighScores;
                break;
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
                                             extrasDisabled(settings_));
            audio_.play(Sfx::TimeTally);
            return;
        }
        if (input != MenuInput::Confirm) return;
        if (extrasDisabled(settings_)[extrasSelection_]) return;
        audio_.play(Sfx::Coin);
        switch (extrasSelection_) {
            case kExtrasWorldSelect: {
                // Arranca en el primero pasado (en el orden del juego).
                secondQuestNext_ = false;
                choosingLevel_ = true;
                for (int index = 0; index < 32; ++index) {
                    if (!levelCleared(index / 4 + 1, index % 4 + 1)) continue;
                    startWorld_ = index / 4 + 1;
                    startLevel_ = index % 4 + 1;
                    break;
                }
                break;
            }
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
            case kExtrasJukebox:
                jukeboxOpen_ = true;
                jukeboxSelection_ = 0;
                break;
            case kExtrasAchievements:
            case kExtrasShop:
            case kExtrasMyExtras:
                extraContentOpen_ = true;
                extrasScreen_ = extrasSelection_ - kExtrasAchievements;
                extraContentSelection_ = 0;
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
        titleSelection_ = (titleSelection_ + (input == MenuInput::Up ? kTitleCount - 1 : 1)) % kTitleCount;
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
        case kTitleExtras:
            titleMenu_ = kMenuExtras;
            // En la primera opcion habilitada (CONTENIDO EXTRA siempre lo esta).
            extrasSelection_ = stepSelection(kExtrasCount - 1, 1, kExtrasCount, extrasDisabled(settings_));
            break;
        default: break;
    }
}

std::vector<int> Game::enabledItemExtras() const {
    std::vector<int> result;
    // Los trucos de items solo se usan en PARTIDA + (la segunda vuelta).
    if (!campaign_ || !gPrimaryHardMode) return result;
    for (int i = 0; i < kExtraContentCount; ++i) {
        const ExtraContent& item = kExtraContent[i];
        if (item.item != ExtraItem::None && settings_.extraOwned(item.id) && settings_.extraEnabled(item.id)) {
            result.push_back(i);
        }
    }
    return result;
}

void Game::applyCheatItem(int extraIndex) {
    // El mismo efecto que tocar el item, en el personaje de quien pauso (en
    // 2 a la vez; si no, el que esta en pantalla).
    Mario& character = (twoPlayerMode_ && pausedBy_ == 1) ? luigi_ : mario_;
    if (character.isInDeathSequence()) return;
    switch (kExtraContent[extraIndex].item) {
        case ExtraItem::Mushroom:
            if (character.size() == MarioSize::Small) character.startGrow();
            campaignNoMushroom_ = false;
            break;
        case ExtraItem::FireFlower:
            campaignNoFlower_ = false;
            if (character.size() == MarioSize::Small) {
                character.startGrow(MarioSize::Big);
            } else if (character.size() == MarioSize::Big) {
                character.startGrow(MarioSize::Fire);
            }
            break;
        case ExtraItem::Star:
            character.setInvincible(true);
            campaignNoStar_ = false;
            break;
        case ExtraItem::HammerSuit:
            character.startGrow(MarioSize::Hammer);
            break;
        case ExtraItem::BoomerangSuit:
            character.startGrow(MarioSize::Boomerang);
            break;
        case ExtraItem::InfiniteStar:
            // Lo que queda del nivel: se apaga al cargar el siguiente (o al
            // volver a intentar este).
            character.setInvincible(true, kInfiniteStarDuration);
            infiniteStarActive_ = true;
            campaignNoStar_ = false;
            break;
        case ExtraItem::None: return;
    }
    audio_.play(Sfx::PowerUp);
    startRumble(character, RumbleKind::Item);
}

void Game::handlePauseInput(MenuInput input) {
    const std::vector<int> items = enabledItemExtras();
    if (pauseItemsOpen_) {
        // Lista de items: confirmar (saltar) lo aplica y vuelve al juego.
        if (input == MenuInput::Back || items.empty()) {
            pauseItemsOpen_ = false;
            audio_.play(Sfx::Bump);
        } else if (input == MenuInput::Up || input == MenuInput::Down) {
            const int count = static_cast<int>(items.size());
            pauseItemSelection_ = (pauseItemSelection_ + (input == MenuInput::Up ? count - 1 : 1)) % count;
            audio_.play(Sfx::TimeTally);
        } else if (input == MenuInput::Confirm) {
            pauseItemsOpen_ = false;
            appState_ = AppState::Playing;
            audio_.resumeMusic();
            applyCheatItem(items[static_cast<size_t>(std::min<int>(pauseItemSelection_, static_cast<int>(items.size()) - 1))]);
            input1_.clearPresses();
            input2_.clearPresses();
        }
        return;
    }
    if (input == MenuInput::Up || input == MenuInput::Down) {
        // ITEMS solo aparece si hay algun truco de item activado.
        const int delta = input == MenuInput::Up ? kPauseCount - 1 : 1;
        do {
            pauseSelection_ = (pauseSelection_ + delta) % kPauseCount;
        } while (pauseSelection_ == kPauseItems && items.empty());
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
        case kPauseItems:
            audio_.play(Sfx::Coin);
            pauseItemsOpen_ = true;
            pauseItemSelection_ = 0;
            return;
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
    if ((marioHurt && !marioWasHurt_) || (luigiHurt && !luigiWasHurt_)) {
        campaignNoDamage_ = false;
        levelUndamaged_ = false;
    }
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
    // Logro "sin recibir dano": solo una partida que arranca en el 1-1 (no
    // ELEGIR MUNDO); continuando un espacio, lo que traia ese espacio.
    campaignNoDamage_ = startWorld_ == 1 && startLevel_ == 1 && !worldSelectFlow_;
    campaignNoSkip_ = campaignNoDamage_;
    campaignNoStar_ = campaignNoDamage_;
    campaignNoMushroom_ = campaignNoDamage_;
    campaignNoFlower_ = campaignNoDamage_;
    campaignAllHammerBros_ = campaignNoDamage_;
    defeatedHammerBros_.clear();
    campaignTime_ = 0.0f;
    campaignOnlyLuigi_ = campaignNoDamage_ && !twoPlayers && asLuigi;
    campaignEnemies_ = 0;
    campaignDeaths_ = {};
    wasDying_ = {};
    setPlayAsLuigi(!twoPlayers && asLuigi);
    twoPlayerMode_ = twoPlayers;
    campaignWorld_ = startWorld_;
    campaignLevel_ = startLevel_;
    customLevelPath_ = campaignLevelPath(campaignWorld_, campaignLevel_);
    customStartCol_ = -1;
    startScenario(TestScenario::CustomLevel);
    campaign_ = true;
}

void Game::countHiddenOneUps() {
    // VIDAS INFINITAS: cuantos 1-UP hay en los bloques de la campana (los de
    // los bloques invisibles y los de los ladrillos), leidos de los niveles.
    gHiddenOneUpTotal = 0;
    for (int world = 1; world <= 8; ++world) {
        for (int level = 1; level <= 4; ++level) {
            Tilemap map;
            if (!map.loadFromFile(campaignLevelPath(world, level))) continue;
            for (const LevelBlock& block : map.blocks()) {
                if (block.item == "one_up") ++gHiddenOneUpTotal;
            }
        }
    }
}

void Game::noteOneUpFound(int col, int row) {
    if (!campaign_ || campaignWorld_ < 1 || campaignWorld_ > 8) return;
    const std::string id = std::to_string(campaignWorld_) + "-" + std::to_string(campaignLevel_) + ":" +
                           std::to_string(col) + "," + std::to_string(row);
    auto& found = settings_.foundOneUps;
    if (std::find(found.begin(), found.end(), id) != found.end()) return;
    found.push_back(id);
    checkExtraProgress();
}

void Game::markLevelCleared() {
    // Pasado en una partida (no en el editor): queda para ELEGIR MUNDO.
    if (!campaign_ || campaignWorld_ < 1 || campaignWorld_ > 8) return;
    if (campaignWorld_ == 1 && campaignLevel_ == 1) completeAchievement("first_step");
    if (campaignWorld_ == 1 && campaignLevel_ == 4) completeAchievement("first_castle");
    if (campaignLevel_ == 4 && coopWorldRun_) completeAchievement("teamwork");
    if (!levelCleared(campaignWorld_, campaignLevel_)) {
        settings_.clearedLevels.push_back(std::to_string(campaignWorld_) + "-" + std::to_string(campaignLevel_));
    }
}

void Game::onLevelCleared() {
    markLevelCleared();
    checkAllHammerBrosCleared();
    settings_.save();   // la alcancia (las monedas del nivel)
    // Logro de la estrella: este nivel de la campana, pasado sin dano.
    if (campaign_ && levelUndamaged_ && campaignWorld_ >= 1 && campaignWorld_ <= 8) {
        const std::string id = std::to_string(campaignWorld_) + "-" + std::to_string(campaignLevel_);
        auto& done = settings_.flawlessLevels;
        if (std::find(done.begin(), done.end(), id) == done.end()) {
            done.push_back(id);
            settings_.save();
            checkExtraProgress();
        }
    }
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
            awardCompletionAchievements();
            settings_.gameCompleted = true;
            if (currentSlot_ >= 0) settings_.saves[static_cast<size_t>(currentSlot_)].world = 0;
            settings_.save();
            summaryPending_ = campaign_;
        }
        appState_ = AppState::DemoEnd;
        audio_.playMusic(Music::Ending, false);
        return;
    }
    campaignWorld_ = world;
    campaignLevel_ = level;
    customLevelPath_ = next;
    checkpointCol_ = -1;
    // 2 a la vez: el que se habia quedado sin vidas vuelve con 1.
    for (Mario* character : {&mario_, &luigi_}) {
        if (!character->isBenched()) continue;
        character->setBenched(false);
        livesOf(*character) = 1;
    }
    loadScenario(TestScenario::CustomLevel);  // conserva puntaje, monedas y vidas
}

// ============================================================ dibujo

void Game::drawCenteredFit(int y, const std::string& text, SDL_Color color) {
    // Con la fuente del juego; si no entra en un renglon, en dos (cortando
    // en el espacio mas cercano a la mitad).
    if (DebugFont::length(text) * 8 <= gLogicalWidth - 8) {
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
    const int width = DebugFont::length(text) * 4 * scale;
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

void Game::renderExtraContent() {
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 245);
    SDL_RenderFillRect(renderer_, nullptr);
    switch (extrasScreen_) {
        case 0: renderAchievements(); break;
        case 1: renderShop(); break;
        default: renderMyExtras(); break;
    }
}

namespace {
// Lista de a 8 renglones (16 px: entran los iconos) que se desplaza con la
// seleccion: el primero visible.
constexpr int kListVisible = 8;
constexpr int kListStep = 16;
int listFirst(int selection, int count) {
    return std::clamp(selection - kListVisible / 2, 0, std::max(0, count - kListVisible));
}
}  // namespace

void Game::renderAchievements() {
    // LOGROS: los cumplidos en amarillo con su premio; los demas, su avance.
    int done = 0;
    for (const Achievement& achievement : kAchievements) done += settings_.achievementDone(achievement.id) ? 1 : 0;
    drawCentered(6, std::string(tr(Txt::Achievements)) + "  " + std::to_string(done) + "/" +
                        std::to_string(kAchievementCount), 2, kMenuWhite);
    const std::string bank = "x" + std::to_string(settings_.coinBank);
    drawCoinAmount((gLogicalWidth - 8 - static_cast<int>(bank.size()) * 8) / 2, 19, settings_.coinBank, kMenuYellow);
    const int listX = gLogicalWidth / 2 - 96;
    const int right = gLogicalWidth / 2 + 116;
    const int first = listFirst(extraContentSelection_, kAchievementCount);
    for (int i = first; i < std::min(kAchievementCount, first + kListVisible); ++i) {
        const Achievement& achievement = kAchievements[i];
        const bool isDone = settings_.achievementDone(achievement.id);
        const int y = 36 + (i - first) * kListStep;
        if (i == extraContentSelection_) titleCursorSprite_.draw(renderer_, 0, 0, listX - 30, y, false);
        achievementIconsSprite_.draw(renderer_, 0, achievement.icon, listX - 18, y - 4, false);
        DebugFont::drawText(renderer_, listX, y, isEnglish() ? achievement.nameEn : achievement.nameEs, 2,
                            isDone ? kMenuYellow : kMenuWhite);
        const int target = extraTarget(achievement);
        std::string tail;
        if (!isDone && achievement.progress != ExtraProgress::None && target > 0) {
            tail = std::to_string(std::min(target, extraProgressValue(settings_, achievement.progress))) + "/" +
                   std::to_string(target);
        }
        // El avance a la derecha, si entra al lado del nombre (si no, en el cuadro).
        const int nameEnd = listX + DebugFont::length(isEnglish() ? achievement.nameEn : achievement.nameEs) * 8;
        if (!tail.empty() && nameEnd + 8 <= right - DebugFont::length(tail) * 8) {
            DebugFont::drawText(renderer_, right - DebugFont::length(tail) * 8, y, tail, 2, kMenuGray);
        }
    }
    // Cuadro de abajo: la condicion, el premio y lo que desbloquea.
    const Achievement& achievement = kAchievements[extraContentSelection_];
    const SDL_Rect box{(gLogicalWidth - 248) / 2, 162, 248, 56};
    SDL_SetRenderDrawColor(renderer_, 20, 20, 40, 255);
    SDL_RenderFillRect(renderer_, &box);
    SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer_, &box);
    const bool isDone = settings_.achievementDone(achievement.id);
    // Arriba a la izquierda: CUMPLIDO!, el avance (los de cantidad) o "PARA
    // CUMPLIRLO:"; a la derecha, el premio.
    {
        const int target = extraTarget(achievement);
        std::string head = tr(isDone ? Txt::AchDone : Txt::AchHowTo);
        if (!isDone && achievement.progress != ExtraProgress::None && target > 0) {
            head = std::to_string(std::min(target, extraProgressValue(settings_, achievement.progress))) + "/" +
                   std::to_string(target);
        }
        DebugFont::drawText(renderer_, box.x + 6, box.y + 4, head, 2, isDone ? kMenuYellow : kMenuDim);
    }
    const std::string reward = std::string(tr(Txt::AchReward)) + " ";
    const int rewardX = box.x + box.w - 6 - (DebugFont::length(reward) * 8 + 8 + 8 +
                                             8 * static_cast<int>(std::to_string(achievement.reward).size()));
    DebugFont::drawText(renderer_, rewardX, box.y + 4, reward, 2, kMenuWhite);
    drawCoinAmount(rewardX + DebugFont::length(reward) * 8, box.y + 4, achievement.reward, kMenuYellow);
    const auto lines = wrapText(isEnglish() ? achievement.condEn : achievement.condEs, 30);
    for (size_t l = 0; l < lines.size() && l < 2; ++l) {
        DebugFont::drawText(renderer_, box.x + 6, box.y + 17 + static_cast<int>(l) * 11, lines[l], 2, kMenuWhite);
    }
    if (achievement.unlocks) {
        const ExtraContent* unlocked = findExtra(achievement.unlocks);
        if (unlocked) {
            DebugFont::drawText(renderer_, box.x + 6, box.y + 42,
                                std::string(tr(Txt::AchUnlocks)) + " " + (isEnglish() ? unlocked->nameEn : unlocked->nameEs),
                                2, kMenuYellow);
        }
    }
    drawCentered(222, tr(Txt::BackHint), 1, kMenuDim);
}

void Game::renderShop() {
    // TIENDA: lo desbloqueado con su precio (gris si no alcanza); lo que
    // todavia no, "?????" y el logro que lo desbloquea.
    drawCentered(6, tr(Txt::Shop), 2, kMenuWhite);
    const std::string bank = "x" + std::to_string(settings_.coinBank);
    drawCoinAmount((gLogicalWidth - 8 - static_cast<int>(bank.size()) * 8) / 2, 19, settings_.coinBank, kMenuYellow);
    const std::vector<int> shop = shopExtras();
    const int count = static_cast<int>(shop.size());
    const int listX = gLogicalWidth / 2 - 96;
    const int right = gLogicalWidth / 2 + 116;
    const int first = listFirst(extraContentSelection_, count);
    for (int i = first; i < std::min(count, first + kListVisible); ++i) {
        const ExtraContent& item = kExtraContent[shop[static_cast<size_t>(i)]];
        const bool unlocked = settings_.extraUnlocked(item.id);
        const bool bought = settings_.extraPurchased(item.id);
        const int y = 36 + (i - first) * kListStep;
        if (i == extraContentSelection_) titleCursorSprite_.draw(renderer_, 0, 0, listX - 30, y, false);
        if (!unlocked) {
            DebugFont::drawText(renderer_, listX, y, "?????", 2, kMenuGray);
            continue;
        }
        drawExtraIcon(item.id, listX - 18, y - 4);
        const std::string name = isEnglish() ? item.nameEn : item.nameEs;
        DebugFont::drawText(renderer_, listX, y, name, 2, bought ? kMenuGray : kMenuWhite);
        const std::string price = std::to_string(item.price);
        if (!bought && listX + DebugFont::length(name) * 8 + 8 <= right - static_cast<int>(price.size()) * 8) {
            DebugFont::drawText(renderer_, right - static_cast<int>(price.size()) * 8, y, price, 2,
                                settings_.coinBank >= item.price ? kMenuYellow : kMenuGray);
        }
    }
    const SDL_Rect box{(gLogicalWidth - 248) / 2, 162, 248, 56};
    SDL_SetRenderDrawColor(renderer_, 20, 20, 40, 255);
    SDL_RenderFillRect(renderer_, &box);
    SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer_, &box);
    if (count > 0) {
        const ExtraContent& item = kExtraContent[shop[static_cast<size_t>(extraContentSelection_)]];
        if (!settings_.extraUnlocked(item.id)) {
            DebugFont::drawText(renderer_, box.x + 6, box.y + 5, tr(Txt::ShopLockedBy), 2, kMenuDim);
            const Achievement* by = achievementUnlocking(item.id);
            if (by) {
                DebugFont::drawText(renderer_, box.x + 6, box.y + 19, isEnglish() ? by->nameEn : by->nameEs, 2,
                                    kMenuYellow);
            }
        } else {
            const auto lines = wrapText(isEnglish() ? item.descEn : item.descEs, 30);
            for (size_t l = 0; l < lines.size() && l < 2; ++l) {
                DebugFont::drawText(renderer_, box.x + 6, box.y + 5 + static_cast<int>(l) * 11, lines[l], 2, kMenuWhite);
            }
            if (settings_.extraPurchased(item.id)) {
                DebugFont::drawText(renderer_, box.x + 6, box.y + 40, tr(Txt::ShopBought), 2, kMenuGray);
            } else {
                const std::string priceLabel = std::string(tr(Txt::ShopPrice)) + " ";
                DebugFont::drawText(renderer_, box.x + 6, box.y + 40, priceLabel, 2, kMenuWhite);
                drawCoinAmount(box.x + 6 + DebugFont::length(priceLabel) * 8, box.y + 40, item.price,
                               settings_.coinBank >= item.price ? kMenuYellow : kMenuGray);
                if (settings_.coinBank < item.price) {
                    const std::string missing = std::string(tr(Txt::ShopMissing)) + " " +
                                                std::to_string(item.price - settings_.coinBank);
                    DebugFont::drawText(renderer_, box.x + box.w - 6 - DebugFont::length(missing) * 8, box.y + 40,
                                        missing, 2, kMenuGray);
                }
            }
        }
    }
    drawCentered(222, tr(Txt::ShopHint), 1, kMenuDim);
    if (confirmingPurchase_ && count > 0) {
        const ExtraContent& item = kExtraContent[shop[static_cast<size_t>(extraContentSelection_)]];
        const SDL_Rect ask{(gLogicalWidth - 220) / 2, 90, 220, 62};
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 245);
        SDL_RenderFillRect(renderer_, &ask);
        SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer_, &ask);
        drawCentered(98, tr(Txt::ShopConfirm), 2, kMenuWhite);
        const std::string amount = "x" + std::to_string(item.price) + "?";
        drawCoinAmount((gLogicalWidth - 8 - static_cast<int>(amount.size()) * 8) / 2, 110, item.price, kMenuYellow, "?");
        const int yesX = gLogicalWidth / 2 - 48;
        const int noX = gLogicalWidth / 2 + 24;
        DebugFont::drawText(renderer_, yesX, 132, tr(Txt::Yes), 2, kMenuWhite);
        DebugFont::drawText(renderer_, noX, 132, tr(Txt::No), 2, kMenuWhite);
        titleCursorSprite_.draw(renderer_, 0, 0, (purchaseChoice_ == 0 ? yesX : noX) - 12, 132, false);
    }
}

void Game::renderMyExtras() {
    // MIS EXTRAS: lo comprado, para activar o desactivar.
    drawCentered(6, tr(Txt::MyExtras), 2, kMenuWhite);
    const std::vector<int> mine = toggleableExtras(settings_);
    const int count = static_cast<int>(mine.size());
    if (count == 0) {
        drawCentered(100, tr(Txt::MyExtrasEmpty), 2, kMenuGray);
        drawCentered(222, tr(Txt::BackHint), 1, kMenuDim);
        return;
    }
    const int listX = gLogicalWidth / 2 - 96;
    const int right = gLogicalWidth / 2 + 116;
    const int first = listFirst(extraContentSelection_, count);
    for (int i = first; i < std::min(count, first + kListVisible); ++i) {
        const ExtraContent& item = kExtraContent[mine[static_cast<size_t>(i)]];
        const bool on = settings_.extraEnabled(item.id);
        const int y = 36 + (i - first) * kListStep;
        if (i == extraContentSelection_) titleCursorSprite_.draw(renderer_, 0, 0, listX - 30, y, false);
        drawExtraIcon(item.id, listX - 18, y - 4);
        DebugFont::drawText(renderer_, listX, y, isEnglish() ? item.nameEn : item.nameEs, 2, kMenuWhite);
        const std::string state = on ? "ON" : "OFF";
        DebugFont::drawText(renderer_, right - static_cast<int>(state.size()) * 8, y, state, 2,
                            on ? kMenuYellow : kMenuGray);
    }
    const ExtraContent& item = kExtraContent[mine[static_cast<size_t>(extraContentSelection_)]];
    const SDL_Rect box{(gLogicalWidth - 248) / 2, 162, 248, 56};
    SDL_SetRenderDrawColor(renderer_, 20, 20, 40, 255);
    SDL_RenderFillRect(renderer_, &box);
    SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer_, &box);
    const auto lines = wrapText(isEnglish() ? item.descEn : item.descEs, 30);
    for (size_t l = 0; l < lines.size() && l < 2; ++l) {
        DebugFont::drawText(renderer_, box.x + 6, box.y + 5 + static_cast<int>(l) * 11, lines[l], 2, kMenuWhite);
    }
    const bool on = settings_.extraEnabled(item.id);
    DebugFont::drawText(renderer_, box.x + 6, box.y + 40, tr(on ? Txt::ExtrasOn : Txt::ExtrasOff), 2,
                        on ? kMenuYellow : kMenuGray);
    drawCentered(222, tr(Txt::MyExtrasHint), 1, kMenuDim);
}

int Game::drawCoinAmount(int x, int y, int amount, SDL_Color color, const std::string& suffix) {
    // Icono de moneda (el del HUD, girando) y "x" cantidad.
    hudCoinSprite_.draw(renderer_, 0, questionBlockAnimFrame_, x, y, false);
    const std::string text = "x" + std::to_string(amount) + suffix;
    DebugFont::drawText(renderer_, x + 8, y, text, 2, color);
    return 8 + static_cast<int>(text.size()) * 8;
}

int Game::finalScore(int player) const {
    // Puntaje de cada jugador: por turnos, el de su estado; de a 2, el suyo;
    // en 1 jugador, todo es del primero.
    if (turnMode_) return player == turnPlayer_ ? score_ : turnStates_[static_cast<size_t>(player)].score;
    if (twoPlayerMode_) return playerScore_[static_cast<size_t>(player)];
    return player == 0 ? score_ : 0;
}

std::string Game::playerLabel(int player) const {
    if (summary_.twoPlayers) return player == 0 ? "MARIO" : "LUIGI";
    return summary_.luigi ? "LUIGI" : "MARIO";
}

void Game::openSummary(bool completed) {
    // Se termino la partida: el resumen (y despues el nombre para la tabla
    // de quien entre en ella, y la tabla).
    saveTopScore();
    summary_ = SummaryData{};
    summary_.enemies = campaignEnemies_;
    summary_.deaths = campaignDeaths_;
    summary_.twoPlayers = twoPlayerMode_ || turnMode_;
    summary_.luigi = playAsLuigi_;
    summary_.scores = {finalScore(0), summary_.twoPlayers ? finalScore(1) : 0};
    summary_.time = campaignTime_;
    summary_.plus = gPrimaryHardMode;
    pendingNames_.clear();
    for (int player = 0; player < (summary_.twoPlayers ? 2 : 1); ++player) {
        if (summary_.scores[static_cast<size_t>(player)] > 0) pendingNames_.push_back(player);
    }
    // Quien no entra en la tabla (llena y con un puntaje menor) no escribe nombre.
    const auto& table = settings_.highScores;
    pendingNames_.erase(std::remove_if(pendingNames_.begin(), pendingNames_.end(),
                                       [&](int player) {
                                           return static_cast<int>(table.size()) >= Settings::kMaxHighScores &&
                                                  summary_.scores[static_cast<size_t>(player)] <= table.back().score;
                                       }),
                        pendingNames_.end());
    campaign_ = false;
    summaryCompleted_ = completed;
    summaryStage_ = 0;
    highlightScore_ = -1;
    appState_ = AppState::Summary;
}

void Game::startNameEntry() {
    summaryStage_ = 1;
    nameEntry_ = playerLabel(pendingNames_.front());
    nameCursor_ = static_cast<int>(nameEntry_.size());
}

void Game::commitName() {
    std::string name = nameEntry_;
    while (!name.empty() && name.back() == ' ') name.pop_back();
    while (!name.empty() && name.front() == ' ') name.erase(name.begin());
    if (name.empty()) name = "???";
    const int player = pendingNames_.front();
    pendingNames_.erase(pendingNames_.begin());
    auto& table = settings_.highScores;
    const Settings::HighScore entry{name, summary_.scores[static_cast<size_t>(player)], summary_.plus};
    auto it = std::find_if(table.begin(), table.end(), [&](const Settings::HighScore& row) { return row.score < entry.score; });
    highlightScore_ = static_cast<int>(it - table.begin());
    table.insert(it, entry);
    if (static_cast<int>(table.size()) > Settings::kMaxHighScores) table.resize(Settings::kMaxHighScores);
    if (highlightScore_ >= Settings::kMaxHighScores) highlightScore_ = -1;
    settings_.save();
    audio_.play(Sfx::Coin);
    if (!pendingNames_.empty()) {
        startNameEntry();
    } else {
        summaryStage_ = 2;
    }
}

namespace {
constexpr char kNameChars[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-";
constexpr int kNameMax = 8;
}  // namespace

bool Game::handleNameTyping(const SDL_Event& event) {
    if (event.type != SDL_KEYDOWN) return false;
    const SDL_Keycode key = event.key.keysym.sym;
    char typed = 0;
    if (key >= SDLK_a && key <= SDLK_z) typed = static_cast<char>('A' + (key - SDLK_a));
    if (key >= SDLK_0 && key <= SDLK_9) typed = static_cast<char>(key);
    if (key == SDLK_MINUS) typed = '-';
    if (key == SDLK_SPACE) typed = ' ';
    if (typed == 0) return false;
    if (nameCursor_ >= kNameMax) return true;
    if (nameCursor_ < static_cast<int>(nameEntry_.size())) {
        nameEntry_[static_cast<size_t>(nameCursor_)] = typed;
    } else {
        nameEntry_.push_back(typed);
    }
    nameCursor_ = std::min(nameCursor_ + 1, kNameMax);
    audio_.play(Sfx::TimeTally);
    return true;
}

void Game::handleSummaryInput(MenuInput input) {
    if (summaryStage_ == 0) {
        if (input != MenuInput::Confirm && input != MenuInput::Back) return;
        audio_.play(Sfx::Coin);
        if (!pendingNames_.empty()) {
            startNameEntry();
        } else {
            summaryStage_ = 2;
        }
        return;
    }
    if (summaryStage_ == 2) {
        if (input != MenuInput::Confirm && input != MenuInput::Back) return;
        if (summaryCompleted_) {
            // Termino el juego: los creditos, y de ahi al titulo.
            audio_.play(Sfx::Coin);
            appState_ = AppState::Credits;
        } else {
            openTitle();
        }
        return;
    }
    // Nombre: arriba/abajo cambia la letra, izquierda/derecha mueve,
    // atras borra y confirmar lo guarda.
    const std::string chars = kNameChars;
    switch (input) {
        case MenuInput::Up:
        case MenuInput::Down: {
            if (nameCursor_ >= static_cast<int>(nameEntry_.size())) {
                if (nameCursor_ >= kNameMax) nameCursor_ = kNameMax - 1;
                while (static_cast<int>(nameEntry_.size()) <= nameCursor_) nameEntry_.push_back(' ');
            }
            char& c = nameEntry_[static_cast<size_t>(nameCursor_)];
            size_t index = chars.find(c);
            if (index == std::string::npos) index = 0;
            const int count = static_cast<int>(chars.size());
            index = static_cast<size_t>((static_cast<int>(index) + (input == MenuInput::Up ? 1 : count - 1)) % count);
            c = chars[index];
            audio_.play(Sfx::TimeTally);
            break;
        }
        case MenuInput::Left:
            if (nameCursor_ > 0) --nameCursor_;
            break;
        case MenuInput::Right:
            if (nameCursor_ < static_cast<int>(nameEntry_.size()) && nameCursor_ < kNameMax - 1) ++nameCursor_;
            break;
        case MenuInput::Back:
            if (nameCursor_ > 0) {
                --nameCursor_;
                nameEntry_.erase(static_cast<size_t>(nameCursor_), 1);
                audio_.play(Sfx::Bump);
            }
            break;
        case MenuInput::Confirm: commitName(); break;
        default: break;
    }
}

namespace {
std::string formatPlayTime(float seconds) {
    const int total = static_cast<int>(seconds);
    char text[16];
    if (total >= 3600) {
        std::snprintf(text, sizeof(text), "%d:%02d:%02d", total / 3600, (total / 60) % 60, total % 60);
    } else {
        std::snprintf(text, sizeof(text), "%02d:%02d", total / 60, total % 60);
    }
    return text;
}
}  // namespace

void Game::renderSummary() {
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderFillRect(renderer_, nullptr);
    if (summaryStage_ == 2) {
        renderHighScores(30);
        return;
    }
    if (summaryStage_ == 1) {
        // Nombre para la tabla.
        const int player = pendingNames_.front();
        drawCentered(50, tr(Txt::EnterName), 2, kMenuYellow);
        drawCentered(74, playerLabel(player) + "  " + std::to_string(summary_.scores[static_cast<size_t>(player)]), 2,
                     kMenuWhite);
        const int x0 = (gLogicalWidth - kNameMax * 16) / 2;
        for (int i = 0; i < kNameMax; ++i) {
            const char c = i < static_cast<int>(nameEntry_.size()) ? nameEntry_[static_cast<size_t>(i)] : ' ';
            const int x = x0 + i * 16 + 4;
            DebugFont::drawText(renderer_, x, 110, std::string(1, c == ' ' ? ' ' : c), 2,
                                i == nameCursor_ ? kMenuYellow : kMenuWhite);
            const SDL_Color line = i == nameCursor_ ? kMenuYellow : kMenuGray;
            SDL_SetRenderDrawColor(renderer_, line.r, line.g, line.b, 255);
            const SDL_Rect under{x - 1, 120, 10, 2};
            SDL_RenderFillRect(renderer_, &under);
        }
        drawCentered(222, tr(Txt::NameHint), 1, kMenuDim);
        return;
    }
    // El resumen.
    drawCentered(24, tr(Txt::SummaryTitle), 2, kMenuYellow);
    if (summary_.plus) drawCentered(38, tr(Txt::NewGamePlus), 2, kMenuWhite);
    const int left = gLogicalWidth / 2 - 112;
    const int right = gLogicalWidth / 2 + 112;
    int y = 60;
    const auto row = [&](const std::string& label, const std::string& value, SDL_Color color) {
        DebugFont::drawText(renderer_, left, y, label, 2, kMenuWhite);
        DebugFont::drawText(renderer_, right - DebugFont::length(value) * 8, y, value, 2, color);
        y += 16;
    };
    row(tr(Txt::SummaryEnemies), std::to_string(summary_.enemies), kMenuYellow);
    if (summary_.twoPlayers) {
        row(std::string(tr(Txt::SummaryDeaths)) + " MARIO", std::to_string(summary_.deaths[0]), kMenuYellow);
        row(std::string(tr(Txt::SummaryDeaths)) + " LUIGI", std::to_string(summary_.deaths[1]), kMenuYellow);
    } else {
        row(tr(Txt::SummaryDeaths), std::to_string(summary_.deaths[0]), kMenuYellow);
    }
    row(tr(Txt::SummaryTime), formatPlayTime(summary_.time), kMenuYellow);
    if (summary_.twoPlayers) {
        row(std::string(tr(Txt::SummaryScore)) + " MARIO", std::to_string(summary_.scores[0]), kMenuYellow);
        row(std::string(tr(Txt::SummaryScore)) + " LUIGI", std::to_string(summary_.scores[1]), kMenuYellow);
    } else {
        row(tr(Txt::SummaryScore), std::to_string(summary_.scores[0]), kMenuYellow);
    }
    drawCentered(222, tr(Txt::ContinueHint), 1, kMenuDim);
}

void Game::renderHighScores(int top) {
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderFillRect(renderer_, nullptr);
    drawCentered(top - 18, tr(Txt::HighScoresTitle), 2, kMenuYellow);
    const auto& table = settings_.highScores;
    if (table.empty()) {
        drawCentered(110, tr(Txt::HighScoresEmpty), 2, kMenuGray);
    }
    const int left = gLogicalWidth / 2 - 100;
    const int right = gLogicalWidth / 2 + 100;
    for (size_t i = 0; i < table.size(); ++i) {
        const int y = top + static_cast<int>(i) * 17;
        const SDL_Color color = static_cast<int>(i) == highlightScore_ ? kMenuYellow : kMenuWhite;
        char rank[8];
        std::snprintf(rank, sizeof(rank), "%2d.", static_cast<int>(i) + 1);
        DebugFont::drawText(renderer_, left, y, rank, 2, color);
        DebugFont::drawText(renderer_, left + 32, y, table[i].name, 2, color);
        std::string score = std::to_string(table[i].score);
        if (table[i].plus) score += "+";   // PARTIDA +
        DebugFont::drawText(renderer_, right - DebugFont::length(score) * 8, y, score, 2, color);
    }
    drawCentered(222, tr(Txt::ContinueHint), 1, kMenuDim);
}

namespace {
// ROCOLA: las musicas (loop: los temas que se repiten) y los sonidos.
struct JukeboxEntry {
    const char* es;
    const char* en;
    bool music;
    int id;
    bool loop;
};
constexpr JukeboxEntry kJukebox[] = {
    {"SUPERFICIE", "OVERWORLD", true, static_cast<int>(Music::Overworld), true},
    {"SUBTERRANEO", "UNDERGROUND", true, static_cast<int>(Music::Underground), true},
    {"AGUA", "UNDERWATER", true, static_cast<int>(Music::Underwater), true},
    {"CASTILLO", "CASTLE", true, static_cast<int>(Music::Castle), true},
    {"ESTRELLA", "STARMAN", true, static_cast<int>(Music::Invincibility), true},
    {"SUPERFICIE RAPIDA", "OVERWORLD FAST", true, static_cast<int>(Music::OverworldFast), true},
    {"SUBTERRANEO RAPIDA", "UNDERGROUND FAST", true, static_cast<int>(Music::UndergroundFast), true},
    {"AGUA RAPIDA", "UNDERWATER FAST", true, static_cast<int>(Music::UnderwaterFast), true},
    {"CASTILLO RAPIDA", "CASTLE FAST", true, static_cast<int>(Music::CastleFast), true},
    {"ESTRELLA RAPIDA", "STARMAN FAST", true, static_cast<int>(Music::InvincibilityFast), true},
    {"NIVEL COMPLETO", "COURSE CLEAR", true, static_cast<int>(Music::LevelComplete), false},
    {"CASTILLO COMPLETO", "CASTLE CLEAR", true, static_cast<int>(Music::CastleComplete), false},
    {"GAME OVER", "GAME OVER", true, static_cast<int>(Music::GameOver), false},
    {"AL CAÑO", "INTO THE PIPE", true, static_cast<int>(Music::IntoPipe), false},
    {"FINAL", "ENDING", true, static_cast<int>(Music::Ending), false},
    {"SALTO", "JUMP", false, static_cast<int>(Sfx::Jump), false},
    {"SALTO CHICO", "SMALL JUMP", false, static_cast<int>(Sfx::JumpSmall), false},
    {"MONEDA", "COIN", false, static_cast<int>(Sfx::Coin), false},
    {"GOLPE", "BUMP", false, static_cast<int>(Sfx::Bump), false},
    {"LADRILLO", "BRICK", false, static_cast<int>(Sfx::Brick), false},
    {"ITEM", "ITEM", false, static_cast<int>(Sfx::Item), false},
    {"CRECER", "POWER UP", false, static_cast<int>(Sfx::PowerUp), false},
    {"ACHICARSE", "POWER DOWN", false, static_cast<int>(Sfx::PowerDown), false},
    {"CAÑO", "PIPE", false, static_cast<int>(Sfx::Pipe), false},
    {"PISOTON", "STOMP", false, static_cast<int>(Sfx::StompKill), false},
    {"PATADA", "KICK", false, static_cast<int>(Sfx::Kick), false},
    {"MUERTE", "DEATH", false, static_cast<int>(Sfx::Death), false},
    {"VIDA", "1-UP", false, static_cast<int>(Sfx::OneUp), false},
    {"BOWSER CAE", "BOWSER FALLS", false, static_cast<int>(Sfx::BowserFall), false},
    {"BOLA DE FUEGO", "FIREBALL", false, static_cast<int>(Sfx::Fireball), false},
    {"FUEGO DE BOWSER", "BOWSER FIRE", false, static_cast<int>(Sfx::BowserFire), false},
    {"CAÑON", "CANNON", false, static_cast<int>(Sfx::BillFirework), false},
    {"BANDERA", "FLAGPOLE", false, static_cast<int>(Sfx::Flagpole), false},
    {"APURATE", "HURRY UP", false, static_cast<int>(Sfx::HurryUp), false},
    {"CUENTA DEL TIEMPO", "TIME TALLY", false, static_cast<int>(Sfx::TimeTally), false},
    {"GAME OVER (SONIDO)", "GAME OVER (SOUND)", false, static_cast<int>(Sfx::GameOver), false},
    {"PAUSA", "PAUSE", false, static_cast<int>(Sfx::Pause), false},
    {"ENREDADERA", "VINE", false, static_cast<int>(Sfx::Vine), false},
};
constexpr int kJukeboxCount = static_cast<int>(std::size(kJukebox));
}  // namespace

void Game::handleJukeboxInput(MenuInput input) {
    if (input == MenuInput::Up || input == MenuInput::Down) {
        const int delta = input == MenuInput::Up ? kJukeboxCount - 1 : 1;
        jukeboxSelection_ = (jukeboxSelection_ + delta) % kJukeboxCount;
    } else if (input == MenuInput::Confirm) {
        const JukeboxEntry& entry = kJukebox[jukeboxSelection_];
        if (entry.music) {
            audio_.playMusic(static_cast<Music>(entry.id), entry.loop);
            jukeboxPlaying_ = jukeboxSelection_;
        } else {
            audio_.play(static_cast<Sfx>(entry.id));
        }
    } else if (input == MenuInput::Back) {
        audio_.stopMusic();
        jukeboxPlaying_ = -1;
        jukeboxOpen_ = false;
        audio_.play(Sfx::Bump);
    }
}

void Game::renderJukebox() {
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 245);
    SDL_RenderFillRect(renderer_, nullptr);
    drawCentered(10, tr(Txt::Jukebox), 2, kMenuWhite);
    constexpr int kVisible = 11;
    const int first = std::clamp(jukeboxSelection_ - kVisible / 2, 0, std::max(0, kJukeboxCount - kVisible));
    const int listX = gLogicalWidth / 2 - 100;
    for (int i = first; i < std::min(kJukeboxCount, first + kVisible); ++i) {
        const JukeboxEntry& entry = kJukebox[i];
        const int y = 32 + (i - first) * 16;
        if (i == jukeboxSelection_) titleCursorSprite_.draw(renderer_, 0, 0, listX - 12, y, false);
        const SDL_Color color = i == jukeboxPlaying_ ? kMenuYellow : kMenuWhite;
        DebugFont::drawText(renderer_, listX, y, isEnglish() ? entry.en : entry.es, 2, color);
        const std::string kind = tr(entry.music ? Txt::JukeboxMusic : Txt::JukeboxSound);
        DebugFont::drawText(renderer_, gLogicalWidth / 2 + 116 - static_cast<int>(kind.size()) * 8, y, kind, 2, kMenuGray);
    }
    drawCentered(222, tr(Txt::JukeboxHint), 1, kMenuDim);
}

void Game::renderSlotPicker() {
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 240);
    SDL_RenderFillRect(renderer_, nullptr);
    drawCentered(6, tr(replacingSlot_ ? Txt::ChooseReplace : Txt::ChooseSlot), 2, kMenuWhite);
    const int boxW = 260;
    const int boxX = (gLogicalWidth - boxW) / 2;
    // PARTIDA NUEVA, siempre la primera.
    {
        const SDL_Rect box{boxX, 20, boxW, 18};
        const bool selected = slotChoice_ == -1 && !replacingSlot_;
        SDL_SetRenderDrawColor(renderer_, 20, 20, 40, 255);
        SDL_RenderFillRect(renderer_, &box);
        const SDL_Color border = selected ? kMenuYellow : SDL_Color{90, 90, 110, 255};
        SDL_SetRenderDrawColor(renderer_, border.r, border.g, border.b, 255);
        SDL_RenderDrawRect(renderer_, &box);
        const std::string label = tr(Txt::NewGame);
        DebugFont::drawText(renderer_, (gLogicalWidth - static_cast<int>(label.size()) * 8) / 2, 25, label, 2,
                            replacingSlot_ ? kMenuGray : selected ? kMenuYellow : kMenuWhite);
    }
    // Las guardadas (las vacias, en gris).
    for (int i = 0; i < Settings::kSaveSlots; ++i) {
        const Settings::SaveSlot& slot = settings_.saves[static_cast<size_t>(i)];
        const int y = 42 + i * 39;
        const SDL_Rect box{boxX, y, boxW, 36};
        const bool selected = i == slotChoice_;
        SDL_SetRenderDrawColor(renderer_, 20, 20, 40, 255);
        SDL_RenderFillRect(renderer_, &box);
        const SDL_Color border = selected ? kMenuYellow : SDL_Color{90, 90, 110, 255};
        SDL_SetRenderDrawColor(renderer_, border.r, border.g, border.b, 255);
        SDL_RenderDrawRect(renderer_, &box);
        // Vista previa: la franja de abajo de la primera pantalla (piso).
        const SDL_Rect thumb{boxX + 3, y + 3, 54, 30};
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
        const SDL_Color text = slot.world > 0 ? kMenuWhite : kMenuGray;
        const int tx = boxX + 64;
        std::string title = std::string(tr(Txt::SlotLabel)) + " " + std::to_string(i + 1);
        if (slot.world > 0) title += "  " + std::to_string(slot.world) + "-" + std::to_string(slot.level);
        DebugFont::drawText(renderer_, tx, y + 6, title, 2, selected ? kMenuYellow : text);
        if (slot.world <= 0) {
            DebugFont::drawText(renderer_, tx, y + 20, tr(Txt::SlotEmpty), 2, text);
            continue;
        }
        const Txt mode = slot.mode == "coop" ? Txt::ModeCoop : slot.mode == "turns" ? Txt::ModeTurns : Txt::OnePlayer;
        std::string line = tr(mode);
        if (slot.mode == "solo" && slot.luigi) line = "LUIGI";
        DebugFont::drawText(renderer_, tx, y + 20, line, 2, text);
        if (slot.secondQuest) {
            const std::string plus = tr(Txt::NewGamePlus);
            DebugFont::drawText(renderer_, boxX + boxW - 6 - static_cast<int>(plus.size()) * 8, y + 20, plus, 2, kMenuYellow);
        }
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
    bool hideTopScore = false;
    {
        std::vector<Txt> ids;
        std::vector<bool> disabled;
        int selection = 0;
        if (titleMenu_ == kMenuPlay) {
            ids = {Txt::OnePlayer, Txt::TwoPlayers, Txt::Extras};
            disabled.assign(ids.size(), false);
            selection = titleSelection_;
        } else if (titleMenu_ == kMenuExtras) {
            ids = {Txt::WorldSelect, Txt::NewGamePlus, Txt::LevelEditor, Txt::Jukebox,
                   Txt::Achievements, Txt::Shop, Txt::MyExtras};
            disabled = extrasDisabled(settings_);
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
            ids = {Txt::Play, Txt::Options, Txt::HighScoresMenu, Txt::Quit};
            disabled.assign(ids.size(), false);
            selection = mainSelection_;
        }
        std::vector<std::string> labels;
        for (Txt id : ids) labels.push_back(tr(id));
        int widest = 0;
        for (const std::string& label : labels) widest = std::max(widest, static_cast<int>(label.size()) * 8);
        const int x = (gLogicalWidth - widest) / 2;
        // Con mas de 4 opciones mas juntas; con mas de 5 (EXTRAS) no entra
        // el TOP- abajo y se saca.
        const int step = ids.size() > 5 ? 11 : ids.size() > 4 ? 12 : 14;
        hideTopScore = ids.size() > 5;
        for (int i = 0; i < static_cast<int>(ids.size()); ++i) {
            const int y = (hideTopScore ? 131 : 134) + i * step;
            if (i == selection) titleCursorSprite_.draw(renderer_, 0, 0, x - 12, y, false);
            DebugFont::drawText(renderer_, x, y, labels[static_cast<size_t>(i)], 2,
                                disabled[static_cast<size_t>(i)] ? kMenuGray : kMenuWhite);
        }
    }

    if (secondQuestNext_) {
        DebugFont::drawText(renderer_, logoX, 122, tr(Txt::SecondQuest), 2, pink);
    }
    char top[24];
    const int best = std::min(std::max(settings_.topScore, score_), 999999);
    std::snprintf(top, sizeof(top), "TOP- %06d", best);
    // Con el nombre de quien lo tiene (el primero de la tabla de puntajes,
    // si es ese mismo record).
    std::string topLine = top;
    if (!settings_.highScores.empty() && settings_.highScores.front().score >= best) {
        topLine += " " + settings_.highScores.front().name;
    }
    if (!hideTopScore) drawCentered(196, topLine, 2, kMenuWhite);

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
    if (extraContentOpen_) renderExtraContent();
    if (jukeboxOpen_) renderJukebox();
}

void Game::renderPauseMenu() {
    // Velo oscuro sobre el juego congelado + caja con las opciones.
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 150);
    SDL_RenderFillRect(renderer_, nullptr);
    // Con la lista de items, un poco mas ancha (icono + nombre largo).
    const int boxW = pauseItemsOpen_ ? 200 : 180;
    const SDL_Rect box{(gLogicalWidth - boxW) / 2, 70, boxW, 100};
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 220);
    SDL_RenderFillRect(renderer_, &box);
    SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer_, &box);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
    drawCentered(78, tr(Txt::Paused), 2, kMenuWhite);
    const std::vector<int> cheats = enabledItemExtras();
    if (pauseItemsOpen_) {
        // Los items activados (CONTENIDO EXTRA): su icono y su nombre.
        // La estrella infinita titila entre los colores de la estrella.
        const int starCycle = static_cast<int>(SDL_GetTicks() / 70) % 4;
        for (size_t i = 0; i < cheats.size(); ++i) {
            const ExtraContent& item = kExtraContent[cheats[i]];
            std::string name = isEnglish() ? item.nameEn : item.nameEs;
            const size_t colon = name.find(": ");
            if (colon != std::string::npos) name = name.substr(colon + 2);   // sin "ITEM: "
            const int y = 96 + static_cast<int>(i) * 18;
            const bool selected = static_cast<int>(i) == pauseItemSelection_;
            if (selected) DebugFont::drawText(renderer_, box.x + 6, y + 4, ">", 2, kMenuWhite);
            const int iconX = box.x + 16;
            switch (item.item) {
                case ExtraItem::Mushroom: mushroomSprite_.draw(renderer_, 0, 0, iconX, y, false); break;
                case ExtraItem::FireFlower: fireFlowerSprite_.draw(renderer_, 0, 0, iconX, y, false); break;
                case ExtraItem::Star: starSprite_.draw(renderer_, 0, 2, iconX, y, false); break;   // la amarilla
                case ExtraItem::InfiniteStar: starSprite_.draw(renderer_, 0, starCycle, iconX, y, false); break;
                case ExtraItem::HammerSuit: hammerFlowerSprite_.draw(renderer_, 0, 0, iconX, y - 4, false); break;
                case ExtraItem::BoomerangSuit:
                    boomerangFlowerSprite_.draw(renderer_, 0, 0, iconX - 2, y - 4, false);
                    break;
                case ExtraItem::None: break;
            }
            DebugFont::drawText(renderer_, box.x + 36, y + 4, name, 2, selected ? kMenuWhite : kMenuYellow);
        }
        return;
    }
    // ITEMS va entre REINICIAR y OPCIONES, solo con trucos de items activados.
    std::vector<std::string> items{tr(Txt::Resume), tr(Txt::RestartLevel)};
    if (!cheats.empty()) items.push_back(tr(Txt::PauseItems));
    items.push_back(tr(Txt::Options));
    items.push_back(tr(Txt::QuitToMenu));
    int shown = pauseSelection_;
    if (cheats.empty() && pauseSelection_ > kPauseItems) --shown;
    drawMenuList(box.x + 24, 96, 14, items, shown);
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
        "DISEÑO Y DESARROLLO",
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
