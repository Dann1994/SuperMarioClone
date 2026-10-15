#include "LevelEditor.h"

#include <SDL_image.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <tuple>

#include "../render/DebugFont.h"
#include "../render/Palettes.h"
#include "../world/WorldType.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#ifndef MARIO_SOURCE_DIR
#define MARIO_SOURCE_DIR "."
#endif

namespace {

namespace fs = std::filesystem;

constexpr int kLevelRows = 16;
constexpr int kNewLevelWidth = 64;
constexpr size_t kMaxUndo = 100;
constexpr int kSubAreaWidth = 32;
// Alto del mastil de la bandera de meta (bola incluida), como el del 1-1.
constexpr int kFlagpoleHeight = 10;

// Piezas de escenografia: mismo orden/tamano que kDecorations en Game.cpp.
struct DecorationInfo {
    const char* type;
    const char* label;
    int width;
    int height;
};
constexpr DecorationInfo kDecorations[] = {
    {"castle_small", "CASTILLO", 80, 80},     {"hill_big", "COLINA GRANDE", 80, 48},
    {"hill_small", "COLINA CHICA", 48, 32},   {"bush_big", "ARBUSTO GRANDE", 80, 16},
    {"bush_small", "ARBUSTO CHICO", 48, 16},  {"cloud_big", "NUBE GRANDE", 64, 32},
    {"cloud_small", "NUBE CHICA", 48, 32},    {"pipe_side", "TUBERIA EN L", 64, 64},
    {"castle_big", "CASTILLO GRANDE", 144, 176}, {"fence", "CERCO", 16, 16},
    {"tree_short", "ARBOL BAJO", 16, 32},     {"tree_tall", "ARBOL ALTO", 16, 48},
    {"castle_wall", "MURALLA", 16, 96},
};

// Contenidos posibles de un bloque '?'/'D'/'H' (campo "item" de "blocks").
const char* const kBlockItems[] = {"coin", "powerup", "mushroom", "fire_flower", "star", "one_up"};
std::string blockItemShort(const std::string& item) {
    if (item == "powerup") return "P";
    if (item == "mushroom") return "M";
    if (item == "fire_flower") return "F";
    if (item == "star") return "E";
    if (item == "one_up") return "1UP";
    return "C";
}

const char* const kWorlds[] = {"overworld", "underground", "underwater", "castle"};

WorldType worldTypeFromName(const std::string& name) {
    if (name == "underground") return WorldType::Underground;
    if (name == "underwater") return WorldType::Underwater;
    if (name == "castle") return WorldType::Castle;
    return WorldType::Overworld;
}

std::string upper(std::string text) {
    for (char& c : text) {
        // 'x' minuscula es un glifo propio de la fuente (el del contador).
        if (c != 'x') c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return text;
}

bool isBlockTile(char tile) { return tile == '?' || tile == 'D' || tile == 'H'; }

const char* const kTabNames[] = {"TERRENO", "ITEMS", "ENEMIGOS", "OBJETOS"};
const char* const kButtonNames[] = {"MOVER",  "GOMA",       "SELECCION", "DESHACER", "BORRAR TODO", "PROBAR",
                                    "PROBAR ACA", "SUBAREA", "NIVELES",   "GUARDAR",  "COMANDOS",    "PALETA", "MUNDO"};
// Botones del cartel de cambios sin guardar.
const char* const kLeaveButtons[] = {"GUARDAR Y SALIR", "SALIR SIN GUARDAR", "CANCELAR"};

// Nombre del tipo de mundo para el boton MUNDO.
const char* worldLabel(const std::string& world) {
    if (world == "overworld") return "SUPERFICIE";
    if (world == "night") return "NOCHE";
    if (world == "snow") return "NIEVE";
    if (world == "night_snow") return "NOCHE NEVADA";
    if (world == "underground") return "SUBTERRANEO";
    if (world == "underwater") return "AGUA";
    if (world == "castle") return "CASTILLO";
    return "AUTOMATICO";
}

// Variantes de colores en el orden del boton PALETA.
const char* const kPalettes[] = {"", "night", "snow", "night_snow", "gray", "mushroom"};

const char* paletteLabel(const std::string& palette) {
    if (palette == "night") return "NOCHE";
    if (palette == "snow") return "NIEVE";
    if (palette == "night_snow") return "NOCHE NEVADA";
    if (palette == "gray") return "GRIS";
    if (palette == "mushroom") return "HONGOS";
    return "NORMAL";
}

// Variante de colores de un area (o del nivel, area == nullptr): la suya, la
// del nivel, o "night" si el tipo de mundo es NOCHE (superficie con el cielo
// negro, como el BackgroundColorCtrl 4 del original).
std::string paletteOf(const nlohmann::json& meta, const nlohmann::json* area) {
    std::string palette = area ? area->value("palette", std::string()) : std::string();
    if (palette.empty()) palette = meta.value("palette", std::string());
    const std::string world = area ? area->value("world", meta.value("world", std::string()))
                                   : meta.value("world", std::string());
    if (palette.empty() && (world == "night" || world == "snow" || world == "night_snow")) palette = world;
    return palette;
}

// Listas del JSON que guardan objetos con "col"/"row" (los que se copian,
// agarran y borran junto con una seleccion).
const char* const kObjectLists[] = {"entities", "platforms", "decorations", "blocks"};

const SDL_Color kWhite{255, 255, 255, 255};
const SDL_Color kYellow{252, 216, 88, 255};
const SDL_Color kGray{150, 150, 160, 255};
const SDL_Color kRed{240, 80, 70, 255};
const SDL_Color kCyan{90, 220, 240, 255};
const SDL_Color kGreen{110, 230, 110, 255};
const SDL_Color kPanel{28, 30, 44, 255};
const SDL_Color kPanelLight{52, 56, 82, 255};

}  // namespace

// ============================================================ ciclo de vida

bool LevelEditor::init() {
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        SDL_Log("SDL_Init fallo: %s", SDL_GetError());
        return false;
    }
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        SDL_Log("IMG_Init fallo: %s", IMG_GetError());
        return false;
    }
    int scale = 2;
    SDL_Rect usable;
    if (SDL_GetDisplayUsableBounds(0, &usable) == 0) {
        scale = std::max(1, std::min(static_cast<int>(usable.w * 0.9f) / kWidth,
                                     static_cast<int>(usable.h * 0.9f) / kHeight));
    }
    window_ = SDL_CreateWindow("Super Mario Clone - Editor de niveles", SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED, kWidth * scale, kHeight * scale, SDL_WINDOW_RESIZABLE);
    if (!window_) return false;
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer_) return false;
    SDL_RenderSetLogicalSize(renderer_, kWidth, kHeight);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    if (!initResources()) return false;
    levelsDir_ = std::string(MARIO_SOURCE_DIR) + "/levels";
    newLevel();
    running_ = true;
    return true;
}

bool LevelEditor::initEmbedded(SDL_Window* window, SDL_Renderer* renderer, const std::string& userLevelsDir) {
    window_ = window;
    renderer_ = renderer;
    embedded_ = true;
    if (!initResources()) return false;
    levelsDir_ = userLevelsDir;
    try {
        fs::create_directories(levelsDir_);
    } catch (const std::exception&) {
    }
    newLevel();
    return true;
}

void LevelEditor::startNew() {
    newLevel();
    undoStack_.clear();
    mode_ = Mode::Edit;
}

LevelEditor::EmbeddedResult LevelEditor::runEmbedded() {
    // La vista del editor (640x360) sobre la ventana del juego; al salir se
    // devuelve la del juego como estaba.
    int gameW = 0;
    int gameH = 0;
    SDL_RenderGetLogicalSize(renderer_, &gameW, &gameH);
    const int cursorShown = SDL_ShowCursor(SDL_QUERY);
    SDL_RenderSetLogicalSize(renderer_, kWidth, kHeight);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_ShowCursor(SDL_ENABLE);
    running_ = true;
    playRequested_ = false;
    while (running_ && !playRequested_) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) handleEvent(event);
        render();
    }
    SDL_RenderSetClipRect(renderer_, nullptr);
    SDL_RenderSetLogicalSize(renderer_, gameW, gameH);
    SDL_ShowCursor(cursorShown);
    if (arrowCursor_) SDL_SetCursor(arrowCursor_);
    EmbeddedResult result;
    if (playRequested_) {
        result.kind = EmbeddedResult::Kind::Play;
        result.path = path_;
        result.startCol = playStartCol_;
    }
    playRequested_ = false;
    return result;
}

void LevelEditor::shutdownEmbedded() {
    if (previewTexture_) SDL_DestroyTexture(previewTexture_);
    previewTexture_ = nullptr;
    for (SDL_Cursor* cursor : {arrowCursor_, handCursor_, moveCursor_, resizeVCursor_, resizeHCursor_}) {
        if (cursor) SDL_FreeCursor(cursor);
    }
    arrowCursor_ = handCursor_ = moveCursor_ = resizeVCursor_ = resizeHCursor_ = nullptr;
}

bool LevelEditor::initResources() {
    const std::string dir = "assets/sprites/";
    bool ok = terrainSprite_.loadFromFile(renderer_, dir + "terrain.png", 16, 16) &&
              brickSprite_.loadFromFile(renderer_, dir + "brick_block.png", 16, 16) &&
              questionSprite_.loadFromFile(renderer_, dir + "question_block.png", 16, 16) &&
              coinSprite_.loadFromFile(renderer_, dir + "coin.png", 16, 16) &&
              pipeSprite_.loadFromFile(renderer_, dir + "pipe.png", 32, 16) &&
              lavaSprite_.loadFromFile(renderer_, dir + "lava.png", 16, 16) &&
              bridgeSprite_.loadFromFile(renderer_, dir + "bridge.png", 16, 16) &&
              sceneryTilesSprite_.loadFromFile(renderer_, dir + "scenery_tiles.png", 16, 16) &&
              flagpoleSprite_.loadFromFile(renderer_, dir + "flagpole.png", 16, 16) &&
              liftSprite_.loadFromFile(renderer_, dir + "lift.png", 8, 8) &&
              goombaSprite_.loadFromFile(renderer_, dir + "goomba.png", 16, 16) &&
              koopaSprite_.loadFromFile(renderer_, dir + "koopa.png", 16, 24) &&
              koopaRedSprite_.loadFromFile(renderer_, dir + "koopa_red.png", 16, 24) &&
              mushroomSprite_.loadFromFile(renderer_, dir + "mushroom.png", 16, 16) &&
              oneUpSprite_.loadFromFile(renderer_, dir + "oneup_mushroom.png", 16, 16) &&
              fireFlowerSprite_.loadFromFile(renderer_, dir + "fire_flower.png", 16, 16) &&
              starSprite_.loadFromFile(renderer_, dir + "star_item.png", 16, 16);
    // Los de los enemigos son opcionales: si falta alguno se ve el rotulo.
    buzzySprite_.loadFromFile(renderer_, dir + "buzzy_beetle.png", 16, 16);
    piranhaSprite_.loadFromFile(renderer_, dir + "piranha_plant.png", 16, 24);
    hammerBroSprite_.loadFromFile(renderer_, dir + "hammer_bro.png", 16, 24);
    lakituSprite_.loadFromFile(renderer_, dir + "lakitu.png", 16, 24);
    blooperSprite_.loadFromFile(renderer_, dir + "blooper.png", 16, 24);
    cheepSprite_.loadFromFile(renderer_, dir + "cheep_cheep.png", 16, 16);
    cheepGraySprite_.loadFromFile(renderer_, dir + "cheep_cheep_gray.png", 16, 16);
    bulletBillSprite_.loadFromFile(renderer_, dir + "bullet_bill.png", 16, 16);
    podobooSprite_.loadFromFile(renderer_, dir + "podoboo.png", 16, 16);
    bowserSprite_.loadFromFile(renderer_, dir + "bowser.png", 32, 32);
    axeSprite_.loadFromFile(renderer_, dir + "axe.png", 16, 16);
    springSprite_.loadFromFile(renderer_, dir + "spring.png", 16, 24);
    retainerSprite_.loadFromFile(renderer_, dir + "retainers.png", 16, 24);
    bridgeChainSprite_.loadFromFile(renderer_, dir + "bridge_chain.png", 16, 16);
    vineSprite_.loadFromFile(renderer_, dir + "vine.png", 16, 16);
    fireballSprite_.loadFromFile(renderer_, dir + "fireball.png", 8, 8);
    marioSprite_.loadFromFile(renderer_, dir + "mario_small.png", 16, 16);
    pipeSideSlices_.loadFromFile(renderer_, dir + "scenery_pipe_side.png", 16, 16);
    terrainGraySprite_.loadRecolored(renderer_, dir + "terrain.png", 16, 16, kGrayColors);
    sceneryTilesSnowSprite_.loadRecolored(renderer_, dir + "scenery_tiles.png", 16, 16, kSnowColors);
    sceneryTilesGraySprite_.loadRecolored(renderer_, dir + "scenery_tiles.png", 16, 16, kGrayColors);
    pipeSnowSprite_.loadRecolored(renderer_, dir + "pipe.png", 32, 16, kSnowColors);
    pipeMushroomSprite_.loadRecolored(renderer_, dir + "pipe.png", 32, 16, kMushroomColors);
    flagpoleSnowSprite_.loadRecolored(renderer_, dir + "flagpole.png", 16, 16, kSnowColors);
    pipeSideVariants_[0].loadRecolored(renderer_, dir + "scenery_pipe_side.png", 16, 16, kSnowColors);
    pipeSideVariants_[1].loadRecolored(renderer_, dir + "scenery_pipe_side.png", 16, 16, kPipeCastleColors);
    pipeSideVariants_[2].loadRecolored(renderer_, dir + "scenery_pipe_side.png", 16, 16, kPipeUndergroundColors);
    for (int i = 0; i < kDecorationCount; ++i) {
        const std::string path = dir + "scenery_" + kDecorations[i].type + ".png";
        decorationSnowSprites_[i].loadRecolored(renderer_, path, kDecorations[i].width, kDecorations[i].height, kSnowColors);
        decorationGraySprites_[i].loadRecolored(renderer_, path, kDecorations[i].width, kDecorations[i].height, kGrayColors);
    }
    for (int i = 0; ok && i < kDecorationCount; ++i) {
        ok = decorationSprites_[i].loadFromFile(renderer_, dir + "scenery_" + kDecorations[i].type + ".png",
                                                 kDecorations[i].width, kDecorations[i].height);
    }
    if (!ok) return false;

    // --- paleta, por pestana (como las categorias de Super Mario Maker)
    const char* const tileLabels[][2] = {
        {"#", "PISO"},           {"S", "BLOQUE DURO"},       {"B", "LADRILLO"},         {"?", "BLOQUE ?"},
        {"D", "LADRILLO ESPECIAL"}, {"H", "BLOQUE INVISIBLE"}, {"C", "LADRILLO + MONEDA"}, {"o", "MONEDA SUELTA"},
        {"T", "TUBERIA"},        {"Y", "TUBERIA HORIZONTAL"}, {"=", "PLATAFORMA FIJA"},  {"L", "LAVA"},
        {"b", "PUENTE BOWSER"},
        {"X", "SOLIDO INVISIBLE"}, {"t", "COPA DE ARBOL"},  {"r", "TRONCO"},             {"m", "HONGO"},
        {"n", "TALLO DE HONGO"}, {"R", "PUENTE DE SOGA"},    {"K", "CANON BILL BALA"},  {"c", "CORAL"},
        {"W", "AGUA"},           {"w", "SUPERFICIE (AGUA)"}, {"N", "BLOQUE DE NUBE"},
    };
    for (const auto& entry : tileLabels) {
        tools_.push_back({ToolKind::Tile, Tab::Terrain, entry[0][0], "", "", 1, entry[1]});
    }
    tools_.push_back({ToolKind::Goal, Tab::Terrain, 0, "", "", 1, "BANDERA DE META"});

    tools_.push_back({ToolKind::Item, Tab::Items, 0, "powerup", "", 1, "HONGO / FLOR (SEGUN TAMANO)"});
    tools_.push_back({ToolKind::Item, Tab::Items, 0, "mushroom", "", 1, "SUPER HONGO"});
    tools_.push_back({ToolKind::Item, Tab::Items, 0, "fire_flower", "", 1, "FLOR DE FUEGO"});
    tools_.push_back({ToolKind::Item, Tab::Items, 0, "star", "", 1, "ESTRELLA"});
    tools_.push_back({ToolKind::Item, Tab::Items, 0, "one_up", "", 1, "HONGO 1-UP"});
    tools_.push_back({ToolKind::Item, Tab::Items, 0, "coin", "", 1, "MONEDA"});
    tools_.push_back({ToolKind::Item, Tab::Items, 0, "coin", "", 10, "10 MONEDAS"});

    tools_.push_back({ToolKind::Entity, Tab::Enemies, 0, "goomba", "", 1, "GOOMBA"});
    tools_.push_back({ToolKind::Entity, Tab::Enemies, 0, "koopa", "green", 1, "KOOPA VERDE"});
    tools_.push_back({ToolKind::Entity, Tab::Enemies, 0, "koopa", "red", 1, "KOOPA ROJO"});
    // El resto del elenco: en el editor se ven como un rotulo (ver
    // entityShortName). Piranha: la fila es el tope de la tuberia.
    const char* const entityLabels[][3] = {
        {"buzzy", "", "BUZZY BEETLE"},   {"piranha", "", "PIRANHA (TOPE DE TUBERIA)"},
        {"hammer_bro", "", "HAMMER BRO"}, {"lakitu", "", "LAKITU"},
        {"blooper", "", "BLOOPER"},       {"cheep", "gray", "CHEEP CHEEP"},
        {"cheep", "red", "CHEEP CHEEP ONDULANTE"}, {"cheep_jump", "", "ZONA CHEEP SALTARINES"},
        {"podoboo", "", "PODOBOO (EN LA LAVA)"}, {"firebar", "", "BARRA DE FUEGO"},
        {"bowser", "", "BOWSER"},         {"axe", "", "HACHA"},
    };
    for (const auto& entry : entityLabels) {
        tools_.push_back({ToolKind::Entity, Tab::Enemies, 0, entry[0], entry[1], 1, entry[2]});
    }

    for (const DecorationInfo& info : kDecorations) {
        // La tuberia en L se arma con tiles (horizontal + vertical).
        if (std::string(info.type) == "pipe_side") continue;
        tools_.push_back({ToolKind::Decoration, Tab::Objects, 0, info.type, "", 1, info.label});
    }
    tools_.push_back({ToolKind::Platform, Tab::Objects, 0, "vertical", "", 1, "PLATAF. VERTICAL"});
    tools_.push_back({ToolKind::Platform, Tab::Objects, 0, "horizontal", "", 1, "PLATAF. HORIZONTAL"});
    tools_.push_back({ToolKind::Platform, Tab::Objects, 0, "lift_up", "", 1, "ELEVADOR QUE SUBE"});
    tools_.push_back({ToolKind::Platform, Tab::Objects, 0, "lift_down", "", 1, "ELEVADOR QUE BAJA"});
    tools_.push_back({ToolKind::Platform, Tab::Objects, 0, "fall", "", 1, "PLATAF. QUE CAE"});
    tools_.push_back({ToolKind::Platform, Tab::Objects, 0, "right", "", 1, "PLATAF. A LA DERECHA (CIELO)"});
    tools_.push_back({ToolKind::Checkpoint, Tab::Objects, 0, "", "", 1, "CHECKPOINT"});
    tools_.push_back({ToolKind::Entity, Tab::Objects, 0, "whirlpool", "", 1, "REMOLINO (AGUA, + / - ANCHO)"});
    tools_.push_back({ToolKind::Start, Tab::Objects, 0, "", "", 1, "INICIO DE MARIO (CLIC DE NUEVO: GIRA)"});

    // Herramientas de los botones (no estan en ninguna pestana).
    tools_.push_back({ToolKind::Eraser, Tab::Count, 0, "", "", 1, "GOMA (BORRAR)"});
    tools_.push_back({ToolKind::Select, Tab::Count, 0, "", "", 1, "SELECCIONAR AREA"});
    tools_.push_back({ToolKind::Move, Tab::Count, 0, "", "", 1, "MOVER (ARRASTRAR LO PUESTO)"});
    arrowCursor_ = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
    handCursor_ = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
    moveCursor_ = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL);
    resizeVCursor_ = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENS);
    resizeHCursor_ = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE);
    return true;
}

