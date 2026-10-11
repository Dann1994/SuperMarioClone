#pragma once

#include <SDL.h>
#include <string>
#include <vector>

#include "../render/SpriteSheet.h"

// Grilla de tiles solidos/vacios/plataforma/ladrillo/interrogacion cargada
// desde un JSON.
//
// Formato actual (placeholder minimo, mientras no existe el editor de
// niveles): un array de strings donde '#' es solido, '=' es plataforma (ver
// isPlatformTile), 'B' es ladrillo rompible (ver isBrickTile), 'C' es
// ladrillo rompible con una moneda parada arriba (ver hasCoinOnTop), '?' es
// bloque de interrogacion (ver isQuestionTile), 'D' es ladrillo
// dispensador (ver isDispenserTile) y '.' es vacio. El editor (seccion 7 de
// la especificacion) va a reemplazar/ampliar este formato con identidad de
// nivel, tiles graficos, enemigos, etc. sin cambiar como el juego consume
// el archivo (misma carpeta /levels). 'T' es tuberia (ver isPipeTile):
// siempre se coloca en pares de 2 columnas de ancho (como en el original),
// cualquier alto -- esta clase deduce sola cual es el "caño" (izquierda vs
// derecha) y cual es el "tope" (la fila de mas arriba de un tramo vertical
// contiguo de 'T') mirando los vecinos, no hace falta un caracter distinto
// para cada parte. 'F' es el mastil de la bandera de meta (ver
// findFlagpole): una columna vertical contigua, la fila de arriba es la
// bola del tope.
// Datos del nivel que no son tiles (campos opcionales del JSON, ver
// Tilemap::loadFromFile). Game.cpp los traduce a sus propias estructuras.
struct LevelBlock {          // "blocks": contenido de un '?' / 'D'
    int col = 0;
    int row = 0;
    std::string item;        // coin, powerup, mushroom, fire_flower, star, one_up
    int hits = 1;            // golpes antes de quedar usado (bloque de N monedas)
};
struct LevelWarp {           // "warps": tuberia que se entra apretando abajo
    int col = 0;             // columna izquierda del tope
    int row = 0;             // fila del tope
    int destCol = 0;         // a donde aparece (tile, esquina sup. izquierda)
    int destRow = 0;
};
struct LevelArea {           // "areas": tramos de columnas con camara propia
    int startCol = 0;
    int endCol = 0;          // inclusive
    std::string world;       // overworld/underground/underwater/castle; "" = el del nivel
};
struct LevelDecoration {     // "decorations": escenografia de fondo
    std::string type;        // castle_small, hill_big, hill_small, bush_big, bush_small, cloud_big, cloud_small
    int col = 0;             // esquina superior izquierda, en tiles
    int row = 0;
};
struct LevelEntity {         // "entities": enemigos del nivel
    std::string type;        // goomba, koopa
    int col = 0;
    int row = 0;             // fila donde apoya (los pies sobre el tope de row+1)
    bool movingRight = false;
    std::string color;       // koopa: green (defecto) / red
    std::string state;       // koopa: walking (defecto) / jumping / flying
};

class Tilemap {
public:
    bool loadFromFile(const std::string& path);

    int tileSize() const { return tileSize_; }
    // Identidad del nivel para el HUD (campo "name" del JSON, ej. "1-1";
    // "TEST" si no lo tiene) y tiempo limite en unidades del timer clasico
    // (campo "time", 400 por defecto como la mayoria de los niveles del
    // original -- ver Game::timeRemaining_).
    const std::string& name() const { return name_; }
    int timeLimit() const { return timeLimit_; }
    // Checkpoints a mitad de nivel (campo "checkpoints" del JSON, lista de
    // columnas, vacia si no tiene): invisibles como en el original, se
    // activan al pasarlos (ver Game::updateCheckpoints).
    const std::vector<int>& checkpointCols() const { return checkpointCols_; }
    const std::vector<LevelBlock>& blocks() const { return blocks_; }
    const std::vector<LevelWarp>& warps() const { return warps_; }
    const std::vector<LevelEntity>& entities() const { return entities_; }
    // Siempre hay al menos una (todo el nivel) si el JSON no trae "areas".
    // Sirven para tener zonas separadas en el mismo mapa (un cuarto bonus
    // subterraneo a la derecha del nivel, por ejemplo): la camara no sale
    // del area donde esta el jugador (ver Game::updateCamera).
    const std::vector<LevelArea>& areas() const { return areas_; }
    const std::vector<LevelDecoration>& decorations() const { return decorations_; }

