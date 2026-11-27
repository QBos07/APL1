#include <criterion/criterion.h>
#include <string.h>

#include "db/driver_repository.h"
#include "db/event_repository.h"
#include "db/membership_repository.h"
#include "db/outcome_repository.h"
#include "db/session_repository.h"
#include "db/team_repository.h"
#include "db_fixture.h"

Test(outcome_repository, nullable_values_scope_links_and_persistence)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);
    apl_team a = {}, b = {};
    cr_assert_eq(apl_team_repository_create(&db.connection, "A", "", &a, &err), APL_OK);
    cr_assert_eq(apl_team_repository_create(&db.connection, "B", "", &b, &err), APL_OK);
    apl_driver driver = {}, other_driver = {};
    cr_assert_eq(apl_driver_repository_create(&db.connection, "Driver", "", "", &driver, &err),
                 APL_OK);
    cr_assert_eq(apl_driver_repository_create(&db.connection, "Other", "", "", &other_driver, &err),
                 APL_OK);
    apl_membership member = {};
    cr_assert_eq(apl_membership_repository_create(&db.connection, a.id, driver.id, "2020-01-01",
                                                  "2020-12-31", "", "", &member, &err),
                 APL_OK);
    cr_assert_eq(apl_membership_repository_create(&db.connection, b.id, other_driver.id,
                                                  "2020-01-01", "", "", "", &member, &err),
                 APL_OK);
    apl_event event = {}, other_event = {};
    cr_assert_eq(apl_event_repository_create(&db.connection, a.id, 0, "Event", "2026-01-01", "", "",
                                             &event, &err),
                 APL_OK);
    cr_assert_eq(apl_event_repository_create(&db.connection, b.id, 0, "Other", "2026-01-01", "", "",
                                             &other_event, &err),
                 APL_OK);
    apl_event_session session = {}, other_session = {};
    cr_assert_eq(
        apl_session_repository_create(&db.connection, a.id, event.id, "Race", &session, &err),
        APL_OK);
    cr_assert_eq(apl_session_repository_create(&db.connection, b.id, other_event.id, "Race",
                                               &other_session, &err),
                 APL_OK);
    apl_outcome input = {.team_id = a.id,
                         .event_id = event.id,
                         .session_id = session.id,
                         .driver_id = other_driver.id};
    apl_outcome saved = {};
    cr_assert_eq(apl_outcome_repository_create(&db.connection, &input, &saved, &err),
                 APL_ERR_INVALID_ARGUMENT);
    input.driver_id = driver.id;
    input.session_id = other_session.id;
    cr_assert_eq(apl_outcome_repository_create(&db.connection, &input, &saved, &err),
                 APL_ERR_INVALID_ARGUMENT);
    input.session_id = session.id;
    input.has_laps = true;
    input.laps = 0;
    input.has_status = true;
    strcpy(input.status, " DNF ");
    cr_assert_eq(apl_outcome_repository_create(&db.connection, &input, &saved, &err), APL_OK, "%s",
                 apl_error_message(&err));
    const int64_t id = saved.id;
    cr_assert(saved.has_laps);
    cr_assert_eq(saved.laps, 0);
    cr_assert_not(saved.has_finish_position);
    cr_assert_str_eq(saved.status, " DNF ");
    apl_event changed_event = {};
    cr_assert_eq(apl_event_repository_update(&db.connection, a.id, event.id, 0, "Renamed",
                                             "2026-01-02", "", "", &changed_event, &err),
                 APL_OK);
    apl_event_session changed_session = {};
    cr_assert_eq(apl_session_repository_update(&db.connection, a.id, event.id, session.id,
                                               "Renamed", &changed_session, &err),
                 APL_OK);
    cr_assert_eq(apl_outcome_repository_get(&db.connection, a.id, id, &saved, &err), APL_OK);
    cr_assert_eq(saved.event_id, event.id);
    cr_assert_eq(saved.session_id, session.id);
    cr_assert_eq(apl_outcome_repository_create(&db.connection, &input, &saved, &err),
                 APL_ERR_CONFLICT);
    cr_assert_eq(apl_outcome_repository_get(&db.connection, b.id, id, &saved, &err),
                 APL_ERR_NOT_FOUND);
    input.has_laps = false;
    input.has_status = false;
    input.has_finish_position = true;
    input.finish_position = 2;
    input.has_notes = true;
    strcpy(input.notes, " entered notes ");
    cr_assert_eq(apl_outcome_repository_update(&db.connection, b.id, id, &input, &saved, &err),
                 APL_ERR_NOT_FOUND);
    cr_assert_eq(apl_outcome_repository_update(&db.connection, a.id, id, &input, &saved, &err),
                 APL_OK);
    cr_assert_not(saved.has_laps);
    cr_assert_not(saved.has_status);
    cr_assert(saved.has_finish_position);
    cr_assert_eq(saved.finish_position, 2);
    cr_assert_str_eq(saved.notes, " entered notes ");
    apl_outcome *rows = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_outcome_repository_list(&db.connection, a.id, event.id, session.id, &rows,
                                             &count, &err),
                 APL_OK);
    cr_assert_eq(count, 1);
    apl_outcome_repository_free_list(rows);
    cr_assert_eq(apl_outcome_repository_list(&db.connection, b.id, event.id, session.id, &rows,
                                             &count, &err),
                 APL_OK);
    cr_assert_eq(count, 0);
    apl_outcome_repository_free_list(rows);
    cr_assert_eq(sqlite3_exec(db.connection.handle,
                              "DELETE FROM team_driver_memberships WHERE "
                              "team_id=1 AND driver_id=1",
                              nullptr, nullptr, nullptr) &
                     0xff,
                 SQLITE_CONSTRAINT);
    cr_assert_eq(sqlite3_exec(db.connection.handle, "DELETE FROM event_sessions", nullptr, nullptr,
                              nullptr) &
                     0xff,
                 SQLITE_CONSTRAINT);
    apl_db_close(&db.connection);
    cr_assert_eq(apl_db_open(&db.connection, db.path, &err), APL_OK);
    cr_assert_eq(apl_outcome_repository_get(&db.connection, a.id, id, &saved, &err), APL_OK);
    cr_assert_not(saved.has_laps);
    cr_assert_eq(saved.finish_position, 2);
    cr_assert_str_eq(saved.notes, " entered notes ");
    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}
