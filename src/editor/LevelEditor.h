#pragma once

#include <SDL.h>

#include <array>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "../render/SpriteSheet.h"
#include "../world/Tilemap.h"

// Editor de niveles (seccion 7 de la especificacion): herramienta de
// desarrollo aparte del juego (mario_editor.exe, ver CMakeLists.txt, opcion
// MARIO_BUILD_EDITOR), que edita el mismo JSON que consume el juego
// (levels/world{N}/{N}-{M}.json) sin recompilar nada.
//
// Lo basico del editor de Super Mario Maker, adaptado a lo que el juego
// soporta: paleta por categorias (Terreno / Items / Enemigos / Objetos) con
// los ultimos usados a mano; items que se sueltan dentro de un bloque;
// agarrar y arrastrar lo ya puesto; "sacudir" (S) para transformar y alas
// (A); goma, deshacer, borrar todo (con confirmacion); seleccion de un area
// para copiar/cortar/pegar/borrar; bandera de meta de un clic; subareas
// conectadas con tuberias; minimapa; y probar el nivel en el juego (desde
// el principio o desde donde esta la vista).
//
// El nivel se guarda en memoria como sus filas de tiles (rows_) + el resto
// del JSON tal cual (meta_: nombre, tiempo, bloques, tuberias, enemigos,
// escenografia, plataformas, checkpoints, areas...). Cada cambio reconstruye
// un Tilemap con loadFromJson y se dibuja con las mismas funciones que usa
// el juego (renderTerrain/renderPipes/renderBlocks/renderHazards), asi lo
// que se ve aca es lo que se ve jugando.
class LevelEditor {
public:
    bool init();
    // Abre este archivo al arrancar (si no, arranca con un nivel nuevo).
    void openFile(const std::string& path);
    void run();
    void shutdown();

private:
    enum class ToolKind { Tile, Goal, Item, Entity, Decoration, Platform, Checkpoint, Eraser, Select };
    enum class Tab { Terrain, Items, Enemies, Objects, Count };
    struct Tool {
        ToolKind kind;
        Tab tab;
        char tile = 0;        // Tile
        std::string type;     // Item: contenido del bloque; Entity/Decoration/Platform: tipo en el JSON
        std::string variant;  // Entity koopa: color
        int hits = 1;         // Item: golpes (bloque de N monedas)
        std::string label;
    };
    struct Snapshot {
        std::vector<std::string> rows;
        nlohmann::json meta;
    };
    // Lo que se copio con la seleccion: tiles del rectangulo + objetos con
    // posicion relativa a su esquina.
    struct Clipboard {
        int width = 0;
        int height = 0;
        std::vector<std::string> tiles;
        nlohmann::json objects = nlohmann::json::object();
        bool valid = false;
    };
    // Objeto agarrado con el mouse (enemigo/plataforma/escenografia).
    struct Grab {
        bool active = false;
        std::string list;
        size_t index = 0;
        int offsetCol = 0;
        int offsetRow = 0;
    };
    enum class Mode { Edit, OpenFile, TextInput, PickWarpDest };
    // World: ocupa todo el ancho, debajo de los demas (cicla el tipo de mundo).
    enum class Button { Eraser, Select, Undo, Reset, Play, PlayHere, SubArea, World, Count };

    // --- modelo
    void newLevel();
    bool loadLevel(const std::string& path);
    bool saveLevel();
    nlohmann::json buildJson() const;
    void rebuildTilemap();
    void pushUndo();
    void undo();
    void markDirty();
    void showMessage(const std::string& text, Uint32 ms = 2000);
    int widthTiles() const;
    int heightTiles() const;
    char tileAt(int col, int row) const;
    void setTile(int col, int row, char tile);
    void resizeWidth(int deltaCols);
    std::string defaultPathForName() const;

    // --- edicion
    void placeAt(int col, int row);
    void placeItemInBlock(int col, int row, const Tool& tool);
    void eraseAt(int col, int row);
    // includeDecorations: la escenografia es fondo -- solo se agarra si lo
    // que se tiene en la mano tambien es escenografia (si no, poner un
    // enemigo sobre un arbusto agarraria el arbusto).
    bool findObjectAt(int col, int row, std::string& list, size_t& index, bool includeDecorations = true) const;
    void cycleBlockItem(int col, int row);
    void changeBlockHits(int col, int row, int delta);
    void toggleWarpAt(int col, int row);
    void shakeAt(int col, int row);
    void toggleWingsAt(int col, int row);
    void cycleWorld();
    void addSubArea();
    void resetLevel();
    void playTest(bool fromView);
    void selectTool(int index);

