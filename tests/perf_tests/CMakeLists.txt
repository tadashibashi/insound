project(insound_perf_tests)

add_executable(insound_perf_tests
    main.cpp
)

target_link_libraries(insound_perf_tests insound)
