#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

#include <criterion/criterion.h>

#include "app/app.h"
#include "app/project_service.h"
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

static apl_team make_team(apl_app *const app, const char *const name, apl_error *const error)
{
    apl_team team = {};
    cr_assert_eq(apl_team_service_create(app, name, "", &team, error), APL_OK,
                 "team create failed: %s", apl_error_message(error));
    return team;
}

static apl_project make_project(apl_app *const app, const int64_t team_id, const char *const title,
                                apl_error *const error)
{
    apl_project project = {};
    cr_assert_eq(apl_project_service_create(app, team_id, title, "", "", "", "", "", 0, 0, "",
                                            &project, error),
                 APL_OK, "project create failed: %s", apl_error_message(error));
    return project;
}

static size_t update_count(apl_app *const app, const int64_t team_id, const int64_t project_id,
                           apl_error *const error)
{
    apl_project_update *updates = nullptr;
    size_t count = 0;
    cr_assert_eq(
        apl_project_service_list_updates(app, team_id, project_id, &updates, &count, error),
        APL_OK);
    apl_project_service_free_updates(updates);
    return count;
}

static size_t action_count(apl_app *const app, const char *const action, apl_error *const error)
{
    apl_activity_filter filter = {};
    (void)snprintf(filter.action, sizeof(filter.action), "%s", action);
    apl_activity_event *events = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_activity_repository_list(&app->db, &filter, &events, &count, error), APL_OK);
    apl_activity_repository_free_events(events);
    return count;
}

Test(project_update_service, logs_updates_and_changes_status_atomically)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);
    const apl_team team = make_team(&app, "Ferrari", &err);
    const apl_project project = make_project(&app, team.id, "Aero", &err);

    apl_project_update update = {};
    apl_project after = {};
    cr_assert_eq(apl_project_service_log_update(&app, team.id, project.id, "2026-04-01",
                                                "  Started mock-up  ", "", "  notes  ", &update,
                                                &after, &err),
                 APL_OK, "update failed: %s", apl_error_message(&err));
    cr_assert_str_eq(update.update_date, "2026-04-01");
    cr_assert_str_eq(update.summary, "Started mock-up");
    cr_assert_str_eq(update.status_change, "");
    cr_assert_str_eq(update.notes, "notes");
    cr_assert_str_eq(after.status, "");

    cr_assert_eq(apl_project_service_log_update(&app, team.id, project.id, "2026-04-05",
                                                "Fitted floor", "  In  Progress ", "", &update,
                                                &after, &err),
                 APL_OK);
    cr_assert_str_eq(after.status, "In Progress");
    cr_assert_str_eq(after.status_key, "in progress");
    cr_assert_eq(action_count(&app, "project_status_changed", &err), 1);

    /* Repeating the same current status keeps the label in the update but does
     * not claim another project transition. */
    cr_assert_eq(apl_project_service_log_update(&app, team.id, project.id, "2026-04-06",
                                                "Still working", "in progress", "", &update, &after,
                                                &err),
                 APL_OK);
    cr_assert_eq(action_count(&app, "project_status_changed", &err), 1);

    cr_assert_eq(update_count(&app, team.id, project.id, &err), 3);
    cr_assert_eq(action_count(&app, "project_update_logged", &err), 3);

    apl_project_update *updates = nullptr;
    size_t count = 0;
    cr_assert_eq(
        apl_project_service_list_updates(&app, team.id, project.id, &updates, &count, &err),
        APL_OK);
    cr_assert_eq(count, 3);
    cr_assert_str_eq(updates[0].update_date, "2026-04-01");
    cr_assert_str_eq(updates[2].update_date, "2026-04-06");
    apl_project_service_free_updates(updates);

    apl_activity_filter filter = {};
    (void)snprintf(filter.action, sizeof(filter.action), "project_update_logged");
    apl_activity_event *events = nullptr;
    count = 0;
    cr_assert_eq(apl_activity_repository_list(&app.db, &filter, &events, &count, &err), APL_OK);
    cr_assert_eq(count, 3);
    cr_assert_str_eq(events[0].entity_type, "project_update");
    apl_activity_change *changes = nullptr;
    size_t change_count = 0;
    cr_assert_eq(
        apl_activity_repository_list_changes(&app.db, events[0].id, &changes, &change_count, &err),
        APL_OK);
    cr_assert_eq(change_count, 4);
    cr_assert_str_eq(changes[0].field, "date");
    cr_assert_str_eq(changes[1].field, "summary");
    cr_assert_str_eq(changes[2].field, "status_change");
    cr_assert_str_eq(changes[3].field, "notes");
    apl_activity_repository_free_changes(changes);
    apl_activity_repository_free_events(events);

    close_test_app(&app, dir, &err);
}

