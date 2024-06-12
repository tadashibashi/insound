#include "insound_tests.h"
#include <insound/Pool.h>

TEST_CASE("Resizable Pool tests")
{
    SECTION("Pool can allocate simple data types, check size")
    {
        Pool pool(256, sizeof(int), alignof(int));
        REQUIRE(pool.maxSize() == 256);

        auto id = pool.allocate();
        new (pool.get(id)) int(5);
        // REQUIRE(pool.size() == 1);
        // REQUIRE(!pool.empty());


        REQUIRE(pool.isValid(id));
        REQUIRE(*(int *)pool.get(id) == 5);

        pool.deallocate(id);
        REQUIRE(!pool.isValid(id));
        // REQUIRE(pool.empty());
        // REQUIRE(pool.size() == 0);
    }

    SECTION("Pool expands after allocations exceed max_size")
    {
        Pool pool(256, sizeof(int), alignof(int));

        for (int i = 0; i < 256; ++i)
        {
            pool.allocate();
        }

        // REQUIRE(pool.full());
        REQUIRE(pool.maxSize() == 256);

        pool.allocate();
        // REQUIRE(!pool.full());
        REQUIRE(pool.maxSize() > 256);
    }
}