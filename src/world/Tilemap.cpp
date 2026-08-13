#include "Tilemap.h"

#include <fstream>
#include <nlohmann/json.hpp>

bool Tilemap::loadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        SDL_Log("No se pudo abrir el nivel: %s", path.c_str());
        return false;
    }

    nlohmann::json json;
    file >> json;

    tileSize_ = json.value("tile_size", 16);
    const auto rows = json.at("rows").get<std::vector<std::string>>();

    height_ = static_cast<int>(rows.size());
    width_ = height_ > 0 ? static_cast<int>(rows[0].size()) : 0;
    solid_.assign(static_cast<size_t>(width_) * height_, 0);
    platform_.assign(static_cast<size_t>(width_) * height_, 0);
    brick_.assign(static_cast<size_t>(width_) * height_, 0);
    everBrick_.assign(static_cast<size_t>(width_) * height_, 0);

    for (int row = 0; row < height_; ++row) {
        const std::string& line = rows[row];
        for (int col = 0; col < width_ && col < static_cast<int>(line.size()); ++col) {
            const char c = line[col];
            const size_t i = static_cast<size_t>(row) * width_ + col;
            solid_[i] = (c == '#' || c == 'B') ? 1 : 0;
            platform_[i] = (c == '=') ? 1 : 0;
            brick_[i] = (c == 'B') ? 1 : 0;
            everBrick_[i] = (c == 'B') ? 1 : 0;
        }
    }

    return true;
}

bool Tilemap::isSolidTile(int col, int row) const {
    if (col < 0 || row < 0 || col >= width_ || row >= height_) {
        return false;
    }
    return solid_[row * width_ + col] != 0;
}

bool Tilemap::isPlatformTile(int col, int row) const {
    if (col < 0 || row < 0 || col >= width_ || row >= height_) {
        return false;
    }
    return platform_[row * width_ + col] != 0;
}

bool Tilemap::isBrickTile(int col, int row) const {
    if (col < 0 || row < 0 || col >= width_ || row >= height_) {
        return false;
    }
    return brick_[row * width_ + col] != 0;
}

bool Tilemap::wasBrickTile(int col, int row) const {
    if (col < 0 || row < 0 || col >= width_ || row >= height_) {
        return false;
    }
    return everBrick_[row * width_ + col] != 0;
}

void Tilemap::breakTile(int col, int row) {
    if (col < 0 || row < 0 || col >= width_ || row >= height_) {
        return;
    }
    const size_t i = static_cast<size_t>(row) * width_ + col;
    if (brick_[i] == 0) return;
    brick_[i] = 0;
    solid_[i] = 0;
}

bool Tilemap::isSolidAtPixel(float x, float y) const {
    if (x < 0.0f || y < 0.0f) {
        return false;
    }
    return isSolidTile(static_cast<int>(x) / tileSize_, static_cast<int>(y) / tileSize_);
}

void Tilemap::render(SDL_Renderer* renderer, SDL_Color backgroundColor, const SpriteSheet& brickSprite,
                      int worldRow, const std::vector<SDL_Point>& hiddenBrickTiles) const {
    auto isHidden = [&](int col, int row) {
        for (const SDL_Point& p : hiddenBrickTiles) {
            if (p.x == col && p.y == row) return true;
        }
        return false;
    };
    SDL_SetRenderDrawColor(renderer, backgroundColor.r, backgroundColor.g, backgroundColor.b, backgroundColor.a);
    SDL_Rect background{0, 0, widthPixels(), heightPixels()};
    SDL_RenderFillRect(renderer, &background);

    SDL_SetRenderDrawColor(renderer, 151, 87, 40, 255);
    for (int row = 0; row < height_; ++row) {
        for (int col = 0; col < width_; ++col) {
            // Los ladrillos se dibujan aparte con sprite propio, ver mas
            // abajo -- no como este rectangulo plano.
            if (!isSolidTile(col, row) || isBrickTile(col, row)) {
                continue;
            }
            SDL_Rect tileRect{col * tileSize_, row * tileSize_, tileSize_, tileSize_};
            SDL_RenderFillRect(renderer, &tileRect);
        }
    }

    for (int row = 0; row < height_; ++row) {
        for (int col = 0; col < width_; ++col) {
            if (!isBrickTile(col, row) || isHidden(col, row)) {
                continue;
            }
            brickSprite.draw(renderer, worldRow, 0, col * tileSize_, row * tileSize_, false);
        }
    }

    // Plataformas ('=', ver isPlatformTile): mas claras y mas finas que un
    // tile solido, para que se distingan a simple vista de que se puede
    // atravesar de un lado.
    SDL_SetRenderDrawColor(renderer, 214, 168, 116, 255);
    for (int row = 0; row < height_; ++row) {
        for (int col = 0; col < width_; ++col) {
            if (!isPlatformTile(col, row)) {
                continue;
            }
            SDL_Rect tileRect{col * tileSize_, row * tileSize_ + tileSize_ / 2, tileSize_, tileSize_ / 2};
            SDL_RenderFillRect(renderer, &tileRect);
        }
    }
}
