#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

#include <criterion/criterion.h>

#include "app/app.h"
#include "app/car_service.h"
#include "app/project_service.h"
#include "app/season_service.h"
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

static apl_season make_season(apl_app *const app, const int64_t team_id, const char *const label,
                              apl_error *const error)
{
    apl_season season = {};
    cr_assert_eq(apl_season_service_create(app, team_id, label, "", "", "", &season, error), APL_OK,
                 "season create failed: %s", apl_error_message(error));
    return season;
}

static apl_car make_car(apl_app *const app, const int64_t team_id, const char *const label,
                        apl_error *const error)
{
    apl_car car = {};
    cr_assert_eq(apl_car_service_create(app, team_id, label, "", "", &car, error), APL_OK,
                 "car create failed: %s", apl_error_message(error));
    return car;
}

static size_t count_events(apl_app *const app, const char *const action, apl_error *const error)
{
    apl_activity_filter filter = {};
    if (action != nullptr) {
        (void)snprintf(filter.action, sizeof(filter.action), "%s", action);
    } else {
        (void)snprintf(filter.entity_type, sizeof(filter.entity_type), "project");
    }
    apl_activity_event *events = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_activity_repository_list(&app->db, &filter, &events, &count, error), APL_OK);
    apl_activity_repository_free_events(events);
    return count;
}

Test(project_service, creates_normalizes_lists_and_appends_activity)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);
    const apl_team team = make_team(&app, "Ferrari", &err);
    const apl_season season = make_season(&app, team.id, "2026", &err);
    const apl_car car = make_car(&app, team.id, "SF-26", &err);

    apl_project project = {};
    cr_assert_eq(apl_project_service_create(&app, team.id, "  Aero upgrade  ", "  Aero  ",
                                            "  New floor  ", "  In  Progress ", "2026-03-01", "",
                                            season.id, car.id, "  notes ", &project, &err),
                 APL_OK, "project create failed: %s", apl_error_message(&err));
    cr_assert_str_eq(project.title, "Aero upgrade");
    cr_assert_str_eq(project.category, "Aero");
    cr_assert_str_eq(project.category_key, "aero");
    cr_assert_str_eq(project.description, "New floor");
    cr_assert_str_eq(project.status, "In Progress");
    cr_assert_str_eq(project.status_key, "in progress");
    cr_assert_str_eq(project.target_date, "2026-03-01");
    cr_assert_str_eq(project.actual_date, "");
    cr_assert_str_eq(project.notes, "notes");
    cr_assert_eq(project.season_id, season.id);
    cr_assert_eq(project.car_id, car.id);

    apl_project_filter filter = {.team_id = team.id};
    apl_project *projects = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_project_service_list(&app, &filter, &projects, &count, &err), APL_OK);
    cr_assert_eq(count, 1);
    cr_assert_eq(projects[0].id, project.id);
    apl_project_service_free_list(projects);

    filter.status_key[0] = '\0';
    (void)snprintf(filter.status_key, sizeof(filter.status_key), "in progress");
    cr_assert_eq(apl_project_service_list(&app, &filter, &projects, &count, &err), APL_OK);
    cr_assert_eq(count, 1);
    apl_project_service_free_list(projects);

    cr_assert_eq(count_events(&app, "project_created", &err), 1);
    cr_assert_eq(count_events(&app, nullptr, &err), 1);

    close_test_app(&app, dir, &err);
}

