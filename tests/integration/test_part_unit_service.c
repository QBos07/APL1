#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

#include <criterion/criterion.h>

#include "app/app.h"
#include "app/part_type_service.h"
#include "app/part_unit_service.h"
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

static apl_part_type make_type(apl_app *const app, const int64_t team_id, const char *const name,
                               apl_error *const error)
{
    apl_part_type type = {};
    cr_assert_eq(apl_part_type_service_create(app, team_id, "Aero", name, "", "", &type, error),
                 APL_OK, "part type create failed: %s", apl_error_message(error));
    return type;
}

static size_t count_events(apl_app *const app, const char *const action, apl_error *const error)
{
    apl_activity_filter filter = {};
    if (action != nullptr) {
        (void)snprintf(filter.action, sizeof(filter.action), "%s", action);
    } else {
        (void)snprintf(filter.entity_type, sizeof(filter.entity_type), "part_unit");
    }
    apl_activity_event *events = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_activity_repository_list(&app->db, &filter, &events, &count, error), APL_OK);
    apl_activity_repository_free_events(events);
    return count;
}

Test(part_unit_service, creates_units_without_serial_and_validates_type)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);
    const apl_team team = make_team(&app, "Ferrari", &err);
    const apl_part_type type = make_type(&app, team.id, "Front Wing", &err);

    apl_part_unit unit = {};
    cr_assert_eq(apl_part_unit_service_create(&app, team.id, type.id, "", "", "", "", &unit, &err),
                 APL_OK, "unit create failed: %s", apl_error_message(&err));
    cr_assert_gt(unit.id, 0);
    cr_assert_eq(unit.type_id, type.id);
    cr_assert_str_eq(unit.serial_number, "");
    cr_assert_str_eq(unit.part_number, "");

    /* A unit with a serial is a distinct row with the same stable ID. */
    apl_part_unit serialized = {};
    cr_assert_eq(apl_part_unit_service_create(&app, team.id, type.id, "  FW-001  ", "P100", "New",
                                              "  spare  ", &serialized, &err),
                 APL_OK);
    cr_assert_str_eq(serialized.serial_number, "FW-001");
    cr_assert_str_eq(serialized.part_number, "P100");
    cr_assert_str_eq(serialized.condition, "New");
    cr_assert_str_eq(serialized.condition_key, "new");
    cr_assert_str_eq(serialized.notes, "spare");
    cr_assert_neq(serialized.id, unit.id);

    /* Cross-team or unknown type is rejected as not-found. */
    const apl_team other = make_team(&app, "McLaren", &err);
    apl_part_unit bad = {};
    cr_assert_eq(apl_part_unit_service_create(&app, other.id, type.id, "", "", "", "", &bad, &err),
                 APL_ERR_NOT_FOUND);
    apl_part_unit missing_type = {};
    cr_assert_eq(
        apl_part_unit_service_create(&app, team.id, 99999, "", "", "", "", &missing_type, &err),
        APL_ERR_NOT_FOUND);

    apl_part_unit_filter filter = {.team_id = team.id};
    apl_part_unit *units = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_part_unit_service_list(&app, &filter, &units, &count, &err), APL_OK);
    cr_assert_eq(count, 2);
    apl_part_unit_service_free_list(units);

    cr_assert_eq(count_events(&app, "part_unit_created", &err), 2);

    close_test_app(&app, dir, &err);
}

Test(part_unit_service, update_archive_and_archive_preserves_history)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);
    const apl_team team = make_team(&app, "Ferrari", &err);
    const apl_part_type type = make_type(&app, team.id, "Front Wing", &err);
    const apl_part_type other_type = make_type(&app, team.id, "Rear Wing", &err);

    apl_part_unit unit = {};
    cr_assert_eq(apl_part_unit_service_create(&app, team.id, type.id, "FW-001", "", "", "", &unit,
                                              &err),
                 APL_OK);

    /* Archiving a type keeps its existing units readable. */
    apl_part_type archived_type = {};
    cr_assert_eq(apl_part_type_service_archive(&app, team.id, type.id, &archived_type, &err),
                 APL_OK);
    apl_part_unit still_readable = {};
    cr_assert_eq(apl_part_unit_service_get(&app, team.id, unit.id, &still_readable, &err), APL_OK);

    /* Reassign the unit to the other active type on update. */
    apl_part_unit updated = {};
    cr_assert_eq(apl_part_unit_service_update(&app, team.id, unit.id, other_type.id, "FW-002",
                                              "P200", "Used", "changed", &updated, &err),
                 APL_OK);
    cr_assert_eq(updated.type_id, other_type.id);
    cr_assert_str_eq(updated.serial_number, "FW-002");
    cr_assert_eq(count_events(&app, "part_unit_updated", &err), 1);

    apl_part_unit archived = {};
    cr_assert_eq(apl_part_unit_service_archive(&app, team.id, unit.id, &archived, &err), APL_OK);
    cr_assert(archived.meta.archived);
    cr_assert_neq(archived.meta.archived_at[0], '\0');

    apl_part_unit_filter active = {.team_id = team.id};
    apl_part_unit *units = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_part_unit_service_list(&app, &active, &units, &count, &err), APL_OK);
    cr_assert_eq(count, 0);
    apl_part_unit_service_free_list(units);
    active.include_archived = true;
    cr_assert_eq(apl_part_unit_service_list(&app, &active, &units, &count, &err), APL_OK);
    cr_assert_eq(count, 1);
    apl_part_unit_service_free_list(units);

    close_test_app(&app, dir, &err);
}

Test(part_unit_service, rolls_back_atomically_when_audit_fails)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);
    const apl_team team = make_team(&app, "Ferrari", &err);
    const apl_part_type type = make_type(&app, team.id, "Front Wing", &err);

    char trigger[512];
    (void)snprintf(trigger, sizeof(trigger),
                   "CREATE TRIGGER fail_unit_activity BEFORE INSERT ON activity_events "
                   "BEGIN SELECT RAISE(ABORT, 'injected'); END;");
    cr_assert_eq(sqlite3_exec(app.db.handle, trigger, nullptr, nullptr, nullptr), SQLITE_OK);
    apl_part_unit failed = {};
    cr_assert_eq(apl_part_unit_service_create(&app, team.id, type.id, "FW-001", "", "", "", &failed,
                                              &err),
                 APL_ERR_DB_QUERY);
    apl_part_unit_filter filter = {.team_id = team.id, .include_archived = true};
    apl_part_unit *units = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_part_unit_service_list(&app, &filter, &units, &count, &err), APL_OK);
    cr_assert_eq(count, 0, "rollback left a partial part unit row");
    apl_part_unit_service_free_list(units);

    close_test_app(&app, dir, &err);
}