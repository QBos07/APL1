#include <criterion/criterion.h>

#include "domain/ledger.h"

Test(ledger, kinds_have_stable_codes_and_round_trip)
{
    cr_assert_str_eq(apl_ledger_kind_code(APL_LEDGER_KIND_OPENING_BALANCE), "opening_balance");
    cr_assert_str_eq(apl_ledger_kind_code(APL_LEDGER_KIND_INCOME), "income");
    cr_assert_str_eq(apl_ledger_kind_code(APL_LEDGER_KIND_EXPENSE), "expense");
    cr_assert_str_eq(apl_ledger_kind_code(APL_LEDGER_KIND_REVERSAL), "reversal");
    cr_assert_str_eq(apl_ledger_kind_code(APL_LEDGER_KIND_ADJUSTMENT), "adjustment");
    for (int kind = APL_LEDGER_KIND_OPENING_BALANCE; kind <= APL_LEDGER_KIND_ADJUSTMENT; ++kind) {
        const apl_ledger_kind value = (apl_ledger_kind)kind;
        cr_assert_eq(apl_ledger_kind_from_code(apl_ledger_kind_code(value)), value);
    }
    cr_assert_eq(apl_ledger_kind_from_code("bogus"), APL_LEDGER_KIND_NONE);
    cr_assert_eq(apl_ledger_kind_from_code(nullptr), APL_LEDGER_KIND_NONE);
    cr_assert(apl_ledger_kind_is_postable(APL_LEDGER_KIND_OPENING_BALANCE));
    cr_assert(apl_ledger_kind_is_postable(APL_LEDGER_KIND_INCOME));
    cr_assert(apl_ledger_kind_is_postable(APL_LEDGER_KIND_EXPENSE));
    cr_assert_not(apl_ledger_kind_is_postable(APL_LEDGER_KIND_REVERSAL));
    cr_assert_not(apl_ledger_kind_is_postable(APL_LEDGER_KIND_ADJUSTMENT));
}

Test(ledger, category_keeps_display_case_and_normalizes_grouping_key)
{
    apl_error err;
    apl_error_init(&err);
    char display[APL_LEDGER_CATEGORY_MAX];
    char key[APL_LEDGER_CATEGORY_MAX];

    cr_assert_eq(apl_ledger_category_normalize("  Tyres   and  Wheels  ", display, sizeof(display),
                                               key, sizeof(key), &err),
                 APL_OK);
    cr_assert_str_eq(display, "Tyres and Wheels");
    cr_assert_str_eq(key, "tyres and wheels");

    cr_assert_eq(
        apl_ledger_category_normalize("   ", display, sizeof(display), key, sizeof(key), &err),
        APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_error_field_id(&err), APL_FIELD_CATEGORY);
    cr_assert_eq(
        apl_ledger_category_normalize(nullptr, display, sizeof(display), key, sizeof(key), &err),
        APL_ERR_INVALID_ARGUMENT);
}

Test(ledger, category_rejects_overlong_values_without_truncation)
{
    apl_error err;
    apl_error_init(&err);
    char display[APL_LEDGER_CATEGORY_MAX];
    char key[APL_LEDGER_CATEGORY_MAX];
    char text[APL_LEDGER_CATEGORY_MAX + 8];
    for (size_t index = 0; index < sizeof(text) - 1; ++index) {
        text[index] = 'a';
    }
    text[sizeof(text) - 1] = '\0';

    cr_assert_eq(
        apl_ledger_category_normalize(text, display, sizeof(display), key, sizeof(key), &err),
        APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_error_field_id(&err), APL_FIELD_CATEGORY);
}

Test(ledger, dates_and_descriptions_are_required_and_validated)
{
    apl_error err;
    apl_error_init(&err);
    char date[APL_REFERENCE_DATE_MAX];
    char description[APL_LEDGER_DESCRIPTION_MAX];

    cr_assert_eq(apl_ledger_date_normalize("2026-02-28", date, sizeof(date), &err), APL_OK);
    cr_assert_str_eq(date, "2026-02-28");
    cr_assert_eq(apl_ledger_date_normalize("", date, sizeof(date), &err), APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_error_field_id(&err), APL_FIELD_DATE);
    cr_assert_eq(apl_ledger_date_normalize("2026-02-30", date, sizeof(date), &err),
                 APL_ERR_INVALID_ARGUMENT);

    cr_assert_eq(apl_ledger_description_normalize("  Paid entry fee ", description,
                                                  sizeof(description), &err),
                 APL_OK);
    cr_assert_str_eq(description, "Paid entry fee");
    cr_assert_eq(apl_ledger_description_normalize(" ", description, sizeof(description), &err),
                 APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_error_field_id(&err), APL_FIELD_DESCRIPTION);
}