Test(project_update_service, rejects_archived_projects_and_invalid_fields_without_writes)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);
    const apl_team team = make_team(&app, "Ferrari", &err);
    const apl_team other = make_team(&app, "McLaren", &err);
    const apl_project project = make_project(&app, team.id, "Aero", &err);

    apl_project_update update = {};
    apl_project after = {};
    cr_assert_eq(apl_project_service_log_update(&app, team.id, project.id, "", "Missing date", "",
                                                "", &update, &after, &err),
                 APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_error_field_id(&err), APL_FIELD_UPDATE_DATE);
    cr_assert_eq(apl_project_service_log_update(&app, team.id, project.id, "2026-02-30", "Bad date",
                                                "", "", &update, &after, &err),
                 APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_error_field_id(&err), APL_FIELD_UPDATE_DATE);
    cr_assert_eq(apl_project_service_log_update(&app, team.id, project.id, "2026-04-01", "   ", "",
                                                "", &update, &after, &err),
                 APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_error_field_id(&err), APL_FIELD_SUMMARY);
    cr_assert_eq(apl_project_service_log_update(&app, other.id, project.id, "2026-04-01",
                                                "Wrong team", "", "", &update, &after, &err),
                 APL_ERR_NOT_FOUND);
    cr_assert_eq(update_count(&app, team.id, project.id, &err), 0);
    cr_assert_eq(action_count(&app, "project_update_logged", &err), 0);

    apl_project archived = {};
    cr_assert_eq(apl_project_service_archive(&app, team.id, project.id, &archived, &err), APL_OK);
    cr_assert_eq(apl_project_service_log_update(&app, team.id, project.id, "2026-04-01",
                                                "Archived project", "", "", &update, &after, &err),
                 APL_ERR_CONFLICT);
    cr_assert_eq(update_count(&app, team.id, project.id, &err), 0);
    cr_assert_eq(action_count(&app, "project_update_logged", &err), 0);

    close_test_app(&app, dir, &err);
}

Test(project_update_service, failed_status_audit_rolls_back_update_and_project)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);
    const apl_team team = make_team(&app, "Ferrari", &err);
    const apl_project project = make_project(&app, team.id, "Aero", &err);

    char *sqlite_error = nullptr;
    cr_assert_eq(
        sqlite3_exec(apl_db_handle(&app.db),
                     "CREATE TRIGGER fail_status_activity BEFORE INSERT ON activity_events "
                     "WHEN NEW.action = 'project_status_changed' "
                     "BEGIN SELECT RAISE(ABORT, 'injected status audit failure'); END;",
                     nullptr, nullptr, &sqlite_error),
        SQLITE_OK);
    sqlite3_free(sqlite_error);

    apl_project_update update = {};
    apl_project after = {};
    cr_assert_eq(apl_project_service_log_update(&app, team.id, project.id, "2026-04-01",
                                                "Status change", "Blocked", "", &update, &after,
                                                &err),
                 APL_ERR_DB_QUERY);

    cr_assert_eq(update_count(&app, team.id, project.id, &err), 0);
    cr_assert_eq(action_count(&app, "project_update_logged", &err), 0);
    cr_assert_eq(action_count(&app, "project_status_changed", &err), 0);

    apl_project reread = {};
    cr_assert_eq(apl_project_service_get(&app, team.id, project.id, &reread, &err), APL_OK);
    cr_assert_str_eq(reread.status, "");

    close_test_app(&app, dir, &err);
}
