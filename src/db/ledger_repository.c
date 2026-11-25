#include "db/ledger_repository.h"

#include <sqlite3.h>
#include <stdckdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/util.h"
#include "db/sqlite_util.h"

#define ENTRY_SELECT                                                                               \
    "SELECT e.id, e.team_id, e.amount_minor, e.currency, e.entry_date, e.kind, e.category, "       \
    "e.category_key, e.description, e.corrects_entry_id, e.created_at, "                           \
    "COALESCE(original.kind, e.kind) "                                                             \
    "FROM ledger_entries e LEFT JOIN ledger_entries original ON original.id = e.corrects_entry_id "

static void read_entry(sqlite3_stmt *const statement, apl_ledger_entry *const entry)
{
    *entry = (apl_ledger_entry){};
    entry->id = sqlite3_column_int64(statement, 0);
    entry->team_id = sqlite3_column_int64(statement, 1);
    entry->amount.minor_units = sqlite3_column_int64(statement, 2);
    apl_db_copy_column_text(statement, 3, entry->amount.currency, sizeof(entry->amount.currency));
    apl_db_copy_column_text(statement, 4, entry->entry_date, sizeof(entry->entry_date));
    entry->kind = apl_ledger_kind_from_code((const char *)sqlite3_column_text(statement, 5));
    apl_db_copy_column_text(statement, 6, entry->category, sizeof(entry->category));
    apl_db_copy_column_text(statement, 7, entry->category_key, sizeof(entry->category_key));
    apl_db_copy_column_text(statement, 8, entry->description, sizeof(entry->description));
    entry->corrects_entry_id =
        sqlite3_column_type(statement, 9) == SQLITE_NULL ? 0 : sqlite3_column_int64(statement, 9);
    apl_db_copy_column_text(statement, 10, entry->created_at, sizeof(entry->created_at));
    entry->classification =
        apl_ledger_kind_from_code((const char *)sqlite3_column_text(statement, 11));
}

static apl_status prepare(sqlite3 *const handle, const char *const sql, sqlite3_stmt **const out,
                          apl_error *const error)
{
    if (sqlite3_prepare_v2(handle, sql, -1, out, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare ledger operation: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_DB_QUERY;
    }
    return APL_OK;
}

static apl_status write_error(sqlite3 *const handle, apl_error *const error)
{
    const int extended = sqlite3_extended_errcode(handle);
    if (extended == SQLITE_CONSTRAINT_UNIQUE || extended == SQLITE_CONSTRAINT_PRIMARYKEY) {
        apl_error_set(error, APL_ERR_CONFLICT,
                      "this ledger entry conflicts with an existing entry (an entry can have only "
                      "one reversal, and a team can have only one opening balance)");
        return APL_ERR_CONFLICT;
    }
    if (extended == SQLITE_CONSTRAINT_FOREIGNKEY) {
        apl_error_set(error, APL_ERR_NOT_FOUND,
                      "the owning team or corrected entry no longer exists");
        return APL_ERR_NOT_FOUND;
    }
    if (extended == SQLITE_CONSTRAINT_CHECK || extended == SQLITE_CONSTRAINT_TRIGGER) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "ledger data violates a stored rule: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_INVALID_ARGUMENT;
    }
    apl_error_set(error, APL_ERR_DB_QUERY, "ledger database operation failed: %s",
                  sqlite3_errmsg(handle));
    return APL_ERR_DB_QUERY;
}

apl_status apl_ledger_repository_create(const apl_db_connection *const connection,
                                        const apl_ledger_entry *const entry,
                                        apl_ledger_entry *const out_entry, apl_error *const error)
{
    if (connection == nullptr || entry == nullptr || out_entry == nullptr || entry->team_id <= 0 ||
        (!apl_ledger_kind_is_postable(entry->kind) && entry->kind != APL_LEDGER_KIND_REVERSAL &&
         entry->kind != APL_LEDGER_KIND_ADJUSTMENT)) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "ledger create arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_entry = (apl_ledger_entry){};
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }

    sqlite3_stmt *statement = nullptr;
    apl_status status =
        prepare(handle,
                "INSERT INTO ledger_entries (team_id, amount_minor, currency, entry_date, kind, "
                "category, category_key, description, corrects_entry_id, created_at) VALUES "
                "(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, "
                "strftime('%Y-%m-%dT%H:%M:%fZ', 'now')) RETURNING id;",
                &statement, error);
    if (status != APL_OK) {
        return status;
    }
    int bind_status = sqlite3_bind_int64(statement, 1, entry->team_id);
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_int64(statement, 2, entry->amount.minor_units);
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_text(statement, 3, entry->amount.currency, -1, SQLITE_TRANSIENT);
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_text(statement, 4, entry->entry_date, -1, SQLITE_TRANSIENT);
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_text(statement, 5, apl_ledger_kind_code(entry->kind), -1,
                                        SQLITE_TRANSIENT);
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_text(statement, 6, entry->category, -1, SQLITE_TRANSIENT);
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_text(statement, 7, entry->category_key, -1, SQLITE_TRANSIENT);
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_text(statement, 8, entry->description, -1, SQLITE_TRANSIENT);
    }
    if (bind_status == SQLITE_OK) {
        if (entry->corrects_entry_id > 0) {
            bind_status = sqlite3_bind_int64(statement, 9, entry->corrects_entry_id);
        } else {
            bind_status = sqlite3_bind_null(statement, 9);
        }
    }
    if (bind_status != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot bind ledger entry: %s",
                      sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }

    const int step_status = sqlite3_step(statement);
    if (step_status != SQLITE_ROW) {
        status = write_error(handle, error);
        sqlite3_finalize(statement);
        return status;
    }
    const int64_t id = sqlite3_column_int64(statement, 0);
    sqlite3_finalize(statement);
    return apl_ledger_repository_get(connection, entry->team_id, id, out_entry, error);
}