void LevelEditor::openFile(const std::string& path) {
    // "levels/world1/1-1.json" relativo: el del proyecto (el que va al
    // repositorio), no la copia que el build deja al lado del juego.
    std::string resolved = path;
    const fs::path given(path);
    if (given.is_relative() && !given.empty() && *given.begin() == "levels") {
        const fs::path source = fs::path(levelsDir_) / given.lexically_relative("levels");
        if (fs::exists(source)) resolved = source.generic_string();
    }
    if (loadLevel(resolved)) undoStack_.clear();
}

void LevelEditor::run() {
    while (running_) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) handleEvent(event);
        render();
    }
}

void LevelEditor::shutdown() {
    if (previewTexture_) SDL_DestroyTexture(previewTexture_);
    for (SDL_Cursor* cursor : {arrowCursor_, handCursor_, moveCursor_, resizeVCursor_, resizeHCursor_}) {
        if (cursor) SDL_FreeCursor(cursor);
    }
    SpriteSheet::rendererDestroyed();
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
    IMG_Quit();
    SDL_Quit();
}

// ============================================================ modelo

void LevelEditor::newLevel() {
    rows_.assign(kLevelRows, std::string(kNewLevelWidth, '.'));
    for (int row = 14; row < kLevelRows; ++row) rows_[row] = std::string(kNewLevelWidth, '#');
    meta_ = nlohmann::json::object();
    meta_["tile_size"] = kTile;
    meta_["name"] = "NUEVO";
    meta_["time"] = 400;
    path_.clear();
    dirty_ = false;
    cameraX_ = 0.0f;
    selectionValid_ = false;
    rebuildTilemap();
}

bool LevelEditor::loadLevel(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        showMessage("NO SE PUDO ABRIR " + path, 3000);
        return false;
    }
    nlohmann::json json;
    try {
        file >> json;
    } catch (const std::exception&) {
        showMessage("ARCHIVO INVALIDO", 3000);
        return false;
    }
    rows_ = json.value("rows", std::vector<std::string>{});
    if (rows_.empty()) rows_.assign(kLevelRows, std::string(kNewLevelWidth, '.'));
    json.erase("rows");
    meta_ = json;
    path_ = path;
    // Las tuberias en L hechas con el decorado de antes pasan a tiles.
    convertSidePipeDecorations();
    dirty_ = false;
    cameraX_ = 0.0f;
    selectionValid_ = false;
    rebuildTilemap();
    showMessage("ABIERTO", 1500);
    return true;
}

std::string LevelEditor::defaultPathForName() const {
    if (embedded_) {
        // Los niveles del jugador, todos en su carpeta.
        return levelsDir_ + "/" + meta_.value("name", std::string("NUEVO")) + ".json";
    }
    // "1-2" -> levels/world1/1-2.json; sin numero de mundo -> levels/custom/.
    const std::string name = meta_.value("name", std::string("NUEVO"));
    const size_t dash = name.find('-');
    const std::string world = dash != std::string::npos ? name.substr(0, dash) : "";
    const bool numeric = !world.empty() && std::all_of(world.begin(), world.end(), [](char c) {
        return std::isdigit(static_cast<unsigned char>(c)) != 0;
    });
    const std::string folder = numeric ? "world" + world : "custom";
    return levelsDir_ + "/" + folder + "/" + name + ".json";
}

bool LevelEditor::saveLevel() {
    if (path_.empty()) path_ = defaultPathForName();
    try {
        fs::create_directories(fs::path(path_).parent_path());
    } catch (const std::exception&) {
    }
    // Marca de "editado a mano": rom_build.py no lo vuelve a generar (salvo
    // con --force), asi los arreglos hechos aca no se pierden.
    if (!embedded_) meta_["edited"] = true;
    const std::string text = buildJson().dump(2) + "\n";
    {
        std::ofstream file(path_);
        if (!file.is_open()) {
            showMessage("NO SE PUDO GUARDAR", 3000);
            return false;
        }
        file << text;
    }
    // El juego lee la copia de levels/ que esta al lado de mario.exe (la
    // pone ahi el build): se actualiza tambien, asi el cambio se ve jugando
    // sin recompilar.
    bool copied = false;
    if (embedded_) {
        dirty_ = false;
        showMessage("GUARDADO", 1500);
        return true;
    }
    try {
        const fs::path source = fs::weakly_canonical(fs::path(path_));
        const fs::path levels = fs::weakly_canonical(fs::path(levelsDir_));
        const fs::path relative = source.lexically_relative(levels);
        if (!relative.empty() && *relative.begin() != "..") {
            char* base = SDL_GetBasePath();
            const fs::path gameLevels = fs::path(base ? base : "") / "levels";
            SDL_free(base);
            if (fs::exists(gameLevels) && fs::weakly_canonical(gameLevels) != levels) {
                fs::create_directories((gameLevels / relative).parent_path());
                std::ofstream copy(gameLevels / relative);
                copy << text;
                copied = copy.good();
            }
        }
    } catch (const std::exception&) {
    }
    dirty_ = false;
    showMessage(copied ? "GUARDADO (Y COPIADO AL JUEGO)" : "GUARDADO", 1500);
    return true;
}

nlohmann::json LevelEditor::buildJson() const {
    nlohmann::json json = meta_;
    // Claves que quedaron en null (meta_[clave] las crea al consultarlas):
    // fuera, ni el juego ni el archivo guardado las quieren.
    for (auto it = json.begin(); it != json.end();) {
        it = it->is_null() ? json.erase(it) : std::next(it);
    }
    json["rows"] = rows_;
    return json;
}

void LevelEditor::rebuildTilemap() { tilemap_.loadFromJson(buildJson()); }

void LevelEditor::pushUndo() {
    undoStack_.push_back({rows_, meta_});
    if (undoStack_.size() > kMaxUndo) undoStack_.erase(undoStack_.begin());
}

void LevelEditor::undo() {
    if (undoStack_.empty()) {
        showMessage("NADA PARA DESHACER");
        return;
    }
    rows_ = undoStack_.back().rows;
    meta_ = undoStack_.back().meta;
    undoStack_.pop_back();
    markDirty();
}

void LevelEditor::markDirty() {
    dirty_ = true;
    confirmQuit_ = false;
    rebuildTilemap();
}

void LevelEditor::showMessage(const std::string& text, Uint32 ms) {
    message_ = text;
    messageUntil_ = SDL_GetTicks() + ms;
}

int LevelEditor::widthTiles() const { return rows_.empty() ? 0 : static_cast<int>(rows_[0].size()); }
int LevelEditor::heightTiles() const { return static_cast<int>(rows_.size()); }

char LevelEditor::tileAt(int col, int row) const {
    if (row < 0 || row >= heightTiles() || col < 0 || col >= widthTiles()) return '.';
    return rows_[row][col];
}

void LevelEditor::setTile(int col, int row, char tile) {
    if (row < 0 || row >= heightTiles() || col < 0 || col >= widthTiles()) return;
    rows_[row][col] = tile;
}

void LevelEditor::resizeWidth(int deltaCols) {
    const int newWidth = std::max(16, widthTiles() + deltaCols);
    if (newWidth == widthTiles()) return;
    pushUndo();
    for (std::string& row : rows_) row.resize(newWidth, '.');
    markDirty();
}

// ============================================================ edicion

void LevelEditor::selectTool(int index) {
    selectedTool_ = index;
    if (tools_[index].tab != Tab::Count) currentTab_ = tools_[index].tab;
}

void LevelEditor::placeAt(int col, int row) {
    const Tool& tool = tools_[selectedTool_];
    // Recientes (como la fila de arriba de la paleta de Mario Maker).
    if (tool.tab != Tab::Count) {
        recents_.erase(std::remove(recents_.begin(), recents_.end(), selectedTool_), recents_.end());
        recents_.insert(recents_.begin(), selectedTool_);
        if (static_cast<int>(recents_.size()) > kMaxRecents) recents_.resize(kMaxRecents);
    }
    switch (tool.kind) {
        case ToolKind::Tile:
            if (tool.tile == 'T') {
                // Las tuberias son siempre de 2 columnas (ver Tilemap::isPipeTile).
                setTile(col, row, 'T');
                setTile(col + 1, row, 'T');
            } else if (tool.tile == 'Y') {
                // Las horizontales, de 2 filas.
                setTile(col, row, 'Y');
                setTile(col, row + 1, 'Y');
            } else {
                setTile(col, row, tool.tile);
            }
            break;
        case ToolKind::Goal:
            // Bandera completa de un clic: bloque duro de base donde se hace
            // clic y el mastil (bola incluida) arriba.
            setTile(col, row, 'S');
            for (int r = row - 1; r >= std::max(0, row - kFlagpoleHeight); --r) setTile(col, r, 'F');
            break;
        case ToolKind::Item:
            placeItemInBlock(col, row, tool);
            return;  // ya marco el cambio (o aviso que no se puede)
        case ToolKind::Entity: {
            auto& list = meta_["entities"];
            if (!list.is_array()) list = nlohmann::json::array();
            for (const auto& entity : list) {
                if (entity.value("col", -1) == col && entity.value("row", -1) == row) return;
            }
            nlohmann::json entity{{"type", tool.type}, {"col", col}, {"row", row}};
            if ((tool.type == "koopa" || tool.type == "cheep") && tool.variant == "red") entity["color"] = "red";
            // Zona de Cheep Cheep saltarines: 40 columnas por defecto (se
            // ajusta en el JSON, campo end_col).
            if (tool.type == "cheep_jump") entity["end_col"] = col + 40;
            // Remolino: 4 bloques de ancho por defecto (+ / - lo cambian).
            if (tool.type == "whirlpool") entity["length"] = 4;
            list.push_back(entity);
            break;
        }
        case ToolKind::Decoration: {
            auto& list = meta_["decorations"];
            if (!list.is_array()) list = nlohmann::json::array();
            nlohmann::json decoration{{"type", tool.type}, {"col", col}, {"row", row}};
            // La tuberia en L va delante de los personajes (se meten adentro)
            // y su tramo vertical es solido (como las del nivel original).
            if (tool.type == "pipe_side") {
                decoration["front"] = true;
                setSidePipeSolid(col, row, 0, true);
                showMessage("TUBERIA EN L: P SOBRE LA BOCA PARA ELEGIR ADONDE LLEVA", 4000);
            }
            list.push_back(decoration);
            break;
        }
        case ToolKind::Platform: {
            auto& list = meta_["platforms"];
            if (!list.is_array()) list = nlohmann::json::array();
            nlohmann::json platform{{"col", col}, {"row", row}, {"width", 3}, {"mode", tool.type}};
            if (tool.type == "vertical") {
                platform["range"] = 4;
                platform["period"] = 4;
            } else if (tool.type == "horizontal") {
                platform["range"] = 6;
                platform["period"] = 5;
            } else if (tool.type == "lift_up" || tool.type == "lift_down") {
                platform["speed"] = 50;
            }
            list.push_back(platform);
            break;
        }
        case ToolKind::Start: {
            // El inicio del jugador: donde se hace clic, parado en esa fila.
            // Clic sobre el que ya esta: mira para el otro lado.
            auto& start = meta_["start"];
            if (start.is_object() && start.value("col", -1) == col && start.value("row", -1) == row) {
                start["facing"] = start.value("facing", std::string("right")) == "left" ? "right" : "left";
                showMessage(std::string("INICIO: MIRA A LA ") +
                            (start["facing"] == "left" ? "IZQUIERDA" : "DERECHA"));
            } else {
                const std::string facing = start.is_object() ? start.value("facing", std::string("right")) : "right";
                start = {{"col", col}, {"row", row}, {"facing", facing}};
                showMessage("INICIO DE MARIO (CLIC DE NUEVO PARA QUE MIRE AL OTRO LADO)");
            }
            break;
        }
        case ToolKind::Checkpoint: {
            auto& list = meta_["checkpoints"];
            if (!list.is_array()) list = nlohmann::json::array();
            for (const auto& value : list) {
                if (value.get<int>() == col) return;
            }
            list.push_back(col);
            break;
        }
        case ToolKind::Eraser:
            eraseAt(col, row);
            return;
        case ToolKind::Select:
        case ToolKind::Move:
            return;
    }
    markDirty();
}

void LevelEditor::placeItemInBlock(int col, int row, const Tool& tool) {
    // Como en Mario Maker: el item se suelta encima de un bloque y queda
    // adentro. Un ladrillo comun pasa a ser ladrillo especial ('D', el que
    // tira items sin romperse).
    char tile = tileAt(col, row);
    if (tile == 'B' || tile == 'C') {
        setTile(col, row, 'D');
        tile = 'D';
    }
    if (!isBlockTile(tile)) {
        showMessage("SOLTA EL ITEM SOBRE UN BLOQUE ? , LADRILLO O BLOQUE INVISIBLE");
        return;
    }
    auto& list = meta_["blocks"];
    if (!list.is_array()) list = nlohmann::json::array();
    for (auto it = list.begin(); it != list.end(); ++it) {
        if (it->value("col", -1) == col && it->value("row", -1) == row) {
            list.erase(it);
            break;
        }
    }
    // Una moneda de un golpe es lo que tiene un bloque sin entrada.
    if (!(tool.type == "coin" && tool.hits <= 1)) {
        nlohmann::json block{{"col", col}, {"row", row}, {"item", tool.type}};
        if (tool.hits > 1) block["hits"] = tool.hits;
        list.push_back(block);
    }
    markDirty();
}

bool LevelEditor::findObjectAt(int col, int row, std::string& listName, size_t& index,
                               bool includeDecorations) const {
    // Enemigos y plataformas antes que la escenografia (que es grande y
    // queda atras de todo).
    const auto& entities = meta_.value("entities", nlohmann::json::array());
    for (size_t i = 0; i < entities.size(); ++i) {
        const int c = entities[i].value("col", -1);
        const int r = entities[i].value("row", -1);
        if (entities[i].value("type", std::string()) == "whirlpool") {
            if (col >= c && col < c + entities[i].value("length", 4) && row >= 1) {
                listName = "entities";
                index = i;
                return true;
            }
            continue;
        }
        // Un Koopa ocupa tambien el tile de arriba.
        const bool tall = entities[i].value("type", std::string()) == "koopa";
        if (c == col && (r == row || (tall && r - 1 == row))) {
            listName = "entities";
            index = i;
            return true;
        }
    }
    const auto& platforms = meta_.value("platforms", nlohmann::json::array());
    for (size_t i = 0; i < platforms.size(); ++i) {
        const int c = platforms[i].value("col", 0);
        if (row == platforms[i].value("row", 0) && col >= c && col < c + platforms[i].value("width", 3)) {
            listName = "platforms";
            index = i;
            return true;
        }
    }
    if (!includeDecorations) return false;
    const auto& decorations = meta_.value("decorations", nlohmann::json::array());
    for (size_t i = decorations.size(); i-- > 0;) {
        const int type = decorationIndex(decorations[i].value("type", std::string()));
        if (type < 0) continue;
        const int c = decorations[i].value("col", 0);
        const int r = decorations[i].value("row", 0);
        const int extra = decorations[i].value("length", 0);
        if (col >= c && col < c + kDecorations[type].width / kTile + extra && row >= r &&
            row < r + kDecorations[type].height / kTile) {
            listName = "decorations";
            index = i;
            return true;
        }
    }
    return false;
}

void LevelEditor::eraseAt(int col, int row) {
    // Tuberia en L: se borra entera (dibujo, tramo solido y su warp).
    if (const int pipe = sidePipeAt(col, row); pipe >= 0) {
        auto& decorations = meta_["decorations"];
        const int pc = decorations[pipe].value("col", 0);
        const int pr = decorations[pipe].value("row", 0);
        // El tramo vertical estirado hacia arriba tambien.
        const int vc = pc + 2 + decorations[pipe].value("length", 0);
        for (int r = sidePipeTop(pipe); r < pr; ++r) {
            setTile(vc, r, '.');
            setTile(vc + 1, r, '.');
        }
        setSidePipeSolid(pc, pr, decorations[pipe].value("length", 0), false);
        decorations.erase(static_cast<size_t>(pipe));
        auto& warps = meta_["warps"];
        if (warps.is_array()) {
            for (auto it = warps.begin(); it != warps.end(); ++it) {
                if (it->value("col", -1) == pc && it->value("row", -1) == pr + 2) {
                    warps.erase(it);
                    break;
                }
            }
        }
        markDirty();
        return;
    }
    // Primero lo que esta "encima" de los tiles (enemigo, plataforma,
    // escenografia); si no hay nada, el tile; si tampoco, un checkpoint.
    std::string listName;
    size_t index = 0;
    if (findObjectAt(col, row, listName, index)) {
        meta_[listName].erase(index);
        markDirty();
        return;
    }
    auto removeAtCell = [&](const char* key) {
        // find en vez de meta_[key]: [] crea la clave en null si no existe.
        auto found = meta_.find(key);
        if (found == meta_.end() || !found->is_array()) return false;
        auto& list = *found;
        for (auto it = list.begin(); it != list.end(); ++it) {
            if (it->value("col", -1) == col && it->value("row", -1) == row) {
                list.erase(it);
                return true;
            }
        }
        return false;
    };
    const char tile = tileAt(col, row);
    if (tile != '.') {
        if (tile == 'Y') {
            // La horizontal, de a pares de filas; si era la boca, con su warp.
            const int top = hpipeTopRow(col, row);
            const int mouth = hpipeLeftCol(col, top);
            setTile(col, top, '.');
            setTile(col, top + 1, '.');
            auto& warps = meta_["warps"];
            if (warps.is_array()) {
                for (auto it = warps.begin(); it != warps.end(); ++it) {
                    if (it->value("col", -1) == col && col == mouth && it->value("row", -1) == top) {
                        warps.erase(it);
                        break;
                    }
                }
            }
            markDirty();
            return;
        }
        if (tile == 'T') {
            // La tuberia se borra de a pares (las dos mitades) y con su warp.
            const int left = tilemap_.isPipeLeftHalf(col, row) ? col : col - 1;
            setTile(left, row, '.');
            setTile(left + 1, row, '.');
            auto& warps = meta_["warps"];
            if (warps.is_array()) {
                for (auto it = warps.begin(); it != warps.end(); ++it) {
                    if (it->value("col", -1) == left && it->value("row", -1) == row) {
                        warps.erase(it);
                        break;
                    }
                }
            }
        }
        setTile(col, row, '.');
        removeAtCell("blocks");  // el contenido del bloque que estaba ahi
        markDirty();
        return;
    }
    // El inicio del jugador.
    if (auto start = meta_.find("start");
        start != meta_.end() && start->is_object() && start->value("col", -2) == col &&
        (start->value("row", -2) == row || start->value("row", -2) == row + 1)) {
        meta_.erase("start");
        markDirty();
        return;
    }
    auto found = meta_.find("checkpoints");
    if (found != meta_.end() && found->is_array()) {
        auto& checkpoints = *found;
        for (auto it = checkpoints.begin(); it != checkpoints.end(); ++it) {
            if (it->get<int>() == col) {
                checkpoints.erase(it);
                markDirty();
                return;
            }
        }
    }
}

