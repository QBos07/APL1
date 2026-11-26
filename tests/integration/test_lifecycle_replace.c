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
#include "db/lifecycle_repository.h"
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
    cr_assert_eq(apl_part_type_service_create(app, team_id, "Aero", type_name, "", "", &typed,
                                              error),
                 APL_OK);
    apl_part_unit unit = {};
    cr_assert_eq(apl_part_unit_service_create(app, team_id, typed.id, serial, "", "", "", &unit,
                                              error),
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

Test(lifecycle_service, replacement_changes_fitment_once_and_appears_in_both_histories)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);
    const apl_team team = make_team(&app, "Ferrari", &err);
    const int64_t car = make_car(&app, team.id, "SF-26", &err);
    const int64_t old_unit = make_unit(&app, team.id, "FW-100", &err);
    const int64_t new_unit = make_unit(&app, team.id, "FW-200", &err);

    cr_assert_eq(apl_lifecycle_service_install(&app, team.id, old_unit, car, "front", "2026-01-01",
                                               "", "", "", "", &(apl_lifecycle_event){}, &err),
                 APL_OK);

    apl_lifecycle_event replaced = {};
    cr_assert_eq(apl_lifecycle_service_replace(&app, team.id, car, "front", old_unit, new_unit,
                                               "2026-01-15", "", "wear", "", "", &replaced, &err),
                 APL_OK, "replace failed: %s", apl_error_message(&err));
    cr_assert_eq(replaced.action, APL_LIFECYCLE_REPLACE);
    cr_assert_eq(replaced.old_unit_id, old_unit);
    cr_assert_eq(replaced.new_unit_id, new_unit);

    /* Derived current fitment is the new unit. */
    apl_fitment_row *rows = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_lifecycle_service_current_fitment(&app, team.id, &rows, &count, &err), APL_OK);
    cr_assert_eq(count, 1);
    cr_assert_eq(rows[0].unit_id, new_unit);
    apl_lifecycle_service_free_fitment(rows);

    /* The replace event appears in both unit histories, once each. */
    apl_lifecycle_history_entry *old_history = nullptr;
    cr_assert_eq(apl_lifecycle_service_list_unit(&app, team.id, old_unit, &old_history, &count,
                                                 &err),
                 APL_OK);
    cr_assert_eq(count, 2); /* install + replace */
    apl_lifecycle_service_free_history(old_history);
    apl_lifecycle_history_entry *new_history = nullptr;
    cr_assert_eq(apl_lifecycle_service_list_unit(&app, team.id, new_unit, &new_history, &count,
                                                 &err),
                 APL_OK);
    cr_assert_eq(count, 1);
    apl_lifecycle_service_free_history(new_history);

    close_test_app(&app, dir, &err);
}

Test(lifecycle_service, failed_replacement_leaves_old_fitment_unchanged)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);
    const apl_team team = make_team(&app, "Ferrari", &err);
    const int64_t car = make_car(&app, team.id, "SF-26", &err);
    const int64_t old_unit = make_unit(&app, team.id, "FW-100", &err);
    const int64_t other = make_unit(&app, team.id, "FW-300", &err);
    const int64_t new_unit = make_unit(&app, team.id, "FW-200", &err);

    cr_assert_eq(apl_lifecycle_service_install(&app, team.id, old_unit, car, "front", "2026-01-01",
                                               "", "", "", "", &(apl_lifecycle_event){}, &err),
                 APL_OK);

    /* Wrong old unit (something not currently fitted) -> conflict. */
    apl_lifecycle_event failed = {};
    cr_assert_eq(apl_lifecycle_service_replace(&app, team.id, car, "front", other, new_unit,
                                               "2026-01-15", "", "", "", "", &failed, &err),
                 APL_ERR_CONFLICT);

    /* New unit already fitted elsewhere -> conflict. */
    const int64_t car2 = make_car(&app, team.id, "SF-27", &err);
    cr_assert_eq(apl_lifecycle_service_install(&app, team.id, new_unit, car2, "front", "2026-01-02",
                                               "", "", "", "", &(apl_lifecycle_event){}, &err),
                 APL_OK);
    cr_assert_eq(apl_lifecycle_service_replace(&app, team.id, car, "front", old_unit, new_unit,
                                               "2026-01-15", "", "", "", "", &failed, &err),
                 APL_ERR_CONFLICT);

    /* Same-unit replacement -> invalid argument. */
    cr_assert_eq(apl_lifecycle_service_replace(&app, team.id, car, "front", old_unit, old_unit,
                                               "2026-01-15", "", "", "", "", &failed, &err),
                 APL_ERR_INVALID_ARGUMENT);

    /* After all failures the old fitment is unchanged. */
    apl_fitment_row *rows = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_lifecycle_service_current_fitment(&app, team.id, &rows, &count, &err), APL_OK);
    cr_assert_eq(count, 2);
    int old_seen = 0;
    for (size_t index = 0; index < count; ++index) {
        if (rows[index].unit_id == old_unit && rows[index].car_id == car) {
            old_seen = 1;
        }
    }
    cr_assert_eq(old_seen, 1);
    apl_lifecycle_service_free_fitment(rows);

    /* No replace event was recorded. */
    apl_activity_filter filter = {};
    (void)snprintf(filter.action, sizeof(filter.action), "lifecycle_replaced");
    apl_activity_event *events = nullptr;
    cr_assert_eq(apl_activity_repository_list(&app.db, &filter, &events, &count, &err), APL_OK);
    cr_assert_eq(count, 0);
    apl_activity_repository_free_events(events);

    close_test_app(&app, dir, &err);
}