#include "Tilemap.h"

#include <cmath>
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
    topCoin_.assign(static_cast<size_t>(width_) * height_, 0);
    question_.assign(static_cast<size_t>(width_) * height_, 0);
    questionUsed_.assign(static_cast<size_t>(width_) * height_, 0);
    questionHits_.assign(static_cast<size_t>(width_) * height_, 0);
    dispenser_.assign(static_cast<size_t>(width_) * height_, 0);
    dispenserUsed_.assign(static_cast<size_t>(width_) * height_, 0);
    dispenserHits_.assign(static_cast<size_t>(width_) * height_, 0);
    pipe_.assign(static_cast<size_t>(width_) * height_, 0);
    flagpole_.assign(static_cast<size_t>(width_) * height_, 0);

    for (int row = 0; row < height_; ++row) {
        const std::string& line = rows[row];
        for (int col = 0; col < width_ && col < static_cast<int>(line.size()); ++col) {
            const char c = line[col];
            const size_t i = static_cast<size_t>(row) * width_ + col;
            solid_[i] = (c == '#' || c == 'B' || c == 'C' || c == '?' || c == 'D' || c == 'T') ? 1 : 0;
            platform_[i] = (c == '=') ? 1 : 0;
            brick_[i] = (c == 'B' || c == 'C') ? 1 : 0;
            everBrick_[i] = (c == 'B' || c == 'C') ? 1 : 0;
            topCoin_[i] = (c == 'C') ? 1 : 0;
            question_[i] = (c == '?') ? 1 : 0;
            dispenser_[i] = (c == 'D') ? 1 : 0;
            pipe_[i] = (c == 'T') ? 1 : 0;
            flagpole_[i] = (c == 'F') ? 1 : 0;
        }
    }

    return true;
}

bool Tilemap::isFlagpoleTile(int col, int row) const {
    if (col < 0 || row < 0 || col >= width_ || row >= height_) {
        return false;
    }
    return flagpole_[row * width_ + col] != 0;
}

