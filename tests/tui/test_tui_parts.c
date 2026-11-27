#include <stdio.h>
#include <string.h>

#include <criterion/criterion.h>

#include "app/app.h"
#include "app/car_service.h"
#include "app/lifecycle_service.h"
#include "app/part_type_service.h"
#include "app/part_unit_service.h"
#include "app/team_service.h"
#include "tui/shell_internal.h"
#include "test_support.h"

static void open_parts_test_app(apl_app *const app, char *const dir, const size_t dir_size,
                                char *const path, const size_t path_size, apl_error *const error)
{
    cr_assert_eq(apl_test_temp_dir(dir, dir_size, error), APL_OK);
    const int written = snprintf(path, path_size, "%s/data.sqlite3", dir);
    cr_assert(written > 0 && (size_t)written < path_size);
    const apl_app_config config = {.database_path = path};
    cr_assert_eq(apl_app_start(app, &config, error), APL_OK);
}

static void close_parts_test_app(apl_app *const app, char *const dir, apl_error *const error)
{
    apl_app_stop(app);
    cr_assert_eq(apl_test_remove_temp_dir(dir, error), APL_OK);
}

Test(tui_parts, filters_compose_against_lifecycle_and_user_entered_part_details)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error error;
    apl_error_init(&error);
    apl_app app = {};
    open_parts_test_app(&app, dir, sizeof(dir), path, sizeof(path), &error);

    apl_team team = {};
    cr_assert_eq(apl_team_service_create(&app, "Filter Team", "", &team, &error), APL_OK);
    apl_car car = {};
    apl_car other_car = {};
    cr_assert_eq(apl_car_service_create(&app, team.id, "SF-26", "", "", &car, &error), APL_OK);
    cr_assert_eq(apl_car_service_create(&app, team.id, "SF-25", "", "", &other_car, &error),
                 APL_OK);

    apl_part_type aero = {};
    apl_part_type wheel = {};
    cr_assert_eq(apl_part_type_service_create(&app, team.id, "Aero", "Front Wing", "CFK", "body",
                                              &aero, &error),
                 APL_OK);
    cr_assert_eq(apl_part_type_service_create(&app, team.id, "Suspension", "Wheel", "", "rim",
                                              &wheel, &error),
                 APL_OK);

    apl_part_unit front_wing = {};
    apl_part_unit rear_wing = {};
    apl_part_unit wheel_unit = {};
    cr_assert_eq(apl_part_unit_service_create(&app, team.id, aero.id, "FW-101", "P-101", "New",
                                              "front notes", &front_wing, &error),
                 APL_OK);
    cr_assert_eq(apl_part_unit_service_create(&app, team.id, aero.id, "FW-202", "P-202", "Used",
                                              "rear notes", &rear_wing, &error),
                 APL_OK);
    cr_assert_eq(apl_part_unit_service_create(&app, team.id, wheel.id, "WH-303", "P-303", "New",
                                              "wheel notes", &wheel_unit, &error),
                 APL_OK);

    apl_lifecycle_event event = {};
    cr_assert_eq(apl_lifecycle_service_install(&app, team.id, front_wing.id, car.id, "front",
                                               "2026-01-10", "", "", "", "", &event, &error),
                 APL_OK);
    cr_assert_eq(apl_lifecycle_service_install(&app, team.id, rear_wing.id, car.id, "rear",
                                               "2026-02-10", "", "", "", "", &event, &error),
                 APL_OK);
    cr_assert_eq(apl_lifecycle_service_install(&app, team.id, wheel_unit.id, other_car.id, "front",
                                               "2026-01-12", "", "", "", "", &event, &error),
                 APL_OK);

    shell_state state = {};
    state.app = &app;
    state.active_team = team;
    state.active_status = APL_ACTIVE_TEAM_ACTIVE;
    state.section = APL_TUI_SECTION_PARTS;
    state.screen = SHELL_SCREEN_PART_UNIT_LIST;
    apl_tui_list_init(&state.list, 8);
    cr_assert_eq(load_part_units(&state, 0, &error), APL_OK);
    cr_assert_eq(state.part_unit_count, 3);

    cr_assert(apply_parts_filter(&state, "INSTALL", "SF-26", "front", "2026-01-01", "2026-01-31",
                                 "fw-101", "aero", &error));
    cr_assert_eq(state.part_unit_count, 1);
    cr_assert_eq(state.part_units[0].id, front_wing.id);

    cr_assert(apply_parts_filter(&state, "replace", "", "", "", "", "", "", &error));
    cr_assert_eq(state.part_unit_count, 0);

    cr_assert(apply_parts_filter(&state, "", "", "", "", "", "rim", "wheel", &error));
    cr_assert_eq(state.part_unit_count, 1);
    cr_assert_eq(state.part_units[0].id, wheel_unit.id);

    apl_part_unit_service_free_list(state.part_units);
    apl_lifecycle_service_free_fitment(state.fitment_rows);
    close_parts_test_app(&app, dir, &error);
}
