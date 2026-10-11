#include "LevelEditor.h"

// Editor de niveles (seccion 7): mario_editor.exe [archivo.json]
// Sin argumento arranca con un nivel nuevo; Ctrl+O abre cualquiera de
// levels/. Herramienta de desarrollo: no va en el build final (ver la
// opcion MARIO_BUILD_EDITOR en CMakeLists.txt).
int main(int argc, char** argv) {
    LevelEditor editor;
    if (!editor.init()) {
        return 1;
    }
    if (argc > 1) editor.openFile(argv[1]);
    editor.run();
    editor.shutdown();
    return 0;
}
