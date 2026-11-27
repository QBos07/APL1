#include <criterion/criterion.h>

#include "db/event_repository.h"
#include "db/session_repository.h"
#include "db/team_repository.h"
#include "db_fixture.h"

Test(session_repository, explicit_sessions_are_event_and_team_scoped)
{
    apl_test_db db = {};
    apl_error error;
    apl_error_init(&error);
    cr_assert_eq(apl_test_db_open(&db, &error), APL_OK);
    apl_team a = {}, b = {};
    cr_assert_eq(apl_team_repository_create(&db.connection, "A", "", &a, &error), APL_OK);
    cr_assert_eq(apl_team_repository_create(&db.connection, "B", "", &b, &error), APL_OK);
    apl_event first = {}, second = {}, other = {};
    cr_assert_eq(apl_event_repository_create(&db.connection, a.id, 0, "First", "2026-01-01", "", "",
                                             &first, &error),
                 APL_OK);
    cr_assert_eq(apl_event_repository_create(&db.connection, a.id, 0, "Second", "2026-01-02", "",
                                             "", &second, &error),
                 APL_OK);
    cr_assert_eq(apl_event_repository_create(&db.connection, b.id, 0, "Other", "2026-01-03", "", "",
                                             &other, &error),
                 APL_OK);

    apl_event_session *rows = nullptr;
    size_t count = 99;
    cr_assert_eq(apl_session_repository_list(&db.connection, a.id, first.id, &rows, &count, &error),
                 APL_OK);
    cr_assert_eq(count, 0);
    cr_assert_null(rows);

    apl_event_session session = {};
    cr_assert_eq(
        apl_session_repository_create(&db.connection, a.id, other.id, "Race", &session, &error),
        APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(
        apl_session_repository_create(&db.connection, a.id, first.id, "  ", &session, &error),
        APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_session_repository_create(&db.connection, a.id, first.id, " Practice ",
                                               &session, &error),
                 APL_OK);
    const int64_t id = session.id;
    cr_assert_str_eq(session.label, " Practice ");
    cr_assert_eq(session.event_id, first.id);
    cr_assert_eq(apl_session_repository_get(&db.connection, b.id, first.id, id, &session, &error),
                 APL_ERR_NOT_FOUND);
    cr_assert_eq(apl_session_repository_get(&db.connection, a.id, second.id, id, &session, &error),
                 APL_ERR_NOT_FOUND);
    cr_assert_eq(apl_session_repository_update(&db.connection, a.id, second.id, id, "Hijack",
                                               &session, &error),
                 APL_ERR_NOT_FOUND);
    cr_assert_eq(apl_session_repository_update(&db.connection, a.id, first.id, id, "Qualifying",
                                               &session, &error),
                 APL_OK);
    cr_assert_eq(session.id, id);
    cr_assert_eq(apl_session_repository_get(&db.connection, a.id, first.id, id, &session, &error),
                 APL_OK);
    cr_assert_str_eq(session.label, "Qualifying");
    cr_assert_eq(apl_session_repository_list(&db.connection, a.id, first.id, &rows, &count, &error),
                 APL_OK);
    cr_assert_eq(count, 1);
    cr_assert_eq(rows[0].id, id);
    apl_session_repository_free_list(rows);
    cr_assert_eq(
        apl_session_repository_list(&db.connection, a.id, second.id, &rows, &count, &error),
        APL_OK);
    cr_assert_eq(count, 0);
    apl_session_repository_free_list(rows);

    sqlite3_stmt *statement = nullptr;
    cr_assert_eq(sqlite3_prepare_v2(db.connection.handle,
                                    "UPDATE event_sessions SET event_id=?1 WHERE id=?2", -1,
                                    &statement, nullptr),
                 SQLITE_OK);
    sqlite3_bind_int64(statement, 1, other.id);
    sqlite3_bind_int64(statement, 2, id);
    cr_assert_eq(sqlite3_step(statement) & 0xff, SQLITE_CONSTRAINT);
    sqlite3_finalize(statement);
    cr_assert_eq(apl_session_repository_get(&db.connection, a.id, first.id, id, &session, &error),
                 APL_OK);
    apl_db_close(&db.connection);
    cr_assert_eq(apl_db_open(&db.connection, db.path, &error), APL_OK);
    cr_assert_eq(apl_session_repository_get(&db.connection, a.id, first.id, id, &session, &error),
                 APL_OK);
    cr_assert_str_eq(session.label, "Qualifying");
    cr_assert_eq(apl_test_db_close(&db, &error), APL_OK);
}