void LevelEditor::cycleBlockItem(int col, int row) {
    if (!isBlockTile(tileAt(col, row))) return;
    pushUndo();
    auto& list = meta_["blocks"];
    if (!list.is_array()) list = nlohmann::json::array();
    for (auto it = list.begin(); it != list.end(); ++it) {
        if (it->value("col", -1) != col || it->value("row", -1) != row) continue;
        const std::string current = it->value("item", std::string("coin"));
        size_t index = 0;
        while (index < std::size(kBlockItems) && current != kBlockItems[index]) ++index;
        const std::string next = kBlockItems[(index + 1) % std::size(kBlockItems)];
        if (next == "coin" && it->value("hits", 1) <= 1) {
            list.erase(it);
        } else {
            (*it)["item"] = next;
        }
        markDirty();
        return;
    }
    list.push_back({{"col", col}, {"row", row}, {"item", kBlockItems[1]}});
    markDirty();
}

void LevelEditor::changeBlockHits(int col, int row, int delta) {
    if (!isBlockTile(tileAt(col, row))) return;
    pushUndo();
    auto& list = meta_["blocks"];
    if (!list.is_array()) list = nlohmann::json::array();
    for (auto& block : list) {
        if (block.value("col", -1) != col || block.value("row", -1) != row) continue;
        block["hits"] = std::clamp(block.value("hits", 1) + delta, 1, 99);
        markDirty();
        return;
    }
    list.push_back({{"col", col}, {"row", row}, {"item", "coin"}, {"hits", std::clamp(1 + delta, 1, 99)}});
    markDirty();
}

int LevelEditor::sidePipeAt(int col, int row) const {
    const auto found = meta_.find("decorations");
    if (found == meta_.end() || !found->is_array()) return -1;
    for (size_t i = 0; i < found->size(); ++i) {
        const auto& decoration = (*found)[i];
        if (decoration.value("type", std::string()) != "pipe_side") continue;
        const int c = decoration.value("col", 0);
        const int r = decoration.value("row", 0);
        const int width = 4 + decoration.value("length", 0);
        if (col >= c && col < c + width && row >= r && row < r + 4) return static_cast<int>(i);
    }
    return -1;
}

void LevelEditor::setSidePipeSolid(int col, int row, int length, bool solid) {
    // Tramo vertical (2 columnas, 4 filas) y los tramos horizontales de mas
    // (la parte de abajo, 2 filas): bloques invisibles 'X'.
    auto mark = [&](int c, int r) {
        if (c < 0 || c >= widthTiles() || r < 0 || r >= heightTiles()) return;
        if (solid && tileAt(c, r) == '.') setTile(c, r, 'X');
        if (!solid && tileAt(c, r) == 'X') setTile(c, r, '.');
    };
    const int vc = col + 2 + length;
    for (int c = vc; c < vc + 2; ++c) {
        for (int r = row; r < row + 4; ++r) mark(c, r);
    }
    for (int c = col + 2; c < vc; ++c) {
        for (int r = row + 2; r < row + 4; ++r) mark(c, r);
    }
}

int LevelEditor::whirlpoolAt(int col) const {
    const auto found = meta_.find("entities");
    if (found == meta_.end() || !found->is_array()) return -1;
    for (size_t i = 0; i < found->size(); ++i) {
        const auto& entity = (*found)[i];
        if (entity.value("type", std::string()) != "whirlpool") continue;
        const int c = entity.value("col", 0);
        if (col >= c && col < c + entity.value("length", 4)) return static_cast<int>(i);
    }
    return -1;
}

int LevelEditor::hpipeTopRow(int col, int row) const {
    // De a pares: si arriba hay otra 'Y', esta es la fila de abajo.
    return tileAt(col, row - 1) == 'Y' ? row - 1 : row;
}

int LevelEditor::hpipeLeftCol(int col, int row) const {
    while (tileAt(col - 1, row) == 'Y') --col;
    return col;
}

void LevelEditor::convertSidePipeDecorations() {
    auto found = meta_.find("decorations");
    if (found == meta_.end() || !found->is_array()) return;
    nlohmann::json kept = nlohmann::json::array();
    for (const auto& decoration : *found) {
        if (decoration.value("type", std::string()) != "pipe_side") {
            kept.push_back(decoration);
            continue;
        }
        const int c = decoration.value("col", 0);
        const int r = decoration.value("row", 0);
        const int vc = c + 2 + decoration.value("length", 0);
        auto put = [&](int col, int row, char tile) {
            if (col < 0 || col >= widthTiles() || row < 0 || row >= heightTiles()) return;
            if (tileAt(col, row) == '.' || tileAt(col, row) == 'X') setTile(col, row, tile);
        };
        for (int col = c; col < vc; ++col) {
            put(col, r + 2, 'Y');
            put(col, r + 3, 'Y');
        }
        for (int col = vc; col < vc + 2; ++col) {
            for (int row = r; row < r + 4; ++row) put(col, row, 'T');
        }
    }
    *found = kept;
}

int LevelEditor::sidePipeTop(int index) const {
    const auto& decoration = meta_["decorations"][static_cast<size_t>(index)];
    const int vc = decoration.value("col", 0) + 2 + decoration.value("length", 0);
    int top = decoration.value("row", 0);
    while (top - 1 >= 0 && tileAt(vc, top - 1) == 'T' && tileAt(vc + 1, top - 1) == 'T') --top;
    return top;
}

LevelEditor::Stretch LevelEditor::pipeTipAt(int col, int row) const {
    Stretch tip;
    if (const int pipe = sidePipeAt(col, row); pipe >= 0) {
        const auto& decoration = meta_["decorations"][static_cast<size_t>(pipe)];
        const int c = decoration.value("col", 0);
        const int r = decoration.value("row", 0);
        const int vc = c + 2 + decoration.value("length", 0);
        if ((col == vc || col == vc + 1) && row == sidePipeTop(pipe)) {
            tip.kind = StretchKind::SideTop;
        } else if (col == c && (row == r + 2 || row == r + 3)) {
            tip.kind = StretchKind::SideMouth;
        }
        tip.index = pipe;
        if (tip.kind != StretchKind::None) return tip;
    }
    // Boca (punta izquierda) de una horizontal.
    if (tileAt(col, row) == 'Y') {
        const int top = hpipeTopRow(col, row);
        if (tileAt(col - 1, top) != 'Y') {
            tip.kind = StretchKind::HMouth;
            tip.left = col;
            tip.bottom = top;
            int right = col;
            while (tileAt(right + 1, top) == 'Y') ++right;
            tip.right = right;
            return tip;
        }
    }
    // Tope de una tuberia vertical comun (la de un tramo de L ya se vio).
    if (tileAt(col, row) == 'T' && tilemap_.isPipeTopTile(col, row) && sidePipeAt(col, row + 1) < 0) {
        tip.kind = StretchKind::PipeTop;
        tip.left = tilemap_.isPipeLeftHalf(col, row) ? col : col - 1;
        int bottom = row;
        while (bottom + 1 < heightTiles() && tileAt(tip.left, bottom + 1) == 'T') ++bottom;
        tip.bottom = bottom;
    }
    return tip;
}

void LevelEditor::applyStretch(int col, int row) {
    auto freeCell = [&](int c, int r) { return c >= 0 && c < widthTiles() && r >= 1 && tileAt(c, r) == '.'; };
    // Sube o baja el tope de una columna doble de 'T' (de left a left+1)
    // hasta newTop; minTop..maxTop limitan. Devuelve donde quedo.
    auto moveTop = [&](int left, int top, int newTop) {
        while (newTop < top && freeCell(left, top - 1) && freeCell(left + 1, top - 1)) {
            --top;
            setTile(left, top, 'T');
            setTile(left + 1, top, 'T');
        }
        while (newTop > top) {
            setTile(left, top, '.');
            setTile(left + 1, top, '.');
            ++top;
        }
        return top;
    };
    if (stretch_.kind == StretchKind::PipeTop) {
        // La tuberia vertical: al menos 2 filas; el warp sigue al tope.
        int top = stretch_.bottom;
        while (top - 1 >= 0 && tileAt(stretch_.left, top - 1) == 'T') --top;
        const int target = std::clamp(row, 1, stretch_.bottom - 1);
        const int newTop = moveTop(stretch_.left, top, target);
        if (newTop != top) {
            auto& warps = meta_["warps"];
            if (warps.is_array()) {
                for (auto& warp : warps) {
                    if (warp.value("col", -1) == stretch_.left && warp.value("row", -1) == top) warp["row"] = newTop;
                }
            }
        }
    } else if (stretch_.kind == StretchKind::HMouth) {
        // La boca de una horizontal: hacia la izquierda agrega tramos (por
        // lugar libre), hacia la derecha los saca (queda al menos 1); el warp
        // de la boca la sigue.
        const int top = stretch_.bottom;
        int left = stretch_.left;
        const int target = std::clamp(col, 0, stretch_.right);
        while (target < left && freeCell(left - 1, top) && freeCell(left - 1, top + 1)) {
            --left;
            setTile(left, top, 'Y');
            setTile(left, top + 1, 'Y');
        }
        while (target > left) {
            setTile(left, top, '.');
            setTile(left, top + 1, '.');
            ++left;
        }
        if (left != stretch_.left) {
            auto& warps = meta_["warps"];
            if (warps.is_array()) {
                for (auto& warp : warps) {
                    if (warp.value("col", -1) == stretch_.left && warp.value("row", -1) == top) warp["col"] = left;
                }
            }
            stretch_.left = left;
        }
    } else if (stretch_.kind == StretchKind::SideTop) {
        const auto& decoration = meta_["decorations"][static_cast<size_t>(stretch_.index)];
        const int base = decoration.value("row", 0);
        const int vc = decoration.value("col", 0) + 2 + decoration.value("length", 0);
        moveTop(vc, sidePipeTop(stretch_.index), std::clamp(row, 1, base));
    } else if (stretch_.kind == StretchKind::SideMouth) {
        auto& decoration = meta_["decorations"][static_cast<size_t>(stretch_.index)];
        const int c = decoration.value("col", 0);
        const int r = decoration.value("row", 0);
        const int length = decoration.value("length", 0);
        const int vc = c + 2 + length;
        int newCol = std::clamp(col, 0, vc - 2);
        // Hacia la izquierda solo por lugar libre (las 2 filas de la boca).
        while (newCol < c && !(freeCell(newCol, r + 2) && freeCell(newCol, r + 3))) ++newCol;
        for (int k = newCol; k < c; ++k) {
            if (!(freeCell(k, r + 2) && freeCell(k, r + 3))) newCol = k + 1;
        }
        if (newCol == c) return;
        setSidePipeSolid(c, r, length, false);
        decoration["col"] = newCol;
        const int newLength = vc - 2 - newCol;
        if (newLength > 0) {
            decoration["length"] = newLength;
        } else {
            decoration.erase("length");
        }
        setSidePipeSolid(newCol, r, newLength, true);
        auto& warps = meta_["warps"];
        if (warps.is_array()) {
            for (auto& warp : warps) {
                if (warp.value("col", -1) == c && warp.value("row", -1) == r + 2) warp["col"] = newCol;
            }
        }
    }
    rebuildTilemap();
}

void LevelEditor::stretchSidePipe(int index, int delta, bool toTop) {
    const auto& decoration = meta_["decorations"][static_cast<size_t>(index)];
    const int left = decoration.value("col", 0) + 2 + decoration.value("length", 0);
    const int base = decoration.value("row", 0);
    // Tope actual del tramo vertical: sube mientras haya 'T' encima.
    int top = base;
    while (top - 1 >= 0 && tileAt(left, top - 1) == 'T' && tileAt(left + 1, top - 1) == 'T') --top;
    auto freeRow = [&](int r) { return r >= 1 && tileAt(left, r) == '.' && tileAt(left + 1, r) == '.'; };
    pushUndo();
    if (delta > 0) {
        if (!freeRow(top - 1)) {
            undoStack_.pop_back();
            showMessage("NO SE PUEDE ESTIRAR MAS (HAY ALGO ARRIBA)");
            return;
        }
        do {
            --top;
            setTile(left, top, 'T');
            setTile(left + 1, top, 'T');
        } while (toTop && freeRow(top - 1));
    } else {
        if (top == base) {
            undoStack_.pop_back();
            showMessage("YA ESTA EN SU ALTO MINIMO");
            return;
        }
        do {
            setTile(left, top, '.');
            setTile(left + 1, top, '.');
            ++top;
        } while (toTop && top < base);
    }
    markDirty();
    showMessage("ALTO DEL CANO: " + std::to_string(base - top + 4));
}

void LevelEditor::toggleWarpAt(int col, int row) {
    // Tuberia en L: el warp va en su boca (se entra caminando a la derecha).
    if (const int pipe = sidePipeAt(col, row); pipe >= 0 && !tilemap_.isPipeTopTile(col, row)) {
        const auto& decoration = meta_["decorations"][static_cast<size_t>(pipe)];
        const int pc = decoration.value("col", 0);
        const int pr = decoration.value("row", 0) + 2;
        auto& list = meta_["warps"];
        if (!list.is_array()) list = nlohmann::json::array();
        for (auto it = list.begin(); it != list.end(); ++it) {
            if (it->value("col", -1) == pc && it->value("row", -1) == pr) {
                pushUndo();
                list.erase(it);
                markDirty();
                showMessage("WARP BORRADO");
                return;
            }
        }
        warpSourceCol_ = pc;
        warpSourceRow_ = pr;
        warpSourceSide_ = true;
        mode_ = Mode::PickWarpDest;
        return;
    }
    if (tileAt(col, row) == 'Y') {
        // Horizontal: el warp va en su boca (punta izquierda, se entra
        // empujando a la derecha).
        const int top = hpipeTopRow(col, row);
        const int mouth = hpipeLeftCol(col, top);
        auto& list = meta_["warps"];
        if (!list.is_array()) list = nlohmann::json::array();
        for (auto it = list.begin(); it != list.end(); ++it) {
            if (it->value("col", -1) == mouth && it->value("row", -1) == top) {
                pushUndo();
                list.erase(it);
                markDirty();
                showMessage("WARP BORRADO");
                return;
            }
        }
        warpSourceCol_ = mouth;
        warpSourceRow_ = top;
        warpSourceSide_ = true;
        mode_ = Mode::PickWarpDest;
        return;
    }
    if (!tilemap_.isPipeTopTile(col, row)) {
        showMessage("P: CON EL MOUSE SOBRE EL TOPE DE UNA TUBERIA O LA BOCA DE UNA HORIZONTAL");
        return;
    }
    warpSourceSide_ = false;
    const int left = tilemap_.isPipeLeftHalf(col, row) ? col : col - 1;
    auto& list = meta_["warps"];
    if (!list.is_array()) list = nlohmann::json::array();
    for (auto it = list.begin(); it != list.end(); ++it) {
        if (it->value("col", -1) == left && it->value("row", -1) == row) {
            pushUndo();
            list.erase(it);
            markDirty();
            showMessage("WARP BORRADO");
            return;
        }
    }
    warpSourceCol_ = left;
    warpSourceRow_ = row;
    mode_ = Mode::PickWarpDest;
}

void LevelEditor::shakeAt(int col, int row) {
    // "Sacudir" (Mario Maker): transforma el enemigo -- Koopa verde <-> rojo.
    std::string listName;
    size_t index = 0;
    if (!findObjectAt(col, row, listName, index) || listName != "entities") return;
    auto& entity = meta_["entities"][index];
    if (entity.value("type", std::string()) != "koopa") {
        showMessage("ESTE ENEMIGO NO SE TRANSFORMA");
        return;
    }
    pushUndo();
    const bool red = entity.value("color", std::string("green")) == "red";
    if (red) {
        entity.erase("color");
    } else {
        entity["color"] = "red";
    }
    // Las alas cambian con el color (en el original el verde salta y el rojo vuela).
    if (entity.contains("state")) entity["state"] = red ? "jumping" : "flying";
    markDirty();
}

void LevelEditor::toggleWingsAt(int col, int row) {
    std::string listName;
    size_t index = 0;
    if (!findObjectAt(col, row, listName, index) || listName != "entities") return;
    auto& entity = meta_["entities"][index];
    if (entity.value("type", std::string()) != "koopa") {
        showMessage("EN ESTE JUEGO SOLO EL KOOPA PUEDE TENER ALAS");
        return;
    }
    pushUndo();
    if (entity.contains("state")) {
        entity.erase("state");
    } else {
        const bool red = entity.value("color", std::string("green")) == "red";
        entity["state"] = red ? "flying" : "jumping";
    }
    markDirty();
}

void LevelEditor::cycleWorld() {
    pushUndo();
    const std::string current = meta_.value("world", std::string());
    size_t index = 0;
    while (index < std::size(kWorlds) && current != kWorlds[index]) ++index;
    // Sin "world" -> overworld -> underground -> underwater -> castle -> sin "world".
    if (current.empty()) {
        meta_["world"] = kWorlds[0];
    } else if (index + 1 < std::size(kWorlds)) {
        meta_["world"] = kWorlds[index + 1];
    } else {
        meta_.erase("world");
    }
    markDirty();
}

int LevelEditor::viewCenterCol() const {
    return static_cast<int>(cameraX_) / kTile + kViewWidth / kTile / 2;
}

void LevelEditor::cyclePalette() {
    pushUndo();
    const int col = viewCenterCol();
    const std::string current = areaPaletteAt(col);
    size_t index = 0;
    while (index < std::size(kPalettes) && current != kPalettes[index]) ++index;
    const std::string next = index + 1 < std::size(kPalettes) ? kPalettes[index + 1] : kPalettes[0];
    // Un "world" viejo de noche/nieve pasa a ser superficie + paleta.
    const auto plainWorld = [](nlohmann::json& owner) {
        const std::string world = owner.value("world", std::string());
        if (world == "night" || world == "snow" || world == "night_snow") owner["world"] = "overworld";
    };
    bool done = false;
    auto found = meta_.find("areas");
    if (found != meta_.end() && found->is_array()) {
        for (auto& area : *found) {
            if (col < area.value("start", 0) || col > area.value("end", 0)) continue;
            plainWorld(area);
            area["palette"] = next;   // "" tambien: que no herede la del nivel
            done = true;
            break;
        }
    }
    if (!done) {
        plainWorld(meta_);
        if (next.empty()) meta_.erase("palette");
        else meta_["palette"] = next;
    }
    showMessage(std::string("PALETA: ") + paletteLabel(next));
    markDirty();
}

void LevelEditor::addSubArea() {
    // Subarea (Mario Maker): un tramo aparte en el mismo mapa, a la derecha,
    // con camara propia (campo "areas" del nivel). Se conecta con tuberias (P).
    pushUndo();
    const int oldWidth = widthTiles();
    auto& areas = meta_["areas"];
    if (!areas.is_array() || areas.empty()) {
        areas = nlohmann::json::array();
        areas.push_back({{"start", 0}, {"end", oldWidth - 1}});
    }
    const int start = oldWidth + 1;
    for (int r = 0; r < heightTiles(); ++r) {
        rows_[r].push_back('#');  // columna separadora, nunca se ve
        rows_[r].append(kSubAreaWidth, r >= 14 ? '#' : '.');
    }
    areas.push_back({{"start", start}, {"end", start + kSubAreaWidth - 1}, {"world", "underground"}});
    cameraX_ = static_cast<float>(start * kTile);
    markDirty();
    showMessage("SUBAREA CREADA: CONECTALA CON TUBERIAS (P SOBRE EL TOPE DE CADA UNA)", 4000);
}

