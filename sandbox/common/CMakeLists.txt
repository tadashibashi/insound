project(insound_sandbox VERSION 0.0.1)

add_library(sandbox_common STATIC common.h common.cpp)
target_link_libraries(sandbox_common PUBLIC insound SDL2::SDL2-static)
target_include_directories(sandbox_common PUBLIC .)

if (NOT ANDROID)
    # Desktop / iOS / WebGL project
    if (WIN32)
        set(EXE_TYPE WIN32)
    elseif(APPLE OR IOS)
        set(EXE_TYPE MACOSX_BUNDLE)
    else()
        set(EXE_TYPE)
    endif()

    # Begin collecting source files
    set(TARGET_SOURCE)

    add_executable(insound_sandbox ${EXE_TYPE})
    list(APPEND TARGET_SOURCE main.cpp)

    target_link_libraries(insound_sandbox PRIVATE sandbox_common SDL2::SDL2main)

    # Mac and iOS bundle
    if (APPLE)
        file(GLOB SANDBOX_RESOURCES  ${CMAKE_CURRENT_SOURCE_DIR}/../assets/*)
        set_source_files_properties(${SANDBOX_RESOURCES} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/assets/")
        list(APPEND TARGET_SOURCE ${SANDBOX_RESOURCES})

        if (CMAKE_BUILD_TYPE MATCHES "Rel")
            set(CODE_SIGN_ID "Apple Development")
        endif()

        set(PRODUCT_NAME                          "Insound Sandbox")
        set(EXECUTABLE_NAME                       ${PROJECT_NAME})
        set(MACOSX_BUNDLE_BUNDLE_NAME             "insound-sandbox")
        set(MACOSX_BUNDLE_INFO_STRING             "insound-sandbox")
        set(MACOSX_BUNDLE_BUNDLE_VERSION          ${PROJECT_VERSION})
        set(MACOSX_BUNDLE_SHORT_VERSION_STRING    ${PROJECT_VERSION})
        set(MACOSX_BUNDLE_LONG_VERSION_STRING     ${PROJECT_VERSION})
        set(MACOSX_BUNDLE_ICON_FILE               "")
        set(MACOSX_BUNDLE_GUI_IDENTIFIER          "org.insound-sandbox.gui")
        set(MACOSX_BUNDLE_COPYRIGHT               "Copyright © 2024")

        if (NOT IOS)
            set(MACOSX_DEPLOYMENT_TARGET 10.15)
        else()
            set(MACOSX_DEPLOYMENT_TARGET 12.0)
        endif()

        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/plist.in ${CMAKE_CURRENT_BINARY_DIR}/Info.plist)
        set_target_properties(insound_sandbox PROPERTIES
            MACOSX_BUNDLE_INFO_PLIST        ${CMAKE_CURRENT_BINARY_DIR}/Info.plist
            BUILD_RPATH                     "@executable_path/../Frameworks"

            XCODE_EMBED_FRAMEWORKS_CODE_SIGN_ON_COPY    YES
            XCODE_EMBED_RESOURCES_CODE_SIGN_ON_COPY     YES
            XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY          "${CODE_SIGN_ID}"
            XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER   "org.insound-sandbox.gui"
            XCODE_ATTRIBUTE_GENERATE_INFOPLIST_FILE     NO
        )

        install(TARGETS ${TARGET_NAME} BUNDLE DESTINATION ${CMAKE_INSTALL_PREFIX})
    endif()

    # Add all collected source files to the target
    target_sources(insound_sandbox PRIVATE ${TARGET_SOURCE})

    # Handle asset directory
    if (EMSCRIPTEN)
        target_link_options(insound_sandbox PRIVATE
            --preload-file ${CMAKE_CURRENT_SOURCE_DIR}/../assets@assets
        )
        set_target_properties(insound_sandbox PROPERTIES
            SUFFIX .html
        )
    else()
        if (NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/assets)
            execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
                ${CMAKE_CURRENT_SOURCE_DIR}/../assets ${CMAKE_CURRENT_BINARY_DIR}/assets)
        endif()
    endif()

    # Emscripten dev server convenience target
    include(add_devserver_target)
    add_devserver_target(insound_sandbox)

endif() # NOT ANDROID
