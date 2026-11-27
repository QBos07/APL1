#include <criterion/criterion.h>

#include "app/event_detail.h"
#include "db/driver_repository.h"
#include "db/membership_repository.h"
#include "db/team_repository.h"
#include "db_fixture.h"

Test(event_detail, shows_missing_outcomes_separately_and_enforces_scope)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);
    apl_app app = {.db = db.connection};
    apl_team a = {}, b = {};
    cr_assert_eq(apl_team_repository_create(&app.db, "A", "", &a, &err), APL_OK);
    cr_assert_eq(apl_team_repository_create(&app.db, "B", "", &b, &err), APL_OK);
    apl_event event = {};
    cr_assert_eq(apl_event_repository_create(&app.db, a.id, 0, "Grand Prix", "2026-01-01", "Track",
                                             "", &event, &err),
                 APL_OK);
    apl_event_detail event_detail = {};
    cr_assert_eq(apl_event_detail_load(&app, b.id, event.id, &event_detail, &err),
                 APL_ERR_NOT_FOUND);
    cr_assert_eq(apl_event_detail_load(&app, a.id, event.id, &event_detail, &err), APL_OK);
    cr_assert_eq(event_detail.session_count, 0);
    apl_event_detail_free(&event_detail);
    apl_event_session session = {};
    cr_assert_eq(apl_session_repository_create(&app.db, a.id, event.id, "Race", &session, &err),
                 APL_OK);
    apl_driver first = {}, second = {}, other = {};
    cr_assert_eq(apl_driver_repository_create(&app.db, "First", "", "", &first, &err), APL_OK);
    cr_assert_eq(apl_driver_repository_create(&app.db, "Second", "", "", &second, &err), APL_OK);
    cr_assert_eq(apl_driver_repository_create(&app.db, "Other", "", "", &other, &err), APL_OK);
    apl_membership membership = {};
    cr_assert_eq(apl_membership_repository_create(&app.db, a.id, first.id, "2020-01-01", "", "", "",
                                                  &membership, &err),
                 APL_OK);
    cr_assert_eq(apl_membership_repository_create(&app.db, a.id, second.id, "2020-01-01",
                                                  "2020-12-31", "", "", &membership, &err),
                 APL_OK);
    cr_assert_eq(apl_membership_repository_create(&app.db, b.id, other.id, "2020-01-01", "", "", "",
                                                  &membership, &err),
                 APL_OK);
    apl_outcome input = {.team_id = a.id,
                         .event_id = event.id,
                         .session_id = session.id,
                         .driver_id = first.id},
                saved = {};
    cr_assert_eq(apl_outcome_repository_create(&app.db, &input, &saved, &err), APL_OK);
    apl_session_detail detail = {};
    cr_assert_eq(apl_session_detail_load(&app, b.id, event.id, session.id, &detail, &err),
                 APL_ERR_NOT_FOUND);
    cr_assert_eq(apl_session_detail_load(&app, a.id, event.id, session.id, &detail, &err), APL_OK);
    cr_assert_str_eq(detail.event.name, "Grand Prix");
    cr_assert_str_eq(detail.session.label, "Race");
    cr_assert_eq(detail.row_count, 2);
    cr_assert_eq(detail.rows[0].driver.id, first.id);
    cr_assert(detail.rows[0].has_outcome);
    cr_assert_not(detail.rows[0].outcome.has_laps);
    cr_assert_eq(detail.rows[1].driver.id, second.id);
    cr_assert_not(detail.rows[1].has_outcome);
    apl_session_detail_free(&detail);
    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}