void LevelEditor::resetLevel() {
    // Reset Rocket: borra todo (se puede deshacer).
    pushUndo();
    const std::string name = meta_.value("name", std::string("NUEVO"));
    const int time = meta_.value("time", 400);
    const std::string path = path_;
    newLevel();
    meta_["name"] = name;
    meta_["time"] = time;
    path_ = path;
    markDirty();
}

void LevelEditor::playTest(bool fromView) {
    if (!saveLevel()) return;
    if (embedded_) {
        playRequested_ = true;
        playStartCol_ = fromView ? static_cast<int>(cameraX_) / kTile + 2 : -1;
        return;
    }
#ifdef _WIN32
    // mario.exe esta en la misma carpeta que el editor (build/), con los
    // assets copiados al lado: se lanza ahi como directorio de trabajo.
    char* base = SDL_GetBasePath();
    const std::string dir = base ? base : "";
    SDL_free(base);
    std::string command = "\"" + dir + "mario.exe\" --level \"" + fs::absolute(path_).string() + "\"";
    if (fromView) {
        command += " --start-col " + std::to_string(static_cast<int>(cameraX_) / kTile + 2);
    }
    STARTUPINFOA startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (CreateProcessA(nullptr, command.data(), nullptr, nullptr, FALSE, 0, nullptr, dir.c_str(), &startup,
                       &process)) {
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
    } else {
        showMessage("NO SE PUDO ABRIR MARIO.EXE", 3000);
    }
#else
    (void)fromView;
#endif
}

// ============================================================ seleccion

void LevelEditor::copySelection(bool cut) {
    if (!hasSelection()) {
        showMessage("PRIMERO SELECCIONA UN AREA (BOTON SELECCION)");
        return;
    }
    clipboard_ = Clipboard{};
    clipboard_.width = selC1_ - selC0_ + 1;
    clipboard_.height = selR1_ - selR0_ + 1;
    for (int r = selR0_; r <= selR1_; ++r) {
        std::string line;
        for (int c = selC0_; c <= selC1_; ++c) line.push_back(tileAt(c, r));
        clipboard_.tiles.push_back(line);
    }
    for (const char* key : kObjectLists) {
        nlohmann::json copied = nlohmann::json::array();
        for (const auto& object : meta_.value(key, nlohmann::json::array())) {
            const int c = object.value("col", -1);
            const int r = object.value("row", -1);
            if (c < selC0_ || c > selC1_ || r < selR0_ || r > selR1_) continue;
            nlohmann::json relative = object;
            relative["col"] = c - selC0_;
            relative["row"] = r - selR0_;
            copied.push_back(relative);
        }
        clipboard_.objects[key] = copied;
    }
    clipboard_.valid = true;
    if (cut) {
        pushUndo();
        eraseSelection();
        showMessage("CORTADO: CTRL+V PEGA DONDE ESTE EL MOUSE");
    } else {
        showMessage("COPIADO: CTRL+V PEGA DONDE ESTE EL MOUSE");
    }
}

void LevelEditor::eraseSelection() {
    if (!hasSelection()) return;
    for (int r = selR0_; r <= selR1_; ++r) {
        for (int c = selC0_; c <= selC1_; ++c) setTile(c, r, '.');
    }
    for (const char* key : kObjectLists) {
        auto& list = meta_[key];
        if (!list.is_array()) continue;
        for (size_t i = list.size(); i-- > 0;) {
            const int c = list[i].value("col", -1);
            const int r = list[i].value("row", -1);
            if (c >= selC0_ && c <= selC1_ && r >= selR0_ && r <= selR1_) list.erase(i);
        }
    }
    markDirty();
}

void LevelEditor::paste(int col, int row) {
    if (!clipboard_.valid) {
        showMessage("NO HAY NADA COPIADO");
        return;
    }
    pushUndo();
    for (int r = 0; r < clipboard_.height; ++r) {
        for (int c = 0; c < clipboard_.width; ++c) {
            const char tile = clipboard_.tiles[r][c];
            if (tile != '.') setTile(col + c, row + r, tile);
        }
    }
    for (const char* key : kObjectLists) {
        auto& list = meta_[key];
        if (!list.is_array()) list = nlohmann::json::array();
        for (const auto& object : clipboard_.objects.value(key, nlohmann::json::array())) {
            nlohmann::json placed = object;
            placed["col"] = object.value("col", 0) + col;
            placed["row"] = object.value("row", 0) + row;
            if (placed["col"].get<int>() >= widthTiles() || placed["row"].get<int>() >= heightTiles()) continue;
            list.push_back(placed);
        }
    }
    selC0_ = col;
    selR0_ = row;
    selC1_ = std::min(widthTiles() - 1, col + clipboard_.width - 1);
    selR1_ = std::min(heightTiles() - 1, row + clipboard_.height - 1);
    selectionValid_ = true;
    markDirty();
}

// ============================================================ entrada

bool LevelEditor::mouseToCell(int x, int y, int& col, int& row) const {
    if (x < 0 || x >= kViewWidth || y < kTopBar || y >= kTopBar + kViewHeight) return false;
    col = static_cast<int>(std::floor((static_cast<float>(x) + cameraX_) / kTile));
    row = (y - kTopBar) / kTile;
    return col >= 0 && col < widthTiles() && row >= 0 && row < heightTiles();
}

SDL_Rect LevelEditor::buttonRect(Button button) const {
    const int i = static_cast<int>(button);
    if (button == Button::World) return SDL_Rect{kPaletteX + 2, kButtonsY + 7 * 16, 124, 14};
    if (button == Button::Commands) return SDL_Rect{kPaletteX + 2, kButtonsY + 5 * 16, 124, 14};
    if (button == Button::Palette) return SDL_Rect{kPaletteX + 2, kButtonsY + 6 * 16, 124, 14};
    return SDL_Rect{kPaletteX + 2 + (i % 2) * 63, kButtonsY + (i / 2) * 16, 61, 14};
}

SDL_Rect LevelEditor::minimapRect() const {
    const int width = std::min(kViewWidth - 8, std::max(1, widthTiles()) * 2);
    return SDL_Rect{4, kTopBar + kViewHeight + 3, width, kLevelRows};
}

int LevelEditor::palettePageCount() const {
    const int count = static_cast<int>(toolsInTab(currentTab_).size());
    const int perPage = kPaletteCols * kPaletteRows;
    if (count <= perPage) return 1;
    return (count + perPage - 2) / (perPage - 1);
}

std::vector<int> LevelEditor::paletteToolsOnPage() const {
    const std::vector<int> all = toolsInTab(currentTab_);
    const int perPage = kPaletteCols * kPaletteRows;
    if (static_cast<int>(all.size()) <= perPage) return all;
    // Paginas de perPage-1 herramientas + la celda "MAS" (-1).
    const int page = tabPage_[static_cast<int>(currentTab_)] % palettePageCount();
    std::vector<int> result;
    for (int i = page * (perPage - 1); i < static_cast<int>(all.size()) && static_cast<int>(result.size()) < perPage - 1; ++i) {
        result.push_back(all[static_cast<size_t>(i)]);
    }
    result.push_back(-1);
    return result;
}

std::vector<int> LevelEditor::toolsInTab(Tab tab) const {
    std::vector<int> result;
    for (int i = 0; i < static_cast<int>(tools_.size()); ++i) {
        if (tools_[i].tab == tab) result.push_back(i);
    }
    return result;
}

void LevelEditor::pressButton(Button button) {
    switch (button) {
        case Button::Move:
            for (int i = 0; i < static_cast<int>(tools_.size()); ++i) {
                if (tools_[i].kind == ToolKind::Move) selectTool(i);
            }
            showMessage("MOVER: AGARRA UN ENEMIGO U OBJETO Y ARRASTRALO");
            break;
        case Button::Open:
            requestLeave(LeaveAction::Open);
            break;
        case Button::Save:
            saveLevel();
            break;
        case Button::Commands:
            mode_ = Mode::Help;
            break;
        case Button::Eraser:
            for (int i = 0; i < static_cast<int>(tools_.size()); ++i) {
                if (tools_[i].kind == ToolKind::Eraser) selectTool(i);
            }
            break;
        case Button::Select:
            for (int i = 0; i < static_cast<int>(tools_.size()); ++i) {
                if (tools_[i].kind == ToolKind::Select) selectTool(i);
            }
            break;
        case Button::Undo:
            undo();
            break;
        case Button::Reset:
            if (!confirmReset_) {
                confirmReset_ = true;
                showMessage("BORRAR TODO EL NIVEL? APRETA BORRAR TODO DE NUEVO (CTRL+Z LO DESHACE)", 4000);
            } else {
                confirmReset_ = false;
                resetLevel();
                showMessage("NIVEL BORRADO (CTRL+Z LO DESHACE)");
            }
            return;
        case Button::Play:
            playTest(false);
            break;
        case Button::PlayHere:
            playTest(true);
            break;
        case Button::SubArea:
            addSubArea();
            break;
        case Button::Palette:
            cyclePalette();
            break;
        case Button::World:
            cycleWorld();
            showMessage(std::string("TIPO DE MUNDO: ") + worldLabel(meta_.value("world", std::string())));
            break;
        case Button::Count:
            break;
    }
    confirmReset_ = false;
}

void LevelEditor::handlePanelClick(int x, int y) {
    // Pestanas.
    if (y >= kTabsY && y < kTabsY + 12) {
        const int tab = (x - kPaletteX) / kPaletteCellW;
        if (tab >= 0 && tab < static_cast<int>(Tab::Count)) currentTab_ = static_cast<Tab>(tab);
        return;
    }
    // Herramientas de la pestana actual.
    const std::vector<int> pageTools = paletteToolsOnPage();
    if (y >= kPaletteTop && y < kPaletteTop + kPaletteRows * kPaletteCellH) {
        const int cx = (x - kPaletteX) / kPaletteCellW;
        const int cy = (y - kPaletteTop) / kPaletteCellH;
        const int index = cy * kPaletteCols + cx;
        if (cx >= 0 && cx < kPaletteCols && index >= 0 && index < static_cast<int>(pageTools.size())) {
            if (pageTools[index] < 0) {
                int& page = tabPage_[static_cast<int>(currentTab_)];
                page = (page + 1) % palettePageCount();
            } else {
                selectTool(pageTools[index]);
            }
        }
        return;
    }
    // Botones.
    for (int i = 0; i < static_cast<int>(Button::Count); ++i) {
        const SDL_Rect rect = buttonRect(static_cast<Button>(i));
        if (x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h) {
            pressButton(static_cast<Button>(i));
            return;
        }
    }
}

void LevelEditor::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_QUIT) {
        if (mode_ == Mode::ConfirmLeave) {
            running_ = false;   // cerrar de nuevo con el cartel abierto: sale
        } else {
            requestLeave(LeaveAction::Quit);
        }
        return;
    }
    if (mode_ == Mode::ConfirmLeave) {
        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
            for (int i = 0; i < 3; ++i) {
                const SDL_Rect r = leaveButtonRect(i);
                if (event.button.x >= r.x && event.button.x < r.x + r.w && event.button.y >= r.y &&
                    event.button.y < r.y + r.h) {
                    mode_ = Mode::Edit;
                    if (i == 0) {
                        if (saveLevel()) doLeave(leaveAction_);
                    } else if (i == 1) {
                        doLeave(leaveAction_);
                    }
                    return;
                }
            }
        } else if (event.type == SDL_KEYDOWN) {
            const SDL_Keycode code = event.key.keysym.sym;
            if (code == SDLK_ESCAPE) {
                mode_ = Mode::Edit;
            } else if (code == SDLK_RETURN || code == SDLK_g) {
                mode_ = Mode::Edit;
                if (saveLevel()) doLeave(leaveAction_);
            } else if (code == SDLK_s) {
                mode_ = Mode::Edit;
                doLeave(leaveAction_);
            }
        }
        return;
    }
    if (mode_ == Mode::Help) {
        // La ventana de comandos se cierra con cualquier tecla o clic.
        if (event.type == SDL_KEYDOWN || event.type == SDL_MOUSEBUTTONDOWN) mode_ = Mode::Edit;
        return;
    }
    if (mode_ == Mode::OpenFile && event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT &&
        !levelFiles_.empty()) {
        const int x = event.button.x;
        const int y = event.button.y;
        const int count = static_cast<int>(levelFiles_.size());
        auto inside = [&](const SDL_Rect& r) { return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h; };
        if (inside(pickerArrowRect(-1))) {
            fileSelection_ = (fileSelection_ + count - 1) % count;
            loadPreview();
        } else if (inside(pickerArrowRect(1))) {
            fileSelection_ = (fileSelection_ + 1) % count;
            loadPreview();
        } else if (inside(SDL_Rect{96, 62, 448, 224})) {
            mode_ = Mode::Edit;
            if (loadLevel(levelFiles_[fileSelection_])) undoStack_.clear();
        }
        return;
    }
    if (event.type == SDL_TEXTINPUT && mode_ == Mode::TextInput) {
        textValue_ += event.text.text;
        return;
    }
    if (event.type == SDL_KEYDOWN) {
        handleKey(event.key);
        return;
    }
    if (mode_ == Mode::OpenFile || mode_ == Mode::TextInput) return;

    if (event.type == SDL_MOUSEWHEEL) {
        cameraX_ -= static_cast<float>(event.wheel.y) * kTile * 2.0f;
        return;
    }
    if (event.type == SDL_MOUSEBUTTONDOWN) {
        const int x = event.button.x;
        const int y = event.button.y;
        if (x >= kPaletteX) {
            handlePanelClick(x, y);
            return;
        }
        if (y >= kTopBar + kViewHeight && structureMapClick(x, y)) {
            draggingMinimap_ = true;
            return;
        }
        if (event.button.button == SDL_BUTTON_MIDDLE) {
            panning_ = true;
            panStartMouseX_ = x;
            panStartCameraX_ = cameraX_;
            return;
        }
        int col = 0, row = 0;
        if (!mouseToCell(x, y, col, row)) return;
        if (mode_ == Mode::PickWarpDest) {
            if (event.button.button == SDL_BUTTON_LEFT) {
                pushUndo();
                auto& list = meta_["warps"];
                if (!list.is_array()) list = nlohmann::json::array();
                nlohmann::json warp{{"col", warpSourceCol_}, {"row", warpSourceRow_}, {"dest_col", col}, {"dest_row", row}};
                if (warpSourceSide_) warp["dir"] = "right";
                list.push_back(warp);
                markDirty();
                showMessage("WARP CREADO");
            }
            mode_ = Mode::Edit;
            return;
        }
        const Tool& tool = tools_[selectedTool_];
        if (event.button.button == SDL_BUTTON_LEFT && tool.kind == ToolKind::Move) {
            // La punta de una tuberia: se estira arrastrandola.
            if (const Stretch tip = pipeTipAt(col, row); tip.kind != StretchKind::None) {
                pushUndo();
                stretch_ = tip;
                SDL_Cursor* cursor = (tip.kind == StretchKind::SideMouth || tip.kind == StretchKind::HMouth) ? resizeHCursor_
                                                                                                   : resizeVCursor_;
                if (cursor) SDL_SetCursor(cursor);
                return;
            }
            // Mover: agarra lo que haya ahi (enemigos y plataformas primero,
            // despues la escenografia) y nunca pone nada.
            std::string listName;
            size_t index = 0;
            if (findObjectAt(col, row, listName, index, true)) {
                pushUndo();
                grab_.active = true;
                grab_.list = listName;
                grab_.index = index;
                grab_.offsetCol = col - meta_[listName][index].value("col", 0);
                grab_.offsetRow = row - meta_[listName][index].value("row", 0);
                grab_.startCol = meta_[listName][index].value("col", 0);
                grab_.startRow = meta_[listName][index].value("row", 0);
                if (moveCursor_) SDL_SetCursor(moveCursor_);
            }
            return;
        }
        if (event.button.button == SDL_BUTTON_LEFT && tool.kind == ToolKind::Select) {
            selecting_ = true;
            selectionValid_ = true;
            selAnchorCol_ = selC0_ = selC1_ = col;
            selAnchorRow_ = selR0_ = selR1_ = row;
            return;
        }
        pushUndo();
        if (event.button.button == SDL_BUTTON_LEFT) {
            // Agarrar lo ya puesto (como en Mario Maker) antes que poner encima.
            std::string listName;
            size_t index = 0;
            if (tool.kind != ToolKind::Eraser && tool.kind != ToolKind::Tile && tool.kind != ToolKind::Item &&
                findObjectAt(col, row, listName, index, tool.kind == ToolKind::Decoration)) {
                grab_.active = true;
                grab_.list = listName;
                grab_.index = index;
                grab_.offsetCol = col - meta_[listName][index].value("col", 0);
                grab_.offsetRow = row - meta_[listName][index].value("row", 0);
                grab_.startCol = meta_[listName][index].value("col", 0);
                grab_.startRow = meta_[listName][index].value("row", 0);
                return;
            }
            if (tool.kind == ToolKind::Eraser) {
                erasing_ = true;
                eraseAt(col, row);
            } else {
                painting_ = true;
                placeAt(col, row);
            }
        } else if (event.button.button == SDL_BUTTON_RIGHT) {
            erasing_ = true;
            eraseAt(col, row);
        }
        lastPaintCol_ = col;
        lastPaintRow_ = row;
        return;
    }
    if (event.type == SDL_MOUSEBUTTONUP) {
        if (stretch_.kind != StretchKind::None) {
            stretch_ = Stretch{};
            markDirty();
        }
        if (grab_.active) {
            grab_.active = false;
            const auto& object = meta_[grab_.list][grab_.index];
            if (grab_.list == "decorations" && object.value("type", std::string()) == "pipe_side") {
                // La tuberia en L se lleva su tramo solido y su warp.
                const int newCol = object.value("col", 0);
                const int newRow = object.value("row", 0);
                const int length = object.value("length", 0);
                // El tramo vertical estirado (tiles 'T' arriba) se mueve igual.
                int extra = 0;
                const int oldVc = grab_.startCol + 2 + length;
                while (grab_.startRow - extra - 1 >= 0 && tileAt(oldVc, grab_.startRow - extra - 1) == 'T' &&
                       tileAt(oldVc + 1, grab_.startRow - extra - 1) == 'T') {
                    ++extra;
                }
                for (int k = 1; k <= extra; ++k) {
                    setTile(oldVc, grab_.startRow - k, '.');
                    setTile(oldVc + 1, grab_.startRow - k, '.');
                }
                setSidePipeSolid(grab_.startCol, grab_.startRow, length, false);
                setSidePipeSolid(newCol, newRow, length, true);
                const int newVc = newCol + 2 + length;
                for (int k = 1; k <= extra && newRow - k >= 1; ++k) {
                    if (tileAt(newVc, newRow - k) != '.' || tileAt(newVc + 1, newRow - k) != '.') break;
                    setTile(newVc, newRow - k, 'T');
                    setTile(newVc + 1, newRow - k, 'T');
                }
                auto& warps = meta_["warps"];
                if (warps.is_array()) {
                    for (auto& warp : warps) {
                        if (warp.value("col", -1) == grab_.startCol && warp.value("row", -1) == grab_.startRow + 2) {
                            warp["col"] = newCol;
                            warp["row"] = newRow + 2;
                        }
                    }
                }
            }
            markDirty();
            if (tools_[selectedTool_].kind == ToolKind::Move && handCursor_) SDL_SetCursor(handCursor_);
        }
        painting_ = erasing_ = panning_ = selecting_ = draggingMinimap_ = false;
        return;
    }
    if (event.type == SDL_MOUSEMOTION) {
        if (panning_) cameraX_ = panStartCameraX_ - static_cast<float>(event.motion.x - panStartMouseX_);
        if (draggingMinimap_) structureMapClick(event.motion.x, event.motion.y);
        int col = 0, row = 0;
        if (!mouseToCell(event.motion.x, event.motion.y, col, row)) {
            hoverCol_ = hoverRow_ = -1;
            return;
        }
        hoverCol_ = col;
        hoverRow_ = row;
        if (stretch_.kind != StretchKind::None) {
            applyStretch(col, row);
            return;
        }
        // Mover: flechas sobre la punta de una tuberia (se estira), la
        // manito sobre lo que se puede agarrar.
        if (tools_[selectedTool_].kind == ToolKind::Move && !grab_.active) {
            std::string listName;
            size_t index = 0;
            const Stretch tip = pipeTipAt(col, row);
            SDL_Cursor* cursor = (tip.kind == StretchKind::SideMouth || tip.kind == StretchKind::HMouth) ? resizeHCursor_
                                 : tip.kind != StretchKind::None     ? resizeVCursor_
                                 : findObjectAt(col, row, listName, index, true) ? handCursor_
                                                                                 : arrowCursor_;
            if (cursor) SDL_SetCursor(cursor);
        }
        if (selecting_) {
            selC0_ = std::min(selAnchorCol_, col);
            selC1_ = std::max(selAnchorCol_, col);
            selR0_ = std::min(selAnchorRow_, row);
            selR1_ = std::max(selAnchorRow_, row);
            return;
        }
        if (grab_.active) {
            auto& object = meta_[grab_.list][grab_.index];
            object["col"] = std::max(0, col - grab_.offsetCol);
            object["row"] = std::max(0, row - grab_.offsetRow);
            rebuildTilemap();
            return;
        }
        // Arrastrar pinta/borra tiles (los objetos van de a uno por clic).
        if ((col != lastPaintCol_ || row != lastPaintRow_) && (painting_ || erasing_)) {
            const ToolKind kind = tools_[selectedTool_].kind;
            // Todas las celdas entre la posicion anterior y la actual: con el
            // mouse rapido el evento de movimiento salta varias de una.
            const int steps = std::max(std::abs(col - lastPaintCol_), std::abs(row - lastPaintRow_));
            for (int i = 1; i <= steps; ++i) {
                const int c = lastPaintCol_ + static_cast<int>(std::lround(
                    static_cast<float>((col - lastPaintCol_) * i) / static_cast<float>(steps)));
                const int r = lastPaintRow_ + static_cast<int>(std::lround(
                    static_cast<float>((row - lastPaintRow_) * i) / static_cast<float>(steps)));
                if (painting_ && kind == ToolKind::Tile) placeAt(c, r);
                if (erasing_) eraseAt(c, r);
            }
            lastPaintCol_ = col;
            lastPaintRow_ = row;
        }
    }
}

