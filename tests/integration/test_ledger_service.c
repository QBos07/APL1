#include <stdio.h>
#include <string.h>

#include <criterion/criterion.h>

#include "app/app.h"
#include "app/ledger_service.h"
#include "app/team_service.h"
#include "test_support.h"

static void open_test_app(apl_app *const app, char *const dir, const size_t dir_size,
                          char *const path, const size_t path_size, apl_error *const error)
{
    cr_assert_eq(apl_test_temp_dir(dir, dir_size, error), APL_OK);
    const int written = snprintf(path, path_size, "%s/data.sqlite3", dir);
    cr_assert(written > 0 && (size_t)written < path_size);
    const apl_app_config config = {.database_path = path};
    cr_assert_eq(apl_app_start(app, &config, error), APL_OK);
}

static void close_test_app(apl_app *const app, char *const dir, apl_error *const error)
{
    apl_app_stop(app);
    cr_assert_eq(apl_test_remove_temp_dir(dir, error), APL_OK);
}

static apl_team create_team(apl_app *const app, const char *const name, const char *const currency,
                            apl_error *const error)
{
    apl_team team = {};
    cr_assert_eq(apl_team_service_create(app, name, "", &team, error), APL_OK, "create failed: %s",
                 apl_error_message(error));
    if (currency != nullptr) {
        cr_assert_eq(apl_team_service_set_currency(app, team.id, currency, &team, error), APL_OK,
                     "currency failed: %s", apl_error_message(error));
    }
    return team;
}

static size_t ledger_count(apl_app *const app, const int64_t team_id, apl_error *const error)
{
    apl_ledger_filter filter = {.team_id = team_id};
    apl_ledger_entry *entries = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_ledger_service_list(app, &filter, &entries, &count, error), APL_OK);
    apl_ledger_service_free_list(entries);
    return count;
}

Test(ledger_service, requires_currency_and_posts_signed_entries_with_totals)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);

    const apl_team team = create_team(&app, "Ferrari", nullptr, &err);
    cr_assert_eq(team.currency[0], '\0');

    apl_ledger_entry entry = {};
    cr_assert_eq(apl_ledger_service_post_income(&app, team.id, "10.00", "2026-01-01", "Prize",
                                                "Test", &entry, &err),
                 APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_error_field_id(&err), APL_FIELD_CURRENCY);
    cr_assert_eq(ledger_count(&app, team.id, &err), 0);

    apl_team currency_team = {};
    cr_assert_eq(apl_team_service_set_currency(&app, team.id, "eur", &currency_team, &err), APL_OK);
    cr_assert_str_eq(currency_team.currency, "EUR");

    cr_assert_eq(apl_ledger_service_post_opening(&app, team.id, "1000.00", "2026-01-01", "Opening",
                                                 "Opening balance", &entry, &err),
                 APL_OK);
    cr_assert_eq(entry.amount.minor_units, 100000);
    cr_assert_eq(entry.kind, APL_LEDGER_KIND_OPENING_BALANCE);

    cr_assert_eq(apl_ledger_service_post_income(&app, team.id, "250.50", "2026-01-05", " Prize ",
                                                "Podium money", &entry, &err),
                 APL_OK);
    cr_assert_eq(entry.amount.minor_units, 25050);
    cr_assert_str_eq(entry.category, "Prize");
    cr_assert_str_eq(entry.category_key, "prize");

    cr_assert_eq(apl_ledger_service_post_expense(&app, team.id, "100.25", "2026-01-06", "Tyres",
                                                 "Practice set", &entry, &err),
                 APL_OK);
    cr_assert_eq(entry.amount.minor_units, -10025);

    apl_ledger_totals totals = {};
    cr_assert_eq(apl_ledger_service_totals(&app, team.id, "", "", "", &totals, &err), APL_OK);
    cr_assert_eq(totals.balance.minor_units, 115025);
    cr_assert_eq(totals.income.minor_units, 25050);
    cr_assert_eq(totals.expense.minor_units, -10025);
    cr_assert_eq(totals.entry_count, 3);

    /* Category and period filters apply to income/expense but not balance. */
    cr_assert_eq(apl_ledger_service_totals(&app, team.id, "2026-01-05", "2026-01-05", "prize",
                                           &totals, &err),
                 APL_OK);
    cr_assert_eq(totals.balance.minor_units, 115025);
    cr_assert_eq(totals.income.minor_units, 25050);
    cr_assert_eq(totals.expense.minor_units, 0);

    apl_ledger_filter filter = {.team_id = team.id, .kind = APL_LEDGER_KIND_EXPENSE};
    apl_ledger_entry *entries = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_ledger_service_list(&app, &filter, &entries, &count, &err), APL_OK);
    cr_assert_eq(count, 1);
    cr_assert_str_eq(entries[0].category, "Tyres");
    apl_ledger_service_free_list(entries);

    /* The second opening balance is rejected without adding another row. */
    cr_assert_eq(apl_ledger_service_post_opening(&app, team.id, "5.00", "2026-01-02", "Opening",
                                                 "Second", &entry, &err),
                 APL_ERR_CONFLICT);
    cr_assert_eq(ledger_count(&app, team.id, &err), 3);

    close_test_app(&app, dir, &err);
}

