#include "SpriteSheet.h"

#include <SDL_image.h>

SpriteSheet::~SpriteSheet() {
    if (texture_) {
        SDL_DestroyTexture(texture_);
    }
}

bool SpriteSheet::loadFromFile(SDL_Renderer* renderer, const std::string& path, int frameWidth, int frameHeight) {
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
