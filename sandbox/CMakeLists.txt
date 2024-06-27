add_subdirectory(common)

set (TARGET_ASSETS_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}/android/app/src/main/assets/assets)
if (NOT EXISTS ${TARGET_ASSETS_FOLDER})
    execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${CMAKE_CURRENT_SOURCE_DIR}/assets ${TARGET_ASSETS_FOLDER})
endif()

