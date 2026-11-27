#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

#include <criterion/criterion.h>

#include "app/app.h"
#include "app/car_service.h"
#include "app/lifecycle_service.h"
#include "app/part_type_service.h"
#include "app/part_unit_service.h"
#include "app/team_service.h"
#include "db/activity_repository.h"
#include "domain/part.h"
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

static int64_t make_unit(apl_app *const app, const int64_t team_id, const char *const serial,
                         apl_error *const error)
{
    char type_name[64];
    (void)snprintf(type_name, sizeof(type_name), "Wing %s", serial);
    apl_part_type typed = {};
    cr_assert_eq(
        apl_part_type_service_create(app, team_id, "Aero", type_name, "", "", &typed, error),
        APL_OK);
    apl_part_unit unit = {};
    cr_assert_eq(
        apl_part_unit_service_create(app, team_id, typed.id, serial, "", "", "", &unit, error),
        APL_OK);
    return unit.id;
}

static int64_t make_car(apl_app *const app, const int64_t team_id, const char *const label,
                        apl_error *const error)
{
    apl_car car = {};
    cr_assert_eq(apl_car_service_create(app, team_id, label, "", "", &car, error), APL_OK);
    return car.id;
}

static size_t count_events(apl_app *const app, const char *const action, apl_error *const error)
{
    apl_activity_filter filter = {};
    if (action != nullptr) {
        (void)snprintf(filter.action, sizeof(filter.action), "%s", action);
    } else {
        (void)snprintf(filter.entity_type, sizeof(filter.entity_type), "lifecycle_event");
    }
    apl_activity_event *events = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_activity_repository_list(&app->db, &filter, &events, &count, error), APL_OK);
    apl_activity_repository_free_events(events);
    return count;
}

Test(lifecycle_service, install_remove_and_current_fitment_lifecycle)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);
    const apl_team team = make_team(&app, "Ferrari", &err);
    const int64_t car = make_car(&app, team.id, "SF-26", &err);
    const int64_t wing = make_unit(&app, team.id, "FW-100", &err);

    apl_lifecycle_event installed = {};
    cr_assert_eq(apl_lifecycle_service_install(&app, team.id, wing, car, "front", "2026-01-10", "",
                                               "", "", "", &installed, &err),
                 APL_OK, "install failed: %s", apl_error_message(&err));
    cr_assert_eq(installed.action, APL_LIFECYCLE_INSTALL);

    apl_fitment_row *rows = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_lifecycle_service_current_fitment(&app, team.id, &rows, &count, &err), APL_OK);
    cr_assert_eq(count, 1);
    cr_assert_eq(rows[0].unit_id, wing);
    apl_lifecycle_service_free_fitment(rows);

    /* Install a second unit at the same position conflicts (use replace). */
    const int64_t wing2 = make_unit(&app, team.id, "FW-200", &err);
    apl_lifecycle_event dup = {};
    cr_assert_eq(apl_lifecycle_service_install(&app, team.id, wing2, car, "front", "2026-01-11", "",
                                               "", "", "", &dup, &err),
                 APL_ERR_CONFLICT);

    /* Removing a unit not currently fitted at front conflicts. */
    apl_lifecycle_event wrong = {};
    cr_assert_eq(apl_lifecycle_service_remove(&app, team.id, wing2, car, "front", "2026-01-12", "",
                                              "", "", "", &wrong, &err),
                 APL_ERR_CONFLICT);

    apl_lifecycle_event removed = {};
    cr_assert_eq(apl_lifecycle_service_remove(&app, team.id, wing, car, "front", "2026-01-12", "",
                                              "", "", "", &removed, &err),
                 APL_OK);
    cr_assert_eq(removed.action, APL_LIFECYCLE_REMOVE);

    cr_assert_eq(apl_lifecycle_service_current_fitment(&app, team.id, &rows, &count, &err), APL_OK);
    cr_assert_eq(count, 1);
    cr_assert_eq(rows[0].unit_id, 0);
    cr_assert_eq(rows[0].car_id, car);
    cr_assert_str_eq(rows[0].position, "front");
    apl_lifecycle_service_free_fitment(rows);

    cr_assert_eq(count_events(&app, "lifecycle_installed", &err), 1);
    cr_assert_eq(count_events(&app, "lifecycle_removed", &err), 1);

    close_test_app(&app, dir, &err);
}

Test(lifecycle_service, repair_keeps_fitment_and_invalid_writes_nothing)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);
    const apl_team team = make_team(&app, "Ferrari", &err);
    const int64_t car = make_car(&app, team.id, "SF-26", &err);
    const int64_t wing = make_unit(&app, team.id, "FW-100", &err);

    cr_assert_eq(apl_lifecycle_service_install(&app, team.id, wing, car, "front", "2026-01-01", "",
                                               "", "", "", &(apl_lifecycle_event){}, &err),
                 APL_OK);

    apl_lifecycle_event repaired = {};
    cr_assert_eq(apl_lifecycle_service_repair(&app, team.id, wing, car, "front", "2026-01-20", "",
                                              "crash", "", "", &repaired, &err),
                 APL_OK);
    cr_assert_eq(repaired.action, APL_LIFECYCLE_REPAIR);

    /* Repair leaves fitment unchanged. */
    apl_fitment_row *rows = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_lifecycle_service_current_fitment(&app, team.id, &rows, &count, &err), APL_OK);
    cr_assert_eq(count, 1);
    cr_assert_eq(rows[0].unit_id, wing);
    apl_lifecycle_service_free_fitment(rows);

    /* An invalid date writes neither a lifecycle nor an activity row. */
    apl_lifecycle_event bad = {};
    cr_assert_eq(apl_lifecycle_service_repair(&app, team.id, wing, car, "front", "not-a-date", "",
                                              "", "", "", &bad, &err),
                 APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(count_events(&app, "lifecycle_repaired", &err), 1);

    close_test_app(&app, dir, &err);
}

Test(lifecycle_service, dispose_requires_removal_first)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);
    const apl_team team = make_team(&app, "Ferrari", &err);
    const int64_t car = make_car(&app, team.id, "SF-26", &err);
    const int64_t wing = make_unit(&app, team.id, "FW-100", &err);

    cr_assert_eq(apl_lifecycle_service_install(&app, team.id, wing, car, "front", "2026-01-01", "",
                                               "", "", "", &(apl_lifecycle_event){}, &err),
                 APL_OK);

    /* Cannot dispose a currently-fitted unit. */
    apl_lifecycle_event disposed = {};
    cr_assert_eq(apl_lifecycle_service_dispose(&app, team.id, wing, "2026-02-01", "", "", "", "",
                                               &disposed, &err),
                 APL_ERR_CONFLICT);

    cr_assert_eq(apl_lifecycle_service_remove(&app, team.id, wing, car, "front", "2026-02-01", "",
                                              "", "", "", &(apl_lifecycle_event){}, &err),
                 APL_OK);
    cr_assert_eq(apl_lifecycle_service_dispose(&app, team.id, wing, "2026-02-02", "", "", "", "",
                                               &disposed, &err),
                 APL_OK);
    cr_assert_eq(disposed.action, APL_LIFECYCLE_DISPOSE);
    cr_assert_eq(count_events(&app, "lifecycle_disposed", &err), 1);

    close_test_app(&app, dir, &err);
}
