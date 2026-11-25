#include <ctype.h>
#include <stdio.h>

#include <criterion/criterion.h>

#include "db/budget_repository.h"
#include "db/ledger_repository.h"
#include "db/team_repository.h"
#include "db_fixture.h"

static apl_team make_team(apl_test_db *const db, const char *const name)
{
    apl_error err;
    apl_error_init(&err);
    apl_team team = {};
    cr_assert_eq(apl_team_repository_create(&db->connection, name, "", &team, &err), APL_OK);
    return team;
}

static apl_ledger_entry make_entry(const apl_team *const team, const apl_ledger_kind kind,
                                   const int64_t amount, const char *const date,
                                   const char *const category)
{
    apl_ledger_entry entry = {
        .team_id = team->id,
        .kind = kind,
        .classification = kind,
        .amount = {.minor_units = amount, .currency = "EUR"},
    };
    (void)snprintf(entry.entry_date, sizeof(entry.entry_date), "%s", date);
    (void)snprintf(entry.category, sizeof(entry.category), "%s", category);
    for (size_t index = 0; category[index] != '\0' && index + 1 < sizeof(entry.category_key);
         ++index) {
        entry.category_key[index] = (char)tolower((unsigned char)category[index]);
    }
    (void)snprintf(entry.description, sizeof(entry.description), "description");
    return entry;
}

Test(ledger_repository, migration_seeds_nothing_and_filters_are_team_scoped)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);
    const apl_team red = make_team(&db, "Red");
    const apl_team blue = make_team(&db, "Blue");

    const apl_ledger_filter empty_filter = {.team_id = red.id};
    apl_ledger_entry *empty = nullptr;
    size_t empty_count = 1;
    cr_assert_eq(
        apl_ledger_repository_list(&db.connection, &empty_filter, &empty, &empty_count, &err),
        APL_OK);
    cr_assert_null(empty);
    cr_assert_eq(empty_count, 0);

    apl_ledger_entry first = make_entry(&red, APL_LEDGER_KIND_EXPENSE, -500, "2026-01-02", "Tyres");
    apl_ledger_entry second = make_entry(&red, APL_LEDGER_KIND_INCOME, 1000, "2026-01-03", "Prize");
    apl_ledger_entry other = make_entry(&blue, APL_LEDGER_KIND_INCOME, 900, "2026-01-03", "Prize");
    apl_ledger_entry stored = {};
    cr_assert_eq(apl_ledger_repository_create(&db.connection, &first, &stored, &err), APL_OK);
    cr_assert_gt(stored.id, 0);
    const int64_t first_id = stored.id;
    cr_assert_eq(stored.classification, APL_LEDGER_KIND_EXPENSE);
    cr_assert_eq(apl_ledger_repository_create(&db.connection, &second, &stored, &err), APL_OK);
    cr_assert_eq(apl_ledger_repository_create(&db.connection, &other, &stored, &err), APL_OK);

    apl_ledger_filter filter = {.team_id = red.id};
    apl_ledger_entry *entries = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_ledger_repository_list(&db.connection, &filter, &entries, &count, &err),
                 APL_OK);
    cr_assert_eq(count, 2);
    cr_assert_str_eq(entries[0].category, "Prize"); /* newest date first */
    cr_assert_str_eq(entries[1].category, "Tyres");
    apl_ledger_repository_free_list(entries);

    (void)snprintf(filter.category_key, sizeof(filter.category_key), "tyres");
    cr_assert_eq(apl_ledger_repository_list(&db.connection, &filter, &entries, &count, &err),
                 APL_OK);
    cr_assert_eq(count, 1);
    cr_assert_str_eq(entries[0].category, "Tyres");
    apl_ledger_repository_free_list(entries);

    filter.kind = APL_LEDGER_KIND_INCOME;
    filter.category_key[0] = '\0';
    (void)snprintf(filter.from_date, sizeof(filter.from_date), "2026-01-03");
    cr_assert_eq(apl_ledger_repository_list(&db.connection, &filter, &entries, &count, &err),
                 APL_OK);
    cr_assert_eq(count, 1);
    cr_assert_str_eq(entries[0].category, "Prize");
    apl_ledger_repository_free_list(entries);

    apl_ledger_entry fetched = {};
    cr_assert_eq(apl_ledger_repository_get(&db.connection, blue.id, first_id, &fetched, &err),
                 APL_ERR_NOT_FOUND);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}

