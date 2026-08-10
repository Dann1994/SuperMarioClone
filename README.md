# Super Mario Clone

Clon experimental de Super Mario Bros clásico para Windows, en C++ y SDL2 (sin motor comercial). Especificación completa en `Documentaciones/especificacion_mario_clone.docx`.

## Requisitos

- [MSYS2](https://www.msys2.org/) (entorno MinGW64) con:
  ```
  pacman -S mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja \
            mingw-w64-x86_64-SDL2 mingw-w64-x86_64-SDL2_image mingw-w64-x86_64-SDL2_mixer \
            mingw-w64-x86_64-SDL2_ttf mingw-w64-x86_64-nlohmann-json
  ```

## Compilar y correr

Desde una shell **MinGW64** de MSYS2, parado en la carpeta del proyecto:

```
cmake -S . -B build -G Ninja
cmake --build build
./build/mario.exe
```

El `.exe` se genera junto con sus DLLs y las carpetas `assets/` y `levels/` copiadas automáticamente al lado.

## Estado actual

- Game loop a 60 FPS con timestep fijo (delta time)
- Ventana redimensionable, resolución lógica interna 256x240 escalada
- Input de teclado con perfiles separados por jugador (`ControlProfile::Player1/Player2`), con buffer de "apretar" para no perder inputs entre pasos fijos de física
- Mario chico: movimiento, fricción/patinada, gravedad, salto de altura variable, correr (botón dedicado), colisión "moderna" contra tilemap de prueba (ver "Selector de escenario" más abajo)
- Spritesheets reales de Mario y Luigi, chico/grande/fuego, recortados de la hoja de referencia (`assets/sprites/{mario,luigi}_{small,big,fire}.png`): quieto/caminar/patinar/saltar en uso; agachado/trepar mástil/nadar/morir ya recortados para cuando se programen esas mecánicas
- Prototipo de Luigi como segundo personaje jugable simultáneo (mismo código de física que Mario, sprite y controles propios). Luigi fuego usa paleta propia (verde en vez de rojo), no la del original que comparte con Mario
- Cambio de tamaño (chico/grande/fuego) con hitbox dinámico manteniendo los pies fijos (`Mario::setSize`); por ahora se prueba con teclas de debug (F1 Mario, F2 Luigi) hasta tener hongo/flor de fuego reales
- Mario/Luigi fuego pueden tirar bolas de fuego (botón de correr tocado, no mantenido): rebotan con parábola baja, explotan en 3 fases al chocar contra un bloque, desaparecen al salir de pantalla o caer en un pozo, máximo 2 en pantalla por personaje. Pose de tirar compuesta en dos capas (torso propio + piernas del frame de movimiento actual), replicando la técnica del juego original
- Estrella (invencibilidad): parpadeo entre 4 paletas (original + 3 variantes) por cada tipo de mundo (overworld/underwater/underground/castle), recortadas de la sección "Star Colors" de la hoja de referencia (`assets/sprites/*_star.png`, generados con `gen_star.py`)
- Tipo de mundo (`WorldType`, sección 5.5): ciclo Overworld → Underwater → Underground → Castle, cada uno con su color de fondo. Underwater ya tiene física propia (nado en vez de salto, velocidad reducida, sin dash); Underground/Castle todavía se comportan como Overworld, pendientes de sus mecánicas
- Colisión Mario↔Luigi (prototipo sin base en el juego original, que no tiene co-op simultáneo): impulso de colisión 1D real (conservación de momento + restitución) usando masa (chico vs grande/fuego pesan distinto) y velocidad real de cada uno — correr pega más fuerte solo, sin casos especiales. Piso mínimo de velocidad de choque para que no queden "pegados" resbalando si se sostiene la tecla contra el otro. Pisada con rebote (más alto si se mantiene salto) que empuja al que salta hacia el lado que mira y tiene cooldown para no rebotar en cadena. El que recibe el golpe se voltea a mirar al otro
- Secuencia de daño clásica (grande/fuego → chico): congela el juego ~2s parpadeando entre el sprite grande y el chico (con frames invisibles intercalados), después queda ~2s invulnerable parpadeando.
- Secuencia de muerte (chico): salto chico y caída con la pose de "muerto" fija. En **modo 2 jugadores** no pausa nada — cada uno cae en su propio `update()`, el que murió reaparece cerca del centro de pantalla (buscando la plataforma sólida más próxima, evitando pozos) invulnerable parpadeando, mientras el otro sigue jugando; solo se congela todo si mueren los dos a la vez (placeholder de "reiniciar desde el checkpoint" hasta que ese sistema exista). En **modo 1 jugador**, Mario es el único jugador, así que su muerte sola ya cuenta como "murieron todos" y también congela+reaparece en el checkpoint.
- Power-ups (hongo, flor de fuego, estrella, moneda) recortados de `Documentaciones/Objetos.png`/`Documentaciones/Tiles.png`, cada uno con su propia paleta según el tipo de mundo actual (mismo criterio que la estrella de invencibilidad). Sin bloques de interrogación todavía, arrancan puestos en el nivel de prueba. La flor de fuego es una decisión propia del clon (no el original): si la agarra chico actúa como el hongo (crece a grande), si la agarra grande pasa a fuego con un parpadeo rápido de paleta entre la forma actual y la de fuego (sobre el mismo frame/pose que estaba jugando, no fuerza quieto) — igual que la secuencia de crecer chico→grande, ahora generalizada para cualquier transición de forma
- Popup de puntaje "1000" al agarrar hongo/flor/estrella (no en la moneda, como el original): sube flotando y desaparece. Usa la fuente bitmap propia (`DebugFont`) en vez de un sprite recortado — el número real en la hoja de referencia es demasiado chico para recortar limpio a mano
- Agachado (grande/fuego, parado, no bajo el agua): nueva `Action::Down` (S para Mario, flecha abajo para Luigi), frena el movimiento horizontal mientras se sostiene
- Modo 1 jugador / 2 jugadores simultáneo, togglable por debug (ver tabla abajo) — difieren principalmente en el manejo de la muerte (ver arriba). Arranca en 1 jugador (solo Mario) por defecto, para agilizar las pruebas
- Selector de escenario de prueba al arrancar (pantalla propia con la fuente bitmap `DebugFont`, `1` o `2` para elegir): separa items y enemigos en niveles distintos para no amontonar todo en un mismo escenario mientras no existe el editor de niveles (sección 7). ESC desde el juego vuelve al selector; ESC en el selector cierra el juego
  - **Items** (`levels/world1/test_items.json`): cada power-up en su propia plataforma bien separada, sin enemigos
  - **Enemigos** (`levels/world1/test_enemies.json`): nivel con paredes en los dos bordes (para que los enemigos que rebotan contra pared no se salgan por un borde sin pared) y los enemigos ya programados
- Primer enemigo: Goomba (sección 5.2), solo en el escenario de enemigos. Camina en línea recta, rebota al chocar una pared, cae por gravedad (misma física que el Mushroom, sin miedo a los bordes). De costado hace daño. Dos muertes distintas según cómo lo maten:
  - Pisotón: se achata (frame "squished" propio, recortado de `Documentaciones/Enemigos.png`) y desaparece solo después de un rato, quieto en el piso
  - Con estrella (invencibilidad) o bola de fuego: cualquier contacto lo elimina de un toque (con fuego, la bola explota ahí mismo en vez de seguir de largo), sin pisotón ni daño para el personaje. Se da vuelta (sprite dibujado boca abajo, `SpriteSheet::draw` ahora soporta flip vertical además del horizontal) y sale disparado en parábola — empuje horizontal constante hacia el lado contrario de donde vino el golpe, más un salto inicial que la gravedad frena y revierte, como una "U" invertida — sin colisionar con nada (ni paredes ni piso) hasta salir de la pantalla, recién ahí desaparece
- Segundo enemigo: Koopa Troopa verde (sección 5.2), solo en el escenario de enemigos, dos instancias de prueba para poder probar caparazón contra caparazón. Misma física de caminata que el Goomba (rebota contra pared, no le teme a los bordes). Tres estados (`KoopaState`):
  - **Caminando**: pisotón lo esconde en el caparazón (rebote normal); de costado hace daño
  - **Caparazón quieto**: pisotón *o* costado lo patean por igual — de costado, hacia el lado contrario a quien lo tocó; de arriba, hacia donde mira el personaje (un pisotón no tiene un "lado" natural). Pisar para patearlo no da rebote (para no relanzar sin querer al que lo pisó); en cambio si nadie lo toca en 8s, sale solo y retoma la caminata
  - **Caparazón en movimiento** (más rápido que caminando): mata a cualquier otro enemigo que toque y sigue de largo, salvo que choque con *otro* caparazón en movimiento — ahí mueren los dos. Pisarlo lo frena (rebote más alto que el resto de los pisotones) y de costado hace daño — para distinguir pisotón de daño acá se usa una regla estricta por geometría de hitbox en vez de velocidad: solo cuenta como pisotón si los pies del personaje quedan en la mitad de arriba del hitbox del caparazón (como si le pegara a la altura de las piernas); si se hunde más que eso (le pega "del pecho para arriba"), es daño
  - Al frenarse o patearse, el personaje que lo pisó/pateó queda inmune a ESE caparazón un instante (0.4s, `Koopa::isImmuneTo`) — necesario porque puede terminar parado encima sin haber rebotado (pisar un caparazón quieto), y sin ese respiro el contacto que sigue lo termina dañando. La inmunidad es puntual: el otro jugador (2P) y los demás enemigos siguen interactuando con el caparazón normal, incluyendo poder matarlos
  - Estrella o fuego lo matan en cualquier estado igual que al Goomba: se esconde en el caparazón (frame `shell1` quieto) en vez del flip vertical que usa el Goomba por no tener arte propio
  - Dos enemigos "no letales" que se cruzan (caminando, o con un caparazón quieto de por medio) rebotan entre sí como si chocaran una pared, en vez de atravesarse — mismo criterio pensado para repetir con los próximos enemigos (`Game::checkGoombaKoopaBounce`/`checkKoopaKoopaBounce`)
  
  Paleta propia por tipo de mundo, mismo criterio que el resto de los items (F7 la cicla)

Desarrollo incremental siguiendo la sección 12 de la especificación: un elemento del elenco a la vez.

## Teclas de debug

Todas provisorias, para probar mecánicas antes de tener sus disparadores reales (power-ups, enemigos, menú). Las marcadas *(Items)* / *(Enemigos)* solo hacen algo en su escenario de prueba correspondiente (ver "Selector de escenario" arriba):

| Tecla | Efecto |
|---|---|
| F1 / F2 | Cicla tamaño de Mario / Luigi (chico→grande→fuego) |
| F3 | Muestra/oculta el overlay de debug (FPS y tipo de mundo actual) |
| F4 *(Items)* | Repone la estrella de prueba |
| F5 / F6 | Activa/desactiva estrella (invencibilidad) en Mario / Luigi |
| F7 | Cicla tipo de mundo (Overworld/Underwater/Underground/Castle) |
| F8 | Activa/desactiva la colisión Mario↔Luigi |
| F9 / F10 | Fuerza la secuencia de daño (o muerte, si es chico) en Mario / Luigi |
| F11 / F12 *(Items)* | Repone el hongo / la flor de fuego de prueba |
| C *(Items)* | Repone la moneda de prueba |
| G *(Enemigos)* | Repone al Goomba de prueba |
| K *(Enemigos)* | Repone los dos Koopa de prueba |
| M | Cambia entre modo 1 jugador y 2 jugadores simultáneo |
| ESC | Vuelve al selector de escenario (o cierra el juego si ya estás ahí) |

## Ideas anotadas para más adelante

No estaban en la especificación original, pero surgieron probando el prototipo y vale la pena no perderlas de vista:

- **Modo cooperativo simultáneo**: Mario y Luigi en pantalla al mismo tiempo. Ya andando (togglable con `M`, ver `twoPlayerMode_` en `src/core/Game.h`), incluyendo colisión entre los dos y manejo de muerte propio. Falta: selección de modo desde un menú real (por ahora es debug), cámara que siga a ambos.
- **Modo clásico de dos jugadores por turnos**: como el Mario original — un jugador a la vez en pantalla, se alternan turno por turno (vida/intento) en vez de jugar simultáneo. Todavía no empezado (el modo 1 jugador actual es solo Mario solo, sin alternancia con Luigi).

Falta definir cómo se elige el modo (probablemente desde el menú principal, sección 10.1 de la especificación, una vez que exista).

## Scripts de assets

`tools/asset_scripts/gen_star.py` (necesita Pillow) genera `assets/sprites/*_star.png` a partir del sheet base correspondiente: detecta los 3 colores del sprite por frecuencia de píxeles (no hace falta indicarle el RGB de cada personaje a mano) y arma un sheet combinado con los 4 tipos de mundo × 3 paletas cada uno (12 bloques de filas, mismo orden que `WorldType` en `src/world/WorldType.h`). Correrlo de nuevo con el mismo comando si se recorta un sheet base nuevo o se le agregan filas:

```
python tools/asset_scripts/gen_star.py assets/sprites/<nombre>.png assets/sprites/<nombre>_star.png
```
