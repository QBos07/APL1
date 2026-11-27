#include <criterion/criterion.h>
#include <string.h>

#include "app/outcome_service.h"
#include "db/driver_repository.h"
#include "db/event_repository.h"
#include "db/membership_repository.h"
#include "db/session_repository.h"
#include "db/team_repository.h"
#include "db_fixture.h"

Test(outcome_service, validation_duplicate_edit_and_rollback)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);
    apl_app app = {.db = db.connection};
    apl_team team = {}, other_team = {};
    cr_assert_eq(apl_team_repository_create(&app.db, "A", "", &team, &err), APL_OK);
    cr_assert_eq(apl_team_repository_create(&app.db, "B", "", &other_team, &err), APL_OK);
    apl_driver driver = {}, outsider = {};
    cr_assert_eq(apl_driver_repository_create(&app.db, "Driver", "", "", &driver, &err), APL_OK);
    cr_assert_eq(apl_driver_repository_create(&app.db, "Other", "", "", &outsider, &err), APL_OK);
    apl_membership membership = {};
    cr_assert_eq(apl_membership_repository_create(&app.db, team.id, driver.id, "2020-01-01", "", "",
                                                  "", &membership, &err),
                 APL_OK);
    apl_event event = {};
    cr_assert_eq(apl_event_repository_create(&app.db, team.id, 0, "Event", "2026-01-01", "", "",
                                             &event, &err),
                 APL_OK);
    apl_event_session session = {};
    cr_assert_eq(apl_session_repository_create(&app.db, team.id, event.id, "Race", &session, &err),
                 APL_OK);
    apl_outcome input = {
        .team_id = team.id, .event_id = event.id, .session_id = session.id, .driver_id = driver.id};
    apl_outcome saved = {};
    input.has_start_position = true;
    input.start_position = 0;
    cr_assert_eq(apl_outcome_service_save(&app, APL_OUTCOME_CREATE, &input, &saved, &err),
                 APL_ERR_INVALID_ARGUMENT);
    input.has_start_position = false;
    input.has_laps = true;
    input.laps = -1;
    cr_assert_eq(apl_outcome_service_save(&app, APL_OUTCOME_CREATE, &input, &saved, &err),
                 APL_ERR_INVALID_ARGUMENT);
    input.laps = 0;
    input.has_status = true;
    input.status[0] = (char)0xc0;
    input.status[1] = '\0';
    cr_assert_eq(apl_outcome_service_save(&app, APL_OUTCOME_CREATE, &input, &saved, &err),
                 APL_ERR_INVALID_ARGUMENT);
    strcpy(input.status, " \t ");
    input.driver_id = outsider.id;
    cr_assert_eq(apl_outcome_service_save(&app, APL_OUTCOME_CREATE, &input, &saved, &err),
                 APL_ERR_NOT_FOUND);
    input.driver_id = driver.id;
    input.team_id = other_team.id;
    cr_assert_eq(apl_outcome_service_save(&app, APL_OUTCOME_CREATE, &input, &saved, &err),
                 APL_ERR_NOT_FOUND);
    input.team_id = team.id;
    cr_assert_eq(apl_outcome_service_save(&app, APL_OUTCOME_CREATE, &input, &saved, &err), APL_OK);
    cr_assert(saved.has_laps);
    cr_assert_eq(saved.laps, 0);
    cr_assert_not(saved.has_status);
    cr_assert_not(saved.has_finish_position);
    int64_t id = saved.id;
    cr_assert_eq(apl_outcome_service_save(&app, APL_OUTCOME_CREATE, &input, &saved, &err),
                 APL_ERR_CONFLICT);
    input.id = id;
    input.has_laps = false;
    input.has_status = true;
    strcpy(input.status, " Finished ");
    cr_assert_eq(apl_outcome_service_save(&app, APL_OUTCOME_EDIT, &input, &saved, &err), APL_OK);
    cr_assert_not(saved.has_laps);
    cr_assert_str_eq(saved.status, " Finished ");
    cr_assert_eq(saved.id, id);
    apl_event_session second = {};
    cr_assert_eq(
        apl_session_repository_create(&app.db, team.id, event.id, "Practice", &second, &err),
        APL_OK);
    input.session_id = second.id;
    cr_assert_eq(apl_outcome_service_save(&app, APL_OUTCOME_EDIT, &input, &saved, &err),
                 APL_ERR_INVALID_ARGUMENT);
    input.session_id = session.id;
    cr_assert_eq(sqlite3_exec(app.db.handle,
                              "CREATE TRIGGER fail_outcome_audit BEFORE INSERT ON activity_events "
                              "WHEN NEW.entity_type='driver_outcome' "
                              "BEGIN SELECT RAISE(ABORT,'audit failure'); END;",
                              nullptr, nullptr, nullptr),
                 SQLITE_OK);
    strcpy(input.status, "Not saved");
    cr_assert_neq(apl_outcome_service_save(&app, APL_OUTCOME_EDIT, &input, &saved, &err), APL_OK);
    cr_assert_eq(apl_outcome_repository_get(&app.db, team.id, id, &saved, &err), APL_OK);
    cr_assert_str_eq(saved.status, " Finished ");
    input.session_id = second.id;
    cr_assert_neq(apl_outcome_service_save(&app, APL_OUTCOME_CREATE, &input, &saved, &err), APL_OK);
    apl_outcome *rows = nullptr;
    size_t count = 0;
    cr_assert_eq(
        apl_outcome_repository_list(&app.db, team.id, event.id, second.id, &rows, &count, &err),
        APL_OK);
    cr_assert_eq(count, 0);
    apl_outcome_repository_free_list(rows);
    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}
