#include <stdio.h>
#include <string.h>

#include <criterion/criterion.h>

#include "app/app.h"
#include "app/team_service.h"
#include "db/team_repository.h"
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

static apl_team create_team(apl_app *const app, const char *const name, const char *const notes,
                            apl_error *const error)
{
    apl_team team = {};
    cr_assert_eq(apl_team_service_create(app, name, notes, &team, error), APL_OK,
                 "create '%s' failed: %s", name, apl_error_message(error));
    return team;
}

Test(team_service, create_normalizes_activates_and_persists)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);

    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);

    const apl_team created = create_team(&app, "  Scuderia Ferrari  ", "  Maranello  ", &err);
    cr_assert_gt(created.id, 0);
    cr_assert_str_eq(created.name, "Scuderia Ferrari");
    cr_assert_str_eq(created.notes, "Maranello");

    apl_team active = {};
    apl_active_team_status active_status = APL_ACTIVE_TEAM_NONE;
    cr_assert_eq(apl_team_service_active(&app, &active, &active_status, &err), APL_OK);
    cr_assert_eq(active_status, APL_ACTIVE_TEAM_ACTIVE);
    cr_assert_eq(active.id, created.id);
    cr_assert_str_eq(active.name, "Scuderia Ferrari");

    /* Selection survives a full application restart. */
    apl_app_stop(&app);
    apl_error_init(&err);
    const apl_app_config config = {.database_path = path};
    cr_assert_eq(apl_app_start(&app, &config, &err), APL_OK);
    cr_assert_eq(apl_team_service_active(&app, &active, &active_status, &err), APL_OK);
    cr_assert_eq(active_status, APL_ACTIVE_TEAM_ACTIVE);
    cr_assert_eq(active.id, created.id);
    cr_assert_str_eq(active.name, "Scuderia Ferrari");

    close_test_app(&app, dir, &err);
}

Test(team_service, blank_name_is_rejected_before_any_write)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);

    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);

    apl_team team = {};
    cr_assert_eq(apl_team_service_create(&app, "   ", "", &team, &err), APL_ERR_INVALID_ARGUMENT);
    cr_assert_neq(strstr(apl_error_message(&err), "required"), nullptr);

    apl_team *teams = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_team_service_list(&app, true, &teams, &count, &err), APL_OK);
    cr_assert_eq(count, 0);

    close_test_app(&app, dir, &err);
}

Test(team_service, duplicate_names_are_rejected_without_changing_active_team)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);

    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);

    const apl_team first = create_team(&app, "Ferrari", "", &err);

    apl_team duplicate = {};
    cr_assert_eq(apl_team_service_create(&app, "ferrari", "", &duplicate, &err), APL_ERR_CONFLICT);

    int64_t active_id = 0;
    apl_team active = {};
    apl_active_team_status active_status = APL_ACTIVE_TEAM_NONE;
    cr_assert_eq(apl_team_service_active(&app, &active, &active_status, &err), APL_OK);
    active_id = active.id;
    cr_assert_eq(active_status, APL_ACTIVE_TEAM_ACTIVE);
    cr_assert_eq(active_id, first.id);

    close_test_app(&app, dir, &err);
}

Test(team_service, update_preserves_id_and_rejects_duplicate_name)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);

    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);

    const apl_team ferrari = create_team(&app, "Ferrari", "old", &err);
    (void)create_team(&app, "McLaren", "", &err);

    apl_team updated = {};
    cr_assert_eq(
        apl_team_service_update(&app, ferrari.id, "Scuderia Ferrari", "new", &updated, &err),
        APL_OK);
    cr_assert_eq(updated.id, ferrari.id);
    cr_assert_str_eq(updated.name, "Scuderia Ferrari");
    cr_assert_str_eq(updated.notes, "new");

    cr_assert_eq(apl_team_service_update(&app, ferrari.id, "mclaren", "", &updated, &err),
                 APL_ERR_CONFLICT);

    close_test_app(&app, dir, &err);
}

Test(team_service, archiving_active_team_falls_back_and_keeps_row)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);

    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);

    const apl_team ferrari = create_team(&app, "Ferrari", "history", &err);
    const apl_team mclaren = create_team(&app, "McLaren", "", &err);
    cr_assert_eq(apl_team_service_set_active(&app, ferrari.id, &err), APL_OK);

    apl_team archived = {};
    cr_assert_eq(apl_team_service_archive(&app, ferrari.id, &archived, &err), APL_OK);
    cr_assert(archived.archived);
    cr_assert_str_eq(archived.notes, "history");

    apl_team active = {};
    apl_active_team_status active_status = APL_ACTIVE_TEAM_NONE;
    cr_assert_eq(apl_team_service_active(&app, &active, &active_status, &err), APL_OK);
    cr_assert_eq(active_status, APL_ACTIVE_TEAM_ACTIVE);
    cr_assert_eq(active.id, mclaren.id);

    apl_team fetched = {};
    cr_assert_eq(apl_team_service_get(&app, ferrari.id, &fetched, &err), APL_OK);
    cr_assert(fetched.archived);
    cr_assert_str_eq(fetched.name, "Ferrari");

    close_test_app(&app, dir, &err);
}