    // --- seleccion
    bool hasSelection() const { return selC1_ >= selC0_ && selR1_ >= selR0_ && selectionValid_; }
    void copySelection(bool cut);
    void eraseSelection();
    void paste(int col, int row);

    // --- entrada
    void handleEvent(const SDL_Event& event);
    void handleKey(const SDL_KeyboardEvent& key);
    void handlePanelClick(int x, int y);
    void pressButton(Button button);
    bool mouseToCell(int x, int y, int& col, int& row) const;
    void listLevelFiles();
    void startTextInput(const std::string& prompt, const std::string& initial, int target);

    // --- dibujo
    void render();
    void renderLevel();
    void renderOverlays();
    void renderPalette();
    void renderStatus();
    void renderMinimap();
    void renderFileList();
    void drawToolIcon(const Tool& tool, const SDL_Rect& cell, int worldRow);
    void drawText(int x, int y, const std::string& text, SDL_Color color, int scale = 1);
    void fillRect(const SDL_Rect& rect, SDL_Color color);
    void drawRect(const SDL_Rect& rect, SDL_Color color);
    int worldRow() const;
    int decorationIndex(const std::string& type) const;
    std::vector<int> toolsInTab(Tab tab) const;
    SDL_Rect buttonRect(Button button) const;
    SDL_Rect minimapRect() const;

    static constexpr int kWidth = 640;
    static constexpr int kHeight = 360;
    static constexpr int kTopBar = 16;
    static constexpr int kViewWidth = 512;
    static constexpr int kViewHeight = 256;
    static constexpr int kPaletteX = kViewWidth;
    static constexpr int kTile = 16;
    static constexpr int kPaletteCols = 4;
    static constexpr int kPaletteCellW = 32;
    static constexpr int kPaletteCellH = 26;
    static constexpr int kTabsY = 36;
    static constexpr int kPaletteTop = 50;
    static constexpr int kRecentsY = 208;  // 6 filas de paleta (kPaletteTop + 6 * kPaletteCellH + 2)
    static constexpr int kButtonsY = 248;
    static constexpr int kMaxRecents = 4;

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    bool running_ = false;

    std::vector<std::string> rows_;
    nlohmann::json meta_;
    std::string path_;
    std::string levelsDir_;
    bool dirty_ = false;
    bool confirmQuit_ = false;
    bool confirmReset_ = false;
    Tilemap tilemap_;
    std::vector<Snapshot> undoStack_;

    std::vector<Tool> tools_;
    int selectedTool_ = 0;
    Tab currentTab_ = Tab::Terrain;
    std::vector<int> recents_;
    float cameraX_ = 0.0f;
    bool showGrid_ = true;
    bool painting_ = false;
    bool erasing_ = false;
    bool panning_ = false;
    bool selecting_ = false;
    bool draggingMinimap_ = false;
    Grab grab_;
    int panStartMouseX_ = 0;
    float panStartCameraX_ = 0.0f;
    int lastPaintCol_ = -1;
    int lastPaintRow_ = -1;
    int hoverCol_ = -1;
    int hoverRow_ = -1;
    std::string message_;
    Uint32 messageUntil_ = 0;

    // Seleccion (en tiles, inclusive) y portapapeles.
    bool selectionValid_ = false;
    int selAnchorCol_ = 0;
    int selAnchorRow_ = 0;
    int selC0_ = 0, selR0_ = 0, selC1_ = -1, selR1_ = -1;
    Clipboard clipboard_;

    Mode mode_ = Mode::Edit;
    std::vector<std::string> levelFiles_;
    int fileSelection_ = 0;
    std::string textPrompt_;
    std::string textValue_;
    int textTarget_ = 0;  // 0 = nombre, 1 = tiempo
    int warpSourceCol_ = 0;
    int warpSourceRow_ = 0;

    // Sprites (los mismos PNG que el juego).
    SpriteSheet terrainSprite_;
    SpriteSheet brickSprite_;
    SpriteSheet questionSprite_;
    SpriteSheet coinSprite_;
    SpriteSheet pipeSprite_;
    SpriteSheet lavaSprite_;
    SpriteSheet bridgeSprite_;
    SpriteSheet sceneryTilesSprite_;
    SpriteSheet flagpoleSprite_;
    SpriteSheet liftSprite_;
    SpriteSheet goombaSprite_;
    SpriteSheet koopaSprite_;
    SpriteSheet koopaRedSprite_;
    SpriteSheet mushroomSprite_;
    SpriteSheet oneUpSprite_;
    SpriteSheet fireFlowerSprite_;
    SpriteSheet starSprite_;
    static constexpr int kDecorationCount = 9;
    std::array<SpriteSheet, kDecorationCount> decorationSprites_;
};
