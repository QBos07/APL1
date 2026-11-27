#include <criterion/criterion.h>
#include <stdio.h>

#include "app/event_service.h"
#include "app/season_service.h"
#include "app/team_service.h"
#include "test_support.h"

Test(event_service, validates_references_preserves_links_and_rolls_back_failures)
{
    apl_error err;
    apl_error_init(&err);
    char dir[APL_TEST_DIR_MAX], path[APL_TEST_PATH_MAX];
    cr_assert_eq(apl_test_temp_dir(dir, sizeof(dir), &err), APL_OK);
    cr_assert_gt(snprintf(path, sizeof(path), "%s/data.sqlite3", dir), 0);
    apl_app app = {};
    apl_app_config config = {.database_path = path};
    cr_assert_eq(apl_app_start(&app, &config, &err), APL_OK);
    apl_team a = {}, b = {};
    cr_assert_eq(apl_team_service_create(&app, "A", "", &a, &err), APL_OK);
    cr_assert_eq(apl_team_service_create(&app, "B", "", &b, &err), APL_OK);
    apl_season foreign_season = {};
    cr_assert_eq(apl_season_service_create(&app, b.id, "Season", "", "", "", &foreign_season, &err),
                 APL_OK);
    apl_event event = {};
    cr_assert_eq(apl_event_service_create(&app, a.id, foreign_season.id, "Event", "2026-01-01", "",
                                          "", &event, &err),
                 APL_ERR_NOT_FOUND);
    cr_assert_eq(apl_event_service_create(&app, a.id, 0, "Event", "", "", "", &event, &err),
                 APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(
        apl_event_service_create(&app, a.id, 0, "Event", "2026-02-30", "", "", &event, &err),
        APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(
        apl_event_service_create(&app, a.id, 0, " Event ", "2026-03-01", "Place", "", &event, &err),
        APL_OK);
    const int64_t event_id = event.id;
    apl_event_session session = {};
    cr_assert_eq(apl_session_service_create(&app, b.id, event_id, "Race", &session, &err),
                 APL_ERR_NOT_FOUND);
    cr_assert_eq(apl_session_service_create(&app, a.id, event_id, " ", &session, &err),
                 APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_session_service_create(&app, a.id, event_id, "Race", &session, &err), APL_OK);
    const int64_t session_id = session.id;
    cr_assert_eq(apl_event_service_update(&app, a.id, event_id, 0, "Revised", "2026-03-02", "", "",
                                          &event, &err),
                 APL_OK);
    cr_assert_eq(event.id, event_id);
    cr_assert_eq(
        apl_session_service_update(&app, a.id, event_id, session_id, "Main race", &session, &err),
        APL_OK);
    cr_assert_eq(session.id, session_id);
    cr_assert_eq(session.event_id, event_id);
    cr_assert_eq(sqlite3_exec(app.db.handle, "DELETE FROM events", nullptr, nullptr, nullptr) &
                     0xff,
                 SQLITE_CONSTRAINT);

    cr_assert_eq(sqlite3_exec(app.db.handle,
                              "CREATE TRIGGER fail_event_audit BEFORE INSERT ON activity_events "
                              "WHEN NEW.entity_type IN ('event','event_session') "
                              "BEGIN SELECT RAISE(ABORT, 'test failure'); END;",
                              nullptr, nullptr, nullptr),
                 SQLITE_OK);
    cr_assert_neq(apl_event_service_update(&app, a.id, event_id, 0, "Not saved", "2026-03-02", "",
                                           "", &event, &err),
                  APL_OK);
    cr_assert_neq(
        apl_session_service_update(&app, a.id, event_id, session_id, "Not saved", &session, &err),
        APL_OK);
    cr_assert_neq(apl_session_service_create(&app, a.id, event_id, "Not created", &session, &err),
                  APL_OK);
    cr_assert_eq(apl_event_repository_get(&app.db, a.id, event_id, &event, &err), APL_OK);
    cr_assert_str_eq(event.name, "Revised");
    cr_assert_eq(apl_session_repository_get(&app.db, a.id, event_id, session_id, &session, &err),
                 APL_OK);
    cr_assert_str_eq(session.label, "Main race");
    apl_event_session *sessions = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_session_repository_list(&app.db, a.id, event_id, &sessions, &count, &err),
                 APL_OK);
    cr_assert_eq(count, 1);
    apl_session_repository_free_list(sessions);
    apl_app_stop(&app);
    cr_assert_eq(apl_test_remove_temp_dir(dir, &err), APL_OK);
}