Test(team_service, archiving_non_active_team_keeps_selection)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);

    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);

    const apl_team ferrari = create_team(&app, "Ferrari", "", &err);
    const apl_team mclaren = create_team(&app, "McLaren", "", &err); /* active */
    cr_assert_eq(apl_team_service_set_active(&app, ferrari.id, &err), APL_OK);

    apl_team archived = {};
    cr_assert_eq(apl_team_service_archive(&app, mclaren.id, &archived, &err), APL_OK);

    apl_team active = {};
    apl_active_team_status active_status = APL_ACTIVE_TEAM_NONE;
    cr_assert_eq(apl_team_service_active(&app, &active, &active_status, &err), APL_OK);
    cr_assert_eq(active_status, APL_ACTIVE_TEAM_ACTIVE);
    cr_assert_eq(active.id, ferrari.id);

    close_test_app(&app, dir, &err);
}

Test(team_service, archived_active_setting_falls_back_and_persists_fallback)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);

    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);

    const apl_team ferrari = create_team(&app, "Ferrari", "", &err);
    const apl_team mclaren = create_team(&app, "McLaren", "", &err);
    cr_assert_eq(apl_team_service_set_active(&app, ferrari.id, &err), APL_OK);

    /* Simulate a selection made before the team was archived (the service
     * archive path selects a fallback, but the read path must still handle a
     * stale archived selection). */
    apl_team archived = {};
    cr_assert_eq(apl_team_repository_archive(&app.db, ferrari.id, &archived, &err), APL_OK);

    apl_team active = {};
    apl_active_team_status active_status = APL_ACTIVE_TEAM_NONE;
    cr_assert_eq(apl_team_service_active(&app, &active, &active_status, &err), APL_OK);
    cr_assert_eq(active_status, APL_ACTIVE_TEAM_FALLBACK);
    cr_assert_eq(active.id, mclaren.id);
    cr_assert_str_eq(active.name, "McLaren");

    cr_assert_eq(apl_team_service_active(&app, &active, &active_status, &err), APL_OK);
    cr_assert_eq(active_status, APL_ACTIVE_TEAM_ACTIVE);
    cr_assert_eq(active.id, mclaren.id);

    close_test_app(&app, dir, &err);
}

Test(team_service, last_active_team_cannot_be_archived)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);

    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);

    const apl_team ferrari = create_team(&app, "Ferrari", "", &err);

    apl_team archived = {};
    cr_assert_eq(apl_team_service_archive(&app, ferrari.id, &archived, &err), APL_ERR_CONFLICT);
    cr_assert_neq(strstr(apl_error_message(&err), "last active team"), nullptr);

    apl_team active = {};
    apl_active_team_status active_status = APL_ACTIVE_TEAM_NONE;
    cr_assert_eq(apl_team_service_active(&app, &active, &active_status, &err), APL_OK);
    cr_assert_eq(active_status, APL_ACTIVE_TEAM_ACTIVE);
    cr_assert_eq(active.id, ferrari.id);

    close_test_app(&app, dir, &err);
}

Test(team_service, list_can_include_archived_teams)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);

    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);

    const apl_team ferrari = create_team(&app, "Ferrari", "", &err);
    (void)create_team(&app, "McLaren", "", &err);
    apl_team archived = {};
    cr_assert_eq(apl_team_service_archive(&app, ferrari.id, &archived, &err), APL_OK);

    apl_team *active_only = nullptr;
    size_t active_count = 0;
    cr_assert_eq(apl_team_service_list(&app, false, &active_only, &active_count, &err), APL_OK);
    cr_assert_eq(active_count, 1);
    cr_assert_str_eq(active_only[0].name, "McLaren");
    apl_team_service_free_list(active_only);

    apl_team *all = nullptr;
    size_t all_count = 0;
    cr_assert_eq(apl_team_service_list(&app, true, &all, &all_count, &err), APL_OK);
    cr_assert_eq(all_count, 2);
    cr_assert_str_eq(all[1].name, "Ferrari");
    cr_assert(all[1].archived);
    apl_team_service_free_list(all);

    close_test_app(&app, dir, &err);
}

Test(team_service, fallback_uses_oldest_created_active_team)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);

    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);

    const apl_team first = create_team(&app, "Alpha", "", &err);
    const apl_team second = create_team(&app, "Beta", "", &err);
    const apl_team third = create_team(&app, "Gamma", "", &err);
    cr_assert_eq(apl_team_service_set_active(&app, first.id, &err), APL_OK);

    /* Bypass the service so the stored selection points at an archived row. */
    apl_team archived = {};
    cr_assert_eq(apl_team_repository_archive(&app.db, first.id, &archived, &err), APL_OK);

    apl_team active = {};
    apl_active_team_status active_status = APL_ACTIVE_TEAM_NONE;
    cr_assert_eq(apl_team_service_active(&app, &active, &active_status, &err), APL_OK);
    cr_assert_eq(active_status, APL_ACTIVE_TEAM_FALLBACK);
    cr_assert_eq(active.id, second.id);

    cr_assert_eq(apl_team_service_set_active(&app, second.id, &err), APL_OK);
    cr_assert_eq(apl_team_repository_archive(&app.db, second.id, &archived, &err), APL_OK);
    cr_assert_eq(apl_team_service_active(&app, &active, &active_status, &err), APL_OK);
    cr_assert_eq(active_status, APL_ACTIVE_TEAM_FALLBACK);
    cr_assert_eq(active.id, third.id);

    close_test_app(&app, dir, &err);
}
