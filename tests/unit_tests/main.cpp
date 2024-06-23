#include <catch2/catch_session.hpp>

int main(int argc, char *argv[])
{
    // setup
    // ...

    auto result = Catch::Session().run(argc, argv);

    // cleanup
    // ...
    return 0;
}