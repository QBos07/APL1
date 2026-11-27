#include <criterion/criterion.h>
#include <string.h>

#include "app/outcome_summary.h"
#include "db/driver_repository.h"
#include "db/event_repository.h"
#include "db/membership_repository.h"
#include "db/outcome_repository.h"
#include "db/season_repository.h"
#include "db/session_repository.h"
#include "db/team_repository.h"
#include "db_fixture.h"

Test(outcome_summary, scopes_recorded_values_and_excludes_missing_laps)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);
    apl_app app = {.db = db.connection};
    apl_team team = {}, other = {};
    cr_assert_eq(apl_team_repository_create(&app.db, "A", "", &team, &err), APL_OK);
    cr_assert_eq(apl_team_repository_create(&app.db, "B", "", &other, &err), APL_OK);
    apl_season season = {};
    cr_assert_eq(apl_season_repository_create(&app.db, team.id, "2026", "", "", "", &season, &err),
                 APL_OK);
    apl_event events[3] = {};
    cr_assert_eq(apl_event_repository_create(&app.db, team.id, season.id, "One", "2026-01-01", "",
                                             "", &events[0], &err),
                 APL_OK);
    cr_assert_eq(apl_event_repository_create(&app.db, team.id, 0, "Two", "2026-02-01", "", "",
                                             &events[1], &err),
                 APL_OK);
    cr_assert_eq(apl_event_repository_create(&app.db, other.id, 0, "Other", "2026-01-01", "", "",
                                             &events[2], &err),
                 APL_OK);
    apl_event_session sessions[3] = {};
    for (size_t i = 0; i < 3; ++i)
        cr_assert_eq(apl_session_repository_create(&app.db, events[i].team_id, events[i].id, "Race",
                                                   &sessions[i], &err),
                     APL_OK);
    apl_driver drivers[2] = {};
    cr_assert_eq(apl_driver_repository_create(&app.db, "One", "", "", &drivers[0], &err), APL_OK);
    cr_assert_eq(apl_driver_repository_create(&app.db, "Two", "", "", &drivers[1], &err), APL_OK);
    apl_membership member = {};
    cr_assert_eq(apl_membership_repository_create(&app.db, team.id, drivers[0].id, "2020-01-01", "",
                                                  "", "", &member, &err),
                 APL_OK);
    cr_assert_eq(apl_membership_repository_create(&app.db, team.id, drivers[1].id, "2020-01-01", "",
                                                  "", "", &member, &err),
                 APL_OK);
    cr_assert_eq(apl_membership_repository_create(&app.db, other.id, drivers[0].id, "2020-01-01",
                                                  "", "", "", &member, &err),
                 APL_OK);
    apl_outcome input = {.team_id = team.id,
                         .event_id = events[0].id,
                         .session_id = sessions[0].id,
                         .driver_id = drivers[0].id},
                saved = {};
    cr_assert_eq(apl_outcome_repository_create(&app.db, &input, &saved, &err), APL_OK);
    apl_outcome_summary_filter filter = {.team_id = team.id};
    apl_outcome_summary summary = {};
    cr_assert_eq(apl_outcome_summary_query(&app, &filter, &summary, &err), APL_OK);
    cr_assert_eq(summary.outcome_count, 1);
    cr_assert_eq(summary.laps_missing_count, 1);
    cr_assert_not(summary.has_laps_total);
    input.driver_id = drivers[1].id;
    input.has_laps = true;
    input.laps = 0;
    cr_assert_eq(apl_outcome_repository_create(&app.db, &input, &saved, &err), APL_OK);
    input.event_id = events[1].id;
    input.session_id = sessions[1].id;
    input.laps = 4;
    cr_assert_eq(apl_outcome_repository_create(&app.db, &input, &saved, &err), APL_OK);
    input.team_id = other.id;
    input.event_id = events[2].id;
    input.session_id = sessions[2].id;
    input.driver_id = drivers[0].id;
    input.laps = 99;
    cr_assert_eq(apl_outcome_repository_create(&app.db, &input, &saved, &err), APL_OK);
    cr_assert_eq(apl_outcome_summary_query(&app, &filter, &summary, &err), APL_OK);
    cr_assert_eq(summary.outcome_count, 3);
    cr_assert_eq(summary.laps_recorded_count, 2);
    cr_assert_eq(summary.laps_missing_count, 1);
    cr_assert(summary.has_laps_total);
    cr_assert_eq(summary.laps_total, 4);
    filter.season_id = season.id;
    cr_assert_eq(apl_outcome_summary_query(&app, &filter, &summary, &err), APL_OK);
    cr_assert_eq(summary.outcome_count, 2);
    cr_assert_eq(summary.laps_total, 0);
    filter.season_id = 0;
    filter.session_id = sessions[1].id;
    cr_assert_eq(apl_outcome_summary_query(&app, &filter, &summary, &err), APL_OK);
    cr_assert_eq(summary.outcome_count, 1);
    cr_assert_eq(summary.laps_total, 4);
    filter.session_id = 0;
    strcpy(filter.from_date, "2026-02-01");
    cr_assert_eq(apl_outcome_summary_query(&app, &filter, &summary, &err), APL_OK);
    cr_assert_eq(summary.outcome_count, 1);
    cr_assert_eq(summary.laps_total, 4);
    filter.event_id = events[0].id;
    cr_assert_eq(apl_outcome_summary_query(&app, &filter, &summary, &err), APL_OK);
    cr_assert_eq(summary.outcome_count, 0);
    cr_assert_not(summary.has_laps_total);
    filter.from_date[0] = '\0';
    filter.event_id = events[2].id;
    cr_assert_eq(apl_outcome_summary_query(&app, &filter, &summary, &err), APL_OK);
    cr_assert_eq(summary.outcome_count, 0);
    cr_assert_not(summary.has_laps_total);
    strcpy(filter.from_date, "2026-02-30");
    cr_assert_eq(apl_outcome_summary_query(&app, &filter, &summary, &err),
                 APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}
