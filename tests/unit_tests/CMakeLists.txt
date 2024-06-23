project(insound_tests)

# Get Catch2
if (NOT TARGET CATCH2::Catch2)
    include (FetchContent)

    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        4e8d92bf02f7d1c8006a0e7a5ecabd8e62d98502
    )

    # If Emscripten platform and the rest of your application is using pthreads / SharedArrayBuffers,
    # You must compile Catch2 with -pthread also. If you are providing your own Catch2, please make sure to do this.
    if (EMSCRIPTEN AND NOT INSOUND_NO_PTHREAD)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
    endif()
    FetchContent_MakeAvailable(Catch2)
endif()

add_executable(insound_tests
    main.cpp
    Pool.test.cpp)

target_link_libraries(insound_tests PRIVATE insound Catch2::Catch2)

