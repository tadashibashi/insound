include (FetchContent)

FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        4e8d92bf02f7d1c8006a0e7a5ecabd8e62d98502
)
FetchContent_MakeAvailable(Catch2)

add_executable(insound_tests
    main.cpp
    main.cpp
    insound_tests.h
    Pool.test.cpp)

target_link_libraries(insound_tests PRIVATE insound Catch2::Catch2)
