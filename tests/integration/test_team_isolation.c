#include <stdio.h>

#include <criterion/criterion.h>

#include "app/app.h"
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

static apl_team create_team(apl_app *const app, const char *const name, const char *const notes,
                            apl_error *const error)
{
    apl_team team = {};
    cr_assert_eq(apl_team_service_create(app, name, notes, &team, error), APL_OK);
    return team;
}

Test(team_isolation, writes_to_one_team_do_not_change_another)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);

    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);

    const apl_team red = create_team(&app, "Red Team", "red notes", &err);
    const apl_team blue = create_team(&app, "Blue Team", "blue notes", &err);

    /* Both IDs remain independently retrievable. */
    apl_team red_before = {};
    apl_team blue_before = {};
    cr_assert_eq(apl_team_service_get(&app, red.id, &red_before, &err), APL_OK);
    cr_assert_eq(apl_team_service_get(&app, blue.id, &blue_before, &err), APL_OK);
    cr_assert_str_eq(red_before.name, "Red Team");
    cr_assert_str_eq(blue_before.name, "Blue Team");

    /* Rename and re-note only the red team. */
    apl_team red_after = {};
    cr_assert_eq(
        apl_team_service_update(&app, red.id, "Red Team Renamed", "red changed", &red_after, &err),
        APL_OK);

    apl_team blue_after_update = {};
    cr_assert_eq(apl_team_service_get(&app, blue.id, &blue_after_update, &err), APL_OK);
    cr_assert_eq(blue_after_update.id, blue_before.id);
    cr_assert_str_eq(blue_after_update.name, "Blue Team");
    cr_assert_str_eq(blue_after_update.notes, "blue notes");
    cr_assert_eq(blue_after_update.archived, blue_before.archived);

    /* Archiving red leaves blue active and independent. */
    cr_assert_eq(apl_team_service_set_active(&app, blue.id, &err), APL_OK);
    apl_team red_archived = {};
    cr_assert_eq(apl_team_service_archive(&app, red.id, &red_archived, &err), APL_OK);

    apl_team blue_after_archive = {};
    cr_assert_eq(apl_team_service_get(&app, blue.id, &blue_after_archive, &err), APL_OK);
    cr_assert_str_eq(blue_after_archive.name, "Blue Team");
    cr_assert_str_eq(blue_after_archive.notes, "blue notes");
    cr_assert_not(blue_after_archive.archived);

    apl_team active = {};
    apl_active_team_status active_status = APL_ACTIVE_TEAM_NONE;
    cr_assert_eq(apl_team_service_active(&app, &active, &active_status, &err), APL_OK);
    cr_assert_eq(active_status, APL_ACTIVE_TEAM_ACTIVE);
    cr_assert_eq(active.id, blue.id);
    cr_assert_str_eq(active.name, "Blue Team");

    /* Filtering is scoped to archive state, not to another team's data. */
    apl_team *active_only = nullptr;
    size_t active_count = 0;
    cr_assert_eq(apl_team_service_list(&app, false, &active_only, &active_count, &err), APL_OK);
    cr_assert_eq(active_count, 1);
    cr_assert_str_eq(active_only[0].name, "Blue Team");
    apl_team_service_free_list(active_only);

    apl_team *all = nullptr;
    size_t all_count = 0;
    cr_assert_eq(apl_team_service_list(&app, true, &all, &all_count, &err), APL_OK);
    cr_assert_eq(all_count, 2);
    cr_assert_str_eq(all[0].name, "Blue Team");
    cr_assert_str_eq(all[1].name, "Red Team Renamed");
    cr_assert(all[1].archived);
    apl_team_service_free_list(all);

    /* The last remaining active team is protected from archive. */
    apl_team last_archive = {};
    cr_assert_eq(apl_team_service_archive(&app, blue.id, &last_archive, &err), APL_ERR_CONFLICT);
    cr_assert_eq(apl_team_service_get(&app, blue.id, &blue_after_archive, &err), APL_OK);
    cr_assert_not(blue_after_archive.archived);
    cr_assert_eq(apl_team_service_active(&app, &active, &active_status, &err), APL_OK);
    cr_assert_eq(active.id, blue.id);

    apl_app_stop(&app);
    cr_assert_eq(apl_test_remove_temp_dir(dir, &err), APL_OK);
}
