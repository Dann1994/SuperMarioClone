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
        {':', {0b000, 0b010, 0b000, 0b010, 0b000}},
        {' ', {0b000, 0b000, 0b000, 0b000, 0b000}},
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