bool Tilemap::findFlagpole(int& col, int& topRow, int& bottomRow) const {
    for (int c = 0; c < width_; ++c) {
        for (int r = 0; r < height_; ++r) {
            if (!isFlagpoleTile(c, r)) continue;
            col = c;
            topRow = r;
            bottomRow = r;
            while (isFlagpoleTile(c, bottomRow + 1)) ++bottomRow;
            return true;
        }
    }
    return false;
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

bool Tilemap::hasCoinOnTop(int col, int row) const {
    if (col < 0 || row < 0 || col >= width_ || row >= height_) {
        return false;
    }
    return topCoin_[row * width_ + col] != 0;
}

void Tilemap::collectTopCoin(int col, int row) {
    if (col < 0 || row < 0 || col >= width_ || row >= height_) {
        return;
    }
    topCoin_[row * width_ + col] = 0;
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

bool Tilemap::isQuestionTile(int col, int row) const {
    if (col < 0 || row < 0 || col >= width_ || row >= height_) {
        return false;
    }
    return question_[row * width_ + col] != 0;
}

bool Tilemap::isQuestionUsed(int col, int row) const {
    if (col < 0 || row < 0 || col >= width_ || row >= height_) {
        return false;
    }
    return questionUsed_[row * width_ + col] != 0;
}

void Tilemap::markQuestionUsed(int col, int row) {
    if (col < 0 || row < 0 || col >= width_ || row >= height_) {
        return;
    }
    questionUsed_[row * width_ + col] = 1;
}

int Tilemap::questionHitCount(int col, int row) const {
    if (col < 0 || row < 0 || col >= width_ || row >= height_) {
        return 0;
    }
    return questionHits_[row * width_ + col];
}

void Tilemap::incrementQuestionHit(int col, int row) {
    if (col < 0 || row < 0 || col >= width_ || row >= height_) {
        return;
    }
    uint8_t& hits = questionHits_[row * width_ + col];
    if (hits < 255) ++hits;
}

bool Tilemap::isDispenserTile(int col, int row) const {
    if (col < 0 || row < 0 || col >= width_ || row >= height_) {
        return false;
    }
    return dispenser_[row * width_ + col] != 0;
}

bool Tilemap::isDispenserUsed(int col, int row) const {
    if (col < 0 || row < 0 || col >= width_ || row >= height_) {
        return false;
    }
    return dispenserUsed_[row * width_ + col] != 0;
}

void Tilemap::markDispenserUsed(int col, int row) {
    if (col < 0 || row < 0 || col >= width_ || row >= height_) {
        return;
    }
    dispenserUsed_[row * width_ + col] = 1;
}

int Tilemap::dispenserHitCount(int col, int row) const {
    if (col < 0 || row < 0 || col >= width_ || row >= height_) {
        return 0;
    }
    return dispenserHits_[row * width_ + col];
}

void Tilemap::incrementDispenserHit(int col, int row) {
    if (col < 0 || row < 0 || col >= width_ || row >= height_) {
        return;
    }
    uint8_t& hits = dispenserHits_[row * width_ + col];
    if (hits < 255) ++hits;
}

bool Tilemap::isPipeTile(int col, int row) const {
    if (col < 0 || row < 0 || col >= width_ || row >= height_) {
        return false;
    }
    return pipe_[row * width_ + col] != 0;
}

bool Tilemap::isPipeTopTile(int col, int row) const {
    return isPipeTile(col, row) && !isPipeTile(col, row - 1);
}

bool Tilemap::isPipeLeftHalf(int col, int row) const {
    return isPipeTile(col + 1, row);
}

bool Tilemap::isSolidAtPixel(float x, float y) const {
    if (x < 0.0f || y < 0.0f) {
        return false;
    }
    return isSolidTile(static_cast<int>(x) / tileSize_, static_cast<int>(y) / tileSize_);
}

void Tilemap::renderTerrain(SDL_Renderer* renderer, SDL_Color backgroundColor, float cameraX) const {
    const int camOffset = static_cast<int>(std::lround(cameraX));
    SDL_SetRenderDrawColor(renderer, backgroundColor.r, backgroundColor.g, backgroundColor.b, backgroundColor.a);
    // El fondo cubre todo el nivel (no solo la pantalla visible): es un
    // color plano, no hace falta desplazarlo con la camara, con que cubra
    // el nivel entero alcanza para cualquier cameraX valido.
    SDL_Rect background{0, 0, widthPixels(), heightPixels()};
    SDL_RenderFillRect(renderer, &background);

    SDL_SetRenderDrawColor(renderer, 151, 87, 40, 255);
    for (int row = 0; row < height_; ++row) {
        for (int col = 0; col < width_; ++col) {
            // Los ladrillos (normales y dispensadores), bloques de
            // interrogacion y tuberias se dibujan aparte con sprite propio
            // (renderBlocks/renderPipes) -- no como este rectangulo plano.
            if (!isSolidTile(col, row) || isBrickTile(col, row) || isQuestionTile(col, row) ||
                isDispenserTile(col, row) || isPipeTile(col, row)) {
                continue;
            }
            SDL_Rect tileRect{col * tileSize_ - camOffset, row * tileSize_, tileSize_, tileSize_};
            SDL_RenderFillRect(renderer, &tileRect);
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
            SDL_Rect tileRect{col * tileSize_ - camOffset, row * tileSize_ + tileSize_ / 2, tileSize_, tileSize_ / 2};
            SDL_RenderFillRect(renderer, &tileRect);
        }
    }
}

void Tilemap::renderBlocks(SDL_Renderer* renderer, const SpriteSheet& brickSprite, int worldRow, float cameraX,
                            const std::vector<SDL_Point>& hiddenBrickTiles, const SpriteSheet* questionSprite,
                            int questionAnimFrame, const SpriteSheet* coinSprite, int coinAnimFrame) const {
    auto isHidden = [&](int col, int row) {
        for (const SDL_Point& p : hiddenBrickTiles) {
            if (p.x == col && p.y == row) return true;
        }
        return false;
    };
    const int camOffset = static_cast<int>(std::lround(cameraX));

    for (int row = 0; row < height_; ++row) {
        for (int col = 0; col < width_; ++col) {
            if (isHidden(col, row)) continue;
            if (isBrickTile(col, row)) {
                brickSprite.draw(renderer, worldRow, 0, col * tileSize_ - camOffset, row * tileSize_, false);
            } else if (isDispenserTile(col, row)) {
                // Usado: mismo frame "vacio" que un bloque de interrogacion
                // usado (Tilemap::kQuestionUsedFrame), a pedido del usuario
                // -- asi se nota que ya no tiene item, en vez de seguir
                // pareciendo un ladrillo cualquiera para siempre. Sin usar
                // sigue igual a un ladrillo comun (no hay forma de
                // "adivinar" que tiene algo adentro, como en el original).
                if (isDispenserUsed(col, row) && questionSprite != nullptr) {
                    questionSprite->draw(renderer, worldRow, kQuestionUsedFrame, col * tileSize_ - camOffset,
                                          row * tileSize_, false);
                } else {
                    brickSprite.draw(renderer, worldRow, 0, col * tileSize_ - camOffset, row * tileSize_, false);
                }
            }
        }
    }

    if (questionSprite != nullptr) {
        for (int row = 0; row < height_; ++row) {
            for (int col = 0; col < width_; ++col) {
                if (!isQuestionTile(col, row) || isHidden(col, row)) {
                    continue;
                }
                const int frame = isQuestionUsed(col, row) ? kQuestionUsedFrame : questionAnimFrame;
                questionSprite->draw(renderer, worldRow, frame, col * tileSize_ - camOffset, row * tileSize_, false);
            }
        }
    }

    // Monedas paradas arriba de un ladrillo ('C', hasCoinOnTop): un tile mas
    // arriba de su ladrillo, no de este mismo -- por eso no pasan por
    // isHidden (esta atado a la posicion del LADRILLO, que se sacude, pero
    // la moneda de arriba se queda quieta hasta que la agarran).
    if (coinSprite != nullptr) {
        for (int row = 0; row < height_; ++row) {
            for (int col = 0; col < width_; ++col) {
                if (!hasCoinOnTop(col, row)) continue;
                coinSprite->draw(renderer, worldRow, coinAnimFrame, col * tileSize_ - camOffset,
                                  (row - 1) * tileSize_, false);
            }
        }
    }
}

void Tilemap::renderPipes(SDL_Renderer* renderer, const SpriteSheet& pipeSprite, int worldRow, float cameraX) const {
    const int camOffset = static_cast<int>(std::lround(cameraX));
    for (int row = 0; row < height_; ++row) {
        for (int col = 0; col < width_; ++col) {
            // Solo se dibuja desde la mitad IZQUIERDA (ver isPipeLeftHalf):
            // el frame ya cubre las dos columnas de una sola vez (32px de
            // ancho), dibujarlo tambien desde la derecha lo duplicaria.
            if (!isPipeTile(col, row) || !isPipeLeftHalf(col, row)) continue;
            const int frame = isPipeTopTile(col, row) ? 0 : 1;  // 0 = tope, 1 = cuerpo
            pipeSprite.draw(renderer, worldRow, frame, col * tileSize_ - camOffset, row * tileSize_, false);
        }
    }
}
