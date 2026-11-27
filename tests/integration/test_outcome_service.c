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

Test(outcome_service, batch_validates_all_rows_and_rolls_back_writes)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);
    apl_app app = {.db = db.connection};
    apl_team team = {};
    cr_assert_eq(apl_team_repository_create(&app.db, "Batch", "", &team, &err), APL_OK);
    apl_event event = {};
    cr_assert_eq(apl_event_repository_create(&app.db, team.id, 0, "Event", "2026-01-01", "", "",
                                             &event, &err),
                 APL_OK);
    apl_event_session session = {};
    cr_assert_eq(apl_session_repository_create(&app.db, team.id, event.id, "Race", &session, &err),
                 APL_OK);
    apl_driver drivers[2] = {};
    apl_membership member = {};
    cr_assert_eq(apl_driver_repository_create(&app.db, "First", "", "", &drivers[0], &err), APL_OK);
    cr_assert_eq(apl_driver_repository_create(&app.db, "Second", "", "", &drivers[1], &err),
                 APL_OK);
    for (size_t i = 0; i < 2; ++i)
        cr_assert_eq(apl_membership_repository_create(&app.db, team.id, drivers[i].id, "2020-01-01",
                                                      "", "", "", &member, &err),
                     APL_OK);
    apl_outcome_batch_row rows[2] = {};
    for (size_t i = 0; i < 2; ++i) {
        rows[i].mode = APL_OUTCOME_CREATE;
        rows[i].input.team_id = team.id;
        rows[i].input.event_id = event.id;
        rows[i].input.session_id = session.id;
        rows[i].input.driver_id = drivers[i].id;
    }
    apl_outcome saved[2] = {{.id = 123}, {.id = 456}};
    apl_error errors[2];
    rows[1].input.has_laps = true;
    rows[1].input.laps = -1;
    cr_assert_eq(apl_outcome_service_save_batch(&app, rows, 2, saved, errors, &err),
                 APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(errors[0].code, APL_OK);
    cr_assert_eq(errors[1].code, APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(saved[0].id, 123);
    apl_outcome *list = nullptr;
    size_t count = 0;
    cr_assert_eq(
        apl_outcome_repository_list(&app.db, team.id, event.id, session.id, &list, &count, &err),
        APL_OK);
    cr_assert_eq(count, 0);
    apl_outcome_repository_free_list(list);
    rows[1].input.laps = 0;
    rows[1].input.driver_id = drivers[0].id;
    cr_assert_eq(apl_outcome_service_save_batch(&app, rows, 2, saved, errors, &err),
                 APL_ERR_CONFLICT);
    cr_assert_eq(errors[0].code, APL_ERR_CONFLICT);
    cr_assert_eq(errors[1].code, APL_ERR_CONFLICT);
    rows[1].input.driver_id = drivers[1].id;
    cr_assert_eq(
        sqlite3_exec(app.db.handle,
                     "CREATE TRIGGER fail_second_outcome BEFORE INSERT ON driver_outcomes "
                     "WHEN NEW.driver_id=2 BEGIN SELECT RAISE(ABORT,'write failure'); END;",
                     nullptr, nullptr, nullptr),
        SQLITE_OK);
    cr_assert_neq(apl_outcome_service_save_batch(&app, rows, 2, saved, errors, &err), APL_OK);
    cr_assert_neq(errors[1].code, APL_OK);
    cr_assert_eq(saved[0].id, 123);
    cr_assert_eq(
        apl_outcome_repository_list(&app.db, team.id, event.id, session.id, &list, &count, &err),
        APL_OK);
    cr_assert_eq(count, 0);
    apl_outcome_repository_free_list(list);
    sqlite3_stmt *activity = nullptr;
    cr_assert_eq(sqlite3_prepare_v2(app.db.handle,
                                    "SELECT count(*) FROM activity_events "
                                    "WHERE entity_type='driver_outcome'",
                                    -1, &activity, nullptr),
                 SQLITE_OK);
    cr_assert_eq(sqlite3_step(activity), SQLITE_ROW);
    cr_assert_eq(sqlite3_column_int(activity, 0), 0);
    sqlite3_finalize(activity);
    cr_assert_eq(
        sqlite3_exec(app.db.handle, "DROP TRIGGER fail_second_outcome", nullptr, nullptr, nullptr),
        SQLITE_OK);
    cr_assert_eq(apl_outcome_service_save_batch(&app, rows, 2, saved, errors, &err), APL_OK);
    cr_assert_not(saved[0].has_laps);
    cr_assert(saved[1].has_laps);
    cr_assert_eq(saved[1].laps, 0);
    rows[0].mode = APL_OUTCOME_EDIT;
    rows[0].input = saved[0];
    rows[0].input.has_laps = true;
    rows[0].input.laps = 12;
    rows[1].mode = APL_OUTCOME_EDIT;
    rows[1].input = saved[1];
    rows[1].input.has_finish_position = true;
    rows[1].input.finish_position = 1;
    cr_assert_eq(apl_outcome_service_save_batch(&app, rows, 2, saved, errors, &err), APL_OK);
    cr_assert_eq(saved[0].laps, 12);
    cr_assert_eq(saved[1].finish_position, 1);
    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}
