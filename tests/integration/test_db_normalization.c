#include <criterion/criterion.h>
#include <stdio.h>

#include "db/migrations.h"
#include "db/outcome_correction_repository.h"
#include "db/outcome_repository.h"
#include "db/part_unit_repository.h"
#include "db/session_repository.h"
#include "db_fixture.h"

static void exec_sql(apl_test_db *const db, const char *const sql)
{
    cr_assert_eq(sqlite3_exec(db->connection.handle, sql, nullptr, nullptr, nullptr), SQLITE_OK,
                 "%s", sqlite3_errmsg(db->connection.handle));
}

static int scalar(apl_test_db *const db, const char *const sql)
{
    sqlite3_stmt *statement = nullptr;
    cr_assert_eq(sqlite3_prepare_v2(db->connection.handle, sql, -1, &statement, nullptr), SQLITE_OK,
                 "%s", sqlite3_errmsg(db->connection.handle));
    cr_assert_eq(sqlite3_step(statement), SQLITE_ROW);
    const int value = sqlite3_column_int(statement, 0);
    sqlite3_finalize(statement);
    return value;
}

/* Start from the actual previous production schema, not a hand-written copy. */
static void open_previous(apl_test_db *const db)
{
    apl_error error = {};
    cr_assert_eq(apl_test_temp_dir(db->directory, sizeof(db->directory), &error), APL_OK);
    (void)snprintf(db->path, sizeof(db->path), "%s/data.sqlite3", db->directory);
    cr_assert_eq(apl_db_open(&db->connection, db->path, &error), APL_OK);
    size_t count = 0;
    const apl_db_migration *const list = apl_db_migrations_list(&count);
    cr_assert_gt(count, 23);
    cr_assert_eq(apl_db_migrations_run_list(&db->connection, list, 23, &error), APL_OK);
    exec_sql(
        db,
        "INSERT INTO teams(id,name,created_at,updated_at) VALUES "
        "(1,'First','created','updated'),(2,'Second','created','updated');"
        "INSERT INTO drivers(id,name,created_at,updated_at) VALUES "
        "(3,'Driver','created','updated');"
        "INSERT INTO team_driver_memberships(team_id,driver_id,start_date,created_at,updated_at) "
        "VALUES(1,3,'2026-01-01','created','updated');"
        "INSERT INTO part_types(id,team_id,category,category_key,name,name_key,created_at,"
        "updated_at) VALUES(4,1,'Category','category','Type','type','created','updated');"
        "INSERT INTO part_units(id,team_id,type_id,serial_number,archived,created_at,updated_at,"
        "archived_at) VALUES(5,1,4,'Serial',1,'created','updated','archived');"
        "INSERT INTO lifecycle_events(id,team_id,action,event_date,unit_id,created_at) "
        "VALUES(6,1,'repair','2026-01-02',5,'created');"
        "INSERT INTO events(id,team_id,name,event_date,created_at,updated_at) "
        "VALUES(7,1,'Event','2026-01-03','created','updated');"
        "INSERT INTO event_sessions(id,team_id,event_id,label,created_at,updated_at) "
        "VALUES(8,1,7,'Session','created','updated');"
        "INSERT INTO driver_outcomes(id,team_id,event_id,session_id,driver_id,laps,status,"
        "created_at,updated_at) VALUES(9,1,7,8,3,0,'DNF','created','updated');"
        "INSERT INTO outcome_corrections(id,team_id,outcome_id,actor,occurred_at,"
        "old_laps,new_laps,new_status) VALUES(10,1,9,'local user','corrected',2,0,'DNF');");
}

static void expect_constraint(apl_test_db *const db, const char *const sql)
{
    cr_assert_eq(sqlite3_exec(db->connection.handle, sql, nullptr, nullptr, nullptr) & 0xff,
                 SQLITE_CONSTRAINT, "unexpected success: %s", sql);
}

