#include <stdio.h>
#include <string.h>

#include <criterion/criterion.h>

#include "app/app.h"
#include "app/car_service.h"
#include "app/driver_service.h"
#include "app/membership_service.h"
#include "app/season_service.h"
#include "app/team_service.h"
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

static apl_driver make_driver(apl_app *const app, const char *const name, apl_error *const error)
{
    apl_driver driver = {};
    cr_assert_eq(apl_driver_service_create(app, name, "", "", &driver, error), APL_OK,
                 "driver create failed: %s", apl_error_message(error));
    return driver;
}

Test(season_service, validates_and_normalizes_without_seeding)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);
    const apl_team team = make_team(&app, "Ferrari", &err);

    size_t count = 1;
    apl_season *seasons = nullptr;
    cr_assert_eq(apl_season_service_list(&app, team.id, true, &seasons, &count, &err), APL_OK);
    cr_assert_eq(count, 0);

    apl_season season = {};
    cr_assert_eq(apl_season_service_create(&app, team.id, "  Opening period  ", "", "", "  notes  ",
                                           &season, &err),
                 APL_OK);
    cr_assert_str_eq(season.label, "Opening period");
    cr_assert_str_eq(season.start_date, "");
    cr_assert_str_eq(season.end_date, "");
    cr_assert_str_eq(season.notes, "notes");

    apl_season duplicate = {};
    cr_assert_eq(
        apl_season_service_create(&app, team.id, "opening period", "", "", "", &duplicate, &err),
        APL_ERR_CONFLICT);

    apl_season reversed = {};
    cr_assert_eq(apl_season_service_create(&app, team.id, "Bad dates", "2026-12-31", "2026-01-01",
                                           "", &reversed, &err),
                 APL_ERR_INVALID_ARGUMENT);
    cr_assert_neq(strstr(apl_error_message(&err), "after"), nullptr);

    apl_season blank = {};
    apl_error_init(&err);
    cr_assert_eq(apl_season_service_create(&app, team.id, "   ", "", "", "", &blank, &err),
                 APL_ERR_INVALID_ARGUMENT);

    cr_assert_eq(apl_season_service_list(&app, team.id, true, &seasons, &count, &err), APL_OK);
    cr_assert_eq(count, 1);

    close_test_app(&app, dir, &err);
}

Test(car_service, accepts_label_only_and_rejects_duplicates_without_merging)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);
    const apl_team team = make_team(&app, "Ferrari", &err);

    apl_car car = {};
    cr_assert_eq(apl_car_service_create(&app, team.id, "  SF-26  ", "", "", &car, &err), APL_OK);
    cr_assert_str_eq(car.label, "SF-26");
    cr_assert_str_eq(car.chassis, "");

    apl_car duplicate = {};
    cr_assert_eq(apl_car_service_create(&app, team.id, "sf-26", "", "", &duplicate, &err),
                 APL_ERR_CONFLICT);

    apl_car blank = {};
    cr_assert_eq(apl_car_service_create(&app, team.id, "  ", "", "", &blank, &err),
                 APL_ERR_INVALID_ARGUMENT);

    size_t count = 0;
    apl_car *cars = nullptr;
    cr_assert_eq(apl_car_service_list(&app, team.id, false, &cars, &count, &err), APL_OK);
    cr_assert_eq(count, 1);
    cr_assert_str_eq(cars[0].label, "SF-26");
    apl_car_service_free_list(cars);

    close_test_app(&app, dir, &err);
}

