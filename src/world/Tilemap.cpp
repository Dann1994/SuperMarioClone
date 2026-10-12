#include "Tilemap.h"

#include <algorithm>

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
    return loadFromJson(json);
}

namespace {

// Lista del JSON del nivel; vacia si falta o no es una lista (un nivel a
// medio armar en el editor puede tener la clave en null).
nlohmann::json arrayField(const nlohmann::json& json, const char* key) {
    auto it = json.find(key);
    if (it == json.end() || !it->is_array()) return nlohmann::json::array();
    return *it;
}

}  // namespace

bool Tilemap::loadFromJson(const nlohmann::json& json) {
    tileSize_ = json.value("tile_size", 16);
    world_ = json.value("world", std::string());
    name_ = json.value("name", std::string("TEST"));
    timeLimit_ = json.value("time", 400);
    checkpointCols_.clear();
    if (json.contains("checkpoints") && json["checkpoints"].is_array()) {
        for (const auto& col : json["checkpoints"]) {
            if (col.is_number_integer()) checkpointCols_.push_back(col.get<int>());
        }
    }

    blocks_.clear();
    for (const auto& entry : arrayField(json, "blocks")) {
        LevelBlock block;
        block.col = entry.at("col").get<int>();
        block.row = entry.at("row").get<int>();
        block.item = entry.value("item", std::string("coin"));
        block.hits = entry.value("hits", 1);
        blocks_.push_back(block);
    }
    warps_.clear();
    for (const auto& entry : arrayField(json, "warps")) {
        LevelWarp warp;
        warp.col = entry.at("col").get<int>();
        warp.row = entry.at("row").get<int>();
        warp.destCol = entry.at("dest_col").get<int>();
        warp.destRow = entry.at("dest_row").get<int>();
        warp.dir = entry.value("dir", std::string("down"));
        warps_.push_back(warp);
    }
    areas_.clear();
    for (const auto& entry : arrayField(json, "areas")) {
        LevelArea area;
        area.startCol = entry.at("start").get<int>();
        area.endCol = entry.at("end").get<int>();
        area.world = entry.value("world", std::string());
        areas_.push_back(area);
    }
    decorations_.clear();
    for (const auto& entry : arrayField(json, "decorations")) {
        LevelDecoration decoration;
        decoration.type = entry.at("type").get<std::string>();
        decoration.col = entry.at("col").get<int>();
        decoration.row = entry.at("row").get<int>();
        decoration.front = entry.value("front", false);
        decorations_.push_back(decoration);
    }
    introArea_ = json.contains("intro") ? json["intro"].value("area", -1) : -1;
    platforms_.clear();
    for (const auto& entry : arrayField(json, "platforms")) {
        LevelPlatform platform;
        platform.col = entry.at("col").get<int>();
        platform.row = entry.at("row").get<int>();
        platform.width = entry.value("width", 3);
        platform.mode = entry.value("mode", std::string("vertical"));
        platform.range = entry.value("range", 0.0f);
        platform.period = entry.value("period", 4.0f);
        platform.speed = entry.value("speed", 50.0f);
        platforms_.push_back(platform);
    }
    entities_.clear();
    for (const auto& entry : arrayField(json, "entities")) {
        LevelEntity entity;
        entity.type = entry.at("type").get<std::string>();
        entity.col = entry.at("col").get<int>();
        entity.row = entry.at("row").get<int>();
        entity.movingRight = entry.value("moving_right", false);
        entity.color = entry.value("color", std::string("green"));
        entity.state = entry.value("state", std::string("walking"));
        entity.length = entry.value("length", 6);
        entity.speed = entry.value("speed", 100.0f);
        entity.endCol = entry.value("end_col", -1);
        entities_.push_back(entity);
    }
    const auto rows = json.at("rows").get<std::vector<std::string>>();

    height_ = static_cast<int>(rows.size());
    width_ = height_ > 0 ? static_cast<int>(rows[0].size()) : 0;
    // Sin "areas" en el JSON: una sola que cubre todo el nivel. Recien aca,
    // con width_ ya calculado para ESTE nivel (antes usaba el del nivel
    // cargado anteriormente, o 0 en el primero).
    if (areas_.empty()) areas_.push_back(LevelArea{0, width_ - 1, std::string()});
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
    hidden_.assign(static_cast<size_t>(width_) * height_, 0);
    coinTile_.assign(static_cast<size_t>(width_) * height_, 0);
    lava_.assign(static_cast<size_t>(width_) * height_, 0);
    hard_.assign(static_cast<size_t>(width_) * height_, 0);
    invisible_.assign(static_cast<size_t>(width_) * height_, 0);
    bridge_.assign(static_cast<size_t>(width_) * height_, 0);
    everBridge_.assign(static_cast<size_t>(width_) * height_, 0);
    scenery_.assign(static_cast<size_t>(width_) * height_, 0);

    for (int row = 0; row < height_; ++row) {
        const std::string& line = rows[row];
        for (int col = 0; col < width_ && col < static_cast<int>(line.size()); ++col) {
            const char c = line[col];
            const size_t i = static_cast<size_t>(row) * width_ + col;
            solid_[i] = (c == '#' || c == 'S' || c == 'B' || c == 'C' || c == '?' || c == 'D' || c == 'T' || c == 'b' ||
                         c == 'X' || isSolidSceneryChar(c)) ? 1 : 0;
            scenery_[i] = isSceneryChar(c) ? static_cast<uint8_t>(c) : 0;
            hard_[i] = (c == 'S') ? 1 : 0;
            invisible_[i] = (c == 'X') ? 1 : 0;
            platform_[i] = (c == '=') ? 1 : 0;
            brick_[i] = (c == 'B' || c == 'C') ? 1 : 0;
            everBrick_[i] = (c == 'B' || c == 'C') ? 1 : 0;
            topCoin_[i] = (c == 'C') ? 1 : 0;
            question_[i] = (c == '?') ? 1 : 0;
            dispenser_[i] = (c == 'D') ? 1 : 0;
            pipe_[i] = (c == 'T') ? 1 : 0;
            flagpole_[i] = (c == 'F') ? 1 : 0;
            hidden_[i] = (c == 'H') ? 1 : 0;
            coinTile_[i] = (c == 'o') ? 1 : 0;
            lava_[i] = (c == 'L') ? 1 : 0;
            bridge_[i] = (c == 'b') ? 1 : 0;
            everBridge_[i] = bridge_[i];
        }
    }

    return true;
}

