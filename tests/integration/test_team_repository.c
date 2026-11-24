#include <stdio.h>
#include <string.h>

#include <criterion/criterion.h>

#include "db/team_repository.h"
#include "db_fixture.h"

static apl_team create_team(apl_test_db *const db, const char *const name, const char *const notes)
{
    apl_error err;
    apl_error_init(&err);
    apl_team team = {};
    cr_assert_eq(apl_team_repository_create(&db->connection, name, notes, &team, &err), APL_OK,
                 "create '%s' failed: %s", name, apl_error_message(&err));
    return team;
}

Test(team_repository, create_stores_normalized_team_without_seeding_others)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);

    const apl_team team = create_team(&db, "Ferrari", "Maranello");

    cr_assert_gt(team.id, 0);
    cr_assert_str_eq(team.name, "Ferrari");
    cr_assert_str_eq(team.notes, "Maranello");
    cr_assert_not(team.archived);
    cr_assert_neq(team.created_at[0], '\0');
    cr_assert_neq(team.updated_at[0], '\0');
    cr_assert_str_eq(team.archived_at, "");

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}

Test(team_repository, duplicate_names_are_case_insensitive_conflicts)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);

    (void)create_team(&db, "Ferrari", "");

    apl_team duplicate = {};
    apl_error_init(&err);
    cr_assert_eq(apl_team_repository_create(&db.connection, "ferrari", "", &duplicate, &err),
                 APL_ERR_CONFLICT);
    cr_assert_neq(strstr(apl_error_message(&err), "already exists"), nullptr);

    apl_team *teams = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_team_repository_list(&db.connection, true, &teams, &count, &err), APL_OK);
    cr_assert_eq(count, 1);
    apl_team_repository_free_list(teams);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}

Test(team_repository, get_missing_team_is_not_found)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);

    apl_team team = {};
    cr_assert_eq(apl_team_repository_get(&db.connection, 999, &team, &err), APL_ERR_NOT_FOUND);
    cr_assert_neq(strstr(apl_error_message(&err), "not found"), nullptr);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}

Test(team_repository, update_preserves_id_and_changes_profile)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);

    const apl_team created = create_team(&db, "Ferrari", "old notes");

    apl_team updated = {};
    cr_assert_eq(apl_team_repository_update(&db.connection, created.id, "Scuderia Ferrari",
                                            "new notes", &updated, &err),
                 APL_OK);
    cr_assert_eq(updated.id, created.id);
    cr_assert_str_eq(updated.name, "Scuderia Ferrari");
    cr_assert_str_eq(updated.notes, "new notes");
    cr_assert_str_eq(updated.created_at, created.created_at);
    cr_assert_not(updated.archived);

    apl_team fetched = {};
    cr_assert_eq(apl_team_repository_get(&db.connection, created.id, &fetched, &err), APL_OK);
    cr_assert_str_eq(fetched.name, "Scuderia Ferrari");
    cr_assert_str_eq(fetched.notes, "new notes");

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}

Test(team_repository, update_missing_team_is_not_found)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);

    apl_team team = {};
    cr_assert_eq(apl_team_repository_update(&db.connection, 999, "Nobody", "", &team, &err),
                 APL_ERR_NOT_FOUND);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}

Test(team_repository, archive_marks_team_and_preserves_timestamp_on_repeat)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);

    const apl_team created = create_team(&db, "Ferrari", "");

    apl_team archived = {};
    cr_assert_eq(apl_team_repository_archive(&db.connection, created.id, &archived, &err), APL_OK);
    cr_assert(archived.archived);
    cr_assert_neq(archived.archived_at[0], '\0');
    char first_archive[APL_TEAM_TIMESTAMP_MAX];
    (void)snprintf(first_archive, sizeof(first_archive), "%s", archived.archived_at);

    apl_team archived_again = {};
    cr_assert_eq(apl_team_repository_archive(&db.connection, created.id, &archived_again, &err),
                 APL_OK);
    cr_assert_str_eq(archived_again.archived_at, first_archive);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}

Test(team_repository, list_filters_archived_and_orders_active_before_archived)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);

    const apl_team zebra = create_team(&db, "Zebra", "");
    (void)create_team(&db, "Alpha", "");
    apl_team archived = {};
    cr_assert_eq(apl_team_repository_archive(&db.connection, zebra.id, &archived, &err), APL_OK);

    apl_team *active = nullptr;
    size_t active_count = 0;
    cr_assert_eq(apl_team_repository_list(&db.connection, false, &active, &active_count, &err),
                 APL_OK);
    cr_assert_eq(active_count, 1);
    cr_assert_str_eq(active[0].name, "Alpha");
    apl_team_repository_free_list(active);

    apl_team *all = nullptr;
    size_t all_count = 0;
    cr_assert_eq(apl_team_repository_list(&db.connection, true, &all, &all_count, &err), APL_OK);
    cr_assert_eq(all_count, 2);
    cr_assert_str_eq(all[0].name, "Alpha");
    cr_assert_str_eq(all[1].name, "Zebra");
    cr_assert(all[1].archived);
    apl_team_repository_free_list(all);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}

Test(team_repository, empty_list_returns_null_and_zero)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);

    apl_team *teams = nullptr;
    size_t count = 99;
    cr_assert_eq(apl_team_repository_list(&db.connection, true, &teams, &count, &err), APL_OK);
    cr_assert_null(teams);
    cr_assert_eq(count, 0);
    apl_team_repository_free_list(teams);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}