Test(membership_service, verifies_references_dates_and_archived_driver_eligibility)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);
    const apl_team team = make_team(&app, "Ferrari", &err);
    const apl_driver driver = make_driver(&app, "Driver One", &err);

    apl_membership membership = {};
    cr_assert_eq(apl_membership_service_create(&app, team.id, driver.id, "2025-01-01", "",
                                               " Race seat ", " notes ", &membership, &err),
                 APL_OK);
    cr_assert_str_eq(membership.start_date, "2025-01-01");
    cr_assert_str_eq(membership.end_date, "");
    cr_assert_str_eq(membership.role, "Race seat");
    cr_assert_str_eq(membership.notes, "notes");

    apl_membership duplicate = {};
    cr_assert_eq(apl_membership_service_create(&app, team.id, driver.id, "2025-01-01", "", "", "",
                                               &duplicate, &err),
                 APL_ERR_CONFLICT);

    apl_membership reversed = {};
    cr_assert_eq(apl_membership_service_create(&app, team.id, driver.id, "2025-06-01", "2025-01-01",
                                               "", "", &reversed, &err),
                 APL_ERR_INVALID_ARGUMENT);

    apl_membership missing_driver = {};
    cr_assert_eq(apl_membership_service_create(&app, team.id, 999999, "2025-01-01", "", "", "",
                                               &missing_driver, &err),
                 APL_ERR_NOT_FOUND);

    apl_driver archived = {};
    cr_assert_eq(apl_driver_service_archive(&app, driver.id, &archived, &err), APL_OK);
    cr_assert(archived.archived);

    apl_membership current_for_archived = {};
    cr_assert_eq(apl_membership_service_create(&app, team.id, driver.id, "2026-01-01", "", "", "",
                                               &current_for_archived, &err),
                 APL_ERR_INVALID_ARGUMENT);
    cr_assert_neq(strstr(apl_error_message(&err), "archived drivers"), nullptr);

    apl_membership historical_for_archived = {};
    cr_assert_eq(apl_membership_service_create(&app, team.id, driver.id, "2024-01-01", "2024-12-31",
                                               "", "", &historical_for_archived, &err),
                 APL_OK);

    size_t count = 0;
    apl_membership *memberships = nullptr;
    cr_assert_eq(
        apl_membership_service_list_for_team(&app, team.id, false, &memberships, &count, &err),
        APL_OK);
    cr_assert_eq(count, 1);
    cr_assert_eq(memberships[0].id, membership.id);
    apl_membership_service_free_list(memberships);

    cr_assert_eq(
        apl_membership_service_list_for_team(&app, team.id, true, &memberships, &count, &err),
        APL_OK);
    cr_assert_eq(count, 2);
    apl_membership_service_free_list(memberships);

    apl_membership ended = {};
    cr_assert_eq(
        apl_membership_service_end(&app, team.id, membership.id, "2024-12-31", &ended, &err),
        APL_ERR_INVALID_ARGUMENT);

    cr_assert_eq(
        apl_membership_service_end(&app, team.id, membership.id, "2025-06-30", &ended, &err),
        APL_OK);
    cr_assert_str_eq(ended.end_date, "2025-06-30");

    cr_assert_eq(apl_membership_service_end(&app, team.id, membership.id, "", &ended, &err),
                 APL_ERR_INVALID_ARGUMENT);

    close_test_app(&app, dir, &err);
}

Test(driver_service, duplicate_names_are_not_merged_and_team_list_uses_memberships)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);
    const apl_team team = make_team(&app, "Ferrari", &err);
    const apl_driver first = make_driver(&app, "Same Name", &err);
    const apl_driver second = make_driver(&app, "Same Name", &err);
    cr_assert_neq(first.id, second.id);

    apl_membership membership = {};
    cr_assert_eq(apl_membership_service_create(&app, team.id, second.id, "2025-01-01", "", "", "",
                                               &membership, &err),
                 APL_OK);

    size_t count = 0;
    apl_driver *drivers = nullptr;
    cr_assert_eq(apl_driver_service_list_for_team(&app, team.id, false, &drivers, &count, &err),
                 APL_OK);
    cr_assert_eq(count, 1);
    cr_assert_eq(drivers[0].id, second.id);
    cr_assert_str_eq(drivers[0].name, "Same Name");
    apl_driver_service_free_list(drivers);

    cr_assert_eq(apl_driver_service_list(&app, true, &drivers, &count, &err), APL_OK);
    cr_assert_eq(count, 2);
    apl_driver_service_free_list(drivers);

    close_test_app(&app, dir, &err);
}
