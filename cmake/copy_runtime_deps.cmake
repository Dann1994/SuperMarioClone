# Copia al directorio del ejecutable todas las DLL de MinGW de las que
# depende (transitivamente) TARGET_EXE, para poder correrlo fuera de una
# shell de MSYS2. Se corre como POST_BUILD porque necesita el .exe ya
# linkeado (y sus DLL directas ya copiadas) para poder resolver el arbol
# completo de dependencias.
if(NOT DEFINED TARGET_EXE OR NOT DEFINED OUT_DIR OR NOT DEFINED SEARCH_DIR)
    message(FATAL_ERROR "TARGET_EXE, OUT_DIR y SEARCH_DIR son obligatorios")
endif()

cmake_policy(SET CMP0207 NEW)

file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES ${TARGET_EXE}
    RESOLVED_DEPENDENCIES_VAR resolved_deps
    UNRESOLVED_DEPENDENCIES_VAR unresolved_deps
    DIRECTORIES ${SEARCH_DIR} ${OUT_DIR}
    PRE_EXCLUDE_REGEXES "api-ms-" "ext-ms-"
    POST_EXCLUDE_REGEXES ".*system32.*" ".*SysWOW64.*" ".*WinSxS.*"
)

foreach(dep ${resolved_deps})
    get_filename_component(dep_dir "${dep}" DIRECTORY)
    if(dep_dir STREQUAL SEARCH_DIR)
        get_filename_component(dep_name "${dep}" NAME)
        if(NOT EXISTS "${OUT_DIR}/${dep_name}")
            file(COPY "${dep}" DESTINATION "${OUT_DIR}")
        endif()
    endif()
endforeach()

if(unresolved_deps)
    message(WARNING "copy_runtime_deps: dependencias no resueltas: ${unresolved_deps}")
endif()