void LevelEditor::startTextInput(const std::string& prompt, const std::string& initial, int target) {
    mode_ = Mode::TextInput;
    textPrompt_ = prompt;
    textValue_ = initial;
    textTarget_ = target;
    SDL_StartTextInput();
}

void LevelEditor::listLevelFiles() {
    levelFiles_.clear();
    try {
        for (const auto& entry : fs::recursive_directory_iterator(levelsDir_)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                levelFiles_.push_back(entry.path().generic_string());
            }
        }
    } catch (const std::exception&) {
    }
    // Orden: world1/1-1 ... world8/8-4 (por numero) y despues los demas.
    auto key = [](const std::string& path) {
        const fs::path p(path);
        const std::string folder = p.parent_path().filename().string();
        const std::string name = p.stem().string();
        int world = 99, level = 99;
        if (folder.rfind("world", 0) == 0 && std::sscanf(name.c_str(), "%d-%d", &world, &level) == 2) {
            return std::make_tuple(0, world, level, name);
        }
        return std::make_tuple(1, 0, 0, folder + "/" + name);
    };
    std::sort(levelFiles_.begin(), levelFiles_.end(),
              [&](const std::string& a, const std::string& b) { return key(a) < key(b); });
    fileSelection_ = 0;
    const std::string current = fs::path(path_).filename().string();
    for (size_t i = 0; i < levelFiles_.size(); ++i) {
        if (!path_.empty() && fs::path(levelFiles_[i]).filename().string() == current) fileSelection_ = static_cast<int>(i);
    }
    mode_ = Mode::OpenFile;
    previewIndex_ = -1;
    loadPreview();
}

void LevelEditor::handleKey(const SDL_KeyboardEvent& key) {
    const SDL_Keycode code = key.keysym.sym;
    const bool ctrl = (key.keysym.mod & KMOD_CTRL) != 0;
    const bool shift = (key.keysym.mod & KMOD_SHIFT) != 0;

    if (mode_ == Mode::TextInput) {
        if (code == SDLK_RETURN) {
            SDL_StopTextInput();
            pushUndo();
            if (textTarget_ == 0 && !textValue_.empty()) {
                const bool wasDefaultPath = path_.empty() || path_ == defaultPathForName();
                meta_["name"] = upper(textValue_);
                // Si el archivo salia del nombre, sigue saliendo del nombre nuevo.
                if (wasDefaultPath) path_.clear();
            } else if (textTarget_ == 1) {
                try {
                    // Como Mario Maker: de 10 a 500 (aca, cualquier valor hasta 999).
                    meta_["time"] = std::clamp(std::stoi(textValue_), 10, 999);
                } catch (const std::exception&) {
                }
            }
            mode_ = Mode::Edit;
            markDirty();
        } else if (code == SDLK_ESCAPE) {
            SDL_StopTextInput();
            mode_ = Mode::Edit;
        } else if (code == SDLK_BACKSPACE && !textValue_.empty()) {
            textValue_.pop_back();
        }
        return;
    }
    if (mode_ == Mode::OpenFile) {
        const int count = static_cast<int>(levelFiles_.size());
        if (code == SDLK_ESCAPE) {
            mode_ = Mode::Edit;
        } else if ((code == SDLK_LEFT || code == SDLK_RIGHT || code == SDLK_UP || code == SDLK_DOWN) && count > 0) {
            // Izquierda/derecha: el anterior/siguiente; arriba/abajo: de a 4
            // (el mismo nivel del mundo anterior/siguiente).
            const int step = code == SDLK_LEFT ? -1 : code == SDLK_RIGHT ? 1 : code == SDLK_UP ? -4 : 4;
            fileSelection_ = ((fileSelection_ + step) % count + count) % count;
            loadPreview();
        } else if (code == SDLK_RETURN && count > 0) {
            mode_ = Mode::Edit;
            if (loadLevel(levelFiles_[fileSelection_])) undoStack_.clear();
        }
        return;
    }
    if (code == SDLK_ESCAPE) {
        mode_ = Mode::Edit;
        selectionValid_ = false;
        return;
    }

    const float step = shift ? kTile * 8.0f : kTile * 2.0f;
    if (ctrl && code == SDLK_s) {
        saveLevel();
    } else if (ctrl && code == SDLK_o) {
        requestLeave(LeaveAction::Open);
    } else if (ctrl && code == SDLK_n) {
        requestLeave(LeaveAction::New);
    } else if (ctrl && code == SDLK_z) {
        undo();
    } else if (ctrl && code == SDLK_c) {
        copySelection(false);
    } else if (ctrl && code == SDLK_x) {
        copySelection(true);
    } else if (ctrl && code == SDLK_v && hoverCol_ >= 0) {
        paste(hoverCol_, hoverRow_);
    } else if (ctrl && code == SDLK_RIGHT) {
        resizeWidth(16);
    } else if (ctrl && code == SDLK_LEFT) {
        resizeWidth(-16);
    } else if (code == SDLK_DELETE && hasSelection()) {
        pushUndo();
        eraseSelection();
    } else if (code == SDLK_LEFT) {
        cameraX_ -= step;
    } else if (code == SDLK_RIGHT) {
        cameraX_ += step;
    } else if (code == SDLK_HOME) {
        cameraX_ = 0.0f;
    } else if (code == SDLK_END) {
        cameraX_ = static_cast<float>(widthTiles() * kTile);
    } else if (code == SDLK_q) {
        selectTool((selectedTool_ + static_cast<int>(tools_.size()) - 1) % static_cast<int>(tools_.size()));
    } else if (code == SDLK_e) {
        selectTool((selectedTool_ + 1) % static_cast<int>(tools_.size()));
    } else if (code == SDLK_TAB) {
        currentTab_ = static_cast<Tab>((static_cast<int>(currentTab_) + 1) % static_cast<int>(Tab::Count));
    } else if (code == SDLK_g) {
        showGrid_ = !showGrid_;
    } else if (code == SDLK_w) {
        cycleWorld();

    } else if (code == SDLK_s && hoverCol_ >= 0) {
        shakeAt(hoverCol_, hoverRow_);
    } else if (code == SDLK_a && hoverCol_ >= 0) {
        toggleWingsAt(hoverCol_, hoverRow_);
    } else if (code == SDLK_i && hoverCol_ >= 0) {
        cycleBlockItem(hoverCol_, hoverRow_);
    } else if ((code == SDLK_PLUS || code == SDLK_KP_PLUS || code == SDLK_EQUALS || code == SDLK_MINUS ||
                code == SDLK_KP_MINUS) &&
               hoverCol_ >= 0 && whirlpoolAt(hoverCol_) >= 0) {
        auto& pool = meta_["entities"][static_cast<size_t>(whirlpoolAt(hoverCol_))];
        const int delta = (code == SDLK_MINUS || code == SDLK_KP_MINUS) ? -1 : 1;
        pushUndo();
        pool["length"] = std::clamp(pool.value("length", 4) + delta, 1, 40);
        markDirty();
        showMessage("REMOLINO: " + std::to_string(pool.value("length", 4)) + " BLOQUES DE ANCHO");
    } else if ((code == SDLK_PLUS || code == SDLK_KP_PLUS || code == SDLK_EQUALS) && hoverCol_ >= 0) {
        if (const int pipe = sidePipeAt(hoverCol_, hoverRow_); pipe >= 0) {
            stretchSidePipe(pipe, 1, shift);
        } else {
            changeBlockHits(hoverCol_, hoverRow_, 1);
        }
    } else if ((code == SDLK_MINUS || code == SDLK_KP_MINUS) && hoverCol_ >= 0) {
        if (const int pipe = sidePipeAt(hoverCol_, hoverRow_); pipe >= 0) {
            stretchSidePipe(pipe, -1, shift);
        } else {
            changeBlockHits(hoverCol_, hoverRow_, -1);
        }
    } else if (code == SDLK_p && hoverCol_ >= 0) {
        toggleWarpAt(hoverCol_, hoverRow_);
    } else if (code == SDLK_F1) {
        mode_ = Mode::Help;
    } else if (code == SDLK_F2) {
        startTextInput("NOMBRE DEL NIVEL (EJ. 1-2)", meta_.value("name", std::string()), 0);
    } else if (code == SDLK_F3) {
        startTextInput("TIEMPO DEL NIVEL", std::to_string(meta_.value("time", 400)), 1);
    } else if (code == SDLK_F5) {
        playTest(shift);
    }
}

// ============================================================ dibujo

std::string LevelEditor::areaPaletteAt(int col) const {
    for (const auto& area : meta_.value("areas", nlohmann::json::array())) {
        if (col >= area.value("start", 0) && col <= area.value("end", 0)) return paletteOf(meta_, &area);
    }
    return paletteOf(meta_, nullptr);
}

int LevelEditor::areaRowAt(int col) const {
    for (const auto& area : meta_.value("areas", nlohmann::json::array())) {
        if (col < area.value("start", 0) || col > area.value("end", 0)) continue;
        if (paletteOf(meta_, &area) == "gray") {
            return static_cast<int>(WorldType::Castle);
        }
        return static_cast<int>(worldTypeFromName(area.value("world", meta_.value("world", std::string()))));
    }
    // Sin areas: la paleta del nivel (GRIS = colores de castillo, como el mapa).
    if (paletteOf(meta_, nullptr) == "gray") return static_cast<int>(WorldType::Castle);
    return worldRow();
}

int LevelEditor::worldRow() const {
    return static_cast<int>(worldTypeFromName(meta_.value("world", std::string())));
}

int LevelEditor::decorationIndex(const std::string& type) const {
    for (int i = 0; i < kDecorationCount; ++i) {
        if (type == kDecorations[i].type) return i;
    }
    return -1;
}

void LevelEditor::fillRect(const SDL_Rect& rect, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(renderer_, &rect);
}

void LevelEditor::drawRect(const SDL_Rect& rect, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    SDL_RenderDrawRect(renderer_, &rect);
}

void LevelEditor::drawText(int x, int y, const std::string& text, SDL_Color color, int scale) {
    DebugFont::drawText(renderer_, x, y, upper(text), scale, color);
}

void LevelEditor::render() {
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);
    renderLevel();
    renderPalette();
    renderStatus();
    if (mode_ == Mode::OpenFile) renderFileList();
    if (mode_ == Mode::ConfirmLeave) renderConfirmLeave();
    if (mode_ == Mode::Help) renderHelp();
    SDL_RenderPresent(renderer_);
}

void LevelEditor::renderLevel() {
    const float maxCamera = std::max(0.0f, static_cast<float>(widthTiles() * kTile - kViewWidth));
    cameraX_ = std::clamp(cameraX_, 0.0f, maxCamera);

    const SDL_Rect view{0, kTopBar, kViewWidth, kViewHeight};
    SDL_RenderSetViewport(renderer_, &view);
    const SDL_Rect clip{0, 0, kViewWidth, kViewHeight};
    SDL_RenderSetClipRect(renderer_, &clip);

    const WorldType levelWorld = worldTypeFromName(meta_.value("world", std::string()));
    const int cam = static_cast<int>(std::lround(cameraX_));
    // Cada area con su tipo de mundo y su fondo (en un nivel como el 1-2 la
    // escena de entrada es de superficie y el resto subterraneo), como en el
    // juego. Sin "areas": todo el nivel con el mundo del nivel.
    struct AreaView {
        int start;
        int end;
        WorldType world;
        bool dark;
    };
    std::vector<AreaView> areaViews;
    for (const auto& area : meta_.value("areas", nlohmann::json::array())) {
        const std::string palette = paletteOf(meta_, &area);
        const bool gray = palette == "gray";
        const WorldType world = gray ? WorldType::Castle : worldTypeFromName(area.value("world", meta_.value("world", std::string())));
        areaViews.push_back({area.value("start", 0), area.value("end", widthTiles() - 1), world,
                             palette == "night" || palette == "night_snow" || gray});
    }
    if (areaViews.empty()) {
        const std::string palette = paletteOf(meta_, nullptr);
        areaViews.push_back({0, widthTiles() - 1, palette == "gray" ? WorldType::Castle : levelWorld,
                             palette == "night" || palette == "night_snow" || palette == "gray"});
    }
    // Lo que no es de ningun area (columnas separadoras): el mundo del nivel.
    {
        const SDL_Color bg = backgroundColorFor(levelWorld);
        SDL_SetRenderDrawColor(renderer_, bg.r, bg.g, bg.b, 255);
        SDL_RenderFillRect(renderer_, nullptr);
    }
    for (const AreaView& area : areaViews) {
        const int left = std::max(0, area.start * kTile - cam);
        const int right = std::min(kViewWidth, (area.end + 1) * kTile - cam);
        if (right <= left) continue;
        const SDL_Rect areaClip{left, 0, right - left, kViewHeight};
        SDL_RenderSetClipRect(renderer_, &areaClip);
        // Mismo orden de dibujo y mismas variantes de paleta que Game::render.
        const std::string palette = areaPaletteAt(area.start);
        const bool gray = palette == "gray";
        const bool snow = palette == "snow" || palette == "night_snow";
        const bool mushroom = palette == "mushroom";
        const int row = static_cast<int>(area.world);
        const int tileRow = gray ? 0 : row;
        const SDL_Color bg = area.dark ? SDL_Color{0, 0, 0, 255} : backgroundColorFor(area.world);
        SDL_SetRenderDrawColor(renderer_, bg.r, bg.g, bg.b, 255);
        SDL_RenderFillRect(renderer_, &areaClip);
        const SpriteSheet& pipeSide = gray || area.world == WorldType::Castle ? pipeSideVariants_[1]
                                      : snow                                  ? pipeSideVariants_[0]
                                      : area.world == WorldType::Underground  ? pipeSideVariants_[2]
                                                                              : pipeSideSlices_;
        for (const auto& decoration : meta_.value("decorations", nlohmann::json::array())) {
            const int index = decorationIndex(decoration.value("type", std::string()));
            if (index < 0) continue;
            const int dx = decoration.value("col", 0) * kTile - cam;
            const int dy = decoration.value("row", 0) * kTile;
            const int length = decoration.value("length", 0);
            const std::string type = decoration.value("type", std::string());
            const bool cloud = type.rfind("cloud", 0) == 0;
            const bool castle = type.rfind("castle", 0) == 0;
            const SpriteSheet& sheet = gray && !cloud              ? decorationGraySprites_[index]
                                       : snow && !cloud && !castle ? decorationSnowSprites_[index]
                                                                   : decorationSprites_[index];
            if (type == "pipe_side" && pipeSide.isLoaded()) {
                // Como en el juego: en piezas, y sin el borde del tope si el
                // tramo vertical sigue hacia arriba.
                const int vc = decoration.value("col", 0) + 2 + length;
                const int pr = decoration.value("row", 0);
                const int pc = decoration.value("col", 0);
                const bool continues = tileAt(vc, pr - 1) == 'T' || tilemap_.isSolidTile(vc, pr - 1) || pr <= 1;
                const bool mouthBlocked = tilemap_.isSolidTile(pc - 1, pr + 2) || tilemap_.isSolidTile(pc - 1, pr + 3);
                for (int r = 0; r < 4; ++r) {
                    const int py = dy + r * kTile;
                    pipeSide.draw(renderer_, r, mouthBlocked ? 1 : 0, dx, py, false);
                    for (int k = 0; k <= length; ++k) pipeSide.draw(renderer_, r, 1, dx + (1 + k) * kTile, py, false);
                    if (r == 0 && continues) continue;
                    pipeSide.draw(renderer_, r, 2, dx + (2 + length) * kTile, py, false);
                    pipeSide.draw(renderer_, r, 3, dx + (3 + length) * kTile, py, false);
                }
            } else {
                sheet.draw(renderer_, 0, 0, dx, dy, false);
            }
        }
        tilemap_.renderTerrain(renderer_, bg, gray ? terrainGraySprite_ : terrainSprite_, tileRow, kViewWidth, cameraX_,
                               false);
        tilemap_.renderScenery(renderer_, gray ? sceneryTilesGraySprite_ : snow ? sceneryTilesSnowSprite_ : sceneryTilesSprite_,
                               tileRow, kViewWidth, cameraX_, snow && !gray ? &sceneryTilesSprite_ : nullptr);
        tilemap_.renderHazards(renderer_, lavaSprite_, bridgeSprite_, kViewWidth, cameraX_);
        tilemap_.renderPipes(renderer_, snow ? pipeSnowSprite_ : mushroom ? pipeMushroomSprite_ : pipeSprite_, row, cameraX_,
                             &pipeSide);
        // Tuberia en L estirada: sin el borde del tope en el medio.
        for (const auto& decoration : meta_.value("decorations", nlohmann::json::array())) {
            if (decoration.value("type", std::string()) != "pipe_side") continue;
            const int c = decoration.value("col", 0) + 2 + decoration.value("length", 0);
            const int r = decoration.value("row", 0);
            if (tileAt(c, r - 1) == 'T' || tilemap_.isSolidTile(c, r - 1) || r <= 1) {
                (snow ? pipeSnowSprite_ : mushroom ? pipeMushroomSprite_ : pipeSprite_)
                    .draw(renderer_, row, 1, c * kTile - cam, r * kTile, false);
            }
        }
        tilemap_.renderBlocks(renderer_, brickSprite_, row, cameraX_, {}, &questionSprite_, 0, &coinSprite_, 0);
    }
    SDL_RenderSetClipRect(renderer_, &clip);

    renderOverlays();

    SDL_RenderSetClipRect(renderer_, nullptr);
    SDL_RenderSetViewport(renderer_, nullptr);
}

