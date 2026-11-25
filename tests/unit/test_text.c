#include <string.h>

#include <criterion/criterion.h>

#include "core/text.h"

Test(text, copies_a_bounded_string)
{
    char out[8] = {};

    apl_text_copy(out, sizeof(out), "hello");

    cr_assert_str_eq(out, "hello");
}

Test(text, null_source_becomes_empty)
{
    char out[8];
    memset(out, 'x', sizeof(out));

    apl_text_copy(out, sizeof(out), nullptr);

    cr_assert_str_empty(out);
}

Test(text, truncates_to_fit)
{
    char out[5] = {};

    apl_text_copy(out, sizeof(out), "hello");

    cr_assert_str_eq(out, "hell");
}

Test(text, unusable_destination_is_a_noop)
{
    apl_text_copy(nullptr, 0, "ignored");
}
