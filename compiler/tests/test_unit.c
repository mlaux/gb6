#include "tests.h"

TEST(test_nop_ret)
{

}

void register_unit_tests(void)
{
    printf("\nUnit tests:\n");
    RUN_TEST(test_nop_ret);
}