    // Bloque invisible ('H', como el del 1-UP del 1-1): no es solido para
    // nada salvo para la cabeza de Mario/Luigi subiendo desde abajo (ver
    // Mario::moveAndCollide); al golpearlo aparece (revealHiddenBlock) y se
    // comporta como un '?' (Game::checkQuestionBlockHit).
    bool isHiddenBlockTile(int col, int row) const;
    void revealHiddenBlock(int col, int row);

    // Moneda suelta ('o', como las del cuarto bonus del 1-1): no es
    // solida, se agarra al tocarla (Game::checkCoinTiles) y la dibuja
    // renderBlocks con la misma animacion que las de arriba de un ladrillo.
    bool isCoinTile(int col, int row) const;
    void collectCoinTile(int col, int row);
    int widthTiles() const { return width_; }
    int heightTiles() const { return height_; }
    int widthPixels() const { return width_ * tileSize_; }
    int heightPixels() const { return height_ * tileSize_; }

    // Solido de verdad ('#' o 'B' sin romper): bloquea siempre, en
    // cualquier direccion (paredes, piso, techo).
    bool isSolidTile(int col, int row) const;
    // Plataforma de un solo sentido ('='): quien la usa decide como
    // tratarla (tipicamente solo bloquea al aterrizar desde arriba, se
    // puede atravesar saltando desde abajo o bajando a proposito) -- el
    // Tilemap no impone ese criterio, solo expone si el tile es de este
    // tipo. Nunca cuenta como pared (HammerBro::applyHorizontalWithWallBounce
    // no la consulta) ni bloquea proyectiles (Hammer::update tampoco).
    bool isPlatformTile(int col, int row) const;
    // Ladrillo rompible ('B', seccion 5.3): solido como cualquier '#' (ver
    // isSolidTile) hasta que se rompe. Game::checkBrickBlockHit es quien
    // decide CUANDO romperlo (Mario grande golpeandolo desde abajo) y llama
    // breakTile(); esta clase solo guarda el estado, no la logica de que lo
    // dispara.
    bool isBrickTile(int col, int row) const;
    // Moneda parada arriba del ladrillo ('C', pedido del usuario): no ocupa
    // tile propio, se dibuja un piso mas arriba de este mismo ladrillo (ver
    // renderBlocks) mientras no la agarren. Game::checkBrickBlockHit la da
    // apenas se golpea el ladrillo de abajo -- se rompa o no se rompa (a
    // diferencia del ladrillo en si, romperlo o no es independiente de
    // esto) -- y la marca agarrada para siempre con collectTopCoin.
    bool hasCoinOnTop(int col, int row) const;
    void collectTopCoin(int col, int row);
    // A diferencia de isBrickTile, esto sigue devolviendo true despues de
    // romperlo (nunca se limpia) -- distingue un agujero hecho a proposito
    // rompiendo un ladrillo del borde real de una plataforma/piso que nunca
    // tuvo nada ahi. Lo usa HammerBro::applyHorizontalWithWallBounce para
    // no tenerle miedo a esos agujeros (ver el comentario de esa funcion).
    bool wasBrickTile(int col, int row) const;
    // Lo rompe: deja de ser solido y de ser ladrillo, como si nunca hubiera
    // estado ahi (salvo para wasBrickTile, ver arriba). No hace nada si el
    // tile no era un ladrillo sin romper.
    void breakTile(int col, int row);
    bool isSolidAtPixel(float x, float y) const;