apl_status apl_ledger_repository_get(const apl_db_connection *const connection,
                                     const int64_t team_id, const int64_t id,
                                     apl_ledger_entry *const out_entry, apl_error *const error)
{
    if (connection == nullptr || out_entry == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "ledger get arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_entry = (apl_ledger_entry){};
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }

    sqlite3_stmt *statement = nullptr;
    apl_status status =
        prepare(handle, ENTRY_SELECT "WHERE e.id = ?2 AND e.team_id = ?1;", &statement, error);
    if (status != APL_OK) {
        return status;
    }
    sqlite3_bind_int64(statement, 1, team_id);
    sqlite3_bind_int64(statement, 2, id);
    const int step_status = sqlite3_step(statement);
    if (step_status == SQLITE_DONE) {
        sqlite3_finalize(statement);
        apl_error_set(error, APL_ERR_NOT_FOUND, "ledger entry %lld was not found for this team",
                      (long long)id);
        return APL_ERR_NOT_FOUND;
    }
    if (step_status != SQLITE_ROW) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot read ledger entry: %s",
                      sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }
    read_entry(statement, out_entry);
    sqlite3_finalize(statement);
    return APL_OK;
}

apl_status apl_ledger_repository_list(const apl_db_connection *const connection,
                                      const apl_ledger_filter *const filter,
                                      apl_ledger_entry **const out_entries, size_t *const out_count,
                                      apl_error *const error)
{
    if (connection == nullptr || out_entries == nullptr || out_count == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "ledger list arguments are nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_entries = nullptr;
    *out_count = 0;
    if (filter == nullptr || filter->team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "ledger list requires a team scope");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }

    sqlite3_stmt *statement = nullptr;
    apl_status status = prepare(handle,
                                ENTRY_SELECT "WHERE e.team_id = ?1 "
                                             "AND (?2 = '' OR e.entry_date >= ?2) "
                                             "AND (?3 = '' OR e.entry_date <= ?3) "
                                             "AND (?4 = '' OR e.category_key = ?4) "
                                             "AND (?5 = '' OR e.kind = ?5) "
                                             "ORDER BY e.entry_date DESC, e.id DESC;",
                                &statement, error);
    if (status != APL_OK) {
        return status;
    }
    const char *const kind_code =
        (filter->kind == APL_LEDGER_KIND_NONE) ? "" : apl_ledger_kind_code(filter->kind);
    int bind_status = sqlite3_bind_int64(statement, 1, filter->team_id);
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_text(statement, 2, filter->from_date, -1, SQLITE_TRANSIENT);
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_text(statement, 3, filter->to_date, -1, SQLITE_TRANSIENT);
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_text(statement, 4, filter->category_key, -1, SQLITE_TRANSIENT);
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_text(statement, 5, kind_code, -1, SQLITE_TRANSIENT);
    }
    if (bind_status != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot bind ledger filters: %s",
                      sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }

    apl_ledger_entry *entries = nullptr;
    size_t count = 0;
    size_t capacity = 0;
    for (;;) {
        const int step_status = sqlite3_step(statement);
        if (step_status == SQLITE_DONE) {
            break;
        }
        if (step_status != SQLITE_ROW) {
            apl_error_set(error, APL_ERR_DB_QUERY, "cannot read ledger list: %s",
                          sqlite3_errmsg(handle));
            status = APL_ERR_DB_QUERY;
            break;
        }
        if (count == capacity) {
            typeof(entries) const grown = apl_grow_array(entries, &capacity, 16, sizeof(*entries));
            if (grown == nullptr) {
                apl_error_set(error, APL_ERR_OUT_OF_MEMORY, "cannot allocate ledger list");
                status = APL_ERR_OUT_OF_MEMORY;
                break;
            }
            entries = grown;
        }
        read_entry(statement, &entries[count]);
        ++count;
    }
    sqlite3_finalize(statement);
    if (status != APL_OK) {
        free(entries);
        return status;
    }
    *out_entries = entries;
    *out_count = count;
    return APL_OK;
}

