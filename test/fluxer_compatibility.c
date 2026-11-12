#include "concord-error.h"
#include "greatest.h"

TEST
unsupported_code_has_description(void)
{
    ASSERT_STR_EQ("CCORD_UNSUPPORTED", ccord_code_as_string(-15));
    ASSERT_STR_EQ("Failure: This operation is not supported by the current service",
                  ccord_strerror(-15));
    PASS();
}

TEST
unsupported_code_is_distinct(void)
{
    ASSERT_NEQ(CCORD_HTTP_CODE, -15);
    ASSERT_NEQ(CCORD_BAD_DECODE, -15);
    ASSERT_NEQ(CCORD_RESOURCE_UNAVAILABLE, -15);
    PASS();
}

GREATEST_MAIN_DEFS();

int
main(int argc, char *argv[])
{
    GREATEST_MAIN_BEGIN();

    RUN_TEST(unsupported_code_has_description);
    RUN_TEST(unsupported_code_is_distinct);

    GREATEST_MAIN_END();
}
