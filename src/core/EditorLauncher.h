#pragma once

// Abre el editor de niveles (mario_editor.exe, al lado del juego) como otro
// proceso, desde EXTRAS > EDITOR DE NIVELES del menu principal.
namespace EditorLauncher {

// Hay editor al lado del juego? (el build final puede no traerlo).
bool available();
// pickLevel: arranca en la lista de niveles (SELECCIONAR); si no, con un
// nivel nuevo (CREAR). Devuelve false si no se pudo abrir.
bool launch(bool pickLevel);
// El editor abierto desde el juego sigue corriendo?
bool running();

}  // namespace EditorLauncher
