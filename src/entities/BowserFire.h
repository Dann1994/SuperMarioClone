#pragma once

#include <SDL.h>

#include "../render/SpriteSheet.h"

// El fuego que escupe Bowser (seccion 5.2): sale de la boca (el origen que
// calcula Bowser::fireOriginX/Y) y avanza a velocidad media
// (Bowser::kFireSpeedX), sin gravedad ni colision con tiles (atraviesa piso
// y paredes igual que el Hammer, ver Hammer.h) ni desaparece al tocar a
// Mario/Luigi (les hace dano, pero sigue de largo, mismo criterio que el
// martillo -- es fuego, no un enemigo con vida propia). Solo se lo saca de
// en medio si sale de la pantalla o si cambia el escenario de prueba.
//
// Ademas de avanzar en X, al azar UNA SOLA VEZ por lanzamiento (en spawn(),
// no cambia a mitad de vuelo) elige entre tres variantes -- derecho (sin
// desvio), o un poco hacia arriba/abajo -- a pedido del usuario, para que
// se note que Bowser "no siempre tira igual" (parecido al original, que
// varia la altura del tiro). Sigue saliendo siempre del mismo origen (la
// boca, ver Bowser::fireOriginX/Y): cuando le toca desviarse, ese desvio de
// unos pocos pixeles pasa de forma PROGRESIVA apenas sale (kVerticalShiftPx
// a lo largo de kVerticalShiftDuration) y despues sigue derecho el resto
// del vuelo -- no es un tiro en diagonal de punta a punta, es "sale, se
// acomoda un poco de altura, y ahi sigue". El desvio hacia abajo nunca pasa
// del piso (maxY en spawn(), ver Bowser::fireMaxY): sin esto, con Bowser
// saltando y el fuego arrancando mas arriba de lo normal, un desvio hacia
// abajo se notaba pasando a traves del piso.
//
// Sprite propio del usuario (Documentaciones/Llamarada_bowser.png, dos
// frames), no de la hoja general de enemigos. Una sola paleta (el fuego no
// cambia de color por WorldType en el original): render() no recibe
// worldRow. Entidad separada de Bowser (que decide cuando y hacia donde
// tirarlo, ver Bowser::consumeFireRequest) por el mismo motivo que Hammer
// es separada de HammerBro.
class BowserFire {
public:
    // maxY: ver el comentario de la clase, tope de "hasta donde puede
    // bajar" para todo el vuelo (no sigue a Bowser si se mueve despues).
    void spawn(float x, float y, float velX, float maxY);
    void reset() { alive_ = false; }
    // screenWidth/screenHeight: para saber cuando salio del todo por un
    // borde (cualquiera de los cuatro, ver el comentario de la clase sobre
    // el desvio vertical) y desaparecer.
    void update(float dt, int screenWidth, int screenHeight);
    // cameraX: ver Game::cameraX_, se resta a la posicion en X antes de dibujar.
    void render(SDL_Renderer* renderer, const SpriteSheet& sprite, float cameraX = 0.0f) const;

    bool isAlive() const { return alive_; }

    float x() const { return x_; }
    float y() const { return y_; }
    int width() const { return kWidth; }
    int height() const { return kHeight; }
    static constexpr int kWidth = 26;
    static constexpr int kHeight = 8;

private:
    static constexpr float kAnimFrameDuration = 0.08f;
    // Cuanto sube/baja como mucho (ver el comentario de la clase) y cuanto
    // tarda en llegar ahi apenas sale -- despues de esa ventana sigue
    // derecho, sin mas cambio vertical.
    static constexpr float kVerticalShiftPixels = 10.0f;
    static constexpr float kVerticalShiftDuration = 0.2f;
    // Probabilidad de que le toque bajar (a pedido del usuario, mas seguido
    // que subir o derecho -- esas dos se reparten el resto por igual, ver
    // spawn()).
    static constexpr float kDownChance = 0.5f;

    float x_ = 0.0f;
    float y_ = 0.0f;
    float maxY_ = 0.0f;                // pies de Bowser al momento del disparo, ver el comentario de la clase
    float velX_ = 0.0f;
    float velY_ = 0.0f;               // solo != 0 mientras dura verticalShiftTimer_
    float verticalShiftTimer_ = 0.0f;  // cuenta regresiva del acomodo progresivo de altura
    bool alive_ = false;
    float animTimer_ = 0.0f;
    int animFrame_ = 0;
};
