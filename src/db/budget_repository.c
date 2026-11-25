#include "db/budget_repository.h"

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>

#include "core/util.h"
#include "db/sqlite_util.h"

#define PLAN_COLUMNS                                                                               \
    "id, team_id, season_id, amount_minor, currency, start_date, end_date, category, "             \
    "category_key, notes, created_at, updated_at"

static void read_plan(sqlite3_stmt *const statement, apl_budget_plan *const plan)
{
    *plan = (apl_budget_plan){};
    plan->id = sqlite3_column_int64(statement, 0);
    plan->team_id = sqlite3_column_int64(statement, 1);
    plan->season_id =
        sqlite3_column_type(statement, 2) == SQLITE_NULL ? 0 : sqlite3_column_int64(statement, 2);
    plan->amount.minor_units = sqlite3_column_int64(statement, 3);
    apl_db_copy_column_text(statement, 4, plan->amount.currency, sizeof(plan->amount.currency));
    apl_db_copy_column_text(statement, 5, plan->start_date, sizeof(plan->start_date));
    apl_db_copy_column_text(statement, 6, plan->end_date, sizeof(plan->end_date));
    apl_db_copy_column_text(statement, 7, plan->category, sizeof(plan->category));
    apl_db_copy_column_text(statement, 8, plan->category_key, sizeof(plan->category_key));
    apl_db_copy_column_text(statement, 9, plan->notes, sizeof(plan->notes));
    apl_db_copy_column_text(statement, 10, plan->created_at, sizeof(plan->created_at));
    apl_db_copy_column_text(statement, 11, plan->updated_at, sizeof(plan->updated_at));
}

static apl_status prepare(sqlite3 *const handle, const char *const sql, sqlite3_stmt **const out,
                          apl_error *const error)
{
    if (sqlite3_prepare_v2(handle, sql, -1, out, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare budget operation: %s",
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
                      "a budget plan already exists for this period and category");
        return APL_ERR_CONFLICT;
    }
    if (extended == SQLITE_CONSTRAINT_FOREIGNKEY) {
        apl_error_set(error, APL_ERR_NOT_FOUND, "the owning team or season no longer exists");
        return APL_ERR_NOT_FOUND;
    }
    if (extended == SQLITE_CONSTRAINT_CHECK || extended == SQLITE_CONSTRAINT_TRIGGER) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "budget data violates a stored rule: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_INVALID_ARGUMENT;
    }
    apl_error_set(error, APL_ERR_DB_QUERY, "budget database operation failed: %s",
                  sqlite3_errmsg(handle));
    return APL_ERR_DB_QUERY;
}

static apl_status bind_plan(sqlite3_stmt *const statement, const int first_index,
                            const apl_budget_plan *const plan)
{
    const int last = first_index + 8;
    int bind_status = sqlite3_bind_int64(statement, first_index, plan->team_id);
    if (bind_status == SQLITE_OK) {
        if (plan->season_id > 0) {
            bind_status = sqlite3_bind_int64(statement, first_index + 1, plan->season_id);
        } else {
            bind_status = sqlite3_bind_null(statement, first_index + 1);
        }
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_int64(statement, first_index + 2, plan->amount.minor_units);
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_text(statement, first_index + 3, plan->amount.currency, -1,
                                        SQLITE_TRANSIENT);
    }
    if (bind_status == SQLITE_OK) {
        bind_status =
            sqlite3_bind_text(statement, first_index + 4, plan->start_date, -1, SQLITE_TRANSIENT);
    }
    if (bind_status == SQLITE_OK) {
        bind_status =
            sqlite3_bind_text(statement, first_index + 5, plan->end_date, -1, SQLITE_TRANSIENT);
    }
    if (bind_status == SQLITE_OK) {
        bind_status =
            sqlite3_bind_text(statement, first_index + 6, plan->category, -1, SQLITE_TRANSIENT);
    }
    if (bind_status == SQLITE_OK) {
        bind_status =
            sqlite3_bind_text(statement, first_index + 7, plan->category_key, -1, SQLITE_TRANSIENT);
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_text(statement, last, plan->notes, -1, SQLITE_TRANSIENT);
    }
    return (apl_status)bind_status;
}

