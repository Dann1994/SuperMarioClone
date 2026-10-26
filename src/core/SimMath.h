#pragma once

#include <cmath>

// Seno y coseno de la simulacion: hechos solo con sumas, restas,
// multiplicaciones y floor (exactos en IEEE en cualquier procesador), en vez
// de los de la biblioteca de C de Windows, que pueden diferir en el ultimo
// bit entre PCs. En red las dos PC tienen que calcular exactamente lo mismo.
namespace sim {

inline double sinDouble(double x) {
    constexpr double kPi = 3.14159265358979323846;
    constexpr double kTwoPi = 2.0 * kPi;
    x -= kTwoPi * std::floor((x + kPi) / kTwoPi);   // a [-pi, pi)
    if (x > kPi / 2.0) x = kPi - x;                  // a [-pi/2, pi/2]
    if (x < -kPi / 2.0) x = -kPi - x;
    const double x2 = x * x;
    // Taylor hasta x^15: error < 1e-12 en [-pi/2, pi/2].
    double term = x;
    double sum = x;
    for (int n = 1; n <= 7; ++n) {
        term *= -x2 / static_cast<double>((2 * n) * (2 * n + 1));
        sum += term;
    }
    return sum;
}

inline float sin(float x) { return static_cast<float>(sinDouble(static_cast<double>(x))); }
inline float cos(float x) { return static_cast<float>(sinDouble(static_cast<double>(x) + 3.14159265358979323846 / 2.0)); }
inline float hypot(float x, float y) { return std::sqrt(x * x + y * y); }

}  // namespace sim
