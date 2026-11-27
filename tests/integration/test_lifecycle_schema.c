#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

#include <criterion/criterion.h>

#include "db/car_repository.h"
#include "db/part_type_repository.h"
#include "db/part_unit_repository.h"
#include "db/team_repository.h"
#include "db_fixture.h"
#include "domain/part.h"

static apl_team make_team(apl_test_db *const db, const char *const name)
{
    apl_error err;
    apl_error_init(&err);
    apl_team team = {};
    cr_assert_eq(apl_team_repository_create(&db->connection, name, "", &team, &err), APL_OK,
                 "team create failed: %s", apl_error_message(&err));
    return team;
}

static int64_t make_unit(apl_test_db *const db, const int64_t team_id, const char *const serial)
{
    apl_error err;
    apl_error_init(&err);
    apl_part_type type = {.team_id = team_id};
    (void)snprintf(type.category, sizeof(type.category), "Aero");
    (void)snprintf(type.category_key, sizeof(type.category_key), "aero");
    (void)snprintf(type.name, sizeof(type.name), "Wing %s", serial);
    (void)snprintf(type.name_key, sizeof(type.name_key), "wing %s", serial);
    apl_part_type created_type = {};
    cr_assert_eq(apl_part_type_repository_create(&db->connection, &type, &created_type, &err),
                 APL_OK);
    apl_part_unit unit = {.team_id = team_id, .type_id = created_type.id};
    (void)snprintf(unit.serial_number, sizeof(unit.serial_number), "%s", serial);
    apl_part_unit created = {};
    cr_assert_eq(apl_part_unit_repository_create(&db->connection, &unit, &created, &err), APL_OK);
    return created.id;
}

static int insert_event(apl_test_db *const db, const int64_t team_id, const char *const action,
                        const int64_t unit_id, const int64_t old_unit, const int64_t new_unit)
{
    char sql[1024];
    const char *const columns =
        "team_id, action, event_date, unit_id, old_unit_id, new_unit_id, position, event_label, "
        "reason, usage, notes, created_at";
    (void)snprintf(
        sql, sizeof(sql),
        "INSERT INTO lifecycle_events (%s) VALUES (%lld, '%s', '2026-01-01', %s, %s, %s, "
        "'', '', '', '', '', strftime('%%Y-%%m-%%dT%%H:%%M:%%fZ', 'now'));",
        columns, (long long)team_id, action, unit_id > 0 ? "?1" : "NULL",
        old_unit > 0 ? "?2" : "NULL", new_unit > 0 ? "?3" : "NULL");
    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(apl_db_handle(&db->connection), sql, -1, &statement, nullptr) !=
        SQLITE_OK) {
        return SQLITE_ERROR;
    }
    if (unit_id > 0) {
        sqlite3_bind_int64(statement, 1, unit_id);
    }
    if (old_unit > 0) {
        sqlite3_bind_int64(statement, 2, old_unit);
    }
    if (new_unit > 0) {
        sqlite3_bind_int64(statement, 3, new_unit);
    }
    const int result = sqlite3_step(statement);
    sqlite3_finalize(statement);
    return result;
}

Test(lifecycle_schema, creates_no_rows_and_accepts_action_shapes)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);
    const apl_team team = make_team(&db, "Ferrari");

    /* No lifecycle/fitment rows are seeded. */
    sqlite3_stmt *statement = nullptr;
    cr_assert_eq(sqlite3_prepare_v2(apl_db_handle(&db.connection),
                                    "SELECT COUNT(*) FROM lifecycle_events;", -1, &statement,
                                    nullptr),
                 SQLITE_OK);
    cr_assert_eq(sqlite3_step(statement), SQLITE_ROW);
    cr_assert_eq(sqlite3_column_int(statement, 0), 0);
    sqlite3_finalize(statement);

    const int64_t old_unit = make_unit(&db, team.id, "FW-100");
    const int64_t new_unit = make_unit(&db, team.id, "FW-101");

    cr_assert_eq(insert_event(&db, team.id, "install", old_unit, 0, 0), SQLITE_DONE);
    cr_assert_eq(insert_event(&db, team.id, "repair", old_unit, 0, 0), SQLITE_DONE);
    cr_assert_eq(insert_event(&db, team.id, "replace", 0, old_unit, new_unit), SQLITE_DONE,
                 "replace references both old/new units in one row");

    /* A non-replace action must carry unit_id; inserting a replace-shaped row
     * without old/new is allowed by schema (validation lives in use cases). */
    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}

Test(lifecycle_schema, rejects_unknown_action_and_cross_team_units)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);
    const apl_team ferrari = make_team(&db, "Ferrari");
    const apl_team mclaren = make_team(&db, "McLaren");
    const int64_t ferrari_unit = make_unit(&db, ferrari.id, "FW-100");

    /* Unknown action is rejected by the CHECK constraint. */
    cr_assert_eq(insert_event(&db, ferrari.id, "bogus", ferrari_unit, 0, 0), SQLITE_CONSTRAINT);

    /* Cross-team unit reference is rejected by the same-team trigger. */
    cr_assert_eq(insert_event(&db, mclaren.id, "install", ferrari_unit, 0, 0), SQLITE_CONSTRAINT);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}

Test(lifecycle_action_registry, maps_codes_and_labels)
{
    cr_assert_eq(apl_lifecycle_action_from_code("install"), APL_LIFECYCLE_INSTALL);
    cr_assert_eq(apl_lifecycle_action_from_code("dispose"), APL_LIFECYCLE_DISPOSE);
    cr_assert_eq(apl_lifecycle_action_from_code("bogus"), APL_LIFECYCLE_NONE);
    cr_assert_eq(apl_lifecycle_action_from_code(nullptr), APL_LIFECYCLE_NONE);
    cr_assert_str_eq(apl_lifecycle_action_code(APL_LIFECYCLE_REPLACE), "replace");
    cr_assert_str_eq(apl_lifecycle_action_label(APL_LIFECYCLE_REPAIR), "Repair");
    cr_assert_str_eq(apl_lifecycle_action_code(APL_LIFECYCLE_NONE), "");
}