#include "db/outcome_correction_repository.h"

#include <sqlite3.h>
#include <stdlib.h>

#include "core/util.h"
#include "db/sqlite_util.h"

static void bind_values(sqlite3_stmt *const s, const int offset, const apl_outcome *const o)
{
    if (o->has_start_position)
        sqlite3_bind_int(s, offset, o->start_position);
    if (o->has_finish_position)
        sqlite3_bind_int(s, offset + 1, o->finish_position);
    if (o->has_laps)
        sqlite3_bind_int(s, offset + 2, o->laps);
    if (o->has_status)
        sqlite3_bind_text(s, offset + 3, o->status, -1, SQLITE_TRANSIENT);
    if (o->has_notes)
        sqlite3_bind_text(s, offset + 4, o->notes, -1, SQLITE_TRANSIENT);
}

static void read_values(sqlite3_stmt *const s, const int offset, apl_outcome *const o)
{
    o->has_start_position = sqlite3_column_type(s, offset) != SQLITE_NULL;
    o->has_finish_position = sqlite3_column_type(s, offset + 1) != SQLITE_NULL;
    o->has_laps = sqlite3_column_type(s, offset + 2) != SQLITE_NULL;
    o->has_status = sqlite3_column_type(s, offset + 3) != SQLITE_NULL;
    o->has_notes = sqlite3_column_type(s, offset + 4) != SQLITE_NULL;
    o->start_position = sqlite3_column_int(s, offset);
    o->finish_position = sqlite3_column_int(s, offset + 1);
    o->laps = sqlite3_column_int(s, offset + 2);
    apl_db_copy_column_text(s, offset + 3, o->status, sizeof(o->status));
    apl_db_copy_column_text(s, offset + 4, o->notes, sizeof(o->notes));
}

apl_status apl_outcome_correction_append(const apl_db_connection *const connection,
                                         const apl_outcome *const before,
                                         const apl_outcome *const after, apl_error *const error)
{
    if (!connection || !apl_db_handle(connection) || !before || !after || before->id <= 0 ||
        before->id != after->id || before->team_id <= 0 || before->team_id != after->team_id) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "invalid outcome correction");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const db = apl_db_handle(connection);
    sqlite3_stmt *s = nullptr;
    const char *const sql = "INSERT INTO outcome_corrections(outcome_id,actor,occurred_at,"
                            "old_start,old_finish,old_laps,old_status,old_notes,"
                            "new_start,new_finish,new_laps,new_status,new_notes) "
                            "SELECT ?2,'local user',strftime('%Y-%m-%dT%H:%M:%fZ','now'),"
                            "?3,?4,?5,?6,?7,?8,?9,?10,?11,?12 WHERE EXISTS "
                            "(SELECT 1 FROM driver_outcomes_scoped WHERE id=?2 AND team_id=?1)";
    if (sqlite3_prepare_v2(db, sql, -1, &s, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare correction: %s", sqlite3_errmsg(db));
        return APL_ERR_DB_QUERY;
    }
    sqlite3_bind_int64(s, 1, before->team_id);
    sqlite3_bind_int64(s, 2, before->id);
    bind_values(s, 3, before);
    bind_values(s, 8, after);
    const int rc = sqlite3_step(s);
    apl_status status = APL_OK;
    if (rc != SQLITE_DONE) {
        status = APL_ERR_DB_QUERY;
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot record correction: %s", sqlite3_errmsg(db));
    } else if (sqlite3_changes(db) != 1) {
        status = APL_ERR_NOT_FOUND;
        apl_error_set(error, status, "outcome not found for this team");
    }
    sqlite3_finalize(s);
    return status;
}

apl_status apl_outcome_correction_list(const apl_db_connection *const connection,
                                       const int64_t team_id, const int64_t outcome_id,
                                       apl_outcome_correction **const out, size_t *const count,
                                       apl_error *const error)
{
    if (!connection || !apl_db_handle(connection) || team_id <= 0 || outcome_id <= 0 || !out ||
        !count) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "invalid correction list arguments");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out = nullptr;
    *count = 0;
    sqlite3 *const db = apl_db_handle(connection);
    sqlite3_stmt *s = nullptr;
    const char *const sql =
        "SELECT id,team_id,outcome_id,actor,occurred_at,"
        "old_start,old_finish,old_laps,old_status,old_notes,"
        "new_start,new_finish,new_laps,new_status,new_notes "
        "FROM outcome_corrections_scoped WHERE team_id=?1 AND outcome_id=?2 ORDER BY id";
    if (sqlite3_prepare_v2(db, sql, -1, &s, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare correction list: %s",
                      sqlite3_errmsg(db));
        return APL_ERR_DB_QUERY;
    }
    sqlite3_bind_int64(s, 1, team_id);
    sqlite3_bind_int64(s, 2, outcome_id);
    apl_outcome_correction *rows = nullptr;
    size_t capacity = 0;
    apl_status status = APL_OK;
    int rc;
    while ((rc = sqlite3_step(s)) == SQLITE_ROW) {
        if (*count == capacity) {
            apl_outcome_correction *const grown = apl_grow_array(rows, &capacity, 8, sizeof(*rows));
            if (!grown) {
                apl_error_set(error, APL_ERR_OUT_OF_MEMORY, "cannot allocate correction list");
                status = APL_ERR_OUT_OF_MEMORY;
                break;
            }
            rows = grown;
        }
        apl_outcome_correction *const row = &rows[(*count)++];
        *row = (apl_outcome_correction){};
        row->id = sqlite3_column_int64(s, 0);
        row->team_id = sqlite3_column_int64(s, 1);
        row->outcome_id = sqlite3_column_int64(s, 2);
        apl_db_copy_column_text(s, 3, row->actor, sizeof(row->actor));
        apl_db_copy_column_text(s, 4, row->occurred_at, sizeof(row->occurred_at));
        read_values(s, 5, &row->before);
        read_values(s, 10, &row->after);
    }
    if (status == APL_OK && rc != SQLITE_DONE) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot read corrections: %s", sqlite3_errmsg(db));
        status = APL_ERR_DB_QUERY;
    }
    sqlite3_finalize(s);
    if (status != APL_OK) {
        free(rows);
        *count = 0;
        return status;
    }
    *out = rows;
    return APL_OK;
}

void apl_outcome_correction_free_list(apl_outcome_correction *const corrections)
{
    free(corrections);
}
