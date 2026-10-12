#include "LevelEditor.h"

#include <SDL_image.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "../render/DebugFont.h"
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
    {"castle_big", "CASTILLO GRANDE", 80, 176},
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
const char* const kButtonNames[] = {"GOMA",       "SELECCION", "DESHACER", "BORRAR TODO",
                                    "PROBAR",     "PROBAR ACA", "SUBAREA", "MUNDO"};

// Nombre del tipo de mundo para el boton MUNDO.
const char* worldLabel(const std::string& world) {
    if (world == "overworld") return "SUPERFICIE";
    if (world == "underground") return "SUBTERRANEO";
    if (world == "underwater") return "AGUA";
    if (world == "castle") return "CASTILLO";
    return "AUTOMATICO";
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
    for (int i = 0; ok && i < kDecorationCount; ++i) {
        ok = decorationSprites_[i].loadFromFile(renderer_, dir + "scenery_" + kDecorations[i].type + ".png",
                                                 kDecorations[i].width, kDecorations[i].height);
    }
    if (!ok) return false;

    // --- paleta, por pestana (como las categorias de Super Mario Maker)
    const char* const tileLabels[][2] = {
        {"#", "PISO"},           {"S", "BLOQUE DURO"},       {"B", "LADRILLO"},         {"?", "BLOQUE ?"},
        {"D", "LADRILLO ESPECIAL"}, {"H", "BLOQUE INVISIBLE"}, {"C", "LADRILLO + MONEDA"}, {"o", "MONEDA SUELTA"},
        {"T", "TUBERIA"},        {"=", "PLATAFORMA FIJA"},   {"L", "LAVA"},             {"b", "PUENTE BOWSER"},
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
        tools_.push_back({ToolKind::Decoration, Tab::Objects, 0, info.type, "", 1, info.label});
    }
    tools_.push_back({ToolKind::Platform, Tab::Objects, 0, "vertical", "", 1, "PLATAF. VERTICAL"});
    tools_.push_back({ToolKind::Platform, Tab::Objects, 0, "horizontal", "", 1, "PLATAF. HORIZONTAL"});
    tools_.push_back({ToolKind::Platform, Tab::Objects, 0, "lift_up", "", 1, "ELEVADOR QUE SUBE"});
    tools_.push_back({ToolKind::Platform, Tab::Objects, 0, "lift_down", "", 1, "ELEVADOR QUE BAJA"});
    tools_.push_back({ToolKind::Platform, Tab::Objects, 0, "fall", "", 1, "PLATAF. QUE CAE"});
    tools_.push_back({ToolKind::Checkpoint, Tab::Objects, 0, "", "", 1, "CHECKPOINT"});

    // Herramientas de los botones (no estan en ninguna pestana).
    tools_.push_back({ToolKind::Eraser, Tab::Count, 0, "", "", 1, "GOMA (BORRAR)"});
    tools_.push_back({ToolKind::Select, Tab::Count, 0, "", "", 1, "SELECCIONAR AREA"});

    levelsDir_ = std::string(MARIO_SOURCE_DIR) + "/levels";
    newLevel();
    running_ = true;
    return true;
}

void LevelEditor::openFile(const std::string& path) {
    if (loadLevel(path)) undoStack_.clear();
}

void LevelEditor::run() {
    while (running_) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) handleEvent(event);
        render();
    }
}

void LevelEditor::shutdown() {
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
    dirty_ = false;
    cameraX_ = 0.0f;
    selectionValid_ = false;
    rebuildTilemap();
    showMessage("ABIERTO", 1500);
    return true;
}

std::string LevelEditor::defaultPathForName() const {
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
    std::ofstream file(path_);
    if (!file.is_open()) {
        showMessage("NO SE PUDO GUARDAR", 3000);
        return false;
    }
    file << buildJson().dump(2) << '\n';
    dirty_ = false;
    showMessage("GUARDADO", 1500);
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
            list.push_back(entity);
            break;
        }
        case ToolKind::Decoration: {
            auto& list = meta_["decorations"];
            if (!list.is_array()) list = nlohmann::json::array();
            nlohmann::json decoration{{"type", tool.type}, {"col", col}, {"row", row}};
            // La tuberia en L va delante de los personajes (se meten adentro).
            if (tool.type == "pipe_side") decoration["front"] = true;
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
        if (col >= c && col < c + kDecorations[type].width / kTile && row >= r &&
            row < r + kDecorations[type].height / kTile) {
            listName = "decorations";
            index = i;
            return true;
        }
    }
    return false;
}

