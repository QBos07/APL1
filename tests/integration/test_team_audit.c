#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

#include <criterion/criterion.h>

#include "app/app.h"
#include "app/team_service.h"
#include "db/activity_repository.h"
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

static size_t activity_count(apl_app *const app, apl_error *const error)
{
    apl_activity_event *events = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_activity_repository_list(&app->db, nullptr, &events, &count, error), APL_OK);
    apl_activity_repository_free_events(events);
    return count;
}

static apl_activity_event latest_event(apl_app *const app, apl_error *const error)
{
    apl_activity_event *events = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_activity_repository_list(&app->db, nullptr, &events, &count, error), APL_OK);
    cr_assert_gt(count, 0);
    const apl_activity_event event = events[0];
    apl_activity_repository_free_events(events);
    return event;
}

Test(team_audit, create_rename_notes_noop_and_archive_are_recorded_once)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);

    apl_team first = {};
    cr_assert_eq(apl_team_service_create(&app, " Ferrari ", " Maranello ", &first, &err), APL_OK);
    cr_assert_eq(activity_count(&app, &err), 1);

    apl_activity_event event = latest_event(&app, &err);
    cr_assert_str_eq(event.action, "team_created");
    cr_assert_str_eq(event.entity_type, "team");
    cr_assert_eq(event.entity_id, first.id);
    cr_assert_str_eq(event.summary, "Created team 'Ferrari'");

    apl_activity_change *changes = nullptr;
    size_t change_count = 0;
    cr_assert_eq(
        apl_activity_repository_list_changes(&app.db, event.id, &changes, &change_count, &err),
        APL_OK);
    cr_assert_eq(change_count, 2);
    cr_assert_str_eq(changes[0].field, "name");
    cr_assert(changes[0].old_missing);
    cr_assert_str_eq(changes[0].new_value, "Ferrari");
    cr_assert_str_eq(changes[1].field, "notes");
    cr_assert(changes[1].old_missing);
    cr_assert_str_eq(changes[1].new_value, "Maranello");
    apl_activity_repository_free_changes(changes);

    apl_team renamed = {};
    cr_assert_eq(
        apl_team_service_update(&app, first.id, "Scuderia Ferrari", "Maranello", &renamed, &err),
        APL_OK);
    cr_assert_eq(activity_count(&app, &err), 2);
    event = latest_event(&app, &err);
    cr_assert_str_eq(event.action, "team_renamed");
    cr_assert_eq(event.entity_id, first.id);
    cr_assert_eq(
        apl_activity_repository_list_changes(&app.db, event.id, &changes, &change_count, &err),
        APL_OK);
    cr_assert_eq(change_count, 1);
    cr_assert_str_eq(changes[0].field, "name");
    cr_assert_str_eq(changes[0].old_value, "Ferrari");
    cr_assert_str_eq(changes[0].new_value, "Scuderia Ferrari");
    apl_activity_repository_free_changes(changes);

    cr_assert_eq(
        apl_team_service_update(&app, first.id, "Scuderia Ferrari", "New notes", &renamed, &err),
        APL_OK);
    cr_assert_eq(activity_count(&app, &err), 3);
    event = latest_event(&app, &err);
    cr_assert_eq(
        apl_activity_repository_list_changes(&app.db, event.id, &changes, &change_count, &err),
        APL_OK);
    cr_assert_eq(change_count, 1);
    cr_assert_str_eq(changes[0].field, "notes");
    cr_assert_str_eq(changes[0].old_value, "Maranello");
    cr_assert_str_eq(changes[0].new_value, "New notes");
    apl_activity_repository_free_changes(changes);

    const char *const updated_before = renamed.updated_at;
    char updated_copy[APL_TEAM_TIMESTAMP_MAX];
    (void)snprintf(updated_copy, sizeof(updated_copy), "%s", updated_before);
    apl_team noop = {};
    cr_assert_eq(
        apl_team_service_update(&app, first.id, "Scuderia Ferrari", "New notes", &noop, &err),
        APL_OK);
    cr_assert_eq(activity_count(&app, &err), 3);
    cr_assert_str_eq(noop.updated_at, updated_copy);

    apl_team second = {};
    cr_assert_eq(apl_team_service_create(&app, "McLaren", "", &second, &err), APL_OK);
    cr_assert_eq(apl_team_service_set_active(&app, first.id, &err), APL_OK);

    apl_team archived = {};
    cr_assert_eq(apl_team_service_archive(&app, first.id, &archived, &err), APL_OK);
    cr_assert(archived.archived);
    const size_t after_archive = activity_count(&app, &err);
    cr_assert_eq(after_archive, 5);
    event = latest_event(&app, &err);
    cr_assert_str_eq(event.action, "team_archived");
    cr_assert_neq(strstr(event.summary, "active team is now 'McLaren'"), nullptr);
    cr_assert_eq(
        apl_activity_repository_list_changes(&app.db, event.id, &changes, &change_count, &err),
        APL_OK);
    cr_assert_eq(change_count, 1);
    cr_assert_str_eq(changes[0].field, "archived");
    cr_assert_str_eq(changes[0].old_value, "false");
    cr_assert_str_eq(changes[0].new_value, "true");
    apl_activity_repository_free_changes(changes);

    apl_team archived_again = {};
    cr_assert_eq(apl_team_service_archive(&app, first.id, &archived_again, &err), APL_OK);
    cr_assert_str_eq(archived_again.archived_at, archived.archived_at);
    cr_assert_eq(activity_count(&app, &err), after_archive);

    close_test_app(&app, dir, &err);
}

