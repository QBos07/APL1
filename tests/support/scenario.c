#include "scenario.h"

#include <stdio.h>

#include "app/budget_service.h"
#include "app/ledger_service.h"
#include "app/team_service.h"

apl_status apl_test_scenario_start(apl_test_scenario *const scenario, apl_error *const error)
{
    if (scenario == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "scenario is nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *scenario = (apl_test_scenario){};

    apl_status status = apl_test_temp_dir(scenario->directory, sizeof(scenario->directory), error);
    if (status != APL_OK) {
        return status;
    }
    const int written =
        snprintf(scenario->path, sizeof(scenario->path), "%s/data.sqlite3", scenario->directory);
    if (written <= 0 || (size_t)written >= sizeof(scenario->path)) {
        apl_error_set(error, APL_ERR_IO, "scenario database path does not fit");
        (void)apl_test_remove_temp_dir(scenario->directory, error);
        scenario->directory[0] = '\0';
        return APL_ERR_IO;
    }

    const apl_app_config config = {.database_path = scenario->path};
    status = apl_app_start(&scenario->app, &config, error);
    if (status != APL_OK) {
        (void)apl_test_remove_temp_dir(scenario->directory, error);
        scenario->directory[0] = '\0';
        return status;
    }
    scenario->app_started = true;

    apl_team team = {};
    status =
        apl_team_service_create(&scenario->app, "Scenario Racing", "Fixture team", &team, error);
    if (status == APL_OK) {
        status = apl_team_service_set_currency(&scenario->app, team.id, "EUR", &team, error);
    }
    if (status != APL_OK) {
        (void)apl_test_scenario_close(scenario, error);
        return status;
    }
    scenario->team_id = team.id;

    apl_ledger_entry entry = {};
    status = apl_ledger_service_post_opening(&scenario->app, team.id, "1000.00", "2026-01-01",
                                             "Opening", "Scenario opening balance", &entry, error);
    if (status == APL_OK) {
        status = apl_ledger_service_post_income(&scenario->app, team.id, "250.00", "2026-01-05",
                                                "Prize", "Scenario income", &entry, error);
    }
    if (status == APL_OK) {
        status = apl_ledger_service_post_expense(&scenario->app, team.id, "100.00", "2026-01-06",
                                                 "Tyres", "Scenario expense", &entry, error);
    }
    apl_budget_plan plan = {};
    if (status == APL_OK) {
        status = apl_budget_service_save(&scenario->app, team.id, 0, "2026-01-01", "2026-01-31",
                                         "200.00", "Tyres", 0, "Scenario budget", &plan, error);
    }
    if (status != APL_OK) {
        (void)apl_test_scenario_close(scenario, error);
    }
    return status;
}

apl_status apl_test_scenario_close(apl_test_scenario *const scenario, apl_error *const error)
{
    if (scenario == nullptr) {
        return APL_OK;
    }
    if (scenario->app_started) {
        apl_app_stop(&scenario->app);
        scenario->app_started = false;
    }
    if (scenario->directory[0] != '\0') {
        const apl_status status = apl_test_remove_temp_dir(scenario->directory, error);
        scenario->directory[0] = '\0';
        return status;
    }
    return APL_OK;
}