Test(project_service, invalid_fields_and_cross_team_links_write_nothing)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);
    const apl_team ferrari = make_team(&app, "Ferrari", &err);
    const apl_team mclaren = make_team(&app, "McLaren", &err);
    const apl_season other_season = make_season(&app, mclaren.id, "2026", &err);
    const apl_car other_car = make_car(&app, mclaren.id, "MCL-26", &err);

    apl_project project = {};
    cr_assert_eq(apl_project_service_create(&app, ferrari.id, "   ", "", "", "", "", "", 0, 0, "",
                                            &project, &err),
                 APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_error_field_id(&err), APL_FIELD_TITLE);

    cr_assert_eq(apl_project_service_create(&app, ferrari.id, "Bad target", "", "", "",
                                            "2026-02-30", "", 0, 0, "", &project, &err),
                 APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_error_field_id(&err), APL_FIELD_TARGET_DATE);

    cr_assert_eq(apl_project_service_create(&app, ferrari.id, "Bad actual", "", "", "", "",
                                            "2026-13-01", 0, 0, "", &project, &err),
                 APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_error_field_id(&err), APL_FIELD_ACTUAL_DATE);

    cr_assert_eq(apl_project_service_create(&app, ferrari.id, "Cross season", "", "", "", "", "",
                                            other_season.id, 0, "", &project, &err),
                 APL_ERR_NOT_FOUND);
    cr_assert_eq(apl_error_field_id(&err), APL_FIELD_SEASON);

    cr_assert_eq(apl_project_service_create(&app, ferrari.id, "Cross car", "", "", "", "", "", 0,
                                            other_car.id, "", &project, &err),
                 APL_ERR_NOT_FOUND);
    cr_assert_eq(apl_error_field_id(&err), APL_FIELD_CAR);

    cr_assert_eq(apl_project_service_create(&app, ferrari.id, "Negative link", "", "", "", "", "",
                                            -1, 0, "", &project, &err),
                 APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_error_field_id(&err), APL_FIELD_SEASON);

    const apl_project_filter filter = {.team_id = ferrari.id, .include_archived = true};
    apl_project *projects = nullptr;
    size_t count = 1;
    cr_assert_eq(apl_project_service_list(&app, &filter, &projects, &count, &err), APL_OK);
    cr_assert_eq(count, 0);
    apl_project_service_free_list(projects);
    cr_assert_eq(count_events(&app, "project_created", &err), 0);

    close_test_app(&app, dir, &err);
}

Test(project_service, update_records_only_changed_fields_and_noop_edits_are_silent)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);
    const apl_team team = make_team(&app, "Ferrari", &err);
    const apl_season season = make_season(&app, team.id, "2026", &err);

    apl_project project = {};
    cr_assert_eq(apl_project_service_create(&app, team.id, "Aero", "Body", "Floor", "Open", "", "",
                                            season.id, 0, "Notes", &project, &err),
                 APL_OK);

    apl_project unchanged = {};
    cr_assert_eq(apl_project_service_update(&app, team.id, project.id, "Aero", "Body", "Floor",
                                            "Open", "", "", season.id, 0, "Notes", &unchanged,
                                            &err),
                 APL_OK);
    cr_assert_eq(count_events(&app, "project_updated", &err), 0);

    apl_project updated = {};
    cr_assert_eq(apl_project_service_update(&app, team.id, project.id, "Aero v2", "Body", "Floor",
                                            "Paused", "2026-05-01", "", 0, 0, "", &updated, &err),
                 APL_OK);
    cr_assert_str_eq(updated.title, "Aero v2");
    cr_assert_str_eq(updated.status, "Paused");
    cr_assert_str_eq(updated.target_date, "2026-05-01");
    cr_assert_eq(updated.season_id, 0);

    apl_activity_filter filter = {.team_id = team.id};
    (void)snprintf(filter.action, sizeof(filter.action), "project_updated");
    apl_activity_event *events = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_activity_repository_list(&app.db, &filter, &events, &count, &err), APL_OK);
    cr_assert_eq(count, 1);
    cr_assert_str_eq(events[0].entity_type, "project");
    cr_assert_eq(events[0].entity_id, project.id);

    apl_activity_change *changes = nullptr;
    size_t change_count = 0;
    cr_assert_eq(
        apl_activity_repository_list_changes(&app.db, events[0].id, &changes, &change_count, &err),
        APL_OK);
    bool saw_title = false;
    bool saw_status = false;
    bool saw_target = false;
    bool saw_season = false;
    bool saw_notes = false;
    for (size_t index = 0; index < change_count; ++index) {
        saw_title = saw_title || strcmp(changes[index].field, "title") == 0;
        saw_status = saw_status || strcmp(changes[index].field, "status") == 0;
        saw_target = saw_target || strcmp(changes[index].field, "target_date") == 0;
        saw_season = saw_season || strcmp(changes[index].field, "season_id") == 0;
        saw_notes = saw_notes || strcmp(changes[index].field, "notes") == 0;
    }
    cr_assert(saw_title);
    cr_assert(saw_status);
    cr_assert(saw_target);
    cr_assert(saw_season);
    cr_assert(saw_notes);
    cr_assert_eq(change_count, 5);
    apl_activity_repository_free_changes(changes);
    apl_activity_repository_free_events(events);

    close_test_app(&app, dir, &err);
}

