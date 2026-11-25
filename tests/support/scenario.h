#ifndef APL_TESTS_SUPPORT_SCENARIO_H
#define APL_TESTS_SUPPORT_SCENARIO_H

#include <stdint.h>

#include "app/app.h"
#include "core/error.h"
#include "test_support.h"

/*
 * Reusable application scenario for integration tests (ARCH-09).
 *
 * The current Phase 4 scenario owns a temporary database and creates one team
 * with a reporting currency, an opening balance, income, an expense, and a
 * budget plan. Later phases can extend the builder with projects, parts, and
 * events without changing test setup code.
 */

typedef struct apl_test_scenario {
    apl_app app;
    char directory[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    int64_t team_id;
    bool app_started;
} apl_test_scenario;

/** Start the scenario. On failure every partial resource is cleaned up. */
apl_status apl_test_scenario_start(apl_test_scenario *scenario, apl_error *error);

/** Stop the app and remove the temporary database directory. */
apl_status apl_test_scenario_close(apl_test_scenario *scenario, apl_error *error);

#endif /* APL_TESTS_SUPPORT_SCENARIO_H */