Test(ledger_service, invalid_values_write_nothing)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);
    const apl_team team = create_team(&app, "Ferrari", "EUR", &err);

    apl_ledger_entry entry = {};
    cr_assert_eq(apl_ledger_service_post_income(&app, team.id, "0", "2026-01-01", "Prize", "Zero",
                                                &entry, &err),
                 APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_error_field_id(&err), APL_FIELD_AMOUNT);
    cr_assert_eq(apl_ledger_service_post_income(&app, team.id, "-1.00", "2026-01-01", "Prize",
                                                "Negative", &entry, &err),
                 APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_ledger_service_post_expense(&app, team.id, "1.234", "2026-01-01", "Tyres",
                                                 "Precision", &entry, &err),
                 APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_ledger_service_post_expense(&app, team.id, "1.00", "", "Tyres", "Blank date",
                                                 &entry, &err),
                 APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_error_field_id(&err), APL_FIELD_DATE);
    cr_assert_eq(apl_ledger_service_post_expense(&app, team.id, "1.00", "2026-01-01", " ",
                                                 "Blank category", &entry, &err),
                 APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_error_field_id(&err), APL_FIELD_CATEGORY);
    cr_assert_eq(apl_ledger_service_post_expense(&app, team.id, "1.00", "2026-01-01", "Tyres", "",
                                                 &entry, &err),
                 APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_error_field_id(&err), APL_FIELD_DESCRIPTION);
    cr_assert_eq(ledger_count(&app, team.id, &err), 0);

    /* An opening balance may be zero or negative; the form's rules are not
     * applied to it. */
    cr_assert_eq(apl_ledger_service_post_opening(&app, team.id, "-25.00", "2026-01-01", "Opening",
                                                 "Starting debt", &entry, &err),
                 APL_OK);
    cr_assert_eq(entry.amount.minor_units, -2500);

    close_test_app(&app, dir, &err);
}

