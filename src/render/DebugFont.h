#pragma once

#include <SDL.h>
#include <string>

// Texto del juego: la fuente del Super Mario Bros. original (8x8, ver
// SmbFontGlyphs.inc) desde escala 2, y una mini de 3x5 en escala 1 (overlay
// de debug, editor). El avance es siempre 4 px por escala.
namespace DebugFont {

void drawText(SDL_Renderer* renderer, int x, int y, const std::string& text, int scale, SDL_Color color);
// Igual, con un borde negro de un punto alrededor de cada letra: para texto
// que va encima del nivel (el HUD sobre nubes o techos claros).
void drawTextOutlined(SDL_Renderer* renderer, int x, int y, const std::string& text, int scale, SDL_Color color);
// Cuantas letras ocupa el texto (la Ñ viene en dos bytes de UTF-8).
int length(const std::string& text);

}  // namespace DebugFont
