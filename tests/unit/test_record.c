#include <criterion/criterion.h>

#include "domain/record.h"

Test(record, kinds_have_stable_codes_and_labels)
{
    cr_assert_str_eq(apl_record_kind_code(APL_RECORD_TEAM), "team");
    cr_assert_str_eq(apl_record_kind_code(APL_RECORD_SEASON), "season");
    cr_assert_str_eq(apl_record_kind_code(APL_RECORD_DRIVER), "driver");
    cr_assert_str_eq(apl_record_kind_code(APL_RECORD_MEMBERSHIP), "membership");
    cr_assert_str_eq(apl_record_kind_code(APL_RECORD_CAR), "car");
    cr_assert_str_eq(apl_record_kind_label(APL_RECORD_TEAM), "Team");
}

Test(record, codes_round_trip)
{
    for (int kind = APL_RECORD_TEAM; kind <= APL_RECORD_ACTIVITY; ++kind) {
        const apl_record_kind expected = (apl_record_kind)kind;
        cr_assert_eq(apl_record_kind_from_code(apl_record_kind_code(expected)), expected);
    }
}

Test(record, unknown_codes_are_rejected)
{
    cr_assert_eq(apl_record_kind_from_code(nullptr), APL_RECORD_NONE);
    cr_assert_eq(apl_record_kind_from_code(""), APL_RECORD_NONE);
    cr_assert_eq(apl_record_kind_from_code("not-a-record"), APL_RECORD_NONE);
    cr_assert_str_empty(apl_record_kind_code(APL_RECORD_NONE));
    cr_assert_str_eq(apl_record_kind_label(APL_RECORD_NONE), "Unknown");
}

Test(record, typed_references_validate_and_compare)
{
    const apl_record_ref ledger = apl_record_ref_make(APL_RECORD_LEDGER_ENTRY, 42);
    const apl_record_ref same = apl_record_ref_make(APL_RECORD_LEDGER_ENTRY, 42);
    const apl_record_ref other = apl_record_ref_make(APL_RECORD_BUDGET_PLAN, 42);

    cr_assert(apl_record_ref_valid(&ledger));
    cr_assert(apl_record_ref_equal(&ledger, &same));
    cr_assert_not(apl_record_ref_equal(&ledger, &other));
    cr_assert_not(apl_record_ref_valid(nullptr));

    const apl_record_ref no_kind = apl_record_ref_make(APL_RECORD_NONE, 1);
    const apl_record_ref no_id = apl_record_ref_make(APL_RECORD_TEAM, 0);
    cr_assert_not(apl_record_ref_valid(&no_kind));
    cr_assert_not(apl_record_ref_valid(&no_id));
}
