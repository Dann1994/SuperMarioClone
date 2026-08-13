#pragma once

#include <SDL.h>
#include <string>
#include <vector>

#include "../render/SpriteSheet.h"

// Grilla de tiles solidos/vacios/plataforma/ladrillo cargada desde un JSON.
//
// Formato actual (placeholder minimo, mientras no existe el editor de
// niveles): un array de strings donde '#' es solido, '=' es plataforma (ver
// isPlatformTile), 'B' es ladrillo rompible (ver isBrickTile) y '.' es
// vacio. El editor (seccion 7 de la especificacion) va a reemplazar/ampliar
// este formato con identidad de nivel, tiles graficos, enemigos, etc. sin
// cambiar como el juego consume el archivo (misma carpeta /levels).
class Tilemap {
public:
    bool loadFromFile(const std::string& path);

    int tileSize() const { return tileSize_; }
    int widthTiles() const { return width_; }
    int heightTiles() const { return height_; }
    int widthPixels() const { return width_ * tileSize_; }
    int heightPixels() const { return height_ * tileSize_; }

    // Solido de verdad ('#' o 'B' sin romper): bloquea siempre, en
    // cualquier direccion (paredes, piso, techo).
    bool isSolidTile(int col, int row) const;
    // Plataforma de un solo sentido ('='): quien la usa decide como
    // tratarla (tipicamente solo bloquea al aterrizar desde arriba, se
    // puede atravesar saltando desde abajo o bajando a proposito) -- el
    // Tilemap no impone ese criterio, solo expone si el tile es de este
    // tipo. Nunca cuenta como pared (HammerBro::applyHorizontalWithWallBounce
    // no la consulta) ni bloquea proyectiles (Hammer::update tampoco).
    bool isPlatformTile(int col, int row) const;
    // Ladrillo rompible ('B', seccion 5.3): solido como cualquier '#' (ver
    // isSolidTile) hasta que se rompe. Game::checkBrickBlockHit es quien
    // decide CUANDO romperlo (Mario grande golpeandolo desde abajo) y llama
    // breakTile(); esta clase solo guarda el estado, no la logica de que lo
    // dispara.
    bool isBrickTile(int col, int row) const;
    // A diferencia de isBrickTile, esto sigue devolviendo true despues de
    // romperlo (nunca se limpia) -- distingue un agujero hecho a proposito
    // rompiendo un ladrillo del borde real de una plataforma/piso que nunca
    // tuvo nada ahi. Lo usa HammerBro::applyHorizontalWithWallBounce para
    // no tenerle miedo a esos agujeros (ver el comentario de esa funcion).
    bool wasBrickTile(int col, int row) const;
    // Lo rompe: deja de ser solido y de ser ladrillo, como si nunca hubiera
    // estado ahi (salvo para wasBrickTile, ver arriba). No hace nada si el
    // tile no era un ladrillo sin romper.
    void breakTile(int col, int row);
    bool isSolidAtPixel(float x, float y) const;

    // brickSprite/worldRow: los tiles de ladrillo se dibujan con sprite
    // propio (una fila por WorldType, mismo criterio que el resto del
    // elenco) en vez del rectangulo plano que usan '#'/'='. hiddenBrickTiles:
    // ladrillos que Game.cpp esta dibujando el sola aparte, desplazados
    // (ver Game::checkBrickBlockHit, sacudida al golpearlo chico sin
    // romperlo) -- esta funcion los salta aca para no dibujarlos dos veces
    // (uno quieto atras y otro desplazado encima).
    void render(SDL_Renderer* renderer, SDL_Color backgroundColor, const SpriteSheet& brickSprite, int worldRow,
                const std::vector<SDL_Point>& hiddenBrickTiles = {}) const;

private:
    int tileSize_ = 16;
    int width_ = 0;
    int height_ = 0;
    std::vector<uint8_t> solid_;     // row-major, 1 = solido ('#' o 'B' sin romper)
    std::vector<uint8_t> platform_;  // row-major, 1 = plataforma ('=')
    std::vector<uint8_t> brick_;     // row-major, 1 = ladrillo sin romper ('B')
    std::vector<uint8_t> everBrick_; // row-major, 1 = fue 'B' al cargar el nivel (breakTile no lo toca)
};
