cmake_minimum_required(VERSION 3.21)
project(SuperMarioClone CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(SDL2_mixer REQUIRED)
find_package(SDL2_ttf REQUIRED)
find_package(nlohmann_json REQUIRED)

file(GLOB_RECURSE MARIO_SOURCES CONFIGURE_DEPENDS src/*.cpp)
# El editor de niveles va tambien adentro del juego (EXTRAS > EDITOR DE
# NIVELES, solo con los niveles del jugador); su main es el del ejecutable
# aparte (ver mas abajo), que edita todos.
list(FILTER MARIO_SOURCES EXCLUDE REGEX ".*/src/editor/editor_main\\.cpp")

add_executable(mario WIN32 ${MARIO_SOURCES})

# Icono del .exe (la cara de Mario): recurso de Windows.
if(WIN32)
    enable_language(RC)
    target_sources(mario PRIVATE resources/app.rc)
endif()

target_include_directories(mario PRIVATE src)

target_link_libraries(mario PRIVATE
    SDL2::SDL2main
    SDL2::SDL2
    SDL2_image::SDL2_image
    SDL2_mixer::SDL2_mixer
    SDL2_ttf::SDL2_ttf
    nlohmann_json::nlohmann_json
)
# Modo en red y actualizaciones (src/net, los de Battle City): sockets UDP de
# Windows, las direcciones de la PC (UPnP / codigo de sala) y WinINet para
# preguntarle a GitHub por versiones nuevas.
if(WIN32)
    target_link_libraries(mario PRIVATE ws2_32 iphlpapi wininet)
endif()

# Copiar las DLL de tiempo de ejecucion (MinGW) junto al ejecutable para poder correrlo directo.
if(WIN32 AND MINGW)
    set(_dll_dir ${CMAKE_CXX_COMPILER})
    get_filename_component(_dll_dir ${_dll_dir} DIRECTORY)
    add_custom_command(TARGET mario POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL2::SDL2>
            $<TARGET_FILE:SDL2_image::SDL2_image>
            $<TARGET_FILE:SDL2_mixer::SDL2_mixer>
            $<TARGET_FILE:SDL2_ttf::SDL2_ttf>
            $<TARGET_FILE_DIR:mario>
    )
    # SDL2_image/mixer/ttf traen sus propias dependencias opcionales
    # (avif, webp, tiff, mp3, ogg, freetype, etc.) que no estan linkeadas
    # directo contra mario y por lo tanto no las cubre el copy de arriba.
    # Sin esto el .exe corre bien desde una shell de MSYS2 (que tiene
    # mingw64/bin en el PATH) pero falla al abrirlo directo en Windows.
    add_custom_command(TARGET mario POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DTARGET_EXE=$<TARGET_FILE:mario>
            -DOUT_DIR=$<TARGET_FILE_DIR:mario>
            -DSEARCH_DIR=${_dll_dir}
            -P ${CMAKE_SOURCE_DIR}/cmake/copy_runtime_deps.cmake
    )
endif()

# Copiar assets y niveles junto al ejecutable para poder correrlo desde build/.
# Target aparte (no POST_BUILD de mario): si solo cambia un asset y el codigo
# no recompila, un POST_BUILD del target mario no se vuelve a ejecutar porque
# ninja lo considera al dia. Este target "ALL" sin OUTPUT se revisa siempre.
add_custom_target(copy_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:mario>/assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/levels $<TARGET_FILE_DIR:mario>/levels
)
add_dependencies(mario copy_assets)

# Editor de niveles (seccion 7 de la especificacion): herramienta de
# desarrollo, ejecutable aparte que NO va en el build final -- configurar con
# -DMARIO_BUILD_EDITOR=OFF para no compilarlo. Comparte con el juego el
# dibujo del nivel (src/world, src/render) y lee/guarda los niveles
# directamente en la carpeta levels/ del proyecto (MARIO_SOURCE_DIR), no en
# la copia de build/.
option(MARIO_BUILD_EDITOR "Compilar el editor de niveles (herramienta de desarrollo)" ON)
if(MARIO_BUILD_EDITOR)
    file(GLOB EDITOR_SOURCES CONFIGURE_DEPENDS src/editor/*.cpp src/world/*.cpp src/render/*.cpp)
    add_executable(mario_editor WIN32 ${EDITOR_SOURCES})
    target_include_directories(mario_editor PRIVATE src)
    target_link_libraries(mario_editor PRIVATE
        SDL2::SDL2main
        SDL2::SDL2
        SDL2_image::SDL2_image
        nlohmann_json::nlohmann_json
    )
    target_compile_definitions(mario_editor PRIVATE MARIO_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    # Mismo directorio de salida que mario: usa los assets y DLLs que copia
    # ese target, y F5 lanza mario.exe desde ahi.
    add_dependencies(mario_editor mario copy_assets)
endif()