apl_status apl_budget_repository_create(const apl_db_connection *const connection,
                                        const apl_budget_plan *const plan,
                                        apl_budget_plan *const out_plan, apl_error *const error)
{
    if (connection == nullptr || plan == nullptr || out_plan == nullptr || plan->team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "budget create arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_plan = (apl_budget_plan){};
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }

    sqlite3_stmt *statement = nullptr;
    apl_status status =
        prepare(handle,
                "INSERT INTO budget_plans (team_id, season_id, amount_minor, currency, start_date, "
                "end_date, category, category_key, notes, created_at, updated_at) VALUES "
                "(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, "
                "strftime('%Y-%m-%dT%H:%M:%fZ', 'now'), "
                "strftime('%Y-%m-%dT%H:%M:%fZ', 'now')) RETURNING " PLAN_COLUMNS ";",
                &statement, error);
    if (status != APL_OK) {
        return status;
    }
    if (bind_plan(statement, 1, plan) != APL_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot bind budget plan: %s",
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
    read_plan(statement, out_plan);
    sqlite3_finalize(statement);
    return APL_OK;
}

apl_status apl_budget_repository_get(const apl_db_connection *const connection,
                                     const int64_t team_id, const int64_t id,
                                     apl_budget_plan *const out_plan, apl_error *const error)
{
    if (connection == nullptr || out_plan == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "budget get arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_plan = (apl_budget_plan){};
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }

    sqlite3_stmt *statement = nullptr;
    apl_status status = prepare(handle,
                                "SELECT " PLAN_COLUMNS " FROM budget_plans "
                                "WHERE id = ?2 AND team_id = ?1;",
                                &statement, error);
    if (status != APL_OK) {
        return status;
    }
    sqlite3_bind_int64(statement, 1, team_id);
    sqlite3_bind_int64(statement, 2, id);
    const int step_status = sqlite3_step(statement);
    if (step_status == SQLITE_DONE) {
        sqlite3_finalize(statement);
        apl_error_set(error, APL_ERR_NOT_FOUND, "budget plan %lld was not found for this team",
                      (long long)id);
        return APL_ERR_NOT_FOUND;
    }
    if (step_status != SQLITE_ROW) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot read budget plan: %s",
                      sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }
    read_plan(statement, out_plan);
    sqlite3_finalize(statement);
    return APL_OK;
}

apl_status apl_budget_repository_update(const apl_db_connection *const connection,
                                        const int64_t team_id, const int64_t id,
                                        const apl_budget_plan *const plan,
                                        apl_budget_plan *const out_plan, apl_error *const error)
{
    if (connection == nullptr || plan == nullptr || out_plan == nullptr || team_id <= 0 ||
        id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "budget update arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_plan = (apl_budget_plan){};
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }

    apl_budget_plan bound = *plan;
    bound.team_id = team_id;
    sqlite3_stmt *statement = nullptr;
    apl_status status =
        prepare(handle,
                "UPDATE budget_plans SET season_id = ?3, amount_minor = ?4, currency = ?5, "
                "start_date = ?6, end_date = ?7, category = ?8, category_key = ?9, notes = ?10, "
                "updated_at = strftime('%Y-%m-%dT%H:%M:%fZ', 'now') "
                "WHERE id = ?2 AND team_id = ?1 RETURNING " PLAN_COLUMNS ";",
                &statement, error);
    if (status != APL_OK) {
        return status;
    }
    /* Bind team (?1), id (?2), then the plan payload (?3..?10). */
    int bind_status = sqlite3_bind_int64(statement, 1, team_id);
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_int64(statement, 2, id);
    }
    if (bind_status == SQLITE_OK && bound.season_id > 0) {
        bind_status = sqlite3_bind_int64(statement, 3, bound.season_id);
    } else if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_null(statement, 3);
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_int64(statement, 4, bound.amount.minor_units);
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_text(statement, 5, bound.amount.currency, -1, SQLITE_TRANSIENT);
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_text(statement, 6, bound.start_date, -1, SQLITE_TRANSIENT);
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_text(statement, 7, bound.end_date, -1, SQLITE_TRANSIENT);
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_text(statement, 8, bound.category, -1, SQLITE_TRANSIENT);
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_text(statement, 9, bound.category_key, -1, SQLITE_TRANSIENT);
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_text(statement, 10, bound.notes, -1, SQLITE_TRANSIENT);
    }
    if (bind_status != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot bind budget update: %s",
                      sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }

    const int step_status = sqlite3_step(statement);
    if (step_status == SQLITE_DONE) {
        sqlite3_finalize(statement);
        apl_error_set(error, APL_ERR_NOT_FOUND, "budget plan %lld was not found for this team",
                      (long long)id);
        return APL_ERR_NOT_FOUND;
    }
    if (step_status != SQLITE_ROW) {
        status = write_error(handle, error);
        sqlite3_finalize(statement);
        return status;
    }
    read_plan(statement, out_plan);
    sqlite3_finalize(statement);
    return APL_OK;
}

