#pragma once

#include <SDL.h>
#include <string>
#include <vector>

// Grilla de tiles solidos/vacios/plataforma cargada desde un JSON.
//
// Formato actual (placeholder minimo, mientras no existe el editor de
// niveles): un array de strings donde '#' es solido, '=' es plataforma (ver
// isPlatformTile) y '.' es vacio. El editor (seccion 7 de la especificacion)
// va a reemplazar/ampliar este formato con identidad de nivel, tiles
// graficos, enemigos, etc. sin cambiar como el juego consume el archivo
// (misma carpeta /levels).
class Tilemap {
public:
    bool loadFromFile(const std::string& path);

    int tileSize() const { return tileSize_; }
    int widthTiles() const { return width_; }
    int heightTiles() const { return height_; }
    int widthPixels() const { return width_ * tileSize_; }
    int heightPixels() const { return height_ * tileSize_; }

    // Solido de verdad ('#'): bloquea siempre, en cualquier direccion
    // (paredes, piso).
    bool isSolidTile(int col, int row) const;
    // Plataforma de un solo sentido ('='): quien la usa decide como
    // tratarla (tipicamente solo bloquea al aterrizar desde arriba, se
    // puede atravesar saltando desde abajo o bajando a proposito) -- el
    // Tilemap no impone ese criterio, solo expone si el tile es de este
    // tipo. Nunca cuenta como pared (HammerBro::applyHorizontalWithWallBounce
    // no la consulta) ni bloquea proyectiles (Hammer::update tampoco).
    bool isPlatformTile(int col, int row) const;
    bool isSolidAtPixel(float x, float y) const;

    void render(SDL_Renderer* renderer, SDL_Color backgroundColor) const;

private:
    int tileSize_ = 16;
    int width_ = 0;
    int height_ = 0;
    std::vector<uint8_t> solid_;     // row-major, 1 = solido ('#')
    std::vector<uint8_t> platform_;  // row-major, 1 = plataforma ('=')
};
