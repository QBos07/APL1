#include <stdio.h>
#include <string.h>

#include <criterion/criterion.h>

#include "app/app.h"
#include "app/part_type_service.h"
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

static size_t count_events(apl_app *const app, const char *const action, apl_error *const error)
{
    apl_activity_filter filter = {};
    if (action != nullptr) {
        (void)snprintf(filter.action, sizeof(filter.action), "%s", action);
    } else {
        (void)snprintf(filter.entity_type, sizeof(filter.entity_type), "part_type");
    }
    apl_activity_event *events = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_activity_repository_list(&app->db, &filter, &events, &count, error), APL_OK);
    apl_activity_repository_free_events(events);
    return count;
}

Test(part_type_service, creates_normalizes_lists_and_appends_activity)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);
    const apl_team team = make_team(&app, "Ferrari", &err);

    apl_part_type type = {};
    cr_assert_eq(apl_part_type_service_create(&app, team.id, "  Aero  ", "  Front Wing  ",
                                              "  Nose  ", "  spare  ", &type, &err),
                 APL_OK, "part type create failed: %s", apl_error_message(&err));
    cr_assert_str_eq(type.category, "Aero");
    cr_assert_str_eq(type.category_key, "aero");
    cr_assert_str_eq(type.name, "Front Wing");
    cr_assert_str_eq(type.name_key, "front wing");
    cr_assert_str_eq(type.description, "Nose");
    cr_assert_str_eq(type.notes, "spare");

    apl_part_type_filter filter = {.team_id = team.id};
    apl_part_type *types = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_part_type_service_list(&app, &filter, &types, &count, &err), APL_OK);
    cr_assert_eq(count, 1);
    cr_assert_eq(types[0].id, type.id);
    apl_part_type_service_free_list(types);

    cr_assert_eq(count_events(&app, "part_type_created", &err), 1);
    cr_assert_eq(count_events(&app, nullptr, &err), 1);

    close_test_app(&app, dir, &err);
}

Test(part_type_service, update_records_only_changed_fields)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);
    const apl_team team = make_team(&app, "Ferrari", &err);

    apl_part_type type = {};
    cr_assert_eq(apl_part_type_service_create(&app, team.id, "Aero", "Front Wing", "", "", &type,
                                              &err),
                 APL_OK);

    /* No-op update writes no audit event. */
    apl_part_type unchanged = {};
    cr_assert_eq(apl_part_type_service_update(&app, team.id, type.id, "Aero", "Front Wing", "",
                                              "", &unchanged, &err),
                 APL_OK);
    cr_assert_eq(count_events(&app, "part_type_updated", &err), 0);

    apl_part_type updated = {};
    cr_assert_eq(apl_part_type_service_update(&app, team.id, type.id, "Aero", "Rear Wing",
                                              "Changed", "", &updated, &err),
                 APL_OK);
    cr_assert_str_eq(updated.name, "Rear Wing");
    cr_assert_eq(count_events(&app, "part_type_updated", &err), 1);

    /* Audit captured name + description, not untouched notes. */
    apl_activity_filter filter = {};
    (void)snprintf(filter.action, sizeof(filter.action), "part_type_updated");
    apl_activity_event *events = nullptr;
    size_t event_count = 0;
    cr_assert_eq(apl_activity_repository_list(&app.db, &filter, &events, &event_count, &err),
                 APL_OK);
    cr_assert_eq(event_count, 1);
    apl_activity_change *changes = nullptr;
    size_t change_count = 0;
    cr_assert_eq(apl_activity_repository_list_changes(&app.db, events[0].id, &changes,
                                                      &change_count, &err),
                 APL_OK);
    cr_assert_eq(change_count, 2);
    cr_assert_str_eq(changes[0].field, "name");
    cr_assert_str_eq(changes[1].field, "description");
    apl_activity_repository_free_changes(changes);
    apl_activity_repository_free_events(events);

    close_test_app(&app, dir, &err);
}

Test(part_type_service, rejects_duplicate_and_unknown_and_rolls_back_atomically)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);
    const apl_team team = make_team(&app, "Ferrari", &err);

    apl_part_type type = {};
    cr_assert_eq(apl_part_type_service_create(&app, team.id, "Aero", "Front Wing", "", "", &type,
                                              &err),
                 APL_OK);

    apl_part_type dup = {};
    cr_assert_eq(apl_part_type_service_create(&app, team.id, "aero", "front wing", "", "", &dup,
                                              &err),
                 APL_ERR_CONFLICT);

    apl_part_type missing = {};
    cr_assert_eq(apl_part_type_service_get(&app, team.id, 99999, &missing, &err), APL_ERR_NOT_FOUND);

    /* Inject a failing audit trigger and confirm the write rolls back. */
    char trigger[512];
    (void)snprintf(trigger, sizeof(trigger),
                   "CREATE TRIGGER fail_type_activity BEFORE INSERT ON activity_events "
                   "BEGIN SELECT RAISE(ABORT, 'injected'); END;");
    cr_assert_eq(sqlite3_exec(app.db.handle, trigger, nullptr, nullptr, nullptr), SQLITE_OK);
    apl_part_type failed = {};
    cr_assert_eq(apl_part_type_service_create(&app, team.id, "Engine", "PU", "", "", &failed, &err),
                 APL_ERR_DB_QUERY);
    apl_part_type_filter filter = {.team_id = team.id, .include_archived = true};
    apl_part_type *types = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_part_type_service_list(&app, &filter, &types, &count, &err), APL_OK);
    cr_assert_eq(count, 1, "rollback left a partial part type row");
    apl_part_type_service_free_list(types);

    close_test_app(&app, dir, &err);
}