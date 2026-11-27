#include "app/outcome_summary.h"

#include <string.h>

apl_status apl_outcome_summary_query(const apl_app *app, const apl_outcome_summary_filter *filter,
                                     apl_outcome_summary *out, apl_error *error)
{
    if (!app || !apl_db_handle(&app->db) || !filter || !out || filter->team_id <= 0 ||
        filter->event_id < 0 || filter->session_id < 0 || filter->season_id < 0 ||
        !memchr(filter->from_date, '\0', sizeof(filter->from_date)) ||
        !memchr(filter->to_date, '\0', sizeof(filter->to_date))) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "invalid outcome summary filter");
        return APL_ERR_INVALID_ARGUMENT;
    }
    char from[APL_REFERENCE_DATE_MAX], to[APL_REFERENCE_DATE_MAX];
    apl_status status =
        apl_reference_date_normalize(filter->from_date, "from date", from, sizeof(from), error);
    if (status == APL_OK)
        status = apl_reference_date_normalize(filter->to_date, "to date", to, sizeof(to), error);
    if (status == APL_OK)
        status = apl_reference_date_range_validate(from, to, "summary", error);
    if (status != APL_OK)
        return status;
    sqlite3 *db = apl_db_handle(&app->db);
    sqlite3_stmt *stmt = nullptr;
    const char *sql = "SELECT count(*),count(o.laps),sum(o.laps) FROM driver_outcomes o "
                      "JOIN events e ON e.id=o.event_id AND e.team_id=o.team_id "
                      "WHERE o.team_id=?1 AND (?2=0 OR o.event_id=?2) "
                      "AND (?3=0 OR o.session_id=?3) AND (?4=0 OR e.season_id=?4) "
                      "AND (?5='' OR e.event_date>=?5) AND (?6='' OR e.event_date<=?6)";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare outcome summary: %s",
                      sqlite3_errmsg(db));
        return APL_ERR_DB_QUERY;
    }
    sqlite3_bind_int64(stmt, 1, filter->team_id);
    sqlite3_bind_int64(stmt, 2, filter->event_id);
    sqlite3_bind_int64(stmt, 3, filter->session_id);
    sqlite3_bind_int64(stmt, 4, filter->season_id);
    sqlite3_bind_text(stmt, 5, from, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, to, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot calculate outcome summary: %s",
                      sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return APL_ERR_DB_QUERY;
    }
    apl_outcome_summary summary = {};
    summary.outcome_count = sqlite3_column_int64(stmt, 0);
    summary.laps_recorded_count = sqlite3_column_int64(stmt, 1);
    summary.laps_missing_count = summary.outcome_count - summary.laps_recorded_count;
    summary.has_laps_total = sqlite3_column_type(stmt, 2) != SQLITE_NULL;
    if (summary.has_laps_total)
        summary.laps_total = sqlite3_column_int64(stmt, 2);
    sqlite3_finalize(stmt);
    *out = summary;
    return APL_OK;
}
