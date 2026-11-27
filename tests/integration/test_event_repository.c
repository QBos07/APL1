#include <criterion/criterion.h>

#include "db/event_repository.h"
#include "db/season_repository.h"
#include "db/team_repository.h"
#include "db_fixture.h"

Test(event_repository, persistence_validation_and_isolation)
{
    apl_test_db db = {};
    apl_error error;
    apl_error_init(&error);
    cr_assert_eq(apl_test_db_open(&db, &error), APL_OK);
    apl_team a = {}, b = {};
    cr_assert_eq(apl_team_repository_create(&db.connection, "A", "", &a, &error), APL_OK);
    cr_assert_eq(apl_team_repository_create(&db.connection, "B", "", &b, &error), APL_OK);
    apl_season season = {};
    cr_assert_eq(
        apl_season_repository_create(&db.connection, b.id, "2026", "", "", "", &season, &error),
        APL_OK);
    apl_season own_season = {};
    cr_assert_eq(
        apl_season_repository_create(&db.connection, a.id, "2026", "", "", "", &own_season, &error),
        APL_OK);
    apl_event event = {};
    cr_assert_eq(apl_event_repository_create(&db.connection, a.id, season.id, "Event", "2026-03-01",
                                             "", "", &event, &error),
                 APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_event_repository_create(&db.connection, a.id, 0, " ", "2026-03-01", "", "",
                                             &event, &error),
                 APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_event_repository_create(&db.connection, a.id, 0, "Event", "2026-02-30", "", "",
                                             &event, &error),
                 APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_event_repository_create(&db.connection, a.id, 0, " Event ", "2026-03-01",
                                             "Location", "Notes", &event, &error),
                 APL_OK);
    cr_assert_str_eq(event.name, " Event ");
    const int64_t id = event.id;
    cr_assert_eq(apl_event_repository_get(&db.connection, b.id, id, &event, &error),
                 APL_ERR_NOT_FOUND);
    cr_assert_eq(apl_event_repository_update(&db.connection, b.id, id, 0, "Other", "2026-03-01", "",
                                             "", &event, &error),
                 APL_ERR_NOT_FOUND);
    cr_assert_eq(apl_event_repository_update(&db.connection, a.id, id, season.id, "Other",
                                             "2026-03-01", "", "", &event, &error),
                 APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_event_repository_get(&db.connection, a.id, id, &event, &error), APL_OK);
    cr_assert_str_eq(event.name, " Event ");
    cr_assert_str_eq(event.location, "Location");
    cr_assert_str_eq(event.notes, "Notes");
    cr_assert_eq(apl_event_repository_update(&db.connection, a.id, id, own_season.id, "Changed",
                                             "2026-03-02", "", "", &event, &error),
                 APL_OK);
    cr_assert_eq(event.id, id);
    cr_assert_eq(event.season_id, own_season.id);
    cr_assert_str_eq(event.date, "2026-03-02");
    apl_event *rows = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_event_repository_list(&db.connection, a.id, &rows, &count, &error), APL_OK);
    cr_assert_eq(count, 1);
    cr_assert_str_eq(rows[0].name, "Changed");
    apl_event_repository_free_list(rows);
    cr_assert_eq(apl_event_repository_list(&db.connection, b.id, &rows, &count, &error), APL_OK);
    cr_assert_eq(count, 0);
    apl_event_repository_free_list(rows);
    cr_assert_eq(sqlite3_exec(db.connection.handle, "BEGIN", nullptr, nullptr, nullptr), SQLITE_OK);
    cr_assert_eq(apl_event_repository_create(&db.connection, a.id, 0, "Rolled back", "2026-04-01",
                                             "", "", &event, &error),
                 APL_OK);
    cr_assert_eq(sqlite3_exec(db.connection.handle, "ROLLBACK", nullptr, nullptr, nullptr),
                 SQLITE_OK);
    cr_assert_eq(apl_event_repository_list(&db.connection, a.id, &rows, &count, &error), APL_OK);
    cr_assert_eq(count, 1);
    apl_event_repository_free_list(rows);
    cr_assert_eq(apl_test_db_close(&db, &error), APL_OK);
}