Test(ledger_repository, corrections_are_linked_scoped_and_limited_to_one_reversal)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);
    const apl_team red = make_team(&db, "Red");
    const apl_team blue = make_team(&db, "Blue");

    apl_ledger_entry original =
        make_entry(&red, APL_LEDGER_KIND_EXPENSE, -500, "2026-01-02", "Tyres");
    apl_ledger_entry stored = {};
    cr_assert_eq(apl_ledger_repository_create(&db.connection, &original, &stored, &err), APL_OK);
    const int64_t original_id = stored.id;

    apl_ledger_entry reversal =
        make_entry(&red, APL_LEDGER_KIND_REVERSAL, 500, "2026-01-03", "Tyres");
    reversal.corrects_entry_id = original_id;
    apl_ledger_entry reversal_stored = {};
    cr_assert_eq(apl_ledger_repository_create(&db.connection, &reversal, &reversal_stored, &err),
                 APL_OK);
    cr_assert_eq(reversal_stored.classification, APL_LEDGER_KIND_EXPENSE);

    apl_ledger_entry duplicate =
        make_entry(&red, APL_LEDGER_KIND_REVERSAL, 500, "2026-01-04", "Tyres");
    duplicate.corrects_entry_id = original_id;
    apl_ledger_entry duplicate_stored = {};
    cr_assert_eq(apl_ledger_repository_create(&db.connection, &duplicate, &duplicate_stored, &err),
                 APL_ERR_CONFLICT);

    apl_ledger_entry cross_team =
        make_entry(&blue, APL_LEDGER_KIND_REVERSAL, 500, "2026-01-03", "Tyres");
    cross_team.corrects_entry_id = original_id;
    cr_assert_eq(apl_ledger_repository_create(&db.connection, &cross_team, &duplicate_stored, &err),
                 APL_ERR_INVALID_ARGUMENT);

    bool found = false;
    apl_ledger_entry found_entry = {};
    cr_assert_eq(apl_ledger_repository_find_reversal(&db.connection, red.id, original_id,
                                                     &found_entry, &found, &err),
                 APL_OK);
    cr_assert(found);
    cr_assert_eq(found_entry.id, reversal_stored.id);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}

Test(budget_repository, plans_are_team_scoped_and_have_no_ledger_side_effects)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);
    const apl_team red = make_team(&db, "Red");
    const apl_team blue = make_team(&db, "Blue");

    const apl_budget_filter empty_filter = {.team_id = red.id};
    apl_budget_plan *empty = nullptr;
    size_t empty_count = 1;
    cr_assert_eq(
        apl_budget_repository_list(&db.connection, &empty_filter, &empty, &empty_count, &err),
        APL_OK);
    cr_assert_null(empty);
    cr_assert_eq(empty_count, 0);

    apl_budget_plan plan = {
        .team_id = red.id, .amount = {.minor_units = 20000, .currency = "EUR"}, .season_id = 0};
    (void)snprintf(plan.start_date, sizeof(plan.start_date), "2026-01-01");
    (void)snprintf(plan.end_date, sizeof(plan.end_date), "2026-01-31");
    (void)snprintf(plan.category, sizeof(plan.category), "Tyres");
    (void)snprintf(plan.category_key, sizeof(plan.category_key), "tyres");
    (void)snprintf(plan.notes, sizeof(plan.notes), "January");

    apl_budget_plan stored = {};
    cr_assert_eq(apl_budget_repository_create(&db.connection, &plan, &stored, &err), APL_OK);
    cr_assert_gt(stored.id, 0);
    cr_assert_eq(stored.amount.minor_units, 20000);

    apl_budget_plan duplicate = {};
    cr_assert_eq(apl_budget_repository_create(&db.connection, &plan, &duplicate, &err),
                 APL_ERR_CONFLICT);

    bool found = false;
    apl_budget_plan scope = {};
    cr_assert_eq(apl_budget_repository_find_scope(&db.connection, red.id, "2026-01-01",
                                                  "2026-01-31", "tyres", &scope, &found, &err),
                 APL_OK);
    cr_assert(found);
    cr_assert_eq(scope.id, stored.id);

    apl_budget_filter filter = {.team_id = blue.id};
    apl_budget_plan *plans = nullptr;
    size_t count = 1;
    cr_assert_eq(apl_budget_repository_list(&db.connection, &filter, &plans, &count, &err), APL_OK);
    cr_assert_eq(count, 0);
    apl_budget_repository_free_list(plans);

    /* Budget plans never create ledger entries. */
    const apl_ledger_filter ledger_filter = {.team_id = red.id};
    apl_ledger_entry *entries = nullptr;
    size_t entry_count = 1;
    cr_assert_eq(
        apl_ledger_repository_list(&db.connection, &ledger_filter, &entries, &entry_count, &err),
        APL_OK);
    cr_assert_eq(entry_count, 0);
    apl_ledger_repository_free_list(entries);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}
