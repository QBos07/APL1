#include <string.h>

#include <criterion/criterion.h>

#include "domain/reference.h"

Test(reference, required_label_is_trimmed)
{
    apl_error err;
    apl_error_init(&err);
    char out[APL_REFERENCE_LABEL_MAX];
    cr_assert_eq(
        apl_reference_label_normalize("  Season 2026  ", "season label", out, sizeof(out), &err),
        APL_OK);
    cr_assert_str_eq(out, "Season 2026");
}

Test(reference, required_label_rejects_blank_and_overlong_values)
{
    apl_error err;
    apl_error_init(&err);
    char out[APL_REFERENCE_LABEL_MAX] = "keep";
    cr_assert_eq(apl_reference_label_normalize("   ", "season label", out, sizeof(out), &err),
                 APL_ERR_INVALID_ARGUMENT);
    cr_assert_neq(strstr(apl_error_message(&err), "required"), nullptr);

    char small[5] = "keep";
    apl_error_init(&err);
    cr_assert_eq(apl_reference_label_normalize("12345", "season label", small, sizeof(small), &err),
                 APL_ERR_INVALID_ARGUMENT);
    cr_assert_neq(strstr(apl_error_message(&err), "too long"), nullptr);
}

Test(reference, optional_text_is_trimmed_and_null_is_empty)
{
    apl_error err;
    apl_error_init(&err);
    char out[APL_REFERENCE_NOTES_MAX] = "keep";
    cr_assert_eq(apl_reference_optional_normalize(nullptr, "notes", out, sizeof(out), &err),
                 APL_OK);
    cr_assert_str_eq(out, "");
    cr_assert_eq(
        apl_reference_optional_normalize("  chassis notes  ", "notes", out, sizeof(out), &err),
        APL_OK);
    cr_assert_str_eq(out, "chassis notes");
}

Test(reference, valid_iso_dates_are_kept_and_optional_dates_may_be_missing)
{
    apl_error err;
    apl_error_init(&err);
    char out[APL_REFERENCE_DATE_MAX];
    cr_assert_eq(apl_reference_date_normalize("2024-02-29", "start date", out, sizeof(out), &err),
                 APL_OK);
    cr_assert_str_eq(out, "2024-02-29");
    cr_assert_eq(apl_reference_date_normalize("  ", "start date", out, sizeof(out), &err), APL_OK);
    cr_assert_str_eq(out, "");
    cr_assert_eq(apl_reference_date_normalize(nullptr, "start date", out, sizeof(out), &err),
                 APL_OK);
    cr_assert_str_eq(out, "");
}

Test(reference, invalid_dates_are_rejected_without_overwriting_output)
{
    const char *const invalid[] = {"2026-13-01", "2026-02-30", "2023-02-29", "26-01-01",
                                   "2026/01/01", "2026-1-1",   "not-a-date"};

    for (size_t index = 0; index < sizeof(invalid) / sizeof(invalid[0]); ++index) {
        apl_error err;
        apl_error_init(&err);
        char out[APL_REFERENCE_DATE_MAX] = "keep";
        cr_assert_eq(
            apl_reference_date_normalize(invalid[index], "start date", out, sizeof(out), &err),
            APL_ERR_INVALID_ARGUMENT);
        cr_assert_str_eq(out, "keep");
    }
}

Test(reference, date_range_rejects_start_after_end_and_allows_missing)
{
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_reference_date_range_validate("2026-01-01", "2026-12-31", "season", &err),
                 APL_OK);
    cr_assert_eq(apl_reference_date_range_validate("", "2026-12-31", "season", &err), APL_OK);
    cr_assert_eq(apl_reference_date_range_validate("2026-01-01", "", "season", &err), APL_OK);
    cr_assert_eq(apl_reference_date_range_validate("2026-12-31", "2026-01-01", "season", &err),
                 APL_ERR_INVALID_ARGUMENT);
    cr_assert_neq(strstr(apl_error_message(&err), "after"), nullptr);
}

Test(reference, membership_start_is_required_and_end_may_be_missing)
{
    apl_error err;
    apl_error_init(&err);
    char start[APL_REFERENCE_DATE_MAX];
    char end[APL_REFERENCE_DATE_MAX];
    cr_assert_eq(apl_membership_dates_normalize("2025-01-15", "", start, sizeof(start), end,
                                                sizeof(end), &err),
                 APL_OK);
    cr_assert_str_eq(start, "2025-01-15");
    cr_assert_str_eq(end, "");

    apl_error_init(&err);
    cr_assert_eq(
        apl_membership_dates_normalize("", "", start, sizeof(start), end, sizeof(end), &err),
        APL_ERR_INVALID_ARGUMENT);
    cr_assert_neq(strstr(apl_error_message(&err), "required"), nullptr);
}

Test(reference, today_minus_days_writes_an_iso_date)
{
    char out[APL_REFERENCE_DATE_MAX];
    apl_reference_date_today_minus_days(30, out, sizeof(out));
    cr_assert_eq(strlen(out), 10);
    cr_assert_eq(out[4], '-');
    cr_assert_eq(out[7], '-');
}