Test(project_service, archive_preserves_links_and_cross_team_reads_fail)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);
    const apl_team ferrari = make_team(&app, "Ferrari", &err);
    const apl_team mclaren = make_team(&app, "McLaren", &err);
    const apl_season season = make_season(&app, ferrari.id, "2026", &err);
    const apl_car car = make_car(&app, ferrari.id, "SF-26", &err);

    apl_project project = {};
    cr_assert_eq(apl_project_service_create(&app, ferrari.id, "Aero", "", "", "", "", "", season.id,
                                            car.id, "", &project, &err),
                 APL_OK);

    apl_project archived = {};
    cr_assert_eq(apl_project_service_archive(&app, ferrari.id, project.id, &archived, &err),
                 APL_OK);
    cr_assert(archived.meta.archived);
    cr_assert_eq(archived.season_id, season.id);
    cr_assert_eq(archived.car_id, car.id);

    const apl_project_filter filter = {.team_id = ferrari.id};
    apl_project *projects = nullptr;
    size_t count = 1;
    cr_assert_eq(apl_project_service_list(&app, &filter, &projects, &count, &err), APL_OK);
    cr_assert_eq(count, 0);
    apl_project_service_free_list(projects);

    apl_project still_readable = {};
    cr_assert_eq(apl_project_service_get(&app, ferrari.id, project.id, &still_readable, &err),
                 APL_OK);
    cr_assert(still_readable.meta.archived);

    apl_project hidden = {};
    cr_assert_eq(apl_project_service_get(&app, mclaren.id, project.id, &hidden, &err),
                 APL_ERR_NOT_FOUND);
    cr_assert_eq(count_events(&app, "project_archived", &err), 1);

    close_test_app(&app, dir, &err);
}

Test(project_service, failed_audit_rolls_back_the_project_write)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);
    const apl_team team = make_team(&app, "Ferrari", &err);

    char *sqlite_error = nullptr;
    cr_assert_eq(sqlite3_exec(apl_db_handle(&app.db),
                              "CREATE TRIGGER fail_activity BEFORE INSERT ON activity_events "
                              "BEGIN SELECT RAISE(ABORT, 'injected audit failure'); END;",
                              nullptr, nullptr, &sqlite_error),
                 SQLITE_OK);
    sqlite3_free(sqlite_error);

    apl_project project = {};
    cr_assert_eq(apl_project_service_create(&app, team.id, "Aero", "", "", "", "", "", 0, 0, "",
                                            &project, &err),
                 APL_ERR_DB_QUERY);

    const apl_project_filter filter = {.team_id = team.id, .include_archived = true};
    apl_project *projects = nullptr;
    size_t count = 1;
    cr_assert_eq(apl_project_service_list(&app, &filter, &projects, &count, &err), APL_OK);
    cr_assert_eq(count, 0);
    apl_project_service_free_list(projects);

    close_test_app(&app, dir, &err);
}

Test(project_service, persists_project_and_timeline_after_reopen)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);
    const apl_team team = make_team(&app, "Ferrari", &err);

    apl_project project = {};
    cr_assert_eq(apl_project_service_create(&app, team.id, "Aero", "Body", "Floor", "Open",
                                            "2026-05-01", "", 0, 0, "notes", &project, &err),
                 APL_OK);
    apl_project_update update = {};
    apl_project after = {};
    cr_assert_eq(apl_project_service_log_update(&app, team.id, project.id, "2026-04-01",
                                                "First update", "", "", &update, &after, &err),
                 APL_OK);
    apl_app_stop(&app);

    apl_app reopened = {};
    const apl_app_config config = {.database_path = path};
    cr_assert_eq(apl_app_start(&reopened, &config, &err), APL_OK, "reopen failed: %s",
                 apl_error_message(&err));
    apl_project reread = {};
    cr_assert_eq(apl_project_service_get(&reopened, team.id, project.id, &reread, &err), APL_OK);
    cr_assert_str_eq(reread.title, "Aero");
    cr_assert_str_eq(reread.status, "Open");
    cr_assert_str_eq(reread.target_date, "2026-05-01");

    apl_project_update *updates = nullptr;
    size_t count = 0;
    cr_assert_eq(
        apl_project_service_list_updates(&reopened, team.id, project.id, &updates, &count, &err),
        APL_OK);
    cr_assert_eq(count, 1);
    cr_assert_str_eq(updates[0].summary, "First update");
    apl_project_service_free_updates(updates);
    apl_app_stop(&reopened);

    cr_assert_eq(apl_test_remove_temp_dir(dir, &err), APL_OK);
}
