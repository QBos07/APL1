#include <criterion/criterion.h>

#include "db/team_repository.h"
#include "db/transaction.h"
#include "db_fixture.h"

Test(transaction, commit_persists_change)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);

    apl_db_transaction tx = {};
    cr_assert_eq(apl_db_transaction_begin(&tx, &db.connection, &err), APL_OK);
    cr_assert(tx.active);

    apl_team team = {};
    cr_assert_eq(apl_team_repository_create(&db.connection, "Ferrari", "", &team, &err), APL_OK);
    cr_assert_eq(apl_db_transaction_commit(&tx, &err), APL_OK);
    cr_assert_not(tx.active);

    apl_team *teams = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_team_repository_list(&db.connection, true, &teams, &count, &err), APL_OK);
    cr_assert_eq(count, 1);
    apl_team_repository_free_list(teams);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}

Test(transaction, rollback_discards_change_and_is_idempotent)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);

    apl_db_transaction tx = {};
    cr_assert_eq(apl_db_transaction_begin(&tx, &db.connection, &err), APL_OK);

    apl_team team = {};
    cr_assert_eq(apl_team_repository_create(&db.connection, "Ferrari", "", &team, &err), APL_OK);
    cr_assert_eq(apl_db_transaction_rollback(&tx, &err), APL_OK);
    cr_assert_not(tx.active);

    /* Cleanup paths may call rollback again. */
    cr_assert_eq(apl_db_transaction_rollback(&tx, &err), APL_OK);

    apl_team *teams = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_team_repository_list(&db.connection, true, &teams, &count, &err), APL_OK);
    cr_assert_eq(count, 0);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}

Test(transaction, nested_begin_is_rejected_and_outer_transaction_still_works)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);

    apl_db_transaction outer = {};
    cr_assert_eq(apl_db_transaction_begin(&outer, &db.connection, &err), APL_OK);

    apl_db_transaction inner = {};
    cr_assert_eq(apl_db_transaction_begin(&inner, &db.connection, &err), APL_ERR_INVALID_ARGUMENT);
    cr_assert_not(inner.active);

    apl_team team = {};
    cr_assert_eq(apl_team_repository_create(&db.connection, "Ferrari", "", &team, &err), APL_OK);
    cr_assert_eq(apl_db_transaction_commit(&outer, &err), APL_OK);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}

Test(transaction, failed_multi_write_rolls_back_first_row)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);

    apl_db_transaction tx = {};
    cr_assert_eq(apl_db_transaction_begin(&tx, &db.connection, &err), APL_OK);

    apl_team first = {};
    cr_assert_eq(apl_team_repository_create(&db.connection, "Ferrari", "", &first, &err), APL_OK);

    apl_team duplicate = {};
    cr_assert_eq(apl_team_repository_create(&db.connection, "ferrari", "", &duplicate, &err),
                 APL_ERR_CONFLICT);

    cr_assert_eq(apl_db_transaction_rollback(&tx, &err), APL_OK);

    apl_team *teams = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_team_repository_list(&db.connection, true, &teams, &count, &err), APL_OK);
    cr_assert_eq(count, 0);
    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}

Test(transaction, commit_without_begin_is_rejected)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);

    apl_db_transaction tx = {};
    cr_assert_eq(apl_db_transaction_commit(&tx, &err), APL_ERR_INVALID_ARGUMENT);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}