apl_status apl_ledger_repository_find_reversal(const apl_db_connection *const connection,
                                               const int64_t team_id, const int64_t original_id,
                                               apl_ledger_entry *const out_entry,
                                               bool *const out_found, apl_error *const error)
{
    if (connection == nullptr || out_entry == nullptr || out_found == nullptr || team_id <= 0 ||
        original_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "reversal lookup arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_entry = (apl_ledger_entry){};
    *out_found = false;
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }

    sqlite3_stmt *statement = nullptr;
    apl_status status = prepare(
        handle,
        ENTRY_SELECT "WHERE e.team_id = ?1 AND e.corrects_entry_id = ?2 AND e.kind = 'reversal' "
                     "ORDER BY e.id ASC LIMIT 1;",
        &statement, error);
    if (status != APL_OK) {
        return status;
    }
    sqlite3_bind_int64(statement, 1, team_id);
    sqlite3_bind_int64(statement, 2, original_id);
    const int step_status = sqlite3_step(statement);
    if (step_status == SQLITE_DONE) {
        sqlite3_finalize(statement);
        return APL_OK;
    }
    if (step_status != SQLITE_ROW) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot read ledger reversal: %s",
                      sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }
    read_entry(statement, out_entry);
    *out_found = true;
    sqlite3_finalize(statement);
    return APL_OK;
}

apl_status apl_ledger_repository_expense_totals(const apl_db_connection *const connection,
                                                const int64_t team_id, const char *const from_date,
                                                const char *const to_date,
                                                apl_ledger_category_total **const out_rows,
                                                size_t *const out_count, apl_error *const error)
{
    if (connection == nullptr || out_rows == nullptr || out_count == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "expense total arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_rows = nullptr;
    *out_count = 0;
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }

    sqlite3_stmt *statement = nullptr;
    apl_status status =
        prepare(handle,
                "SELECT e.category_key, e.category, e.amount_minor "
                "FROM ledger_entries e LEFT JOIN ledger_entries original "
                "ON original.id = e.corrects_entry_id "
                "WHERE e.team_id = ?1 "
                "AND (?2 = '' OR e.entry_date >= ?2) "
                "AND (?3 = '' OR e.entry_date <= ?3) "
                "AND CASE WHEN e.kind IN ('income', 'expense', 'opening_balance') THEN e.kind "
                "ELSE original.kind END = 'expense' "
                "ORDER BY e.category_key ASC, e.id ASC;",
                &statement, error);
    if (status != APL_OK) {
        return status;
    }
    const char *const from = (from_date != nullptr) ? from_date : "";
    const char *const to = (to_date != nullptr) ? to_date : "";
    sqlite3_bind_int64(statement, 1, team_id);
    sqlite3_bind_text(statement, 2, from, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, to, -1, SQLITE_TRANSIENT);

    apl_ledger_category_total *rows = nullptr;
    size_t count = 0;
    size_t capacity = 0;
    for (;;) {
        const int step_status = sqlite3_step(statement);
        if (step_status == SQLITE_DONE) {
            break;
        }
        if (step_status != SQLITE_ROW) {
            apl_error_set(error, APL_ERR_DB_QUERY, "cannot read expense totals: %s",
                          sqlite3_errmsg(handle));
            status = APL_ERR_DB_QUERY;
            break;
        }
        char key[APL_LEDGER_CATEGORY_MAX];
        char category[APL_LEDGER_CATEGORY_MAX];
        apl_db_copy_column_text(statement, 0, key, sizeof(key));
        apl_db_copy_column_text(statement, 1, category, sizeof(category));
        const int64_t amount = sqlite3_column_int64(statement, 2);

        if (count == 0 || strcmp(rows[count - 1].category_key, key) != 0) {
            if (count == capacity) {
                typeof(rows) const grown = apl_grow_array(rows, &capacity, 8, sizeof(*rows));
                if (grown == nullptr) {
                    apl_error_set(error, APL_ERR_OUT_OF_MEMORY, "cannot allocate expense totals");
                    status = APL_ERR_OUT_OF_MEMORY;
                    break;
                }
                rows = grown;
            }
            rows[count] = (apl_ledger_category_total){};
            (void)snprintf(rows[count].category, sizeof(rows[count].category), "%s", category);
            (void)snprintf(rows[count].category_key, sizeof(rows[count].category_key), "%s", key);
            ++count;
        }
        int64_t sum = 0;
        if (ckd_add(&sum, rows[count - 1].total.minor_units, amount)) {
            apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "expense total is too large");
            apl_error_set_field(error, APL_FIELD_AMOUNT);
            status = APL_ERR_INVALID_ARGUMENT;
            break;
        }
        rows[count - 1].total.minor_units = sum;
    }
    sqlite3_finalize(statement);
    if (status != APL_OK) {
        free(rows);
        return status;
    }
    *out_rows = rows;
    *out_count = count;
    return APL_OK;
}

void apl_ledger_repository_free_list(apl_ledger_entry *const entries)
{
    free(entries);
}

void apl_ledger_repository_free_category_totals(apl_ledger_category_total *const rows)
{
    free(rows);
}