namespace {

// Rotulo corto (cabe en un tile) de los enemigos que el editor no dibuja con
// su sprite.
std::string entityShortName(const std::string& type) {
    if (type == "buzzy") return "BZ";
    if (type == "piranha") return "PP";
    if (type == "hammer_bro") return "HB";
    if (type == "lakitu") return "LK";
    if (type == "blooper") return "BL";
    if (type == "cheep") return "CC";
    if (type == "cheep_jump") return "CJ";
    if (type == "podoboo") return "PO";
    if (type == "firebar") return "FB";
    if (type == "bowser") return "BW";
    if (type == "axe") return "AX";
    if (type == "bullet_bill") return "BB";
    if (type == "bullet_frenzy") return "F-BB";   // frenesi de Bill Balas
    if (type == "cheep_frenzy") return "F-CC";    // frenesi de Cheep Cheeps
    if (type == "flame_frenzy") return "F-LL";    // llamas de Bowser
    if (type == "loop") return "LAB";             // laberinto
    if (type == "warp_zone") return "WZ";
    if (type == "spring") return "TR";
    if (type == "vine") return "EN";
    if (type == "retainer") return "TD";
    if (type == "bridge_chain") return "CAD";
    if (type == "whirlpool") return "REM";
    return type.substr(0, 3);
}

}  // namespace

LevelEditor::EntitySprite LevelEditor::entitySprite(const nlohmann::json& entity, int levelRow) const {
    const std::string type = entity.value("type", std::string());
    // Hojas con una fila por mundo (superficie, agua, subterraneo, castillo).
    auto byWorld = [levelRow](const SpriteSheet& sheet, int w, int h) {
        EntitySprite sprite;
        sprite.sheet = sheet.isLoaded() ? &sheet : nullptr;
        sprite.w = w;
        sprite.h = h;
        sprite.row = levelRow;
        return sprite;
    };
    auto single = [](const SpriteSheet& sheet, int w, int h, int frame = 0) {
        EntitySprite sprite;
        sprite.sheet = sheet.isLoaded() ? &sheet : nullptr;
        sprite.w = w;
        sprite.h = h;
        sprite.frame = frame;
        return sprite;
    };
    if (type == "goomba") return byWorld(goombaSprite_, 16, 16);
    if (type == "koopa") {
        const bool red = entity.value("color", std::string("green")) == "red";
        EntitySprite sprite = byWorld(red ? koopaRedSprite_ : koopaSprite_, 16, 24);
        if (red) sprite.row = 0;
        return sprite;
    }
    if (type == "buzzy") return byWorld(buzzySprite_, 16, 16);
    if (type == "piranha") return byWorld(piranhaSprite_, 16, 24);
    if (type == "hammer_bro") return byWorld(hammerBroSprite_, 16, 24);
    if (type == "lakitu") return byWorld(lakituSprite_, 16, 24);
    if (type == "blooper") return byWorld(blooperSprite_, 16, 24);
    if (type == "cheep") {
        // El rojo es igual en todos los mundos; el gris va por tipo de mundo.
        if (entity.value("color", std::string()) == "red") return single(cheepSprite_, 16, 16);
        return byWorld(cheepGraySprite_, 16, 16);
    }
    if (type == "bullet_bill") return byWorld(bulletBillSprite_, 16, 16);
    if (type == "podoboo") return single(podobooSprite_, 16, 16);
    if (type == "bowser") {
        // Como el juego: en el castillo, la paleta verde de superficie
        // (BowserPaletteData del original).
        EntitySprite sprite = byWorld(bowserSprite_, 32, 32);
        if (sprite.row == static_cast<int>(WorldType::Castle)) sprite.row = static_cast<int>(WorldType::Overworld);
        return sprite;
    }
    if (type == "axe") return single(axeSprite_, 16, 16);
    if (type == "spring") return single(springSprite_, 16, 24);
    if (type == "retainer") {
        const std::string name = meta_.value("name", std::string());
        return single(retainerSprite_, 16, 24, (!name.empty() && name[0] == '8') ? 1 : 0);
    }
    if (type == "bridge_chain") return single(bridgeChainSprite_, 16, 16);
    if (type == "vine") return byWorld(vineSprite_, 16, 16);
    return EntitySprite{};
}

void LevelEditor::renderOverlays() {
    const int cam = static_cast<int>(std::lround(cameraX_));
    const int levelRow = worldRow();

    // Mastil de la bandera (en el juego lo dibuja la entidad Flagpole) y
    // bloques invisibles (en el juego no se ven: aca, con un contorno).
    for (int r = 0; r < heightTiles(); ++r) {
        for (int c = 0; c < widthTiles(); ++c) {
            const int x = c * kTile - cam;
            if (x < -kTile || x > kViewWidth) continue;
            if (tileAt(c, r) == 'F') {
                const bool top = tileAt(c, r - 1) != 'F';
                const std::string palette = areaPaletteAt(c);
                const bool snow = palette == "snow" || palette == "night_snow";
                (snow ? flagpoleSnowSprite_ : flagpoleSprite_).draw(renderer_, areaRowAt(c), top ? 0 : 1, x, r * kTile, false);
            } else if (tileAt(c, r) == 'X') {
                drawRect({x + 2, r * kTile + 2, kTile - 4, kTile - 4}, kGray);
            } else if (tileAt(c, r) == 'H') {
                drawRect({x + 1, r * kTile + 1, kTile - 2, kTile - 2}, kCyan);
                drawText(x + 6, r * kTile + 5, "H", kCyan);
            }
        }
    }

    // Plataformas: tramos + rotulo del modo.
    for (const auto& platform : meta_.value("platforms", nlohmann::json::array())) {
        const int x = platform.value("col", 0) * kTile - cam;
        const int y = platform.value("row", 0) * kTile;
        const int width = platform.value("width", 3) * kTile;
        for (int offset = 0; offset < width; offset += 8) liftSprite_.draw(renderer_, 0, 0, x + offset, y, false);
        const std::string mode = platform.value("mode", std::string("vertical"));
        const char* label = mode == "horizontal" ? "<>" : mode == "lift_up" ? "SUBE" : mode == "lift_down" ? "BAJA"
                          : mode == "fall" ? "CAE" : mode == "balance" ? "BAL" : mode == "right" ? ">>" : "V";
        drawText(x + 1, y + 10, label, kYellow);
    }

    // Enemigos (con alas: rotulo arriba).
    for (const auto& entity : meta_.value("entities", nlohmann::json::array())) {
        const int x = entity.value("col", 0) * kTile - cam;
        const int feet = (entity.value("row", 0) + 1) * kTile;
        const std::string type = entity.value("type", std::string());
        if (type == "whirlpool") {
            // Remolino: la zona entera (todo el alto), con su ancho.
            const int width = entity.value("length", 4) * kTile;
            SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
            fillRect({x, kTile, width, 15 * kTile}, SDL_Color{20, 40, 160, 90});
            SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
            drawRect({x, kTile, width, 15 * kTile}, SDL_Color{120, 160, 255, 255});
            drawText(x + 2, 2 * kTile, "REMOLINO", kWhite);
            continue;
        }
        if (type == "firebar" && fireballSprite_.isLoaded()) {
            // Barra de fuego: desde el centro del bloque, sus bolas en linea
            // (asi se ve el largo; en el juego gira).
            const int cy = entity.value("row", 0) * kTile + 4;
            const int length = entity.value("length", 6);
            for (int k = 0; k < length; ++k) fireballSprite_.draw(renderer_, 0, 0, x + 4 + k * 8, cy, false);
            if (entity.value("hard_only", false)) {
                fillRect({x, cy - 8, 7, 7}, SDL_Color{170, 30, 30, 230});
                drawText(x + 1, cy - 7, "D", kWhite);
            }
            continue;
        }
        const EntitySprite sprite = entitySprite(entity, areaRowAt(entity.value("col", 0)));
        const int xs = x + static_cast<int>(entity.value("x_offset", 0.0));
        const int ys = static_cast<int>(entity.value("y_offset", 0.0));
        if (sprite.sheet) {
            // Donde lo pone el juego: los que caminan, parados sobre su fila;
            // los que flotan/nadan, con su fila como borde de arriba; la
            // planta asomando de su tuberia (col/row = tope izquierdo).
            int top = feet - sprite.h;
            int left = xs;
            if (type == "lakitu" || type == "blooper" || type == "cheep" || type == "bullet_bill" ||
                type == "axe" || type == "bridge_chain" || type == "vine") {
                top = entity.value("row", 0) * kTile + ys;
            } else if (type == "piranha") {
                top = entity.value("row", 0) * kTile - sprite.h;
                left = xs + kTile / 2;
            } else if (type == "podoboo") {
                top = entity.value("row", 0) * kTile;
            }
            sprite.sheet->draw(renderer_, sprite.row, sprite.frame, left, top, false);
            if (entity.contains("state")) drawText(x - 2, top - 7, "ALAS", kWhite);
            if (entity.value("hard_only", false)) {
                // Solo en el modo dificil (segunda vuelta).
                fillRect({left, top - 1, 7, 7}, SDL_Color{170, 30, 30, 230});
                drawText(left + 1, top, "D", kWhite);
            }
            if (entity.contains("end_col")) {
                const int endX = entity.value("end_col", 0) * kTile - cam;
                drawRect({x, top - 3, endX - x, 2}, kYellow);
            }
        } else {
            const std::string label = entityShortName(type);
            fillRect({x, feet - 16, kTile, kTile}, SDL_Color{120, 0, 120, 200});
            drawText(x + 1, feet - 11, label, kWhite);
            if (entity.contains("end_col")) {
                const int endX = entity.value("end_col", 0) * kTile - cam;
                drawRect({x, feet - 18, endX - x, 2}, kYellow);
            }
        }
    }

    // Inicio del jugador.
    if (const auto start = meta_.find("start"); start != meta_.end() && start->is_object()) {
        const int sx = start->value("col", 0) * kTile - cam;
        const int sy = (start->value("row", 0) + 1) * kTile - 16;
        const bool left = start->value("facing", std::string("right")) == "left";
        if (marioSprite_.isLoaded()) marioSprite_.draw(renderer_, 0, 0, sx, sy, left);
        drawText(sx - 4, sy - 8, left ? "< INICIO" : "INICIO >", kWhite);
    }

    // Contenido de los bloques.
    for (const auto& block : meta_.value("blocks", nlohmann::json::array())) {
        const int x = block.value("col", 0) * kTile - cam;
        const int y = block.value("row", 0) * kTile;
        std::string text = blockItemShort(block.value("item", std::string("coin")));
        const int hits = block.value("hits", 1);
        if (hits > 1) text += "x" + std::to_string(hits);
        fillRect({x, y - 7, static_cast<int>(text.size()) * 4 + 2, 7}, SDL_Color{0, 0, 0, 170});
        drawText(x + 1, y - 6, text, kYellow);
    }

    // Tuberias con warp: numero en la entrada y en el destino.
    int warpNumber = 1;
    for (const auto& warp : meta_.value("warps", nlohmann::json::array())) {
        const std::string label = "W" + std::to_string(warpNumber++);
        const int sx = warp.value("col", 0) * kTile - cam;
        const int sy = warp.value("row", 0) * kTile;
        // La entrada, del tamano de la boca: en una vertical 2 de ancho (el
        // tope), en una horizontal ("dir": "right") 2 de alto (la boca).
        if (warp.value("dir", std::string()) == "right") {
            drawRect({sx, sy, kTile, kTile * 2}, kGreen);
            drawText(sx - 10, sy + 12, label, kGreen);
        } else {
            drawRect({sx, sy, kTile * 2, kTile}, kGreen);
            drawText(sx + 9, sy - 7, label, kGreen);
        }
        const int dx = warp.value("dest_col", 0) * kTile - cam;
        const int dy = warp.value("dest_row", 0) * kTile;
        drawRect({dx, dy, kTile, kTile}, kGreen);
        drawText(dx + 2, dy + 5, label, kGreen);
    }
    if (mode_ == Mode::PickWarpDest) {
        drawRect({warpSourceCol_ * kTile - cam, warpSourceRow_ * kTile, warpSourceSide_ ? kTile : kTile * 2,
                  warpSourceSide_ ? kTile * 2 : kTile},
                 kGreen);
    }

    // Checkpoints y areas.
    for (const auto& checkpoint : meta_.value("checkpoints", nlohmann::json::array())) {
        const int x = checkpoint.get<int>() * kTile - cam;
        fillRect({x, 0, 2, kViewHeight}, kCyan);
        drawText(x + 4, 2, "CHECKPOINT", kCyan);
    }
    for (const auto& area : meta_.value("areas", nlohmann::json::array())) {
        const int x0 = area.value("start", 0) * kTile - cam;
        const int x1 = (area.value("end", 0) + 1) * kTile - cam;
        drawRect({x0, 0, x1 - x0, kViewHeight}, kRed);
        drawText(x0 + 3, 9, "AREA " + area.value("world", std::string("(NIVEL)")), kRed);
    }

    if (showGrid_) {
        // Lineas de 1 pixel de la ventana (no de la vista logica, que con la
        // ventana agrandada las hacia de 3-4 px): se dibuja sin la escala y
        // se pasa cada coordenada a pixeles reales.
        float sx = 1.0f;
        float sy = 1.0f;
        SDL_RenderGetScale(renderer_, &sx, &sy);
        SDL_Rect vp;
        SDL_RenderGetViewport(renderer_, &vp);
        SDL_Rect clip;
        SDL_RenderGetClipRect(renderer_, &clip);
        const bool clipped = SDL_RenderIsClipEnabled(renderer_);
        SDL_RenderSetLogicalSize(renderer_, 0, 0);
        SDL_RenderSetScale(renderer_, 1.0f, 1.0f);
        const auto realX = [&](int x) { return static_cast<int>((vp.x + x) * sx); };
        const auto realY = [&](int y) { return static_cast<int>((vp.y + y) * sy); };
        SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 40);
        for (int c = 0; c <= widthTiles(); ++c) {
            const int x = c * kTile - cam;
            if (x >= 0 && x <= kViewWidth) SDL_RenderDrawLine(renderer_, realX(x), realY(0), realX(x), realY(kViewHeight));
        }
        for (int r = 0; r <= heightTiles(); ++r) {
            SDL_RenderDrawLine(renderer_, realX(0), realY(r * kTile), realX(kViewWidth), realY(r * kTile));
        }
        // Volver a la vista logica con el mismo viewport (el mapa va corrido
        // kTopBar hacia abajo) y recorte: si no, lo que se dibuja despues
        // (el cuadro bajo el mouse, la seleccion) queda desfasado.
        SDL_RenderSetLogicalSize(renderer_, kWidth, kHeight);
        SDL_RenderSetViewport(renderer_, &vp);
        SDL_RenderSetClipRect(renderer_, clipped ? &clip : nullptr);
    }
    // Borde derecho del nivel.
    fillRect({widthTiles() * kTile - cam, 0, 2, kViewHeight}, kRed);
    // La fila 0 no se ve en el juego (la vista muestra las 15 de abajo,
    // ver kWorldViewOffsetY en Game.h).
    fillRect({0, 0, kViewWidth, kTile}, SDL_Color{0, 0, 0, 140});
    drawText(kViewWidth - 150, 6, "FILA 0: FUERA DE PANTALLA EN EL JUEGO", kGray);

    // Seleccion.
    if (hasSelection()) {
        const SDL_Rect rect{selC0_ * kTile - cam, selR0_ * kTile, (selC1_ - selC0_ + 1) * kTile,
                            (selR1_ - selR0_ + 1) * kTile};
        fillRect(rect, SDL_Color{90, 160, 255, 50});
        drawRect(rect, SDL_Color{90, 160, 255, 255});
    }

    // Cursor: celda bajo el mouse + vista previa de lo que se va a poner.
    if (hoverCol_ >= 0 && !grab_.active) {
        const SDL_Rect cell{hoverCol_ * kTile - cam, hoverRow_ * kTile, kTile, kTile};
        const Tool& tool = tools_[selectedTool_];
        if (mode_ == Mode::PickWarpDest) {
            drawRect(cell, kGreen);
        } else if (tool.kind == ToolKind::Eraser) {
            drawRect(cell, kRed);
            drawText(cell.x + 6, cell.y + 5, "X", kRed);
        } else if (tool.kind == ToolKind::Select) {
            drawRect(cell, SDL_Color{90, 160, 255, 255});
        } else if (clipboard_.valid && (SDL_GetModState() & KMOD_CTRL)) {
            drawRect({cell.x, cell.y, clipboard_.width * kTile, clipboard_.height * kTile}, SDL_Color{90, 160, 255, 255});
        } else {
            SDL_Rect preview = cell;
            if (tool.kind == ToolKind::Tile && tool.tile == 'T') preview.w = kTile * 2;
            if (tool.kind == ToolKind::Goal) {
                preview.y -= kTile * kFlagpoleHeight;
                preview.h += kTile * kFlagpoleHeight;
            }
            if (tool.kind == ToolKind::Decoration) {
                const int index = decorationIndex(tool.type);
                preview.w = kDecorations[index].width;
                preview.h = kDecorations[index].height;
            } else if (tool.kind == ToolKind::Platform) {
                preview.w = kTile * 3;
                preview.h = 8;
            }
            drawRect(preview, kYellow);
        }
    }
}

