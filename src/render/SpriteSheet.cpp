#include "SpriteSheet.h"

#include <SDL_image.h>

#include <cstdlib>

SpriteSheet::~SpriteSheet() {
    if (texture_ && sRendererAlive) {
        SDL_DestroyTexture(texture_);
    }
}

bool SpriteSheet::loadFromFile(SDL_Renderer* renderer, const std::string& path, int frameWidth, int frameHeight) {
    // Si se recarga, libera la textura anterior.
    if (texture_) SDL_DestroyTexture(texture_);
    texture_ = IMG_LoadTexture(renderer, path.c_str());
    if (!texture_) {
        SDL_Log("No se pudo cargar el spritesheet '%s': %s", path.c_str(), IMG_GetError());
        return false;
    }
    SDL_SetTextureScaleMode(texture_, SDL_ScaleModeNearest);
    frameWidth_ = frameWidth;
    frameHeight_ = frameHeight;
    return true;
}

bool SpriteSheet::loadRecolored(SDL_Renderer* renderer, const std::string& path, int frameWidth, int frameHeight,
                                const std::vector<std::pair<SDL_Color, SDL_Color>>& colors) {
    SDL_Surface* raw = IMG_Load(path.c_str());
    if (!raw) {
        SDL_Log("No se pudo cargar el spritesheet '%s': %s", path.c_str(), IMG_GetError());
        return false;
    }
    SDL_Surface* surface = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(raw);
    if (!surface) return false;
    SDL_LockSurface(surface);
    for (int y = 0; y < surface->h; ++y) {
        Uint32* row = reinterpret_cast<Uint32*>(static_cast<Uint8*>(surface->pixels) + y * surface->pitch);
        for (int x = 0; x < surface->w; ++x) {
            Uint8 r = 0, g = 0, b = 0, a = 0;
            SDL_GetRGBA(row[x], surface->format, &r, &g, &b, &a);
            if (a == 0) continue;
            for (const auto& [from, to] : colors) {
                if (std::abs(r - from.r) <= 3 && std::abs(g - from.g) <= 3 && std::abs(b - from.b) <= 3) {
                    row[x] = SDL_MapRGBA(surface->format, to.r, to.g, to.b, a);
                    break;
                }
            }
        }
    }
    SDL_UnlockSurface(surface);
    if (texture_) SDL_DestroyTexture(texture_);
    texture_ = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    if (!texture_) return false;
    SDL_SetTextureScaleMode(texture_, SDL_ScaleModeNearest);
    frameWidth_ = frameWidth;
    frameHeight_ = frameHeight;
    return true;
}

void SpriteSheet::draw(SDL_Renderer* renderer, int row, int col, int destX, int destY, bool flipHorizontal,
                        bool flipVertical) const {
    SDL_Rect src{col * frameWidth_, row * frameHeight_, frameWidth_, frameHeight_};
    SDL_Rect dst{destX, destY, frameWidth_, frameHeight_};
    int flip = SDL_FLIP_NONE;
    if (flipHorizontal) flip |= SDL_FLIP_HORIZONTAL;
    if (flipVertical) flip |= SDL_FLIP_VERTICAL;
    SDL_RenderCopyEx(renderer, texture_, &src, &dst, 0.0, nullptr, static_cast<SDL_RendererFlip>(flip));
}

int SpriteSheet::rowCount() const {
    if (!texture_ || frameHeight_ == 0) return 0;
    int h = 0;
    SDL_QueryTexture(texture_, nullptr, nullptr, nullptr, &h);
    return h / frameHeight_;
}

void SpriteSheet::drawSlice(SDL_Renderer* renderer, int row, int col, int offsetY, int height,
                             int destX, int destY, bool flipHorizontal) const {
    SDL_Rect src{col * frameWidth_, row * frameHeight_ + offsetY, frameWidth_, height};
    SDL_Rect dst{destX, destY + offsetY, frameWidth_, height};
    SDL_RenderCopyEx(renderer, texture_, &src, &dst, 0.0, nullptr,
                      flipHorizontal ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
}

void SpriteSheet::drawScaled(SDL_Renderer* renderer, int row, int col, const SDL_Rect& dst) const {
    SDL_Rect src{col * frameWidth_, row * frameHeight_, frameWidth_, frameHeight_};
    SDL_RenderCopy(renderer, texture_, &src, &dst);
}
