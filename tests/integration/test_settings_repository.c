#include <criterion/criterion.h>

#include "db/settings_repository.h"
#include "db/team_repository.h"
#include "db_fixture.h"

static apl_team create_team(apl_test_db *const db, const char *const name, apl_error *const err)
{
    apl_team team = {};
    cr_assert_eq(apl_team_repository_create(&db->connection, name, "", &team, err), APL_OK);
    return team;
}

Test(settings_repository, fresh_database_has_no_active_team)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);

    int64_t team_id = -1;
    cr_assert_eq(apl_settings_repository_get_active_team_id(&db.connection, &team_id, &err),
                 APL_OK);
    cr_assert_eq(team_id, 0);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}

Test(settings_repository, active_team_can_be_set_switched_and_cleared)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);

    const apl_team first = create_team(&db, "Ferrari", &err);
    const apl_team second = create_team(&db, "McLaren", &err);

    cr_assert_eq(apl_settings_repository_set_active_team_id(&db.connection, first.id, &err),
                 APL_OK);
    int64_t active = 0;
    cr_assert_eq(apl_settings_repository_get_active_team_id(&db.connection, &active, &err), APL_OK);
    cr_assert_eq(active, first.id);

    cr_assert_eq(apl_settings_repository_set_active_team_id(&db.connection, second.id, &err),
                 APL_OK);
    cr_assert_eq(apl_settings_repository_get_active_team_id(&db.connection, &active, &err), APL_OK);
    cr_assert_eq(active, second.id);

    cr_assert_eq(apl_settings_repository_clear_active_team_id(&db.connection, &err), APL_OK);
    cr_assert_eq(apl_settings_repository_get_active_team_id(&db.connection, &active, &err), APL_OK);
    cr_assert_eq(active, 0);

    /* Clearing an already-empty setting is safe. */
    cr_assert_eq(apl_settings_repository_clear_active_team_id(&db.connection, &err), APL_OK);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}

Test(settings_repository, archived_team_cannot_be_selected_and_previous_value_stays)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);

    const apl_team first = create_team(&db, "Ferrari", &err);
    const apl_team archived = create_team(&db, "McLaren", &err);
    cr_assert_eq(apl_settings_repository_set_active_team_id(&db.connection, first.id, &err),
                 APL_OK);

    apl_team stored = {};
    cr_assert_eq(apl_team_repository_archive(&db.connection, archived.id, &stored, &err), APL_OK);

    cr_assert_eq(apl_settings_repository_set_active_team_id(&db.connection, archived.id, &err),
                 APL_ERR_NOT_FOUND);

    int64_t active = 0;
    cr_assert_eq(apl_settings_repository_get_active_team_id(&db.connection, &active, &err), APL_OK);
    cr_assert_eq(active, first.id);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}

Test(settings_repository, selection_survives_connection_reopen)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);

    const apl_team team = create_team(&db, "Ferrari", &err);
    cr_assert_eq(apl_settings_repository_set_active_team_id(&db.connection, team.id, &err), APL_OK);

    apl_db_close(&db.connection);
    db.opened = false;
    cr_assert_eq(apl_db_open(&db.connection, db.path, &err), APL_OK);
    db.opened = true;

    int64_t active = 0;
    cr_assert_eq(apl_settings_repository_get_active_team_id(&db.connection, &active, &err), APL_OK);
    cr_assert_eq(active, team.id);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}