void LevelEditor::drawToolIcon(const Tool& tool, const SDL_Rect& cell, int levelRow) {
    const SDL_Rect icon{cell.x + (cell.w - 16) / 2, cell.y + 3, 16, 16};
    // Misma variante de colores que el area que se ve (PALETA), como el mapa.
    const std::string palette = areaPaletteAt(viewCenterCol());
    const bool gray = palette == "gray";
    const bool snow = palette == "snow" || palette == "night_snow";
    const bool mushroom = palette == "mushroom";
    const int tileRow = gray ? 0 : levelRow;
    const SpriteSheet& terrain = gray ? terrainGraySprite_ : terrainSprite_;
    const SpriteSheet& scenery = gray ? sceneryTilesGraySprite_ : snow ? sceneryTilesSnowSprite_ : sceneryTilesSprite_;
    const SpriteSheet& pipe = snow ? pipeSnowSprite_ : mushroom ? pipeMushroomSprite_ : pipeSprite_;
    const SpriteSheet& pipeSide = gray || levelRow == static_cast<int>(WorldType::Castle) ? pipeSideVariants_[1]
                                  : snow                                                  ? pipeSideVariants_[0]
                                  : levelRow == static_cast<int>(WorldType::Underground)  ? pipeSideVariants_[2]
                                                                                          : pipeSideSlices_;
    switch (tool.kind) {
        case ToolKind::Tile:
            switch (tool.tile) {
                case '#': terrain.drawScaled(renderer_, tileRow, 0, icon); break;
                case 'S': terrain.drawScaled(renderer_, tileRow, 1, icon); break;
                case 'B': brickSprite_.drawScaled(renderer_, levelRow, 0, icon); break;
                case 'C':
                    brickSprite_.drawScaled(renderer_, levelRow, 0, {icon.x, icon.y + 6, 16, 10});
                    coinSprite_.drawScaled(renderer_, levelRow, 0, {icon.x + 4, icon.y, 8, 8});
                    break;
                case '?': questionSprite_.drawScaled(renderer_, levelRow, 0, icon); break;
                case 'D':
                    brickSprite_.drawScaled(renderer_, levelRow, 0, icon);
                    drawText(icon.x + 6, icon.y + 5, "D", kYellow);
                    break;
                case 'H':
                    drawRect(icon, kCyan);
                    drawText(icon.x + 6, icon.y + 5, "H", kCyan);
                    break;
                case 'o': coinSprite_.drawScaled(renderer_, levelRow, 0, icon); break;
                case 'T': pipe.drawScaled(renderer_, levelRow, 0, {icon.x - 4, icon.y + 4, 24, 12}); break;
                case 'Y':
                    pipeSide.drawScaled(renderer_, 2, 0, {icon.x - 2, icon.y + 2, 8, 8});
                    pipeSide.drawScaled(renderer_, 3, 0, {icon.x - 2, icon.y + 10, 8, 8});
                    pipeSide.drawScaled(renderer_, 2, 1, {icon.x + 6, icon.y + 2, 8, 8});
                    pipeSide.drawScaled(renderer_, 3, 1, {icon.x + 6, icon.y + 10, 8, 8});
                    pipeSide.drawScaled(renderer_, 2, 1, {icon.x + 14, icon.y + 2, 8, 8});
                    pipeSide.drawScaled(renderer_, 3, 1, {icon.x + 14, icon.y + 10, 8, 8});
                    break;
                case 'L': lavaSprite_.drawScaled(renderer_, 0, 0, icon); break;
                case 'b': bridgeSprite_.drawScaled(renderer_, 0, 0, icon); break;
                case '=': fillRect({icon.x, icon.y + 4, 16, 6}, SDL_Color{214, 168, 116, 255}); break;
                case 'X':
                    drawRect(icon, kGray);
                    drawText(icon.x + 6, icon.y + 5, "X", kGray);
                    break;
                case 't': scenery.drawScaled(renderer_, levelRow, 1, icon); break;
                case 'r': scenery.drawScaled(renderer_, levelRow, 3, icon); break;
                case 'm': scenery.drawScaled(renderer_, levelRow, 5, icon); break;
                case 'n': scenery.drawScaled(renderer_, levelRow, 8, icon); break;
                case 'R': scenery.drawScaled(renderer_, levelRow, 10, icon); break;
                case 'K': sceneryTilesSprite_.drawScaled(renderer_, levelRow, 11, icon); break;
                case 'c': scenery.drawScaled(renderer_, levelRow, 14, icon); break;
                case 'W': scenery.drawScaled(renderer_, levelRow, 16, icon); break;
                case 'w': scenery.drawScaled(renderer_, levelRow, 18, icon); break;
                // Bloque de nube: no usa la paleta de la escenografia (como en el mapa).
                case 'N': sceneryTilesSprite_.drawScaled(renderer_, levelRow, 19, icon); break;
                default: break;
            }
            break;
        case ToolKind::Goal:
            (snow ? flagpoleSnowSprite_ : flagpoleSprite_).drawScaled(renderer_, levelRow, 0, {icon.x, icon.y - 2, 16, 6});
            (snow ? flagpoleSnowSprite_ : flagpoleSprite_).drawScaled(renderer_, levelRow, 1, {icon.x, icon.y + 4, 16, 9});
            terrain.drawScaled(renderer_, tileRow, 1, {icon.x + 4, icon.y + 13, 8, 6});
            break;
        case ToolKind::Item: {
            const SpriteSheet* sprite = &coinSprite_;
            if (tool.type == "powerup" || tool.type == "mushroom") sprite = &mushroomSprite_;
            if (tool.type == "fire_flower") sprite = &fireFlowerSprite_;
            if (tool.type == "star") sprite = &starSprite_;
            if (tool.type == "one_up") sprite = &oneUpSprite_;
            sprite->drawScaled(renderer_, levelRow, 0, icon);
            if (tool.type == "powerup") drawText(icon.x + 12, icon.y + 12, "F", kYellow);
            if (tool.hits > 1) drawText(icon.x + 8, icon.y + 13, "x" + std::to_string(tool.hits), kYellow);
            break;
        }
        case ToolKind::Entity:
            {
                nlohmann::json probe{{"type", tool.type}};
                if (!tool.variant.empty()) probe["color"] = tool.variant;
                const EntitySprite sprite = entitySprite(probe, levelRow);
                if (sprite.sheet) {
                    // Entra en el icono (16x16) sin deformarse.
                    const float scale = std::min(16.0f / static_cast<float>(sprite.w), 18.0f / static_cast<float>(sprite.h));
                    const int w = static_cast<int>(static_cast<float>(sprite.w) * scale);
                    const int h = static_cast<int>(static_cast<float>(sprite.h) * scale);
                    sprite.sheet->drawScaled(renderer_, sprite.row, sprite.frame,
                                             {icon.x + (16 - w) / 2, icon.y + 16 - h, w, h});
                } else if (tool.type == "firebar" && fireballSprite_.isLoaded()) {
                    for (int k = 0; k < 3; ++k) {
                        fireballSprite_.draw(renderer_, 0, 0, icon.x - 4 + k * 8, icon.y + 4, false);
                    }
                } else {
                    fillRect(icon, SDL_Color{120, 0, 120, 255});
                    drawText(icon.x + 1, icon.y + 5, entityShortName(tool.type), kWhite);
                }
            }
            break;
        case ToolKind::Decoration: {
            const int index = decorationIndex(tool.type);
            const DecorationInfo& info = kDecorations[index];
            const float scale = std::min(26.0f / static_cast<float>(info.width), 18.0f / static_cast<float>(info.height));
            const int w = static_cast<int>(static_cast<float>(info.width) * scale);
            const int h = static_cast<int>(static_cast<float>(info.height) * scale);
            const bool cloud = tool.type.rfind("cloud", 0) == 0;
            const bool castle = tool.type.rfind("castle", 0) == 0;
            const SpriteSheet& sheet = gray && !cloud              ? decorationGraySprites_[index]
                                       : snow && !cloud && !castle ? decorationSnowSprites_[index]
                                                                   : decorationSprites_[index];
            sheet.drawScaled(renderer_, 0, 0, {cell.x + (cell.w - w) / 2, cell.y + 21 - h, w, h});
            break;
        }
        case ToolKind::Platform: {
            for (int i = 0; i < 3; ++i) liftSprite_.drawScaled(renderer_, 0, 0, {icon.x - 4 + i * 8, icon.y + 4, 8, 8});
            const char* label = tool.type == "horizontal" ? "<>" : tool.type == "lift_up" ? "S"
                              : tool.type == "lift_down" ? "B" : tool.type == "fall" ? "!" : tool.type == "right" ? ">>" : "V";
            drawText(icon.x + 5, icon.y + 14, label, kYellow);
            break;
        }
        case ToolKind::Start:
            if (marioSprite_.isLoaded()) marioSprite_.drawScaled(renderer_, 0, 0, icon);
            break;
        case ToolKind::Checkpoint:
            fillRect({icon.x + 7, icon.y, 2, 16}, kCyan);
            drawText(icon.x + 1, icon.y + 5, "CP", kCyan);
            break;
        case ToolKind::Eraser:
        case ToolKind::Select:
        case ToolKind::Move:
            break;
    }
}

void LevelEditor::renderPalette() {
    fillRect({kPaletteX, 0, kWidth - kPaletteX, kHeight}, kPanel);
    const int levelRow = areaRowAt(static_cast<int>((cameraX_ + kViewWidth / 2.0f) / kTile));
    drawText(kPaletteX + 4, 4, "HERRAMIENTA:", kGray);
    drawText(kPaletteX + 4, 12, tools_[selectedTool_].label, kWhite);
    // Que hay bajo el mouse (ayuda para S/A/I/P).
    std::string listName;
    size_t index = 0;
    std::string hover;
    if (hoverCol_ >= 0 && findObjectAt(hoverCol_, hoverRow_, listName, index)) {
        hover = meta_[listName][index].value("type", meta_[listName][index].value("mode", std::string()));
    } else if (hoverCol_ >= 0 && isBlockTile(tileAt(hoverCol_, hoverRow_))) {
        hover = "BLOQUE (I: CONTENIDO)";
    } else if (hoverCol_ >= 0 && tilemap_.isPipeTopTile(hoverCol_, hoverRow_)) {
        hover = "TUBERIA (P: WARP)";
    }
    if (!hover.empty()) drawText(kPaletteX + 4, 22, "BAJO EL MOUSE: " + hover, kGray);

    // Pestanas.
    for (int t = 0; t < static_cast<int>(Tab::Count); ++t) {
        const SDL_Rect rect{kPaletteX + t * kPaletteCellW, kTabsY, kPaletteCellW, 12};
        fillRect(rect, t == static_cast<int>(currentTab_) ? kPanelLight : kPanel);
        drawRect(rect, kGray);
        const std::string name = std::string(kTabNames[t]).substr(0, 4);
        drawText(rect.x + 4, rect.y + 4, name, t == static_cast<int>(currentTab_) ? kYellow : kGray);
    }
    // Herramientas de la pestana (de a paginas si no entran todas: la
    // ultima celda pasa a la pagina siguiente).
    const std::vector<int> pageTools = paletteToolsOnPage();
    for (size_t i = 0; i < pageTools.size(); ++i) {
        const SDL_Rect cell{kPaletteX + static_cast<int>(i % kPaletteCols) * kPaletteCellW,
                            kPaletteTop + static_cast<int>(i / kPaletteCols) * kPaletteCellH, kPaletteCellW,
                            kPaletteCellH};
        if (pageTools[i] < 0) {
            drawRect({cell.x + 2, cell.y + 2, cell.w - 4, cell.h - 4}, kGray);
            drawText(cell.x + 6, cell.y + 6, "MAS", kYellow);
            drawText(cell.x + 6, cell.y + 14, std::to_string(tabPage_[static_cast<int>(currentTab_)] + 1) + "/" +
                                                  std::to_string(palettePageCount()), kGray);
            continue;
        }
        if (pageTools[i] == selectedTool_) fillRect(cell, kPanelLight);
        drawToolIcon(tools_[pageTools[i]], cell, areaRowAt(viewCenterCol()));
        if (pageTools[i] == selectedTool_) drawRect(cell, kYellow);
    }
    // Botones (goma, seleccion, deshacer, borrar todo, probar, subarea).
    for (int i = 0; i < static_cast<int>(Button::Count); ++i) {
        const Button button = static_cast<Button>(i);
        const SDL_Rect rect = buttonRect(button);
        bool active = false;
        if (button == Button::Eraser) active = tools_[selectedTool_].kind == ToolKind::Eraser;
        if (button == Button::Select) active = tools_[selectedTool_].kind == ToolKind::Select;
        if (button == Button::Move) active = tools_[selectedTool_].kind == ToolKind::Move;
        SDL_Color color = active ? kPanelLight : SDL_Color{40, 44, 64, 255};
        if (button == Button::Reset && confirmReset_) color = SDL_Color{130, 40, 40, 255};
        fillRect(rect, color);
        drawRect(rect, active ? kYellow : kGray);
        std::string label = button == Button::World
            ? std::string("MUNDO: ") + worldLabel(meta_.value("world", std::string()))
            : std::string(kButtonNames[i]);
        if (button == Button::Palette) label = std::string("PALETA: ") + paletteLabel(areaPaletteAt(viewCenterCol()));

        if (button == Button::Move) {
            drawArrowIcon(rect.x + 4, rect.y + 2, active ? kYellow : kWhite);
            drawText(rect.x + 16, rect.y + 5, label, active ? kYellow : kWhite);
        } else {
            drawText(rect.x + 3, rect.y + 5, label, active ? kYellow : kWhite);
        }
    }
}

void LevelEditor::drawArrowIcon(int x, int y, SDL_Color color) {
    // Flecha de puntero (10 px de alto), con borde negro.
    static const char* const kArrow[] = {
        "X       ", "XX      ", "X.X     ", "X..X    ", "X...X   ", "X....X  ",
        "X..XXX  ", "X.X     ", "XX      ", "X       ",
    };
    for (int r = 0; r < 10; ++r) {
        for (int c = 0; kArrow[r][c]; ++c) {
            const char p = kArrow[r][c];
            if (p == ' ') continue;
            fillRect({x + c, y + r, 1, 1}, p == 'X' ? SDL_Color{0, 0, 0, 255} : color);
        }
    }
}

void LevelEditor::requestLeave(LeaveAction action) {
    if (!dirty_) {
        doLeave(action);
        return;
    }
    leaveAction_ = action;
    mode_ = Mode::ConfirmLeave;
}

void LevelEditor::doLeave(LeaveAction action) {
    switch (action) {
        case LeaveAction::Quit:
            running_ = false;
            break;
        case LeaveAction::Open:
            listLevelFiles();
            break;
        case LeaveAction::New:
            newLevel();
            undoStack_.clear();
            break;
    }
}

SDL_Rect LevelEditor::leaveButtonRect(int index) const {
    const int w = 120;
    const int gap = 8;
    const int total = 3 * w + 2 * gap;
    return SDL_Rect{(kWidth - total) / 2 + index * (w + gap), kHeight / 2 + 8, w, 16};
}

void LevelEditor::renderConfirmLeave() {
    fillRect({0, 0, kWidth, kHeight}, SDL_Color{0, 0, 0, 150});
    const SDL_Rect box{(kWidth - 420) / 2, kHeight / 2 - 40, 420, 76};
    fillRect(box, SDL_Color{30, 30, 46, 255});
    drawRect(box, kYellow);
    const std::string title = leaveAction_ == LeaveAction::Quit ? "SALIR SIN GUARDAR?" : "HAY CAMBIOS SIN GUARDAR";
    drawText(box.x + (box.w - static_cast<int>(title.size()) * 12) / 2, box.y + 10, title, kWhite, 2);
    const char* const labels[] = {leaveAction_ == LeaveAction::Quit ? kLeaveButtons[0] : "GUARDAR",
                                  leaveAction_ == LeaveAction::Quit ? kLeaveButtons[1] : "DESCARTAR",
                                  kLeaveButtons[2]};
    for (int i = 0; i < 3; ++i) {
        const SDL_Rect r = leaveButtonRect(i);
        fillRect(r, i == 1 ? SDL_Color{110, 40, 40, 255} : SDL_Color{40, 44, 64, 255});
        drawRect(r, kGray);
        const std::string text = labels[i];
        drawText(r.x + (r.w - static_cast<int>(text.size()) * 6) / 2, r.y + 5, text, kWhite);
    }
    drawText(box.x + 8, box.y + 64, "ENTER: GUARDAR   S: SIN GUARDAR   ESC: CANCELAR", kGray);
}

namespace {
// Bandas del mapa de estructura (y de cada una, 16 de alto).
constexpr int kSkyBandY = 16 + 256 + 4;
constexpr int kMainBandY = kSkyBandY + 22;
constexpr int kSubBandY = kMainBandY + 22;

SDL_Color tileMapColor(char tile) {
    if (tile == '#' || tile == 'S' || tile == 'N') return SDL_Color{160, 90, 40, 255};
    if (tile == 'B' || tile == 'C' || tile == 'D' || tile == 'K') return SDL_Color{200, 110, 60, 255};
    if (tile == '?') return SDL_Color{250, 200, 60, 255};
    if (tile == 'T' || tile == 'Y' || tile == 't' || tile == 'm' || tile == 'c') return SDL_Color{60, 200, 60, 255};
    if (tile == 'L') return SDL_Color{230, 60, 30, 255};
    if (tile == 'W' || tile == 'w') return SDL_Color{90, 160, 255, 255};
    if (tile == 'F') return SDL_Color{230, 230, 230, 255};
    if (tile == 'b' || tile == 'R') return SDL_Color{200, 200, 200, 255};
    return SDL_Color{0, 0, 0, 0};
}

const char* areaLabel(const nlohmann::json& area) {
    if (area.value("cloud", false)) return "CIELO";
    const std::string world = area.value("world", std::string());
    if (world == "underground") return "SUBTERRANEO";
    if (world == "underwater") return "AGUA";
    if (world == "castle") return "CASTILLO";
    return "SUPERFICIE";
}
}  // namespace

std::vector<LevelEditor::MapBox> LevelEditor::structureBoxes() const {
    std::vector<MapBox> boxes;
    const auto areas = meta_.value("areas", nlohmann::json::array());
    const int left = 4;
    const int right = kViewWidth - 4;
    if (areas.empty()) {
        MapBox box;
        box.area = -1;
        box.start = 0;
        box.end = std::max(0, widthTiles() - 1);
        box.rect = {left, kMainBandY, right - left, 16};
        boxes.push_back(box);
        return boxes;
    }
    // Principal: el area mas larga.
    int main = 0;
    for (size_t i = 0; i < areas.size(); ++i) {
        const int w = areas[i].value("end", 0) - areas[i].value("start", 0);
        if (w > areas[main].value("end", 0) - areas[main].value("start", 0)) main = static_cast<int>(i);
    }
    const int mainStart = areas[main].value("start", 0);
    const int mainEnd = areas[main].value("end", 0);
    const float scale = static_cast<float>(right - left) / static_cast<float>(mainEnd - mainStart + 1);
    auto inArea = [&](int col, size_t i) { return col >= areas[i].value("start", 0) && col <= areas[i].value("end", 0); };
    auto inMain = [&](int col) { return col >= mainStart && col <= mainEnd; };
    MapBox mainBox;
    mainBox.area = main;
    mainBox.start = mainStart;
    mainBox.end = mainEnd;
    mainBox.rect = {left, kMainBandY, right - left, 16};
    boxes.push_back(mainBox);
    std::vector<MapBox> others;
    for (size_t i = 0; i < areas.size(); ++i) {
        if (static_cast<int>(i) == main) continue;
        MapBox box;
        box.area = static_cast<int>(i);
        box.start = areas[i].value("start", 0);
        box.end = areas[i].value("end", 0);
        box.band = areas[i].value("cloud", false) ? 0 : 2;
        // Por donde se conecta con la principal: un caño o una enredadera
        // que lleva ahi, uno que vuelve de ahi, o adonde cae desde el cielo.
        int anchor = -1;
        for (const auto& warp : meta_.value("warps", nlohmann::json::array())) {
            if (warp.contains("level")) continue;   // zona warp: lleva a otro nivel
            const int c = warp.value("col", -1);
            const int d = warp.value("dest_col", -1);
            if (inMain(c) && inArea(d, i)) anchor = c;
            else if (anchor < 0 && inArea(c, i) && inMain(d)) anchor = d;
        }
        for (const auto& block : meta_.value("blocks", nlohmann::json::array())) {
            if (block.contains("dest_col") && inMain(block.value("col", -1)) && inArea(block.value("dest_col", -1), i)) {
                anchor = block.value("col", -1);
                box.band = 0;   // se sube por una enredadera: va arriba
            }
        }
        if (anchor < 0 && inMain(areas[i].value("fall_col", -1))) anchor = areas[i].value("fall_col", -1);
        const int width = std::max(6, static_cast<int>(static_cast<float>(box.end - box.start + 1) * scale));
        box.anchorX = anchor >= 0 ? left + static_cast<int>(static_cast<float>(anchor - mainStart) * scale) : -1;
        int x = box.anchorX >= 0 ? box.anchorX - width / 4 : right - width;
        box.rect = {std::clamp(x, left, std::max(left, right - width)), box.band == 0 ? kSkyBandY : kSubBandY,
                    std::min(width, right - left), 16};
        others.push_back(box);
    }
    // Sin encimarse dentro de cada banda.
    std::sort(others.begin(), others.end(), [](const MapBox& a, const MapBox& b) {
        return a.band != b.band ? a.band < b.band : a.rect.x < b.rect.x;
    });
    for (size_t i = 1; i < others.size(); ++i) {
        const MapBox& prev = others[i - 1];
        MapBox& box = others[i];
        if (box.band == prev.band && box.rect.x < prev.rect.x + prev.rect.w + 3) {
            box.rect.x = prev.rect.x + prev.rect.w + 3;
            if (box.rect.x + box.rect.w > right) box.rect.w = std::max(4, right - box.rect.x);
        }
    }
    boxes.insert(boxes.end(), others.begin(), others.end());
    return boxes;
}