void LevelEditor::eraseAt(int col, int row) {
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

void LevelEditor::toggleWarpAt(int col, int row) {
    if (!tilemap_.isPipeTopTile(col, row)) {
        showMessage("P: CON EL MOUSE SOBRE EL TOPE DE UNA TUBERIA");
        return;
    }
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
    if (button == Button::World) return SDL_Rect{kPaletteX + 2, kButtonsY + 4 * 16, 124, 14};
    return SDL_Rect{kPaletteX + 2 + (i % 2) * 63, kButtonsY + (i / 2) * 16, 61, 14};
}

SDL_Rect LevelEditor::minimapRect() const {
    const int width = std::min(kViewWidth - 8, std::max(1, widthTiles()) * 2);
    return SDL_Rect{4, kTopBar + kViewHeight + 3, width, kLevelRows};
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
    const std::vector<int> tabTools = toolsInTab(currentTab_);
    if (y >= kPaletteTop && y < kRecentsY - 2) {
        const int cx = (x - kPaletteX) / kPaletteCellW;
        const int cy = (y - kPaletteTop) / kPaletteCellH;
        const int index = cy * kPaletteCols + cx;
        if (cx >= 0 && cx < kPaletteCols && index >= 0 && index < static_cast<int>(tabTools.size())) {
            selectTool(tabTools[index]);
        }
        return;
    }
    // Recientes.
    if (y >= kRecentsY + 8 && y < kRecentsY + 8 + kPaletteCellH) {
        const int index = (x - kPaletteX) / kPaletteCellW;
        if (index >= 0 && index < static_cast<int>(recents_.size())) selectTool(recents_[index]);
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
        if (dirty_ && !confirmQuit_) {
            confirmQuit_ = true;
            showMessage("HAY CAMBIOS SIN GUARDAR: CERRAR DE NUEVO PARA SALIR (CTRL+S GUARDA)", 5000);
        } else {
            running_ = false;
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
        const SDL_Rect mini = minimapRect();
        if (y >= mini.y && y < mini.y + mini.h + 2 && x >= mini.x && x < mini.x + mini.w) {
            draggingMinimap_ = true;
            cameraX_ = static_cast<float>((x - mini.x) * widthTiles() / mini.w * kTile - kViewWidth / 2);
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
                list.push_back({{"col", warpSourceCol_}, {"row", warpSourceRow_}, {"dest_col", col}, {"dest_row", row}});
                markDirty();
                showMessage("WARP CREADO");
            }
            mode_ = Mode::Edit;
            return;
        }
        const Tool& tool = tools_[selectedTool_];
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
        if (grab_.active) {
            grab_.active = false;
            markDirty();
        }
        painting_ = erasing_ = panning_ = selecting_ = draggingMinimap_ = false;
        return;
    }
    if (event.type == SDL_MOUSEMOTION) {
        if (panning_) cameraX_ = panStartCameraX_ - static_cast<float>(event.motion.x - panStartMouseX_);
        if (draggingMinimap_) {
            const SDL_Rect mini = minimapRect();
            cameraX_ = static_cast<float>((event.motion.x - mini.x) * widthTiles() / mini.w * kTile - kViewWidth / 2);
        }
        int col = 0, row = 0;
        if (!mouseToCell(event.motion.x, event.motion.y, col, row)) {
            hoverCol_ = hoverRow_ = -1;
            return;
        }
        hoverCol_ = col;
        hoverRow_ = row;
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
    std::sort(levelFiles_.begin(), levelFiles_.end());
    fileSelection_ = 0;
    mode_ = Mode::OpenFile;
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
        if (code == SDLK_ESCAPE) {
            mode_ = Mode::Edit;
        } else if (code == SDLK_UP && !levelFiles_.empty()) {
            fileSelection_ = (fileSelection_ + static_cast<int>(levelFiles_.size()) - 1) %
                             static_cast<int>(levelFiles_.size());
        } else if (code == SDLK_DOWN && !levelFiles_.empty()) {
            fileSelection_ = (fileSelection_ + 1) % static_cast<int>(levelFiles_.size());
        } else if (code == SDLK_RETURN && !levelFiles_.empty()) {
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
        listLevelFiles();
    } else if (ctrl && code == SDLK_n) {
        newLevel();
        undoStack_.clear();
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
    } else if ((code == SDLK_PLUS || code == SDLK_KP_PLUS || code == SDLK_EQUALS) && hoverCol_ >= 0) {
        changeBlockHits(hoverCol_, hoverRow_, 1);
    } else if ((code == SDLK_MINUS || code == SDLK_KP_MINUS) && hoverCol_ >= 0) {
        changeBlockHits(hoverCol_, hoverRow_, -1);
    } else if (code == SDLK_p && hoverCol_ >= 0) {
        toggleWarpAt(hoverCol_, hoverRow_);
    } else if (code == SDLK_F2) {
        startTextInput("NOMBRE DEL NIVEL (EJ. 1-2)", meta_.value("name", std::string()), 0);
    } else if (code == SDLK_F3) {
        startTextInput("TIEMPO DEL NIVEL", std::to_string(meta_.value("time", 400)), 1);
    } else if (code == SDLK_F5) {
        playTest(shift);
    }
}

// ============================================================ dibujo

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
    SDL_RenderPresent(renderer_);
}

void LevelEditor::renderLevel() {
    const float maxCamera = std::max(0.0f, static_cast<float>(widthTiles() * kTile - kViewWidth));
    cameraX_ = std::clamp(cameraX_, 0.0f, maxCamera);

    const SDL_Rect view{0, kTopBar, kViewWidth, kViewHeight};
    SDL_RenderSetViewport(renderer_, &view);
    const SDL_Rect clip{0, 0, kViewWidth, kViewHeight};
    SDL_RenderSetClipRect(renderer_, &clip);

    const int row = worldRow();
    const WorldType world = worldTypeFromName(meta_.value("world", std::string()));
    const int cam = static_cast<int>(std::lround(cameraX_));
    // Mismo orden de dibujo que Game::render.
    const SDL_Color bg = backgroundColorFor(world);
    SDL_SetRenderDrawColor(renderer_, bg.r, bg.g, bg.b, 255);
    SDL_RenderFillRect(renderer_, nullptr);
    for (const auto& decoration : meta_.value("decorations", nlohmann::json::array())) {
        const int index = decorationIndex(decoration.value("type", std::string()));
        if (index < 0) continue;
        decorationSprites_[index].draw(renderer_, 0, 0, decoration.value("col", 0) * kTile - cam,
                                       decoration.value("row", 0) * kTile, false);
    }
    tilemap_.renderTerrain(renderer_, bg, terrainSprite_, row, kViewWidth, cameraX_, false);
    tilemap_.renderScenery(renderer_, sceneryTilesSprite_, row, kViewWidth, cameraX_);
    tilemap_.renderHazards(renderer_, lavaSprite_, bridgeSprite_, kViewWidth, cameraX_);
    tilemap_.renderPipes(renderer_, pipeSprite_, row, cameraX_);
    tilemap_.renderBlocks(renderer_, brickSprite_, row, cameraX_, {}, &questionSprite_, 0, &coinSprite_, 0);

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
    return "??";
}

}  // namespace

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
                flagpoleSprite_.draw(renderer_, levelRow, top ? 0 : 1, x, r * kTile, false);
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
                          : mode == "fall" ? "CAE" : mode == "balance" ? "BAL" : "V";
        drawText(x + 1, y + 10, label, kYellow);
    }

    // Enemigos (con alas: rotulo arriba).
    for (const auto& entity : meta_.value("entities", nlohmann::json::array())) {
        const int x = entity.value("col", 0) * kTile - cam;
        const int feet = (entity.value("row", 0) + 1) * kTile;
        const std::string type = entity.value("type", std::string());
        if (type == "goomba") {
            goombaSprite_.draw(renderer_, levelRow, 0, x, feet - 16, false);
        } else if (type == "koopa") {
            const bool red = entity.value("color", std::string("green")) == "red";
            (red ? koopaRedSprite_ : koopaSprite_).draw(renderer_, red ? 0 : levelRow, 0, x, feet - 24, false);
            if (entity.contains("state")) drawText(x - 2, feet - 31, "ALAS", kWhite);
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
        drawText(sx + 9, sy - 7, label, kGreen);
        const int dx = warp.value("dest_col", 0) * kTile - cam;
        const int dy = warp.value("dest_row", 0) * kTile;
        drawRect({dx, dy, kTile, kTile}, kGreen);
        drawText(dx + 2, dy + 5, label, kGreen);
    }
    if (mode_ == Mode::PickWarpDest) {
        drawRect({warpSourceCol_ * kTile - cam, warpSourceRow_ * kTile, kTile * 2, kTile}, kGreen);
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
        SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 28);
        for (int c = 0; c <= widthTiles(); ++c) {
            const int x = c * kTile - cam;
            if (x >= 0 && x <= kViewWidth) SDL_RenderDrawLine(renderer_, x, 0, x, kViewHeight);
        }
        for (int r = 0; r <= heightTiles(); ++r) SDL_RenderDrawLine(renderer_, 0, r * kTile, kViewWidth, r * kTile);
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
    switch (tool.kind) {
        case ToolKind::Tile:
            switch (tool.tile) {
                case '#': terrainSprite_.drawScaled(renderer_, levelRow, 0, icon); break;
                case 'S': terrainSprite_.drawScaled(renderer_, levelRow, 1, icon); break;
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
                case 'T': pipeSprite_.drawScaled(renderer_, levelRow, 0, {icon.x - 4, icon.y + 4, 24, 12}); break;
                case 'L': lavaSprite_.drawScaled(renderer_, 0, 0, icon); break;
                case 'b': bridgeSprite_.drawScaled(renderer_, 0, 0, icon); break;
                case '=': fillRect({icon.x, icon.y + 4, 16, 6}, SDL_Color{214, 168, 116, 255}); break;
                case 'X':
                    drawRect(icon, kGray);
                    drawText(icon.x + 6, icon.y + 5, "X", kGray);
                    break;
                case 't': sceneryTilesSprite_.drawScaled(renderer_, levelRow, 1, icon); break;
                case 'r': sceneryTilesSprite_.drawScaled(renderer_, levelRow, 3, icon); break;
                case 'm': sceneryTilesSprite_.drawScaled(renderer_, levelRow, 5, icon); break;
                case 'n': sceneryTilesSprite_.drawScaled(renderer_, levelRow, 8, icon); break;
                case 'R': sceneryTilesSprite_.drawScaled(renderer_, levelRow, 10, icon); break;
                case 'K': sceneryTilesSprite_.drawScaled(renderer_, levelRow, 11, icon); break;
                case 'c': sceneryTilesSprite_.drawScaled(renderer_, levelRow, 14, icon); break;
                case 'W': sceneryTilesSprite_.drawScaled(renderer_, levelRow, 16, icon); break;
                case 'w': sceneryTilesSprite_.drawScaled(renderer_, levelRow, 18, icon); break;
                default: break;
            }
            break;
        case ToolKind::Goal:
            flagpoleSprite_.drawScaled(renderer_, levelRow, 0, {icon.x, icon.y - 2, 16, 6});
            flagpoleSprite_.drawScaled(renderer_, levelRow, 1, {icon.x, icon.y + 4, 16, 9});
            terrainSprite_.drawScaled(renderer_, levelRow, 1, {icon.x + 4, icon.y + 13, 8, 6});
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
            if (tool.type == "goomba") {
                goombaSprite_.drawScaled(renderer_, levelRow, 0, icon);
            } else if (tool.type != "koopa") {
                fillRect(icon, SDL_Color{120, 0, 120, 255});
                drawText(icon.x + 1, icon.y + 5, entityShortName(tool.type), kWhite);
            } else {
                const bool red = tool.variant == "red";
                (red ? koopaRedSprite_ : koopaSprite_).drawScaled(renderer_, red ? 0 : levelRow, 0,
                                                                  {icon.x + 2, icon.y - 2, 12, 18});
            }
            break;
        case ToolKind::Decoration: {
            const int index = decorationIndex(tool.type);
            const DecorationInfo& info = kDecorations[index];
            const float scale = std::min(26.0f / static_cast<float>(info.width), 18.0f / static_cast<float>(info.height));
            const int w = static_cast<int>(static_cast<float>(info.width) * scale);
            const int h = static_cast<int>(static_cast<float>(info.height) * scale);
            decorationSprites_[index].drawScaled(renderer_, 0, 0, {cell.x + (cell.w - w) / 2, cell.y + 21 - h, w, h});
            break;
        }
        case ToolKind::Platform: {
            for (int i = 0; i < 3; ++i) liftSprite_.drawScaled(renderer_, 0, 0, {icon.x - 4 + i * 8, icon.y + 4, 8, 8});
            const char* label = tool.type == "horizontal" ? "<>" : tool.type == "lift_up" ? "S"
                              : tool.type == "lift_down" ? "B" : tool.type == "fall" ? "!" : "V";
            drawText(icon.x + 5, icon.y + 14, label, kYellow);
            break;
        }
        case ToolKind::Checkpoint:
            fillRect({icon.x + 7, icon.y, 2, 16}, kCyan);
            drawText(icon.x + 1, icon.y + 5, "CP", kCyan);
            break;
        case ToolKind::Eraser:
        case ToolKind::Select:
            break;
    }
}

