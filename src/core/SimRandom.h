#pragma once

#include <cstdint>

// Azar de la simulacion (enemigos, items, tiempos): un generador propio en
// vez de std::rand para poder sembrarlo igual en todas las PC de una
// partida en red (cada una simula el mismo juego con los mismos botones).
// Fuera de la red se siembra con la hora al arrancar (Game::init).
namespace sim {

inline uint32_t& randomState() {
    static uint32_t state = 0x12345678u;
    return state;
}

inline void seedRandom(uint32_t seed) { randomState() = seed != 0 ? seed : 0x12345678u; }

// 0..32767, como rand() con RAND_MAX chico (xorshift32).
inline int random15() {
    uint32_t& x = randomState();
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return static_cast<int>((x >> 8) & 0x7FFF);
}

// [0, 1]
inline float random01() { return static_cast<float>(random15()) / 32767.0f; }

inline float randomRange(float minVal, float maxVal) { return minVal + random01() * (maxVal - minVal); }

}  // namespace sim
