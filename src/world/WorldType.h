#pragma once

#include <SDL.h>

// Tipo de entorno del nivel actual (seccion 5.5 de la especificacion:
// superficie, subterraneo, acuatico, castillos). Por ahora solo Underwater
// tiene fisica/animacion propia (nado, velocidad reducida); Underground y
// Castle se comportan como Overworld hasta que se implementen sus mecanicas
// (y eventualmente sus paletas de estrella propias, ya extraidas en
// tools/asset_scripts/gen_star.py).
enum class WorldType {
    Overworld,
    Underwater,
    Underground,
    Castle,
};

inline WorldType nextWorldType(WorldType type) {
    switch (type) {
        case WorldType::Overworld: return WorldType::Underwater;
        case WorldType::Underwater: return WorldType::Underground;
        case WorldType::Underground: return WorldType::Castle;
        case WorldType::Castle: default: return WorldType::Overworld;
    }
}

// Color de fondo para distinguir el tipo de mundo a simple vista mientras no
// hay tileset/paleta de fondo propia por nivel.
inline SDL_Color backgroundColorFor(WorldType type) {
    switch (type) {
        case WorldType::Underwater: return SDL_Color{66, 66, 255, 255};    // el azul de la paleta del agua (el fondo de sus tiles)
        case WorldType::Underground: return SDL_Color{0, 0, 0, 255};      // negro, como el original
        case WorldType::Castle: return SDL_Color{0, 0, 0, 255};           // negro
        case WorldType::Overworld: default: return SDL_Color{92, 148, 252, 255}; // celeste original
    }
}
