#pragma once

#include <SDL.h>
#include <string>

// Spritesheet en grilla de frames de tamano fijo (seccion 8.1 de la
// especificacion): una fila por animacion, sin frames duplicados por
// direccion (se resuelve con flip horizontal via SDL_RenderCopyEx).
class SpriteSheet {
public:
    ~SpriteSheet();
    SpriteSheet() = default;
    SpriteSheet(const SpriteSheet&) = delete;
    SpriteSheet& operator=(const SpriteSheet&) = delete;

    bool loadFromFile(SDL_Renderer* renderer, const std::string& path, int frameWidth, int frameHeight);
    void draw(SDL_Renderer* renderer, int row, int col, int destX, int destY, bool flipHorizontal) const;
    // Dibuja solo una franja horizontal del frame (offsetY..offsetY+height dentro
    // del frame), en destX/destY + offsetY. Para componer sprites en capas, como
    // la pose de tirar fuego (torso propio + piernas del frame de movimiento actual).
    void drawSlice(SDL_Renderer* renderer, int row, int col, int offsetY, int height,
                    int destX, int destY, bool flipHorizontal) const;
    bool isLoaded() const { return texture_ != nullptr; }
    // Cantidad de filas del sheet (alto de la textura / alto de frame). Sirve
    // para calcular offsets de bloque, como en las variantes de paleta de estrella.
    int rowCount() const;

private:
    SDL_Texture* texture_ = nullptr;
    int frameWidth_ = 0;
    int frameHeight_ = 0;
};
