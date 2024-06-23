#include <catch2/catch_test_macros.hpp>
#include <insound/core.h>

using namespace insound;

TEST_CASE("Resizable Pool tests")
{
    SECTION("Pool can allocate simple data types, check size")
    {
        Pool<int> pool(256);

        REQUIRE(pool.maxSize() == 256);

        auto id = pool.allocate();
        new (pool.get(id)) int(5);

        REQUIRE(pool.isValid(id));
        REQUIRE(*(int *)pool.get(id) == 5);

        pool.deallocate(id);
        REQUIRE(!pool.isValid(id));
    }

    SECTION("Pool expands after allocations exceed max_size")
    {
        Pool<int> pool(256);

        for (int i = 0; i < 256; ++i)
        {
            pool.allocate();
        }

        REQUIRE(pool.maxSize() == 256);

        pool.allocate();
        REQUIRE(pool.maxSize() > 256);
    }
}
