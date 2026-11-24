#include <criterion/criterion.h>

#include "tui/text.h"

Test(tui_text, append_is_all_or_nothing)
{
    char value[8] = "ab";

    cr_assert_eq(apl_tui_text_append(value, sizeof(value), "cd"), 4);
    cr_assert_str_eq(value, "abcd");

    cr_assert_eq(apl_tui_text_append(value, sizeof(value), "efgh"), 4);
    cr_assert_str_eq(value, "abcd");

    cr_assert_eq(apl_tui_text_append(nullptr, 4, "x"), 0);
    cr_assert_eq(apl_tui_text_append(value, 0, "x"), 0);
    cr_assert_eq(apl_tui_text_append(value, sizeof(value), nullptr), 4);
}

Test(tui_text, backspace_removes_complete_utf8_code_points)
{
    char value[16] = "caf\xc3\xa9";

    cr_assert_eq(apl_tui_text_backspace(value), 3);
    cr_assert_str_eq(value, "caf");
    cr_assert_eq(apl_tui_text_backspace(value), 2);
    cr_assert_str_eq(value, "ca");
    cr_assert_eq(apl_tui_text_backspace(nullptr), 0);
}

Test(tui_text, truncation_never_splits_a_sequence)
{
    char value[16] = "a\xc3\xa9z";
    cr_assert_eq(apl_tui_text_truncate_utf8(value, 2), 1);
    cr_assert_str_eq(value, "a");

    char fits[16] = "abc";
    cr_assert_eq(apl_tui_text_truncate_utf8(fits, 3), 3);
    cr_assert_str_eq(fits, "abc");
}

Test(tui_text, case_insensitive_comparison_and_prefix)
{
    cr_assert(apl_tui_text_equal_ci("Ferrari", "fERRARI"));
    cr_assert_not(apl_tui_text_equal_ci("Ferrari", "Ferra"));
    cr_assert(apl_tui_text_starts_with_ci("Team switch", "team"));
    cr_assert_not(apl_tui_text_starts_with_ci("team", "teams"));
    cr_assert_not(apl_tui_text_equal_ci(nullptr, "x"));
    cr_assert(apl_tui_text_equal_ci(nullptr, nullptr));
}