bool Tilemap::isSceneryChar(char c) {
    return c == 't' || c == 'r' || c == 'm' || c == 'n' || c == 'R' || c == 'K' || c == 'c' || c == 'W' || c == 'w';
}

bool Tilemap::isSolidSceneryChar(char c) {
    return c == 't' || c == 'm' || c == 'R' || c == 'K' || c == 'c';
}

char Tilemap::sceneryAt(int col, int row) const {
    if (col < 0 || row < 0 || col >= width_ || row >= height_) return 0;
    return static_cast<char>(scenery_[static_cast<size_t>(row) * width_ + col]);
}

bool Tilemap::isCannonTopTile(int col, int row) const {
    return sceneryAt(col, row) == 'K' && sceneryAt(col, row - 1) != 'K';
}

void Tilemap::renderScenery(SDL_Renderer* renderer, const SpriteSheet& sprite, int worldRow, int viewWidth,
                            float cameraX) const {
    const int camOffset = static_cast<int>(std::lround(cameraX));
    const int firstCol = std::max(0, camOffset / tileSize_);
    const int lastCol = std::min(width_ - 1, (camOffset + viewWidth) / tileSize_ + 1);
    for (int row = 0; row < height_; ++row) {
        for (int col = firstCol; col <= lastCol; ++col) {
            const char c = sceneryAt(col, row);
            const int x = col * tileSize_ - camOffset;
            const int y = row * tileSize_;
            int frame = -1;
            switch (c) {
                case 't':  // copa de arbol: bordes redondeados donde termina
                case 'm': {  // sombrero de hongo, mismo criterio
                    const int base = c == 't' ? 0 : 4;
                    const bool left = sceneryAt(col - 1, row) != c;
                    const bool right = sceneryAt(col + 1, row) != c;
                    frame = base + (left ? 0 : right ? 2 : 1);
                    break;
                }
                case 'r': frame = 3; break;
                case 'n': frame = sceneryAt(col, row - 1) == 'm' ? 7 : 8; break;
                case 'R':
                    frame = 10;
                    // Baranda del puente de soga, en el tile de arriba si esta libre.
                    if (!isSolidTile(col, row - 1)) sprite.draw(renderer, worldRow, 9, x, y - tileSize_, false);
                    break;
                case 'K': {
                    // Boca, calavera y el resto base (columna del canon).
                    const bool top = sceneryAt(col, row - 1) != 'K';
                    const bool second = !top && sceneryAt(col, row - 2) != 'K';
                    frame = top ? 11 : second ? 12 : 13;
                    break;
                }
                case 'c': frame = sceneryAt(col, row - 1) == 'c' ? 15 : 14; break;
                case 'W': frame = sceneryAt(col, row - 1) == 'W' ? 17 : 16; break;
                case 'w': frame = 18; break;
                default: break;
            }
            if (frame >= 0) sprite.draw(renderer, worldRow, frame, x, y, false);
        }
    }
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

bool Tilemap::isHiddenBlockTile(int col, int row) const {
    if (col < 0 || row < 0 || col >= width_ || row >= height_) {
        return false;
    }
    return hidden_[row * width_ + col] != 0;
}

bool Tilemap::isInvisibleSolidTile(int col, int row) const {
    if (col < 0 || row < 0 || col >= width_ || row >= height_) {
        return false;
    }
    return invisible_[row * width_ + col] != 0;
}

bool Tilemap::isHardBlockTile(int col, int row) const {
    if (col < 0 || row < 0 || col >= width_ || row >= height_) {
        return false;
    }
    return hard_[row * width_ + col] != 0;
}

bool Tilemap::isLavaTile(int col, int row) const {
    if (col < 0 || row < 0 || col >= width_ || row >= height_) {
        return false;
    }
    return lava_[row * width_ + col] != 0;
}

bool Tilemap::isBridgeTile(int col, int row) const {
    if (col < 0 || row < 0 || col >= width_ || row >= height_) {
        return false;
    }
    return bridge_[row * width_ + col] != 0;
}

void Tilemap::collapseBridgeTile(int col, int row) {
    if (!isBridgeTile(col, row)) return;
    const size_t i = static_cast<size_t>(row) * width_ + col;
    bridge_[i] = 0;
    solid_[i] = 0;
}

bool Tilemap::hasAnyBridge() const {
    for (uint8_t value : bridge_) {
        if (value) return true;
    }
    return false;
}

void Tilemap::restoreBridges() {
    for (size_t i = 0; i < everBridge_.size(); ++i) {
        if (!everBridge_[i]) continue;
        bridge_[i] = 1;
        solid_[i] = 1;
    }
}

void Tilemap::renderHazards(SDL_Renderer* renderer, const SpriteSheet& lavaSprite, const SpriteSheet& bridgeSprite,
                            int viewWidth, float cameraX) const {
    const int camOffset = static_cast<int>(std::lround(cameraX));
    const int firstCol = std::max(0, camOffset / tileSize_);
    const int lastCol = std::min(width_ - 1, (camOffset + viewWidth) / tileSize_ + 1);
    for (int row = 0; row < height_; ++row) {
        for (int col = firstCol; col <= lastCol; ++col) {
            const int x = col * tileSize_ - camOffset;
            const int y = row * tileSize_;
            if (isLavaTile(col, row)) {
                // Fila 0 = superficie (no hay lava arriba), fila 1 = relleno.
                const int frame = isLavaTile(col, row - 1) ? 1 : 0;
                lavaSprite.draw(renderer, frame, 0, x, y, false);
            } else if (isBridgeTile(col, row)) {
                bridgeSprite.draw(renderer, 0, 0, x, y, false);
            }
        }
    }
}

bool Tilemap::isCoinTile(int col, int row) const {
    if (col < 0 || row < 0 || col >= width_ || row >= height_) {
        return false;
    }
    return coinTile_[row * width_ + col] != 0;
}

void Tilemap::collectCoinTile(int col, int row) {
    if (!isCoinTile(col, row)) return;
    coinTile_[row * width_ + col] = 0;
}

void Tilemap::revealHiddenBlock(int col, int row) {
    if (!isHiddenBlockTile(col, row)) return;
    const size_t i = static_cast<size_t>(row) * width_ + col;
    hidden_[i] = 0;
    solid_[i] = 1;
    question_[i] = 1;
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

void Tilemap::renderTerrain(SDL_Renderer* renderer, SDL_Color backgroundColor, const SpriteSheet& terrainSprite,
                            int worldRow, int viewWidth, float cameraX, bool fillBackground) const {
    const int camOffset = static_cast<int>(std::lround(cameraX));
    SDL_SetRenderDrawColor(renderer, backgroundColor.r, backgroundColor.g, backgroundColor.b, backgroundColor.a);
    // El fondo es un color plano: cubre toda la pantalla (nullptr = todo el
    // viewport), no el rectangulo del nivel -- con un nivel mas angosto que
    // la pantalla la camara queda en X negativa (centrado, ver
    // Game::updateCamera) y el rectangulo del nivel no la cubriria entera.
    if (fillBackground) SDL_RenderFillRect(renderer, nullptr);

    for (int row = 0; row < height_; ++row) {
        for (int col = 0; col < width_; ++col) {
            // Los ladrillos (normales y dispensadores), bloques de
            // interrogacion, tuberias y el puente se dibujan aparte con su
            // propio sprite (renderBlocks/renderPipes/renderHazards).
            if (!isSolidTile(col, row) || isBrickTile(col, row) || isQuestionTile(col, row) ||
                isDispenserTile(col, row) || isPipeTile(col, row) || isBridgeTile(col, row) ||
                isInvisibleSolidTile(col, row) || sceneryAt(col, row) != 0) {
                continue;
            }
            const int x = col * tileSize_ - camOffset;
            if (x + tileSize_ < 0 || x > viewWidth) continue;
            terrainSprite.draw(renderer, worldRow, isHardBlockTile(col, row) ? 1 : 0, x, row * tileSize_, false);
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
        // Monedas sueltas ('o'): en su propio tile.
        for (int row = 0; row < height_; ++row) {
            for (int col = 0; col < width_; ++col) {
                if (!isCoinTile(col, row)) continue;
                coinSprite->draw(renderer, worldRow, coinAnimFrame, col * tileSize_ - camOffset, row * tileSize_,
                                  false);
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
