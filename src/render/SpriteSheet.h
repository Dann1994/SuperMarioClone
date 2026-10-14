#pragma once

#include <SDL.h>
#include <string>
#include <utility>
#include <vector>

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
    // Llamar antes de SDL_DestroyRenderer: el renderer libera todas sus
    // texturas, asi los SpriteSheet que se destruyen despues (miembros del
    // juego o del editor) no vuelven a liberarlas (se caia al salir).
    static void rendererDestroyed() { sRendererAlive = false; }
    // Igual, pero cambiando colores exactos (from -> to) al cargar: las
    // variantes de paleta del original que no tienen hoja propia (nieve).
    bool loadRecolored(SDL_Renderer* renderer, const std::string& path, int frameWidth, int frameHeight,
                       const std::vector<std::pair<SDL_Color, SDL_Color>>& colors);
    void draw(SDL_Renderer* renderer, int row, int col, int destX, int destY, bool flipHorizontal,
               bool flipVertical = false) const;
    // Dibuja solo una franja horizontal del frame (offsetY..offsetY+height dentro
    // del frame), en destX/destY + offsetY. Para componer sprites en capas, como
    // la pose de tirar fuego (torso propio + piernas del frame de movimiento actual).
    void drawSlice(SDL_Renderer* renderer, int row, int col, int offsetY, int height,
                    int destX, int destY, bool flipHorizontal) const;
    // Dibuja el frame estirado/achicado a dst (los iconos de la paleta del
    // editor de niveles, que tienen que entrar en una celda fija).
    void drawScaled(SDL_Renderer* renderer, int row, int col, const SDL_Rect& dst) const;
    // Oscurece (o vuelve a la normal con 255) todo lo que se dibuje despues.
    void setBrightness(Uint8 value) const {
        if (texture_) SDL_SetTextureColorMod(texture_, value, value, value);
    }
    int frameWidth() const { return frameWidth_; }
    int frameHeight() const { return frameHeight_; }
    bool isLoaded() const { return texture_ != nullptr; }
    static inline bool sRendererAlive = true;
    // Cantidad de filas del sheet (alto de la textura / alto de frame). Sirve
    // para calcular offsets de bloque, como en las variantes de paleta de estrella.
    int rowCount() const;
    // Intercambia la textura (y el tamano de frame) con otra hoja, sin copiar
    // nada: lo usa Mario para cambiar de paleta segun el tipo de mundo.
    void swap(SpriteSheet& other) noexcept {
        std::swap(texture_, other.texture_);
        std::swap(frameWidth_, other.frameWidth_);
        std::swap(frameHeight_, other.frameHeight_);
    }

private:
    SDL_Texture* texture_ = nullptr;
    int frameWidth_ = 0;
    int frameHeight_ = 0;
};
