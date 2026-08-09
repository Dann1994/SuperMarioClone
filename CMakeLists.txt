cmake_minimum_required(VERSION 3.20)
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

add_executable(mario WIN32 ${MARIO_SOURCES})

target_include_directories(mario PRIVATE src)

target_link_libraries(mario PRIVATE
    SDL2::SDL2main
    SDL2::SDL2
    SDL2_image::SDL2_image
    SDL2_mixer::SDL2_mixer
    SDL2_ttf::SDL2_ttf
    nlohmann_json::nlohmann_json
)

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