Test(team_audit, audit_failure_rolls_back_team_create)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);

    char *sqlite_error = nullptr;
    cr_assert_eq(sqlite3_exec(apl_db_handle(&app.db),
                              "CREATE TRIGGER fail_activity BEFORE INSERT ON activity_events "
                              "BEGIN SELECT RAISE(ABORT, 'injected audit failure'); END;",
                              nullptr, nullptr, &sqlite_error),
                 SQLITE_OK);
    sqlite3_free(sqlite_error);

    apl_team team = {};
    cr_assert_eq(apl_team_service_create(&app, "Ferrari", "", &team, &err), APL_ERR_DB_QUERY);

    apl_team *teams = nullptr;
    size_t count = 1;
    cr_assert_eq(apl_team_service_list(&app, true, &teams, &count, &err), APL_OK);
    cr_assert_eq(count, 0);
    apl_team_service_free_list(teams);

    close_test_app(&app, dir, &err);
}

Test(team_audit, audit_failure_rolls_back_archive_and_active_team_fallback)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);

    apl_team first = {};
    apl_team second = {};
    cr_assert_eq(apl_team_service_create(&app, "Ferrari", "", &first, &err), APL_OK);
    cr_assert_eq(apl_team_service_create(&app, "McLaren", "", &second, &err), APL_OK);
    cr_assert_eq(apl_team_service_set_active(&app, first.id, &err), APL_OK);
    const size_t events_before = activity_count(&app, &err);

    char *sqlite_error = nullptr;
    cr_assert_eq(sqlite3_exec(apl_db_handle(&app.db),
                              "CREATE TRIGGER fail_activity BEFORE INSERT ON activity_events "
                              "BEGIN SELECT RAISE(ABORT, 'injected audit failure'); END;",
                              nullptr, nullptr, &sqlite_error),
                 SQLITE_OK);
    sqlite3_free(sqlite_error);

    apl_team archived = {};
    cr_assert_eq(apl_team_service_archive(&app, first.id, &archived, &err), APL_ERR_DB_QUERY);

    apl_team stored = {};
    cr_assert_eq(apl_team_service_get(&app, first.id, &stored, &err), APL_OK);
    cr_assert_not(stored.archived);

    apl_team active = {};
    apl_active_team_status active_status = APL_ACTIVE_TEAM_NONE;
    cr_assert_eq(apl_team_service_active(&app, &active, &active_status, &err), APL_OK);
    cr_assert_eq(active.id, first.id);
    cr_assert_eq(activity_count(&app, &err), events_before);

    close_test_app(&app, dir, &err);
}