void LevelEditor::renderStructureMap() {
    const auto areas = meta_.value("areas", nlohmann::json::array());
    const std::vector<MapBox> boxes = structureBoxes();
    const float viewCenter = (cameraX_ + kViewWidth / 2.0f) / kTile;
    for (const MapBox& box : boxes) {
        // La linea que la une con la principal.
        if (box.band != 1 && box.anchorX >= 0) {
            const SDL_Color line = box.band == 0 ? SDL_Color{150, 210, 255, 255} : SDL_Color{120, 220, 120, 255};
            const int lx = std::clamp(box.anchorX, box.rect.x + 1, box.rect.x + box.rect.w - 2);
            const int y0 = box.band == 0 ? box.rect.y + box.rect.h : kMainBandY + 16;
            const int y1 = box.band == 0 ? kMainBandY : box.rect.y;
            fillRect({box.anchorX, std::min(y0, y1), 1, std::abs(y1 - y0)}, line);
            if (lx != box.anchorX) fillRect({std::min(lx, box.anchorX), box.band == 0 ? y0 : y1 - 1,
                                             std::abs(lx - box.anchorX) + 1, 1}, line);
        }
        const nlohmann::json area = box.area >= 0 ? areas[static_cast<size_t>(box.area)] : nlohmann::json::object();
        const std::string world = area.value("world", meta_.value("world", std::string("overworld")));
        const std::string palette = paletteOf(meta_, &area);
        SDL_Color bg = box.band == 0 ? SDL_Color{110, 170, 250, 255}
                       : (world == "underground" || world == "castle" || palette == "night" ||
                          palette == "night_snow" || palette == "gray") ? SDL_Color{12, 12, 20, 255}
                       : world == "underwater" ? SDL_Color{40, 40, 170, 255}
                                               : SDL_Color{70, 110, 200, 255};
        fillRect(box.rect, bg);
        const int width = box.end - box.start + 1;
        for (int c = 0; c < width; ++c) {
            const int x0 = box.rect.x + c * box.rect.w / width;
            const int x1 = box.rect.x + (c + 1) * box.rect.w / width;
            for (int r = 0; r < heightTiles() && r < 16; ++r) {
                const SDL_Color color = tileMapColor(tileAt(box.start + c, r));
                if (color.a == 0) continue;
                fillRect({x0, box.rect.y + r, std::max(1, x1 - x0), 1}, color);
            }
        }
        drawRect({box.rect.x - 1, box.rect.y - 1, box.rect.w + 2, box.rect.h + 2}, SDL_Color{70, 70, 90, 255});
        if (box.band != 1 && box.area >= 0) {
            // El nombre adentro si entra; si no, al costado (del lado con lugar).
            const std::string label = areaLabel(area);
            const int labelWidth = static_cast<int>(label.size()) * 4;
            int lx = box.rect.x + 1;
            if (labelWidth + 2 > box.rect.w) {
                lx = box.rect.x - labelWidth - 3 >= 4 ? box.rect.x - labelWidth - 3 : box.rect.x + box.rect.w + 3;
            }
            drawText(lx, box.rect.y + 5, label, kWhite);
        }
        // Lo que se ve ahora.
        if (viewCenter >= box.start && viewCenter <= box.end + 1) {
            const int vx = box.rect.x + static_cast<int>((cameraX_ / kTile - box.start) * box.rect.w / width);
            const int vw = std::max(2, (kViewWidth / kTile) * box.rect.w / width);
            const int x0 = std::max(box.rect.x, vx);
            drawRect({x0, box.rect.y - 1, std::max(2, std::min(vx + vw, box.rect.x + box.rect.w) - x0), box.rect.h + 2},
                     kYellow);
        }
    }
}

bool LevelEditor::structureMapClick(int x, int y) {
    for (const MapBox& box : structureBoxes()) {
        if (x < box.rect.x || x >= box.rect.x + box.rect.w || y < box.rect.y - 2 || y >= box.rect.y + box.rect.h + 2) {
            continue;
        }
        const int width = box.end - box.start + 1;
        const float col = static_cast<float>(box.start) + static_cast<float>((x - box.rect.x) * width) / box.rect.w;
        cameraX_ = col * kTile - kViewWidth / 2.0f;
        return true;
    }
    return false;
}

void LevelEditor::renderHelp() {
    // Ventana de comandos, con la fuente del juego.
    fillRect({0, 0, kWidth, kHeight}, SDL_Color{0, 0, 0, 170});
    const SDL_Rect box{12, 8, kWidth - 24, kHeight - 16};
    fillRect(box, SDL_Color{24, 24, 38, 250});
    drawRect(box, kYellow);
    drawText((kWidth - 8 * 8) / 2, 16, "COMANDOS", kYellow, 2);
    struct Line {
        bool title;
        const char* text;
    };
    static const Line kLines[] = {
        {true, "PONER Y BORRAR"},
        {false, "CLIC IZQ: PONER     CLIC DER: BORRAR"},
        {false, "RUEDA, FLECHAS O BOTON DEL MEDIO: DESPLAZAR"},
        {true, "OBJETOS Y ENEMIGOS"},
        {false, "MOVER: AGARRAR Y ARRASTRAR LO PUESTO"},
        {false, "ITEMS: SOLTALOS SOBRE UN BLOQUE"},
        {false, "SOBRE UN BLOQUE: I CONTENIDO   + / - GOLPES"},
        {false, "SOBRE UN ENEMIGO: S TRANSFORMA   A ALAS"},
        {true, "TUBERIAS"},
        {false, "P SOBRE EL TOPE (O LA BOCA DE UNA HORIZONTAL): ADONDE LLEVA"},
        {false, "MOVER SOBRE LA PUNTA: ARRASTRAR PARA ESTIRAR"},
        {false, "EN L: HORIZONTAL PEGADA A UNA VERTICAL (EL CODO VA SOLO)"},
        {true, "EDICION"},
        {false, "SELECCION: CTRL+C  CTRL+X  CTRL+V  SUPR"},
        {false, "CTRL+Z DESHACER   TAB PESTANA   G GRILLA"},
        {false, "W TIPO DE MUNDO   CTRL+FLECHAS: ANCHO"},
        {true, "ARCHIVO"},
        {false, "CTRL+N NUEVO   CTRL+O ABRIR   CTRL+S GUARDAR"},
        {false, "F2 NOMBRE   F3 TIEMPO   F5 PROBAR (SHIFT: DESDE ACA)"},
        {false, "F1 ESTA VENTANA"},
    };
    int y = 34;
    for (const Line& line : kLines) {
        if (line.title) y += 2;
        drawText(line.title ? 24 : 40, y, line.text, line.title ? kGreen : kWhite, 2);
        y += 14;
    }
    drawText((kWidth - 34 * 8) / 2, kHeight - 19, "CUALQUIER TECLA O CLIC PARA CERRAR", kGray, 2);
}

void LevelEditor::renderMinimap() {
    const SDL_Rect mini = minimapRect();
    fillRect({mini.x - 1, mini.y - 1, mini.w + 2, mini.h + 2}, SDL_Color{10, 10, 14, 255});
    const int width = widthTiles();
    if (width <= 0) return;
    for (int c = 0; c < width; ++c) {
        const int x0 = mini.x + c * mini.w / width;
        const int x1 = mini.x + (c + 1) * mini.w / width;
        for (int r = 0; r < heightTiles() && r < mini.h; ++r) {
            const char tile = tileAt(c, r);
            SDL_Color color{40, 60, 110, 255};
            if (tile == '#' || tile == 'S') color = SDL_Color{160, 90, 40, 255};
            else if (tile == 'B' || tile == 'C' || tile == 'D') color = SDL_Color{200, 110, 60, 255};
            else if (tile == '?') color = SDL_Color{250, 200, 60, 255};
            else if (tile == 'T') color = SDL_Color{60, 200, 60, 255};
            else if (tile == 'L') color = SDL_Color{230, 60, 30, 255};
            else if (tile == 'F') color = SDL_Color{230, 230, 230, 255};
            else if (tile == 'b') color = SDL_Color{200, 200, 200, 255};
            if (tile == '.' || tile == 'H') continue;
            fillRect({x0, mini.y + r, std::max(1, x1 - x0), 1}, color);
        }
    }
    // Lo que se ve ahora en el editor.
    const int vx0 = mini.x + static_cast<int>(cameraX_ / kTile) * mini.w / width;
    const int vw = std::max(2, (kViewWidth / kTile) * mini.w / width);
    drawRect({vx0, mini.y - 1, std::min(vw, mini.x + mini.w - vx0), mini.h + 2}, kYellow);
}

void LevelEditor::renderStatus() {
    // Barra de arriba: nombre, mundo, ancho, tiempo y archivo.
    fillRect({0, 0, kViewWidth, kTopBar}, kPanel);
    const std::string title = meta_.value("name", std::string("NUEVO")) + (dirty_ ? " *" : "");
    drawText(4, 3, title, kWhite, 2);
    const std::string world = meta_.value("world", std::string("(SEGUN ESCENARIO)"));
    drawText(120, 2, "MUNDO: " + world + "   ANCHO: " + std::to_string(widthTiles()) +
                         "   TIEMPO: " + std::to_string(meta_.value("time", 400)), kGray);
    drawText(120, 9, path_.empty() ? "(SIN GUARDAR)" : path_, kGray);

    // Abajo: el mapa de la estructura del nivel (los comandos estan en el
    // boton COMANDOS).
    fillRect({0, kTopBar + kViewHeight, kViewWidth, kHeight - kTopBar - kViewHeight}, kPanel);
    renderStructureMap();

    const int yMsg = kHeight - 10;
    if (mode_ == Mode::TextInput) {
        fillRect({0, yMsg - 2, kViewWidth, kHeight - yMsg + 2}, SDL_Color{60, 40, 90, 255});
        drawText(4, yMsg, textPrompt_ + "  (ENTER OK / ESC CANCELA)", kWhite);
        drawText(260, yMsg - 1, textValue_ + "_", kYellow, 2);
    } else if (mode_ == Mode::PickWarpDest) {
        drawText(4, yMsg, "WARP: CLIC EN EL DESTINO (ESC CANCELA)", kGreen);
    } else if (SDL_GetTicks() < messageUntil_) {
        drawText(4, yMsg, message_, kGreen);
    } else if (hoverCol_ >= 0) {
        drawText(4, yMsg, "COL " + std::to_string(hoverCol_) + "  FILA " + std::to_string(hoverRow_), kYellow);
    }
}

SDL_Rect LevelEditor::pickerArrowRect(int direction) const {
    return direction < 0 ? SDL_Rect{40, 150, 40, 48} : SDL_Rect{kWidth - 80, 150, 40, 48};
}

void LevelEditor::loadPreview() {
    if (levelFiles_.empty() || previewIndex_ == fileSelection_) return;
    previewIndex_ = fileSelection_;
    previewMeta_ = nlohmann::json::object();
    try {
        std::ifstream file(levelFiles_[fileSelection_]);
        nlohmann::json json;
        file >> json;
        previewMeta_ = json;
        previewMap_.loadFromJson(json);
    } catch (const std::exception&) {
        previewMeta_ = nlohmann::json::object();
    }
    renderPreviewTexture();
}

void LevelEditor::renderPreviewTexture() {
    // El principio del nivel (32 x 16 bloques) con los graficos del juego.
    if (!previewTexture_) {
        previewTexture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 512, 256);
        if (previewTexture_) SDL_SetTextureScaleMode(previewTexture_, SDL_ScaleModeNearest);
    }
    if (!previewTexture_ || previewMeta_.empty()) return;
    SDL_SetRenderTarget(renderer_, previewTexture_);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);
    // Cada area con su mundo y su fondo, como en el editor y en el juego.
    auto areas = previewMeta_.value("areas", nlohmann::json::array());
    if (areas.empty()) areas.push_back({{"start", 0}, {"end", previewMap_.widthTiles() - 1}});
    int row = 0;
    for (const auto& area : areas) {
        const int left = area.value("start", 0) * kTile;
        const int right = (area.value("end", 0) + 1) * kTile;
        if (left >= 512 || right <= 0) continue;
        const SDL_Rect clip{left, 0, std::min(512, right) - left, 256};
        SDL_RenderSetClipRect(renderer_, &clip);
        const std::string palette = paletteOf(previewMeta_, &area);
        const WorldType world = palette == "gray" ? WorldType::Castle
                                : worldTypeFromName(area.value("world", previewMeta_.value("world", std::string())));
        const int areaRow = static_cast<int>(world);
        if (left == 0) row = areaRow;   // para los enemigos de la primera
        const bool dark = palette == "night" || palette == "night_snow" || palette == "gray";
        const SDL_Color bg = dark ? SDL_Color{0, 0, 0, 255} : backgroundColorFor(world);
        SDL_SetRenderDrawColor(renderer_, bg.r, bg.g, bg.b, 255);
        SDL_RenderFillRect(renderer_, &clip);
        for (const auto& decoration : previewMeta_.value("decorations", nlohmann::json::array())) {
            const int index = decorationIndex(decoration.value("type", std::string()));
            if (index < 0 || decoration.value("col", 0) > 32) continue;
            decorationSprites_[index].draw(renderer_, 0, 0, decoration.value("col", 0) * kTile,
                                           decoration.value("row", 0) * kTile, false);
        }
        previewMap_.renderTerrain(renderer_, bg, terrainSprite_, areaRow, 512, 0.0f, false);
        previewMap_.renderScenery(renderer_, sceneryTilesSprite_, areaRow, 512, 0.0f);
        previewMap_.renderHazards(renderer_, lavaSprite_, bridgeSprite_, 512, 0.0f);
        previewMap_.renderPipes(renderer_, pipeSprite_, areaRow, 0.0f, &pipeSideSlices_);
        previewMap_.renderBlocks(renderer_, brickSprite_, areaRow, 0.0f, {}, &questionSprite_, 0, &coinSprite_, 0);
    }
    SDL_RenderSetClipRect(renderer_, nullptr);
    for (const auto& entity : previewMeta_.value("entities", nlohmann::json::array())) {
        const int col = entity.value("col", 0);
        if (col > 32 || entity.value("hard_only", false)) continue;
        const EntitySprite sprite = entitySprite(entity, row);
        if (!sprite.sheet) continue;
        sprite.sheet->draw(renderer_, sprite.row, sprite.frame, col * kTile,
                           (entity.value("row", 0) + 1) * kTile - sprite.h, false);
    }
    SDL_SetRenderTarget(renderer_, nullptr);
}

void LevelEditor::renderFileList() {
    // Selector de niveles: vista previa del elegido y flechas para recorrer.
    fillRect({0, 0, kWidth, kHeight}, SDL_Color{12, 12, 20, 255});
    if (levelFiles_.empty()) {
        drawText(40, 40, "NO HAY NIVELES EN " + levelsDir_, kRed, 2);
        return;
    }
    const int count = static_cast<int>(levelFiles_.size());
    const std::string name = previewMeta_.value("name", fs::path(levelFiles_[fileSelection_]).stem().string());
    drawText((kWidth - 12 * 8) / 2, 8, "ELEGIR NIVEL", kGray, 2);
    const int nameWidth = static_cast<int>(name.size()) * 4 * 4;
    drawText((kWidth - nameWidth) / 2, 24, name, kWhite, 4);
    // Que es: el mundo del nivel y si ya se retoco a mano.
    const auto areas = previewMeta_.value("areas", nlohmann::json::array());
    std::string info = std::to_string(fileSelection_ + 1) + " DE " + std::to_string(count);
    if (!areas.empty()) {
        int main = 0;
        for (size_t i = 0; i < areas.size(); ++i) {
            if (areas[i].value("end", 0) - areas[i].value("start", 0) >
                areas[main].value("end", 0) - areas[main].value("start", 0)) {
                main = static_cast<int>(i);
            }
        }
        info += "   " + std::string(areaLabel(areas[static_cast<size_t>(main)]));
    }
    if (previewMeta_.value("edited", false)) info += "   (EDITADO)";
    drawText((kWidth - static_cast<int>(info.size()) * 8) / 2, 48, info, previewMeta_.value("edited", false) ? kYellow : kGray, 2);
    // Vista previa (el principio del nivel) con marco.
    const SDL_Rect preview{96, 62, 448, 224};
    fillRect({preview.x - 3, preview.y - 3, preview.w + 6, preview.h + 6}, kYellow);
    if (previewTexture_) SDL_RenderCopy(renderer_, previewTexture_, nullptr, &preview);
    // El nivel entero, en chiquito.
    const int width = previewMap_.widthTiles();
    const SDL_Rect strip{64, 294, 512, 16};
    fillRect(strip, SDL_Color{30, 30, 46, 255});
    if (width > 0 && !previewMeta_.empty()) {
        const auto rows = previewMeta_.value("rows", std::vector<std::string>{});
        for (int c = 0; c < width; ++c) {
            const int x0 = strip.x + c * strip.w / width;
            const int x1 = strip.x + (c + 1) * strip.w / width;
            for (int r = 0; r < static_cast<int>(rows.size()) && r < 16; ++r) {
                const char tile = c < static_cast<int>(rows[r].size()) ? rows[r][c] : '.';
                const SDL_Color color = tileMapColor(tile);
                if (color.a == 0) continue;
                fillRect({x0, strip.y + r, std::max(1, x1 - x0), 1}, color);
            }
        }
        drawRect({strip.x, strip.y - 1, 32 * strip.w / width, strip.h + 2}, kYellow);
    }
    // Flechas.
    for (int direction : {-1, 1}) {
        const SDL_Rect r = pickerArrowRect(direction);
        fillRect(r, SDL_Color{40, 44, 64, 255});
        drawRect(r, kGray);
        drawText(r.x + 12, r.y + 16, direction < 0 ? "<" : ">", kWhite, 2);
    }
    drawText((kWidth - 58 * 8) / 2, 322, "< > NIVEL   ARRIBA / ABAJO: MUNDO   ENTER: ABRIR   ESC: VOLVER", kGray, 2);
    drawText((kWidth - 26 * 8) / 2, 340, "CLIC EN LA VISTA PARA ABRIR", kGray, 2);
}