void LevelEditor::renderPalette() {
    fillRect({kPaletteX, 0, kWidth - kPaletteX, kHeight}, kPanel);
    const int levelRow = worldRow();
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
    // Herramientas de la pestana.
    const std::vector<int> tabTools = toolsInTab(currentTab_);
    for (size_t i = 0; i < tabTools.size(); ++i) {
        const SDL_Rect cell{kPaletteX + static_cast<int>(i % kPaletteCols) * kPaletteCellW,
                            kPaletteTop + static_cast<int>(i / kPaletteCols) * kPaletteCellH, kPaletteCellW,
                            kPaletteCellH};
        if (tabTools[i] == selectedTool_) fillRect(cell, kPanelLight);
        drawToolIcon(tools_[tabTools[i]], cell, levelRow);
        if (tabTools[i] == selectedTool_) drawRect(cell, kYellow);
    }
    // Recientes.
    drawText(kPaletteX + 4, kRecentsY, "RECIENTES", kGray);
    for (size_t i = 0; i < recents_.size(); ++i) {
        const SDL_Rect cell{kPaletteX + static_cast<int>(i) * kPaletteCellW, kRecentsY + 8, kPaletteCellW,
                            kPaletteCellH};
        if (recents_[i] == selectedTool_) fillRect(cell, kPanelLight);
        drawToolIcon(tools_[recents_[i]], cell, levelRow);
    }
    // Botones (goma, seleccion, deshacer, borrar todo, probar, subarea).
    for (int i = 0; i < static_cast<int>(Button::Count); ++i) {
        const Button button = static_cast<Button>(i);
        const SDL_Rect rect = buttonRect(button);
        bool active = false;
        if (button == Button::Eraser) active = tools_[selectedTool_].kind == ToolKind::Eraser;
        if (button == Button::Select) active = tools_[selectedTool_].kind == ToolKind::Select;
        SDL_Color color = active ? kPanelLight : SDL_Color{40, 44, 64, 255};
        if (button == Button::Reset && confirmReset_) color = SDL_Color{130, 40, 40, 255};
        fillRect(rect, color);
        drawRect(rect, active ? kYellow : kGray);
        const std::string label = button == Button::World
            ? std::string("MUNDO: ") + worldLabel(meta_.value("world", std::string()))
            : std::string(kButtonNames[i]);
        drawText(rect.x + 3, rect.y + 5, label, active ? kYellow : kWhite);
    }
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

    // Abajo: minimapa + ayuda.
    fillRect({0, kTopBar + kViewHeight, kViewWidth, kHeight - kTopBar - kViewHeight}, kPanel);
    renderMinimap();
    const int y0 = kTopBar + kViewHeight + 24;
    const char* const help[] = {
        "CLIC IZQ: PONER / AGARRAR Y MOVER   CLIC DER: BORRAR   RUEDA, FLECHAS O BOTON DEL MEDIO: DESPLAZAR",
        "ITEMS: SOLTALOS SOBRE UN BLOQUE   SOBRE UN ENEMIGO: S SACUDIR (TRANSFORMA)  A ALAS",
        "SOBRE UN BLOQUE: I CONTENIDO  + / - GOLPES     SOBRE UNA TUBERIA: P WARP (DESPUES CLIC EN EL DESTINO)",
        "SELECCION: CTRL+C / CTRL+X / CTRL+V / SUPR    CTRL+Z DESHACER   TAB PESTANA   G GRILLA   W TIPO DE MUNDO",
        "CTRL+N NUEVO  CTRL+O ABRIR  CTRL+S GUARDAR  F2 NOMBRE  F3 TIEMPO  F5 PROBAR (SHIFT: DESDE ACA)  CTRL+FLECHAS ANCHO",
    };
    for (int i = 0; i < 5; ++i) drawText(4, y0 + i * 8, help[i], kGray);

    const int yMsg = y0 + 42;
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

void LevelEditor::renderFileList() {
    fillRect({40, 30, kWidth - 80, kHeight - 60}, SDL_Color{20, 20, 30, 240});
    drawRect({40, 30, kWidth - 80, kHeight - 60}, kYellow);
    drawText(50, 38, "ABRIR NIVEL  (FLECHAS + ENTER, ESC CANCELA)", kWhite);
    const int perPage = 34;
    const int first = std::max(0, fileSelection_ - perPage + 1);
    for (int i = first; i < static_cast<int>(levelFiles_.size()) && i < first + perPage; ++i) {
        std::string shown = levelFiles_[i];
        const size_t pos = shown.find("/levels/");
        if (pos != std::string::npos) shown = shown.substr(pos + 1);
        const int y = 50 + (i - first) * 8;
        if (i == fileSelection_) drawText(50, y, ">", kYellow);
        drawText(58, y, shown, i == fileSelection_ ? kYellow : kGray);
    }
    if (levelFiles_.empty()) drawText(50, 50, "NO HAY NIVELES EN " + levelsDir_, kRed);
}