apl_status apl_budget_repository_find_scope(const apl_db_connection *const connection,
                                            const int64_t team_id, const char *const start_date,
                                            const char *const end_date,
                                            const char *const category_key,
                                            apl_budget_plan *const out_plan, bool *const out_found,
                                            apl_error *const error)
{
    if (connection == nullptr || start_date == nullptr || end_date == nullptr ||
        category_key == nullptr || out_plan == nullptr || out_found == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "budget scope lookup arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_plan = (apl_budget_plan){};
    *out_found = false;
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }

    sqlite3_stmt *statement = nullptr;
    apl_status status =
        prepare(handle,
                "SELECT " PLAN_COLUMNS " FROM budget_plans "
                "WHERE team_id = ?1 AND start_date = ?2 AND end_date = ?3 AND category_key = ?4;",
                &statement, error);
    if (status != APL_OK) {
        return status;
    }
    sqlite3_bind_int64(statement, 1, team_id);
    sqlite3_bind_text(statement, 2, start_date, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, end_date, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 4, category_key, -1, SQLITE_TRANSIENT);
    const int step_status = sqlite3_step(statement);
    if (step_status == SQLITE_DONE) {
        sqlite3_finalize(statement);
        return APL_OK;
    }
    if (step_status != SQLITE_ROW) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot read budget scope: %s",
                      sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }
    read_plan(statement, out_plan);
    *out_found = true;
    sqlite3_finalize(statement);
    return APL_OK;
}

apl_status apl_budget_repository_list(const apl_db_connection *const connection,
                                      const apl_budget_filter *const filter,
                                      apl_budget_plan **const out_plans, size_t *const out_count,
                                      apl_error *const error)
{
    if (connection == nullptr || out_plans == nullptr || out_count == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "budget list arguments are nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_plans = nullptr;
    *out_count = 0;
    if (filter == nullptr || filter->team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "budget list requires a team scope");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }

    sqlite3_stmt *statement = nullptr;
    apl_status status = prepare(handle,
                                "SELECT " PLAN_COLUMNS " FROM budget_plans "
                                "WHERE team_id = ?1 "
                                "AND (?2 = '' OR end_date >= ?2) "
                                "AND (?3 = '' OR start_date <= ?3) "
                                "AND (?4 = '' OR category_key = ?4) "
                                "AND (?5 = 0 OR season_id = ?5) "
                                "ORDER BY start_date ASC, category_key ASC, id ASC;",
                                &statement, error);
    if (status != APL_OK) {
        return status;
    }
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
        bind_status = sqlite3_bind_int64(statement, 5, filter->season_id);
    }
    if (bind_status != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot bind budget filters: %s",
                      sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }

    apl_budget_plan *plans = nullptr;
    size_t count = 0;
    size_t capacity = 0;
    for (;;) {
        const int step_status = sqlite3_step(statement);
        if (step_status == SQLITE_DONE) {
            break;
        }
        if (step_status != SQLITE_ROW) {
            apl_error_set(error, APL_ERR_DB_QUERY, "cannot read budget list: %s",
                          sqlite3_errmsg(handle));
            status = APL_ERR_DB_QUERY;
            break;
        }
        if (count == capacity) {
            typeof(plans) const grown = apl_grow_array(plans, &capacity, 8, sizeof(*plans));
            if (grown == nullptr) {
                apl_error_set(error, APL_ERR_OUT_OF_MEMORY, "cannot allocate budget list");
                status = APL_ERR_OUT_OF_MEMORY;
                break;
            }
            plans = grown;
        }
        read_plan(statement, &plans[count]);
        ++count;
    }
    sqlite3_finalize(statement);
    if (status != APL_OK) {
        free(plans);
        return status;
    }
    *out_plans = plans;
    *out_count = count;
    return APL_OK;
}

void apl_budget_repository_free_list(apl_budget_plan *const plans)
{
    free(plans);
}
