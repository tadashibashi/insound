add_executable(insound_sandbox main.cpp)

target_link_libraries(insound_sandbox PRIVATE insound SDL2::SDL2)

if (EMSCRIPTEN)
    target_link_options(insound_sandbox PRIVATE
        --preload-file ${CMAKE_CURRENT_SOURCE_DIR}/assets@assets
    )
    set_target_properties(insound_sandbox PROPERTIES
        SUFFIX .html
    )
else()
    if (NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/assets)
        execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
            ${CMAKE_CURRENT_SOURCE_DIR}/assets ${CMAKE_CURRENT_BINARY_DIR}/assets)

    endif()
endif()

include(add_emrun_target)
add_emrun_target(insound_sandbox)
