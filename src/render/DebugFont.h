#pragma once

#include <SDL.h>
#include <string>

// Fuente bitmap minima (3x5 px por caracter) para overlays de debug (FPS,
// etc). No depende de ningun asset externo, asi que esta disponible desde
// el arranque del proyecto, antes de tener fuentes reales para los menus.
namespace DebugFont {

void drawText(SDL_Renderer* renderer, int x, int y, const std::string& text, int scale, SDL_Color color);

}  // namespace DebugFont
