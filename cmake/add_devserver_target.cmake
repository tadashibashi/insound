
# Add a custom target that serves a provided .html target via `emrun` when platform is "Emscripten" and emrun
# can be found. The added custom target name is "serve_<TARGET_NAME>", and when built runs the emrun server.
#
# It is safe to call this macro when target is not Emscripten - no target will be added in this case
macro(add_devserver_target TARGET_NAME)

    if (EMSCRIPTEN)
        set(SERVE_TARGET serve_${TARGET_NAME})
        find_program(NODE node)
        set(DEVSERVER ${INSOUND_ROOT_DIR}/tools/dev-server/main.js)

        if (NODE)
            add_custom_target(${SERVE_TARGET}
                COMMAND ${NODE} ${DEVSERVER} $<TARGET_FILE:${TARGET_NAME}>
                USES_TERMINAL
            )
            add_dependencies(${SERVE_TARGET} ${TARGET_NAME})
        else()
            message(WARNING "Called `add_devserver_target`, but NodeJS could not be found")
        endif()
    endif()
endmacro()