Test(ledger_service, corrections_append_entries_and_preserve_history)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);
    const apl_team team = create_team(&app, "Ferrari", "EUR", &err);

    apl_ledger_entry opening = {};
    cr_assert_eq(apl_ledger_service_post_opening(&app, team.id, "1000.00", "2026-01-01", "Opening",
                                                 "Opening balance", &opening, &err),
                 APL_OK);
    apl_ledger_entry expense = {};
    cr_assert_eq(apl_ledger_service_post_expense(&app, team.id, "100.25", "2026-01-06", "Tyres",
                                                 "Practice set", &expense, &err),
                 APL_OK);

    apl_ledger_entry reversal = {};
    apl_ledger_entry replacement = {};
    cr_assert_eq(apl_ledger_service_correct(&app, team.id, expense.id, "80.00", "2026-01-07",
                                            "Tyres", "Corrected entry fee", &reversal, &replacement,
                                            &err),
                 APL_OK, "correction failed: %s", apl_error_message(&err));
    cr_assert_eq(reversal.kind, APL_LEDGER_KIND_REVERSAL);
    cr_assert_eq(reversal.amount.minor_units, 10025);
    cr_assert_eq(reversal.corrects_entry_id, expense.id);
    cr_assert_eq(reversal.classification, APL_LEDGER_KIND_EXPENSE);
    cr_assert_eq(replacement.kind, APL_LEDGER_KIND_ADJUSTMENT);
    cr_assert_eq(replacement.amount.minor_units, -8000);
    cr_assert_eq(replacement.corrects_entry_id, expense.id);

    const apl_ledger_entry original = expense;
    cr_assert_eq(original.amount.minor_units, -10025);
    cr_assert_eq(ledger_count(&app, team.id, &err), 4);

    apl_ledger_totals totals = {};
    cr_assert_eq(apl_ledger_service_totals(&app, team.id, "", "", "", &totals, &err), APL_OK);
    cr_assert_eq(totals.balance.minor_units, 92000);
    cr_assert_eq(totals.expense.minor_units, -8000);

    /* A second correction of the same original is rejected and adds nothing. */
    apl_ledger_entry duplicate_reversal = {};
    cr_assert_eq(apl_ledger_service_correct(&app, team.id, expense.id, "", "2026-01-08", "Tyres",
                                            "Again", &duplicate_reversal, nullptr, &err),
                 APL_ERR_CONFLICT);
    cr_assert_eq(ledger_count(&app, team.id, &err), 4);

    /* A pure reversal leaves the original inspectable and nets income to zero. */
    apl_ledger_entry income = {};
    cr_assert_eq(apl_ledger_service_post_income(&app, team.id, "50.00", "2026-01-09", "Prize",
                                                "Small prize", &income, &err),
                 APL_OK);
    cr_assert_eq(apl_ledger_service_correct(&app, team.id, income.id, "", "2026-01-10", "Prize",
                                            "Entered twice", &reversal, nullptr, &err),
                 APL_OK);
    cr_assert_eq(reversal.amount.minor_units, -5000);
    cr_assert_eq(apl_ledger_service_totals(&app, team.id, "", "", "", &totals, &err), APL_OK);
    cr_assert_eq(totals.income.minor_units, 0);
    cr_assert_eq(totals.balance.minor_units, 92000);

    /* Correction entries themselves cannot be corrected again. */
    cr_assert_eq(apl_ledger_service_correct(&app, team.id, replacement.id, "", "2026-01-11",
                                            "Tyres", "Chain", &duplicate_reversal, nullptr, &err),
                 APL_ERR_CONFLICT);

    close_test_app(&app, dir, &err);
}

Test(ledger_service, entries_are_team_isolated)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);
    const apl_team red = create_team(&app, "Red", "EUR", &err);
    const apl_team blue = create_team(&app, "Blue", "EUR", &err);

    apl_ledger_entry entry = {};
    cr_assert_eq(apl_ledger_service_post_income(&app, red.id, "20.00", "2026-01-01", "Prize",
                                                "Red prize", &entry, &err),
                 APL_OK);
    cr_assert_eq(apl_ledger_service_get(&app, blue.id, entry.id, &entry, &err), APL_ERR_NOT_FOUND);
    cr_assert_eq(ledger_count(&app, blue.id, &err), 0);

    close_test_app(&app, dir, &err);
}

Test(ledger_service, currency_cannot_change_after_money_records_exist)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);
    apl_team team = create_team(&app, "Ferrari", "EUR", &err);

    apl_team updated = {};
    cr_assert_eq(apl_team_service_set_currency(&app, team.id, "USD", &updated, &err), APL_OK);
    cr_assert_str_eq(updated.currency, "USD");
    team = updated;

    apl_ledger_entry entry = {};
    cr_assert_eq(apl_ledger_service_post_income(&app, team.id, "20.00", "2026-01-01", "Prize",
                                                "Prize", &entry, &err),
                 APL_OK);
    cr_assert_eq(apl_team_service_set_currency(&app, team.id, "EUR", &updated, &err),
                 APL_ERR_CONFLICT);

    close_test_app(&app, dir, &err);
}