    // Bloque de interrogacion ('?', seccion 5.3): SIEMPRE solido, se rompa o
    // no se rompa nada (a diferencia del ladrillo, nunca deja de serlo) --
    // solo cambia de "sin usar" (anima, tira el item) a "usado" (quieto,
    // fila 4 del sprite, ver Game::checkQuestionBlockHit) la primera vez que
    // lo golpean, para siempre. Esta clase solo guarda ese estado, no la
    // logica de que item le toca a cada uno ni cuando dispararlo -- eso es
    // de Game.cpp, igual que con isBrickTile/breakTile.
    bool isQuestionTile(int col, int row) const;
    bool isQuestionUsed(int col, int row) const;
    void markQuestionUsed(int col, int row);
    // Cuantas veces ya lo golpearon (0 la primera vez, antes de marcarlo
    // usado): a diferencia de isQuestionUsed (que es binario), esto le
    // permite a Game::checkQuestionBlockHit soportar bloques de "N golpes"
    // (ver QuestionBlockSpawn::maxHits) sin que esta clase sepa nada de esa
    // cuenta maxima -- solo lleva la cuenta, Game.cpp decide cuando llamar
    // markQuestionUsed.
    int questionHitCount(int col, int row) const;
    void incrementQuestionHit(int col, int row);

    // Ladrillo dispensador ('D'): variante de ladrillo que "actua como el
    // de interrogacion" (pedido del usuario) -- SIEMPRE solido, nunca se
    // rompe (a diferencia de isBrickTile/breakTile), y tira un item cada vez
    // que lo golpean hasta agotar sus golpes (ver dispenserHitCount, mismo
    // criterio de conteo que isQuestionTile/questionHitCount), quedando
    // "usado" para siempre en el ultimo. Mientras no este usado se dibuja
    // como un ladrillo normal (ver renderBlocks); usado, con el mismo frame
    // "vacio" del bloque de interrogacion (Tilemap::kQuestionUsedFrame) --
    // un ladrillo no tiene forma propia de verse "vacio".
    bool isDispenserTile(int col, int row) const;
    bool isDispenserUsed(int col, int row) const;
    void markDispenserUsed(int col, int row);
    int dispenserHitCount(int col, int row) const;
    void incrementDispenserHit(int col, int row);

    // Tuberia ('T', seccion 5.3): siempre solida, nunca se rompe. Se coloca
    // en el JSON como un bloque de 2 columnas de ancho x N filas de alto,
    // sin distinguir tope/cuerpo/izquierda/derecha con el caracter -- estas
    // consultas lo deducen mirando los vecinos:
    //  - isPipeTopTile: la fila de mas arriba de un tramo vertical contiguo
    //    (arriba no hay otra 'T') -- ahi es donde Game::checkPipeWarp
    //    detecta que el personaje esta parado para poder entrar.
    //  - isPipeLeftHalf: existe una 'T' a la derecha (col+1) -- si no, es la
    //    mitad derecha (existe una 'T' a la izquierda). No da resultados
    //    utiles en una tuberia mal formada (ancho != 2), el nivel de prueba
    //    tiene que respetar el par de columnas.
    bool isPipeTile(int col, int row) const;
    bool isPipeTopTile(int col, int row) const;
    bool isPipeLeftHalf(int col, int row) const;

    // Mastil de la bandera de meta ('F', seccion 5.3): no es solido (se
    // atraviesa, lo agarra Game::checkFlagpoleGrab), y lo dibuja la entidad
    // Flagpole, no esta clase. El bloque base de abajo es un '#' comun.
    // findFlagpole devuelve el primer tramo vertical de 'F' del nivel
    // (columna, fila del tope = la bola, ultima fila del mastil); false si
    // el nivel no tiene bandera.
    bool isFlagpoleTile(int col, int row) const;
    bool findFlagpole(int& col, int& topRow, int& bottomRow) const;

    // Separado en dos pasadas (en vez de un solo render()) para que
    // Game.cpp pueda dibujar los items que salen de un bloque JUSTO en el
    // medio (ver el comentario del usuario: el item tiene que quedar
    // tapado por el bloque mientras "emerge", no encima) -- primero el
    // terreno (fondo + tiles solidos + plataformas, renderTerrain), despues
    // los items, despues los bloques (ladrillo/interrogacion, renderBlocks)
    // que quedan arriba de todo tapando lo que haga falta.
    //
    // cameraX: cuanto se desplazo la camara (ver Game::cameraX_) -- se
    // resta a la posicion en X antes de dibujar, mismo criterio que el
    // resto del elenco (entidades/render()).
    void renderTerrain(SDL_Renderer* renderer, SDL_Color backgroundColor, float cameraX = 0.0f) const;

