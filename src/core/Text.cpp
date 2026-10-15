#include "Text.h"

#include <array>
#include <cstddef>

namespace {

bool gEnglish = false;

struct Entry {
    const char* es;
    const char* en;
};

// Mismo orden que Txt.
constexpr std::array<Entry, static_cast<size_t>(Txt::Count)> kTexts{{
    {"1 JUGADOR", "1 PLAYER GAME"},
    {"2 JUGADORES", "2 PLAYER GAME"},
    {"CONTINUAR", "CONTINUE"},
    {"ELEGIR MUNDO", "WORLD SELECT"},
    {"CONFIGURACION", "OPTIONS"},
    {"MODO PRUEBA", "TEST MODE"},
    {"CREDITOS", "CREDITS"},
    {"SALIR", "QUIT"},

    {"PAUSA", "PAUSED"},
    {"REANUDAR", "RESUME"},
    {"REINICIAR NIVEL", "RESTART LEVEL"},
    {"SALIR AL MENU", "QUIT TO MENU"},

    {"PANTALLA", "DISPLAY"},
    {"VENTANA", "WINDOWED"},
    {"COMPLETA", "FULLSCREEN"},
    {"TAMANO DE VENTANA", "WINDOW SIZE"},
    {"AUTO", "AUTO"},
    {"PROPORCION", "ASPECT RATIO"},
    {"MOSTRAR FPS", "SHOW FPS"},
    {"VOLUMEN GENERAL", "MASTER VOLUME"},
    {"VOLUMEN MUSICA", "MUSIC VOLUME"},
    {"VOLUMEN EFECTOS", "SFX VOLUME"},
    {"IDIOMA", "LANGUAGE"},
    {"ESPANOL", "ENGLISH"},
    {"VIBRACION DEL MANDO", "GAMEPAD RUMBLE"},
    {"CONTROLES", "CONTROLS"},
    {"VOLVER", "BACK"},
    {"SI", "ON"},
    {"NO", "OFF"},

    {"JUGADOR 1 (TECLADO)", "PLAYER 1 (KEYBOARD)"},
    {"JUGADOR 2 (TECLADO)", "PLAYER 2 (KEYBOARD)"},
    {"MANDO (LOS DOS)", "GAMEPAD (BOTH)"},
    {"IZQUIERDA", "LEFT"},
    {"DERECHA", "RIGHT"},
    {"SALTAR", "JUMP"},
    {"CORRER / FUEGO", "RUN / FIRE"},
    {"ABAJO", "DOWN"},
    {"APRETA UNA TECLA (ESC CANCELA)", "PRESS A KEY (ESC CANCELS)"},
    {"APRETA UN BOTON DEL MANDO (ESC CANCELA)", "PRESS A GAMEPAD BUTTON (ESC CANCELS)"},
    {"RESTAURAR DE FABRICA", "RESET TO DEFAULTS"},

    {"NIVEL COMPLETO", "COURSE CLEAR"},
    {"FIN DE LA DEMO", "END OF DEMO"},
    {"GRACIAS POR JUGAR", "THANK YOU FOR PLAYING"},
    {"ENTER / A: ELEGIR    ESC / B: VOLVER", "ENTER / A: SELECT    ESC / B: BACK"},
    {"< > : CAMBIAR VALOR", "< > : CHANGE VALUE"},
    {"ENTER / A: CAMBIAR    ESC / B: VOLVER", "ENTER / A: CHANGE    ESC / B: BACK"},
    {"(PROXIMAMENTE)", "(COMING SOON)"},

    {"GRACIAS MARIO!", "THANK YOU MARIO!"},
    {"PERO NUESTRA PRINCESA ESTA", "BUT OUR PRINCESS IS IN"},
    {"EN OTRO CASTILLO!", "ANOTHER CASTLE!"},
    {"TU AVENTURA TERMINO.", "YOUR QUEST IS OVER."},
    {"TE DAMOS UNA NUEVA AVENTURA!", "WE PRESENT YOU A NEW QUEST."},
    {"FIN", "THE END"},

    {"CON QUIEN JUGAS?", "CHOOSE PLAYER"},
    {"GRACIAS LUIGI!", "THANK YOU LUIGI!"},
    {"ELEGI EL NIVEL", "SELECT LEVEL"},
    {"ARRIBA", "UP"},
    {"BIENVENIDO A ZONA WARP!", "WELCOME TO WARP ZONE!"},
    {"SEGUNDA VUELTA", "SECOND QUEST"},
    {"COMO JUGAR?", "HOW TO PLAY?"},
    {"A LA VEZ", "TOGETHER"},
    {"POR TURNOS", "TAKE TURNS"},
    {"MUNDO", "WORLD"},
    {"TIEMPO", "TIME"},
    {"CHOQUE ENTRE JUGADORES", "PLAYERS BUMP"},
    {"JUGAR", "PLAY"},
    {"VOLVER", "BACK"},
    {"EXTRAS", "EXTRAS"},
    {"EDITOR DE NIVELES", "LEVEL EDITOR"},
    {"CREAR", "CREATE"},
    {"SELECCIONAR", "SELECT"},
    {"PARTIDA +", "NEW GAME +"},
    {"OPCIONES EXTRA", "EXTRA OPTIONS"},
    {"APRETA EL BOTON B", "PUSH BUTTON B"},
    {"PARA ELEGIR UN MUNDO", "TO SELECT A WORLD"},
    {"ELEGIR PARTIDA", "CHOOSE A FILE"},
    {"PARTIDA", "FILE"},
    {"VACIA", "EMPTY"},
    {"SOBRESCRIBIR", "OVERWRITE"},
    {"ESTA PARTIDA?", "THIS FILE?"},
    {"2 A LA VEZ", "2 TOGETHER"},
    {"2 POR TURNOS", "2 TAKING TURNS"},
    {"ESTA SEGURO QUE QUIERE", "ARE YOU SURE YOU WANT"},
    {"CERRAR EL JUEGO?", "TO QUIT THE GAME?"},
    {"SI", "YES"},
    {"NO", "NO"},
}};

}  // namespace

void setEnglish(bool english) { gEnglish = english; }

const char* tr(Txt id) {
    const Entry& entry = kTexts[static_cast<size_t>(id)];
    return gEnglish ? entry.en : entry.es;
}