Test(db_normalization, populated_previous_schema_keeps_ids_values_and_history)
{
    apl_test_db db = {};
    open_previous(&db);
    apl_error error = {};
    cr_assert_eq(apl_db_migrate(&db.connection, &error), APL_OK, "%s", apl_error_message(&error));
    cr_assert_eq(apl_db_migrate(&db.connection, &error), APL_OK);
    apl_db_close(&db.connection);
    cr_assert_eq(apl_db_open(&db.connection, db.path, &error), APL_OK);
    cr_assert_eq(scalar(&db, "SELECT count(*) FROM pragma_foreign_key_check"), 0);
    cr_assert_eq(scalar(&db, "PRAGMA foreign_keys"), 1);
    cr_assert_eq(scalar(&db, "SELECT count(*) FROM pragma_table_info('part_units') "
                             "WHERE name='team_id'"),
                 0);
    cr_assert_eq(scalar(&db, "SELECT count(*) FROM pragma_table_info('event_sessions') "
                             "WHERE name='team_id'"),
                 0);
    cr_assert_eq(scalar(&db, "SELECT count(*) FROM pragma_table_info('driver_outcomes') "
                             "WHERE name IN ('team_id','event_id')"),
                 0);
    cr_assert_eq(scalar(&db, "SELECT count(*) FROM pragma_table_info('outcome_corrections') "
                             "WHERE name='team_id'"),
                 0);
    apl_part_unit unit = {};
    cr_assert_eq(apl_part_unit_repository_get(&db.connection, 1, 5, &unit, &error), APL_OK);
    cr_assert_eq(unit.team_id, 1);
    cr_assert_eq(unit.type_id, 4);
    cr_assert_str_eq(unit.serial_number, "Serial");
    cr_assert(unit.meta.archived);
    cr_assert_str_eq(unit.meta.archived_at, "archived");
    apl_event_session session = {};
    cr_assert_eq(apl_session_repository_get(&db.connection, 1, 7, 8, &session, &error), APL_OK);
    cr_assert_str_eq(session.label, "Session");
    apl_outcome outcome = {};
    cr_assert_eq(apl_outcome_repository_get(&db.connection, 1, 9, &outcome, &error), APL_OK);
    cr_assert_eq(outcome.event_id, 7);
    cr_assert_eq(outcome.session_id, 8);
    cr_assert_eq(outcome.driver_id, 3);
    cr_assert(outcome.has_laps);
    cr_assert_eq(outcome.laps, 0);
    cr_assert_not(outcome.has_finish_position);
    cr_assert_str_eq(outcome.status, "DNF");
    cr_assert_str_eq(outcome.created_at, "created");
    apl_outcome_correction *corrections = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_outcome_correction_list(&db.connection, 1, 9, &corrections, &count, &error),
                 APL_OK);
    cr_assert_eq(count, 1);
    cr_assert_eq(corrections[0].id, 10);
    cr_assert_eq(corrections[0].before.laps, 2);
    cr_assert_eq(corrections[0].after.laps, 0);
    cr_assert_str_eq(corrections[0].occurred_at, "corrected");
    apl_outcome_correction_free_list(corrections);
    cr_assert_eq(scalar(&db, "SELECT count(*) FROM lifecycle_events WHERE id=6 AND unit_id=5"), 1);
    cr_assert_eq(apl_test_db_close(&db, &error), APL_OK);
}

Test(db_normalization, normalized_schema_enforces_parent_scope_and_append_only_history)
{
    apl_test_db db = {};
    open_previous(&db);
    apl_error error = {};
    cr_assert_eq(apl_db_migrate(&db.connection, &error), APL_OK);
    exec_sql(&db,
             "INSERT INTO part_types(id,team_id,category,category_key,name,name_key,created_at,"
             "updated_at) VALUES(11,2,'Category','category','Other','other','created','updated');"
             "INSERT INTO events(id,team_id,name,event_date,created_at,updated_at) "
             "VALUES(12,2,'Other','2026-01-03','created','updated');"
             "INSERT INTO event_sessions(id,event_id,label,created_at,updated_at) "
             "VALUES(13,12,'Other','created','updated');");
    expect_constraint(&db, "UPDATE part_types SET team_id=2 WHERE id=4");
    expect_constraint(&db, "UPDATE part_units SET type_id=11 WHERE id=5");
    expect_constraint(&db, "UPDATE events SET team_id=2 WHERE id=7");
    expect_constraint(&db, "UPDATE event_sessions SET event_id=12 WHERE id=8");
    expect_constraint(&db, "UPDATE driver_outcomes SET session_id=13 WHERE id=9");
    expect_constraint(&db, "DELETE FROM team_driver_memberships WHERE driver_id=3");
    expect_constraint(&db, "UPDATE outcome_corrections SET new_laps=100 WHERE id=10");
    expect_constraint(&db, "DELETE FROM outcome_corrections WHERE id=10");
    expect_constraint(&db, "DELETE FROM driver_outcomes WHERE id=9");
    expect_constraint(&db, "INSERT INTO part_units(type_id,created_at,updated_at) "
                           "VALUES(999,'created','updated')");
    expect_constraint(&db, "INSERT INTO outcome_corrections(outcome_id,actor,occurred_at) "
                           "VALUES(999,'local user','created')");
    expect_constraint(&db,
                      "INSERT INTO lifecycle_events(team_id,action,event_date,unit_id,created_at) "
                      "VALUES(2,'repair','2026-01-03',5,'created')");
    const apl_outcome before = {.id = 9, .team_id = 2};
    const apl_outcome after = before;
    cr_assert_eq(apl_outcome_correction_append(&db.connection, &before, &after, &error),
                 APL_ERR_NOT_FOUND);
    cr_assert_str_eq(apl_error_message(&error), "outcome not found for this team");
    cr_assert_eq(scalar(&db, "SELECT count(*) FROM outcome_corrections"), 1);
    cr_assert_eq(scalar(&db, "SELECT count(*) FROM pragma_foreign_key_check"), 0);
    cr_assert_eq(apl_test_db_close(&db, &error), APL_OK);
}

Test(db_normalization, conflicting_legacy_ownership_aborts_without_discarding_data)
{
    apl_test_db db = {};
    open_previous(&db);
    /* The old schema allowed a parent owner change to contradict its units. */
    exec_sql(&db, "UPDATE part_types SET team_id=2 WHERE id=4");
    apl_error error = {};
    cr_assert_neq(apl_db_migrate(&db.connection, &error), APL_OK);
    cr_assert_eq(scalar(&db, "SELECT max(version) FROM apl_schema_migrations"), 23);
    cr_assert_eq(scalar(&db, "SELECT team_id FROM part_units WHERE id=5"), 1);
    cr_assert_eq(scalar(&db, "SELECT count(*) FROM outcome_corrections WHERE id=10"), 1);
    /* A failed migration is atomic and can be retried after explicit repair. */
    exec_sql(&db, "UPDATE part_types SET team_id=1 WHERE id=4");
    cr_assert_eq(apl_db_migrate(&db.connection, &error), APL_OK);
    cr_assert_eq(apl_test_db_close(&db, &error), APL_OK);
}