    // brickSprite/worldRow: los tiles de ladrillo se dibujan con sprite
    // propio (una fila por WorldType, mismo criterio que el resto del
    // elenco) en vez del rectangulo plano que usaban en el terreno.
    // hiddenBrickTiles: ladrillos que Game.cpp esta dibujando el sola
    // aparte, desplazados (ver Game::checkBrickBlockHit, sacudida al
    // golpearlo chico sin romperlo) -- esta funcion los salta aca para no
    // dibujarlos dos veces (uno quieto atras y otro desplazado encima).
    // questionSprite/questionAnimFrame: mismo criterio para los bloques de
    // interrogacion sin usar (los usados van siempre en la columna fija
    // kQuestionUsedFrame, sin animar) -- tambien se saltan los que esten en
    // hiddenBrickTiles (mismo pool de sacudida que el ladrillo, ver
    // Game::startBlockBump).
    // coinSprite/coinAnimFrame: para las monedas paradas arriba de un
    // ladrillo (hasCoinOnTop) sin agarrar todavia -- se dibujan un tile mas
    // arriba de su ladrillo, con la misma paleta/animacion que la moneda
    // suelta del piso (Coin.h, ver Game::brickCoinOnTopAnimFrame_).
    void renderBlocks(SDL_Renderer* renderer, const SpriteSheet& brickSprite, int worldRow, float cameraX = 0.0f,
                       const std::vector<SDL_Point>& hiddenBrickTiles = {},
                       const SpriteSheet* questionSprite = nullptr, int questionAnimFrame = 0,
                       const SpriteSheet* coinSprite = nullptr, int coinAnimFrame = 0) const;

    // Tuberia: sprite propio (una fila por WorldType, 2 columnas -- 0 =
    // tope, 1 = cuerpo -- de 32x16 cada una, dos tiles de ancho de una sola
    // vez, ver el comentario de isPipeTile) en vez del rectangulo plano del
    // terreno. Dibuja una sola vez por par de columnas (se salta la mitad
    // derecha, ya se dibujo junto con la izquierda).
    void renderPipes(SDL_Renderer* renderer, const SpriteSheet& pipeSprite, int worldRow, float cameraX = 0.0f) const;

    // Columna fija del frame "usado" en question_block.png (ver el
    // comentario de render()): las primeras kQuestionUsedFrame columnas son
    // la animacion sin usar.
    static constexpr int kQuestionUsedFrame = 3;

private:
    int tileSize_ = 16;
    std::string name_ = "TEST";
    int timeLimit_ = 400;
    std::vector<int> checkpointCols_;
    std::vector<LevelBlock> blocks_;
    std::vector<LevelWarp> warps_;
    std::vector<LevelEntity> entities_;
    std::vector<LevelArea> areas_;
    std::vector<LevelDecoration> decorations_;
    std::vector<uint8_t> hidden_;        // row-major, 1 = bloque invisible sin revelar ('H')
    std::vector<uint8_t> coinTile_;      // row-major, 1 = moneda suelta sin agarrar ('o')
    int width_ = 0;
    int height_ = 0;
    std::vector<uint8_t> solid_;     // row-major, 1 = solido ('#', 'B' sin romper, o '?')
    std::vector<uint8_t> platform_;  // row-major, 1 = plataforma ('=')
    std::vector<uint8_t> brick_;     // row-major, 1 = ladrillo sin romper ('B' o 'C')
    std::vector<uint8_t> everBrick_; // row-major, 1 = fue 'B'/'C' al cargar el nivel (breakTile no lo toca)
    std::vector<uint8_t> topCoin_;   // row-major, 1 = tiene moneda arriba sin agarrar ('C')
    std::vector<uint8_t> question_;      // row-major, 1 = bloque de interrogacion ('?')
    std::vector<uint8_t> questionUsed_;  // row-major, 1 = ya lo golpearon la ultima vez que le tocaba
    std::vector<uint8_t> questionHits_;  // row-major, cuantas veces lo golpearon hasta ahora
    std::vector<uint8_t> dispenser_;     // row-major, 1 = ladrillo dispensador ('D')
    std::vector<uint8_t> dispenserUsed_; // row-major, 1 = ya lo golpearon la ultima vez que le tocaba
    std::vector<uint8_t> dispenserHits_; // row-major, cuantas veces lo golpearon hasta ahora
    std::vector<uint8_t> pipe_;          // row-major, 1 = tuberia ('T')
    std::vector<uint8_t> flagpole_;      // row-major, 1 = mastil de la bandera ('F')
};
