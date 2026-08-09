#pragma once

#include <SDL.h>
#include <string>
#include <vector>

// Grilla de tiles solidos/vacios cargada desde un JSON.
//
// Formato actual (placeholder minimo, mientras no existe el editor de
// niveles): un array de strings donde '#' es solido y '.' es vacio. El
// editor (seccion 7 de la especificacion) va a reemplazar/ampliar este
// formato con identidad de nivel, tiles graficos, enemigos, etc. sin
// cambiar como el juego consume el archivo (misma carpeta /levels).
class Tilemap {
public:
    bool loadFromFile(const std::string& path);

    int tileSize() const { return tileSize_; }
    int widthTiles() const { return width_; }
    int heightTiles() const { return height_; }
    int widthPixels() const { return width_ * tileSize_; }
    int heightPixels() const { return height_ * tileSize_; }

    bool isSolidTile(int col, int row) const;
    bool isSolidAtPixel(float x, float y) const;

    void render(SDL_Renderer* renderer, SDL_Color backgroundColor) const;

private:
    int tileSize_ = 16;
    int width_ = 0;
    int height_ = 0;
    std::vector<uint8_t> solid_;  // row-major, 1 = solido
};
