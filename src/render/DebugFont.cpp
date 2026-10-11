#include "DebugFont.h"

#include <array>
#include <unordered_map>

namespace {

using Glyph = std::array<uint8_t, 5>;  // 5 filas, 3 columnas (bits 2..0 = col 0..2)

const std::unordered_map<char, Glyph>& glyphTable() {
    static const std::unordered_map<char, Glyph> table = {
        {'0', {0b111, 0b101, 0b101, 0b101, 0b111}},
        {'1', {0b010, 0b110, 0b010, 0b010, 0b111}},
        {'2', {0b111, 0b001, 0b111, 0b100, 0b111}},
        {'3', {0b111, 0b001, 0b111, 0b001, 0b111}},
        {'4', {0b101, 0b101, 0b111, 0b001, 0b001}},
        {'5', {0b111, 0b100, 0b111, 0b001, 0b111}},
        {'6', {0b111, 0b100, 0b111, 0b101, 0b111}},
        {'7', {0b111, 0b001, 0b001, 0b001, 0b001}},
        {'8', {0b111, 0b101, 0b111, 0b101, 0b111}},
        {'9', {0b111, 0b101, 0b111, 0b001, 0b111}},
        {'F', {0b111, 0b100, 0b111, 0b100, 0b100}},
        {'P', {0b111, 0b101, 0b111, 0b100, 0b100}},
        {'S', {0b111, 0b100, 0b111, 0b001, 0b111}},
        // Agregadas para el selector de escenario de prueba (Game::renderScenarioSelect):
        // el resto del alfabeto se suma recien cuando haga falta, este font
        // es un placeholder "hasta tener fuentes reales para los menus".
        {'A', {0b010, 0b101, 0b111, 0b101, 0b101}},
        {'E', {0b111, 0b100, 0b110, 0b100, 0b111}},
        {'G', {0b111, 0b100, 0b101, 0b101, 0b111}},
        {'I', {0b111, 0b010, 0b010, 0b010, 0b111}},
        {'M', {0b101, 0b111, 0b111, 0b101, 0b101}},
        {'N', {0b101, 0b111, 0b111, 0b111, 0b101}},
        {'O', {0b111, 0b101, 0b101, 0b101, 0b111}},
        {'R', {0b111, 0b101, 0b111, 0b110, 0b101}},
        {'T', {0b111, 0b010, 0b010, 0b010, 0b010}},
        {'U', {0b101, 0b101, 0b101, 0b101, 0b111}},
        // Agregadas para el cartel de tipo de mundo (Game::render, cerca del FPS).
        {'C', {0b111, 0b100, 0b100, 0b100, 0b111}},
        {'D', {0b110, 0b101, 0b101, 0b101, 0b110}},
        {'L', {0b100, 0b100, 0b100, 0b100, 0b111}},
        {'V', {0b101, 0b101, 0b101, 0b101, 0b010}},
        {'W', {0b101, 0b101, 0b101, 0b111, 0b101}},
        {':', {0b000, 0b010, 0b000, 0b010, 0b000}},
        {' ', {0b000, 0b000, 0b000, 0b000, 0b000}},
        // Agregadas para "HAMMER BRO"/"LAKITU"/"BLOOPER"/"CHEEP CHEEP" en el
        // selector de escenario (Game::renderScenarioSelect): faltaban
        // desde el principio, se notaba como un hueco en esas palabras.
        {'B', {0b111, 0b101, 0b111, 0b101, 0b111}},
        {'H', {0b101, 0b101, 0b111, 0b101, 0b101}},
        {'K', {0b101, 0b101, 0b110, 0b101, 0b101}},
        // Flecha de seleccion del selector de escenario (navegable con mando).
        {'>', {0b100, 0b010, 0b001, 0b010, 0b100}},
        // HUD: el "x" chico del contador de monedas y el guion de "1-1".
        {'x', {0b000, 0b101, 0b010, 0b101, 0b000}},
        {'-', {0b000, 0b000, 0b111, 0b000, 0b000}},
        // Agregadas para el editor de niveles (rutas, ayuda de teclas).
        {'J', {0b001, 0b001, 0b001, 0b101, 0b111}},
        {'Q', {0b010, 0b101, 0b101, 0b110, 0b011}},
        {'X', {0b101, 0b101, 0b010, 0b101, 0b101}},
        {'Y', {0b101, 0b101, 0b010, 0b010, 0b010}},
        {'Z', {0b111, 0b001, 0b010, 0b100, 0b111}},
        {'/', {0b001, 0b001, 0b010, 0b100, 0b100}},
        {'.', {0b000, 0b000, 0b000, 0b000, 0b010}},
        {',', {0b000, 0b000, 0b000, 0b010, 0b100}},
        {'+', {0b000, 0b010, 0b111, 0b010, 0b000}},
        {'=', {0b000, 0b111, 0b000, 0b111, 0b000}},
        {'(', {0b010, 0b100, 0b100, 0b100, 0b010}},
        {')', {0b010, 0b001, 0b001, 0b001, 0b010}},
        {'*', {0b000, 0b101, 0b010, 0b101, 0b000}},
        {'?', {0b111, 0b001, 0b010, 0b000, 0b010}},
        {'_', {0b000, 0b000, 0b000, 0b000, 0b111}},
        {'!', {0b010, 0b010, 0b010, 0b000, 0b010}},
        {'<', {0b001, 0b010, 0b100, 0b010, 0b001}},
        {'[', {0b110, 0b100, 0b100, 0b100, 0b110}},
        {']', {0b011, 0b001, 0b001, 0b001, 0b011}},
        {'#', {0b101, 0b111, 0b101, 0b111, 0b101}},
    };
    return table;
}

}  // namespace

void DebugFont::drawText(SDL_Renderer* renderer, int x, int y, const std::string& text, int scale, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    const int glyphWidth = 3 * scale;
    const int advance = glyphWidth + scale;  // un pixel de separacion entre letras
    int cursorX = x;

    for (char c : text) {
        const auto& table = glyphTable();
        auto it = table.find(c);
        if (it == table.end()) {
            cursorX += advance;
            continue;
        }
        const Glyph& glyph = it->second;
        for (int row = 0; row < 5; ++row) {
            for (int col = 0; col < 3; ++col) {
                if (glyph[row] & (1 << (2 - col))) {
                    SDL_Rect pixel{cursorX + col * scale, y + row * scale, scale, scale};
                    SDL_RenderFillRect(renderer, &pixel);
                }
            }
        }
        cursorX += advance;
    }
}
