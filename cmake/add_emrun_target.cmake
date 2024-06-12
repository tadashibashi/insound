
# Add a custom target that serves a provided .html target via `emrun` when platform is "Emscripten" and emrun
# can be found. The added custom target name is "serve_<TARGET_NAME>", and when built runs the emrun server.
#
# It is safe to call this macro when target is not Emscripten - no target will be added in this case
macro(add_emrun_target TARGET_NAME)

    if (EMSCRIPTEN)
        find_program(EMRUN emrun)
        set(SERVE_TARGET serve_${TARGET_NAME})

        if (EMRUN)
            add_custom_target(${SERVE_TARGET}
                COMMAND ${EMRUN} $<TARGET_FILE:${TARGET_NAME}>
                USES_TERMINAL
            )
            add_dependencies(${SERVE_TARGET} ${TARGET_NAME})
        else()
            message(WARNING "Called add_emrun_target, but the emrun executable could not be found")
        endif()
    endif()

endmacro()
