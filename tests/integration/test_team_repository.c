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
    cr_assert_not(team.meta.archived);
    cr_assert_neq(team.meta.created_at[0], '\0');
    cr_assert_neq(team.meta.updated_at[0], '\0');
    cr_assert_str_eq(team.meta.archived_at, "");

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
    cr_assert_str_eq(updated.meta.created_at, created.meta.created_at);
    cr_assert_not(updated.meta.archived);

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
    cr_assert(archived.meta.archived);
    cr_assert_neq(archived.meta.archived_at[0], '\0');
    char first_archive[APL_TEAM_TIMESTAMP_MAX];
    (void)snprintf(first_archive, sizeof(first_archive), "%s", archived.meta.archived_at);

    apl_team archived_again = {};
    cr_assert_eq(apl_team_repository_archive(&db.connection, created.id, &archived_again, &err),
                 APL_OK);
    cr_assert_str_eq(archived_again.meta.archived_at, first_archive);

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
    cr_assert(all[1].meta.archived);
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

Test(team_repository, blank_name_is_rejected_by_storage_constraint)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);

    apl_team team = {};
    cr_assert_eq(apl_team_repository_create(&db.connection, "   ", "", &team, &err),
                 APL_ERR_INVALID_ARGUMENT);
    cr_assert_neq(strstr(apl_error_message(&err), "constraint"), nullptr);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}

Test(team_repository, archive_missing_team_is_not_found)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);

    apl_team team = {};
    cr_assert_eq(apl_team_repository_archive(&db.connection, 999, &team, &err), APL_ERR_NOT_FOUND);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}

Test(team_repository, null_arguments_are_rejected)
{
    apl_team team = {};
    apl_error err;
    apl_error_init(&err);

    cr_assert_eq(apl_team_repository_create(nullptr, "Ferrari", "", &team, &err),
                 APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_team_repository_get(nullptr, 1, &team, &err), APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_team_repository_update(nullptr, 1, "Ferrari", "", &team, &err),
                 APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_team_repository_archive(nullptr, 1, &team, &err), APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_team_repository_list(nullptr, true, nullptr, nullptr, &err),
                 APL_ERR_INVALID_ARGUMENT);
}

Test(team_repository, closed_connection_is_rejected)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);
    apl_db_close(&db.connection);
    db.opened = false;

    apl_team team = {};
    apl_team *teams = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_team_repository_create(&db.connection, "Ferrari", "", &team, &err),
                 APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_team_repository_get(&db.connection, 1, &team, &err), APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_team_repository_update(&db.connection, 1, "Ferrari", "", &team, &err),
                 APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_team_repository_archive(&db.connection, 1, &team, &err),
                 APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_team_repository_list(&db.connection, true, &teams, &count, &err),
                 APL_ERR_INVALID_ARGUMENT);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}

Test(team_repository, sql_special_characters_are_stored_as_data)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);

    const char *const tricky_name = "O'Brien \"Racing\"; DROP TABLE teams; --";
    const char *const tricky_notes = "notes with 'quotes', % percent, and ? placeholders";
    const apl_team created = create_team(&db, tricky_name, tricky_notes);
    cr_assert_str_eq(created.name, tricky_name);
    cr_assert_str_eq(created.notes, tricky_notes);

    apl_team fetched = {};
    cr_assert_eq(apl_team_repository_get(&db.connection, created.id, &fetched, &err), APL_OK);
    cr_assert_str_eq(fetched.name, tricky_name);
    cr_assert_str_eq(fetched.notes, tricky_notes);

    apl_team *teams = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_team_repository_list(&db.connection, true, &teams, &count, &err), APL_OK);
    cr_assert_eq(count, 1);
    cr_assert_str_eq(teams[0].name, tricky_name);
    apl_team_repository_free_list(teams);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}
