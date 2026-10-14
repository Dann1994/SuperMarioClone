#pragma once

#include <SDL.h>

#include <utility>
#include <vector>

// Recoloreos de las paletas del original que no tienen hoja propia (se
// aplican al cargar, SpriteSheet::loadRecolored). Los usan el juego y el
// editor de niveles, asi se ven igual.
using ColorSwaps = std::vector<std::pair<SDL_Color, SDL_Color>>;

// Paleta 0 de superficie -> la de nieve (DaySnowPaletteData del original,
// colores de la seccion "Snow" de Documentaciones/Tiles.png): verde claro a
// blanco, verde a gris oscuro, negro a gris claro.
inline const ColorSwaps kSnowColors = {
    {SDL_Color{140, 214, 0, 255}, SDL_Color{255, 255, 255, 255}},
    {SDL_Color{16, 148, 0, 255}, SDL_Color{99, 99, 99, 255}},
    {SDL_Color{0, 0, 0, 255}, SDL_Color{173, 173, 173, 255}},
};
// Areas de hongos (estilo 1 del encabezado: 4-2 zona warp, 4-3): la paleta
// 0 es la de hongos del original (naranja y rojo en vez de los verdes): los
// caños salen naranjas.
inline const ColorSwaps kMushroomColors = {
    {SDL_Color{140, 214, 0, 255}, SDL_Color{230, 156, 33, 255}},
    {SDL_Color{16, 148, 0, 255}, SDL_Color{181, 49, 33, 255}},
};
// Superficie -> paleta de castillo (BackgroundColorCtrl 7, el 6-3): las
// paletas 0 (verdes) y 1 (marron/rosado) del original, comparando tile a
// tile las secciones Overworld y Castle de Documentaciones/Tiles.png.
inline const ColorSwaps kGrayColors = {
    {SDL_Color{140, 214, 0, 255}, SDL_Color{255, 255, 255, 255}},
    {SDL_Color{16, 148, 0, 255}, SDL_Color{173, 173, 173, 255}},
    {SDL_Color{156, 74, 0, 255}, SDL_Color{173, 173, 173, 255}},
    {SDL_Color{255, 206, 197, 255}, SDL_Color{255, 255, 255, 255}},
    {SDL_Color{0, 0, 0, 255}, SDL_Color{99, 99, 99, 255}},
};
// La tuberia en L (scenery_pipe_side.png, dibujada en superficie) con los
// colores de las tuberias de cada mundo (las filas de pipe.png): en el
// subterraneo el borde verde oscuro, en el castillo grises. En el agua queda
// verde (la de salida del 2-2 es verde en el original).
inline const ColorSwaps kPipeUndergroundColors = {
    {SDL_Color{0, 0, 0, 255}, SDL_Color{8, 74, 0, 255}},
};
inline const ColorSwaps kPipeCastleColors = {
    {SDL_Color{140, 214, 0, 255}, SDL_Color{255, 255, 255, 255}},
    {SDL_Color{16, 148, 0, 255}, SDL_Color{173, 173, 173, 255}},
    {SDL_Color{0, 0, 0, 255}, SDL_Color{99, 99, 99, 255}},
};
