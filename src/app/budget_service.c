#include "app/budget_service.h"

#include <limits.h>
#include <stdckdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/audit.h"
#include "app/use_case.h"
#include "core/util.h"
#include "db/budget_repository.h"
#include "db/ledger_repository.h"
#include "db/project_repository.h"
#include "db/season_repository.h"
#include "db/team_repository.h"
#include "db/transaction.h"
#include "domain/record.h"

static apl_status load_money_team(const apl_app *const app, const int64_t team_id,
                                  apl_team *const out_team, apl_error *const error)
{
    apl_status status = apl_team_repository_get(&app->db, team_id, out_team, error);
    if (status != APL_OK) {
        return apl_use_case_with_context(status, "cannot load team", error);
    }
    if (out_team->meta.archived) {
        apl_error_set(error, APL_ERR_CONFLICT,
                      "archived teams cannot receive budget plans; choose an active team");
        return APL_ERR_CONFLICT;
    }
    if (out_team->currency[0] == '\0') {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                      "choose a reporting currency for team '%s' before planning a budget",
                      out_team->name);
        apl_error_set_field(error, APL_FIELD_CURRENCY);
        return APL_ERR_INVALID_ARGUMENT;
    }
    return APL_OK;
}

static bool same_plan(const apl_budget_plan *const left, const apl_budget_plan *const right)
{
    return strcmp(left->start_date, right->start_date) == 0 &&
           strcmp(left->end_date, right->end_date) == 0 &&
           strcmp(left->category, right->category) == 0 &&
           strcmp(left->category_key, right->category_key) == 0 &&
           left->amount.minor_units == right->amount.minor_units &&
           strcmp(left->amount.currency, right->amount.currency) == 0 &&
           left->season_id == right->season_id && left->project_id == right->project_id &&
           strcmp(left->notes, right->notes) == 0;
}

static apl_status save_plan(const apl_app *const app, const int64_t team_id, const int64_t plan_id,
                            const int64_t project_id, const char *const start_date,
                            const char *const end_date, const char *const amount_text,
                            const char *const category, const int64_t season_id,
                            const char *const notes, apl_budget_plan *const out_plan,
                            apl_error *const error)
{
    if (plan_id < 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "budget plan ID is invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    if (app == nullptr || out_plan == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "budget save arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_plan = (apl_budget_plan){};

    apl_team team = {};
    apl_status status = load_money_team(app, team_id, &team, error);
    if (status != APL_OK) {
        return status;
    }

    apl_budget_plan plan = {.team_id = team_id, .season_id = season_id, .project_id = project_id};
    status = apl_use_case_with_field(
        apl_ledger_date_normalize(start_date, plan.start_date, sizeof(plan.start_date), error),
        APL_FIELD_START_DATE, error);
    if (status == APL_OK) {
        status = apl_use_case_with_field(
            apl_ledger_date_normalize(end_date, plan.end_date, sizeof(plan.end_date), error),
            APL_FIELD_END_DATE, error);
    }
    if (status == APL_OK) {
        status = apl_reference_date_range_validate(plan.start_date, plan.end_date, "budget period",
                                                   error);
        if (status != APL_OK) {
            apl_error_set_field(error, APL_FIELD_PERIOD);
        }
    }
    if (status == APL_OK) {
        status = apl_use_case_with_field(
            apl_ledger_category_normalize(category, plan.category, sizeof(plan.category),
                                          plan.category_key, sizeof(plan.category_key), error),
            APL_FIELD_CATEGORY, error);
    }
    if (status == APL_OK) {
        status = apl_use_case_with_field(
            apl_money_parse(amount_text, team.currency, &plan.amount, error), APL_FIELD_AMOUNT,
            error);
        if (status == APL_OK && plan.amount.minor_units < 0) {
            apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                          "a planned budget amount cannot be negative");
            apl_error_set_field(error, APL_FIELD_AMOUNT);
            status = APL_ERR_INVALID_ARGUMENT;
        }
    }
    if (status == APL_OK) {
        status = apl_use_case_with_field(
            apl_reference_optional_normalize(notes, "budget notes", plan.notes, sizeof(plan.notes),
                                             error),
            APL_FIELD_NOTES, error);
    }
    if (status == APL_OK && plan.season_id > 0) {
        apl_season season = {};
        status = apl_season_repository_get(&app->db, team_id, plan.season_id, &season, error);
        if (status != APL_OK) {
            apl_error_set_field(error, APL_FIELD_SEASON);
        }
    }
    if (status == APL_OK && plan.project_id > 0) {
        apl_project project = {};
        status = apl_project_repository_get(&app->db, team_id, plan.project_id, &project, error);
        if (status == APL_OK && project.meta.archived) {
            apl_error_set(error, APL_ERR_CONFLICT,
                          "archived projects cannot receive new budget plans");
            status = APL_ERR_CONFLICT;
        }
        if (status != APL_OK) {
            apl_error_set_field(error, APL_FIELD_PROJECT);
        }
    }
    if (status != APL_OK) {
        return apl_use_case_with_context(status, "cannot save budget plan", error);
    }

    apl_db_transaction transaction = {};
    status = apl_use_case_begin(&transaction, app, "cannot save budget plan", error);
    if (status != APL_OK) {
        return status;
    }

    apl_budget_plan existing = {};
    bool found = false;
    if (plan_id > 0) {
        status = apl_budget_repository_get(&app->db, team_id, plan_id, &existing, error);
        if (status == APL_OK) {
            plan.id = plan_id;
            plan.archived = existing.archived;
            if (existing.archived) {
                (void)snprintf(plan.archived_at, sizeof(plan.archived_at), "%s",
                               existing.archived_at);
            }
            found = true;
            if (same_plan(&existing, &plan)) {
                *out_plan = existing;
                return apl_use_case_commit(&transaction, "cannot save budget plan", error);
            }
            status =
                apl_budget_repository_update(&app->db, team_id, plan_id, &plan, out_plan, error);
        }
    } else {
        status = apl_budget_repository_find_scope(&app->db, team_id, plan.project_id,
                                                  plan.start_date, plan.end_date, plan.category_key,
                                                  &existing, &found, error);
        if (status == APL_OK && found) {
            plan.id = existing.id;
            plan.archived = false;
            plan.archived_at[0] = '\0';
            if (same_plan(&existing, &plan) && !existing.archived) {
                *out_plan = existing;
                return apl_use_case_commit(&transaction, "cannot save budget plan", error);
            }
            status = apl_budget_repository_update(&app->db, team_id, existing.id, &plan, out_plan,
                                                  error);
        } else if (status == APL_OK) {
            status = apl_budget_repository_create(&app->db, &plan, out_plan, error);
        }
    }

    if (status == APL_OK) {
        char planned[64];
        (void)apl_money_format(&out_plan->amount, planned, sizeof(planned));
        char season_text[32] = "";
        if (out_plan->season_id > 0) {
            (void)snprintf(season_text, sizeof(season_text), "%lld",
                           (long long)out_plan->season_id);
        }
        apl_activity_change changes[7] = {};
        apl_audit_change_text(&changes[0], "period_start", found ? existing.start_date : nullptr,
                              !found, out_plan->start_date, false);
        apl_audit_change_text(&changes[1], "period_end", found ? existing.end_date : nullptr,
                              !found, out_plan->end_date, false);
        apl_audit_change_text(&changes[2], "category", found ? existing.category : nullptr, !found,
                              out_plan->category, false);
        char old_amount[64];
        if (found) {
            (void)apl_money_format(&existing.amount, old_amount, sizeof(old_amount));
        }
        apl_audit_change_text(&changes[3], "planned", found ? old_amount : nullptr, !found, planned,
                              false);
        apl_audit_change_text(&changes[4], "season", nullptr, out_plan->season_id == 0, season_text,
                              out_plan->season_id == 0);
        char project_text[32] = "";
        if (out_plan->project_id > 0) {
            (void)snprintf(project_text, sizeof(project_text), "%lld",
                           (long long)out_plan->project_id);
        }
        char old_project[32] = "";
        const bool old_project_present = found && existing.project_id > 0;
        if (old_project_present) {
            (void)snprintf(old_project, sizeof(old_project), "%lld",
                           (long long)existing.project_id);
        }
        apl_audit_change_text(&changes[5], "project", old_project_present ? old_project : nullptr,
                              !old_project_present, project_text, out_plan->project_id == 0);
        apl_audit_change_text(&changes[6], "notes", found ? existing.notes : nullptr, !found,
                              out_plan->notes, false);
        char summary[APL_ACTIVITY_SUMMARY_MAX];
        (void)snprintf(summary, sizeof(summary), "%s budget plan for %s (%s to %s)",
                       found ? "Updated" : "Created", out_plan->category, out_plan->start_date,
                       out_plan->end_date);
        status = apl_audit_append(app, team_id, found ? "budget_updated" : "budget_created",
                                  apl_record_kind_code(APL_RECORD_BUDGET_PLAN), out_plan->id,
                                  summary, changes, APL_COUNT_OF(changes), error);
    }
    if (status != APL_OK) {
        return apl_use_case_rollback(&transaction, status, "cannot save budget plan", error);
    }
    return apl_use_case_commit(&transaction, "cannot save budget plan", error);
}

apl_status apl_budget_service_save(const apl_app *const app, const int64_t team_id,
                                   const int64_t project_id, const char *const start_date,
                                   const char *const end_date, const char *const amount_text,
                                   const char *const category, const int64_t season_id,
                                   const char *const notes, apl_budget_plan *const out_plan,
                                   apl_error *const error)
{
    return save_plan(app, team_id, 0, project_id, start_date, end_date, amount_text, category,
                     season_id, notes, out_plan, error);
}

apl_status apl_budget_service_update(const apl_app *const app, const int64_t team_id,
                                     const int64_t id, const int64_t project_id,
                                     const char *const start_date, const char *const end_date,
                                     const char *const amount_text, const char *const category,
                                     const int64_t season_id, const char *const notes,
                                     apl_budget_plan *const out_plan, apl_error *const error)
{
    if (id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "budget update arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return save_plan(app, team_id, id, project_id, start_date, end_date, amount_text, category,
                     season_id, notes, out_plan, error);
}

apl_status apl_budget_service_archive(const apl_app *const app, const int64_t team_id,
                                      const int64_t id, apl_budget_plan *const out_plan,
                                      apl_error *const error)
{
    if (app == nullptr || out_plan == nullptr || team_id <= 0 || id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "budget archive arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_plan = (apl_budget_plan){};

    apl_db_transaction transaction = {};
    apl_status status = apl_use_case_begin(&transaction, app, "cannot archive budget plan", error);
    if (status != APL_OK) {
        return status;
    }
    apl_budget_plan existing = {};
    status = apl_budget_repository_get(&app->db, team_id, id, &existing, error);
    if (status == APL_OK && existing.archived) {
        *out_plan = existing;
        return apl_use_case_commit(&transaction, "cannot archive budget plan", error);
    }
    if (status == APL_OK) {
        status = apl_budget_repository_archive(&app->db, team_id, id, out_plan, error);
    }
    if (status == APL_OK) {
        apl_activity_change change = {};
        apl_audit_change_bool(&change, "archived", false, true);
        char summary[APL_ACTIVITY_SUMMARY_MAX];
        (void)snprintf(summary, sizeof(summary), "Archived budget plan for %s", out_plan->category);
        status = apl_audit_append(app, team_id, "budget_archived",
                                  apl_record_kind_code(APL_RECORD_BUDGET_PLAN), id, summary,
                                  &change, 1, error);
    }
    if (status != APL_OK) {
        return apl_use_case_rollback(&transaction, status, "cannot archive budget plan", error);
    }
    return apl_use_case_commit(&transaction, "cannot archive budget plan", error);
}

apl_status apl_budget_service_get(const apl_app *const app, const int64_t team_id, const int64_t id,
                                  apl_budget_plan *const out_plan, apl_error *const error)
{
    if (app == nullptr || out_plan == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "budget get arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return apl_use_case_with_context(
        apl_budget_repository_get(&app->db, team_id, id, out_plan, error),
        "cannot load budget plan", error);
}

apl_status apl_budget_service_list(const apl_app *const app, const apl_budget_filter *const filter,
                                   apl_budget_plan **const out_plans, size_t *const out_count,
                                   apl_error *const error)
{
    if (app == nullptr || filter == nullptr || filter->team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "budget list arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return apl_use_case_with_context(
        apl_budget_repository_list(&app->db, filter, out_plans, out_count, error),
        "cannot list budget plans", error);
}

/** Find or append a report row by category key. */
static apl_budget_report_row *row_for_category(apl_budget_report_row **const rows,
                                               size_t *const count, size_t *const capacity,
                                               const int64_t project_id, const char *const category,
                                               const char *const category_key,
                                               const char *const currency, apl_error *const error)
{
    for (size_t index = 0; index < *count; ++index) {
        if (!(*rows)[index].has_plan && strcmp((*rows)[index].category_key, category_key) == 0) {
            return &(*rows)[index];
        }
    }
    if (*count == *capacity) {
        typeof(*rows) const grown = apl_grow_array(*rows, capacity, 8, sizeof(**rows));
        if (grown == nullptr) {
            apl_error_set(error, APL_ERR_OUT_OF_MEMORY, "cannot allocate budget report");
            return nullptr;
        }
        *rows = grown;
    }
    apl_budget_report_row *const row = &(*rows)[(*count)++];
    *row = (apl_budget_report_row){.project_id = project_id};
    (void)snprintf(row->category, sizeof(row->category), "%s", category);
    (void)snprintf(row->category_key, sizeof(row->category_key), "%s", category_key);
    (void)snprintf(row->planned.currency, sizeof(row->planned.currency), "%s", currency);
    (void)snprintf(row->actual.currency, sizeof(row->actual.currency), "%s", currency);
    (void)snprintf(row->variance.currency, sizeof(row->variance.currency), "%s", currency);
    return row;
}

static apl_status report_filtered(const apl_app *const app, const int64_t team_id,
                                  const int64_t project_id, const bool include_archived,
                                  const char *const from_date, const char *const to_date,
                                  const char *const category_text,
                                  apl_budget_report_row **const out_rows, size_t *const out_count,
                                  apl_error *const error)
{
    if (app == nullptr || out_rows == nullptr || out_count == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "budget report arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_rows = nullptr;
    *out_count = 0;

    apl_team team = {};
    apl_status status = load_money_team(app, team_id, &team, error);
    if (status != APL_OK) {
        return status;
    }

    char from[APL_REFERENCE_DATE_MAX];
    char to[APL_REFERENCE_DATE_MAX];
    char category_key[APL_LEDGER_CATEGORY_MAX] = "";
    status =
        apl_reference_date_normalize(from_date, "report start date", from, sizeof(from), error);
    if (status != APL_OK) {
        apl_error_set_field(error, APL_FIELD_START_DATE);
    }
    if (status == APL_OK) {
        status = apl_reference_date_normalize(to_date, "report end date", to, sizeof(to), error);
        if (status != APL_OK) {
            apl_error_set_field(error, APL_FIELD_END_DATE);
        }
    }
    if (status == APL_OK) {
        status = apl_reference_date_range_validate(from, to, "budget report", error);
        if (status != APL_OK) {
            apl_error_set_field(error, APL_FIELD_PERIOD);
        }
    }
    if (status == APL_OK && category_text != nullptr && category_text[0] != '\0') {
        char label[APL_LEDGER_CATEGORY_MAX];
        status = apl_ledger_category_normalize(category_text, label, sizeof(label), category_key,
                                               sizeof(category_key), error);
    }
    if (status != APL_OK) {
        return apl_use_case_with_context(status, "cannot build budget report", error);
    }

    apl_budget_plan *plans = nullptr;
    size_t plan_count = 0;
    apl_budget_filter plan_filter = {
        .team_id = team_id, .project_id = project_id, .include_archived = include_archived};
    (void)snprintf(plan_filter.from_date, sizeof(plan_filter.from_date), "%s", from);
    (void)snprintf(plan_filter.to_date, sizeof(plan_filter.to_date), "%s", to);
    (void)snprintf(plan_filter.category_key, sizeof(plan_filter.category_key), "%s", category_key);
    status = apl_budget_repository_list(&app->db, &plan_filter, &plans, &plan_count, error);
    if (status != APL_OK) {
        return apl_use_case_with_context(status, "cannot build budget report", error);
    }

    const apl_ledger_filter ledger_filter = {.team_id = team_id, .project_id = project_id};
    apl_ledger_entry *entries = nullptr;
    size_t entry_count = 0;
    status = apl_ledger_repository_list(&app->db, &ledger_filter, &entries, &entry_count, error);
    if (status != APL_OK) {
        apl_budget_repository_free_list(plans);
        return apl_use_case_with_context(status, "cannot build budget report", error);
    }

    size_t capacity = plan_count + 8;
    apl_budget_report_row *rows = calloc(capacity, sizeof(*rows));
    if (rows == nullptr) {
        apl_budget_repository_free_list(plans);
        apl_ledger_repository_free_list(entries);
        apl_error_set(error, APL_ERR_OUT_OF_MEMORY, "cannot allocate budget report");
        return APL_ERR_OUT_OF_MEMORY;
    }
    size_t row_count = 0;
    for (size_t index = 0; index < plan_count; ++index) {
        apl_budget_report_row *const row = &rows[row_count++];
        const apl_budget_plan *const plan = &plans[index];
        row->plan_id = plan->id;
        row->season_id = plan->season_id;
        row->project_id = plan->project_id;
        row->has_plan = true;
        row->archived = plan->archived;
        row->planned = plan->amount;
        (void)snprintf(row->actual.currency, sizeof(row->actual.currency), "%s", team.currency);
        (void)snprintf(row->variance.currency, sizeof(row->variance.currency), "%s", team.currency);
        (void)snprintf(row->start_date, sizeof(row->start_date), "%s", plan->start_date);
        (void)snprintf(row->end_date, sizeof(row->end_date), "%s", plan->end_date);
        (void)snprintf(row->category, sizeof(row->category), "%s", plan->category);
        (void)snprintf(row->category_key, sizeof(row->category_key), "%s", plan->category_key);
        (void)snprintf(row->notes, sizeof(row->notes), "%s", plan->notes);
    }

    for (size_t index = 0; index < entry_count; ++index) {
        const apl_ledger_entry *const entry = &entries[index];
        if (entry->classification != APL_LEDGER_KIND_EXPENSE) {
            continue;
        }
        if (strcmp(entry->amount.currency, team.currency) != 0) {
            apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                          "ledger entry %lld uses %s but the team reports in %s",
                          (long long)entry->id, entry->amount.currency, team.currency);
            apl_error_set_field(error, APL_FIELD_CURRENCY);
            status = APL_ERR_INVALID_ARGUMENT;
            break;
        }
        if (from[0] != '\0' && strcmp(entry->entry_date, from) < 0) {
            continue;
        }
        if (to[0] != '\0' && strcmp(entry->entry_date, to) > 0) {
            continue;
        }
        if (category_key[0] != '\0' && strcmp(entry->category_key, category_key) != 0) {
            continue;
        }
        apl_budget_report_row *target = nullptr;
        apl_budget_report_row *team_fallback = nullptr;
        for (size_t plan_index = 0; plan_index < plan_count; ++plan_index) {
            const apl_budget_plan *const plan = &plans[plan_index];
            if (strcmp(plan->category_key, entry->category_key) != 0) {
                continue;
            }
            if (strcmp(entry->entry_date, plan->start_date) < 0 ||
                strcmp(entry->entry_date, plan->end_date) > 0) {
                continue;
            }
            if (plan->project_id == entry->project_id) {
                target = &rows[plan_index];
                break;
            }
            if (plan->project_id == 0 && team_fallback == nullptr) {
                team_fallback = &rows[plan_index];
            }
        }
        if (target == nullptr) {
            target = team_fallback;
        }
        if (target == nullptr) {
            target = row_for_category(&rows, &row_count, &capacity, project_id, entry->category,
                                      entry->category_key, team.currency, error);
            if (target == nullptr) {
                status = APL_ERR_OUT_OF_MEMORY;
                break;
            }
        }
        int64_t sum = 0;
        if (ckd_add(&sum, target->actual.minor_units, entry->amount.minor_units)) {
            apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "budget actual total is too large");
            apl_error_set_field(error, APL_FIELD_AMOUNT);
            status = APL_ERR_INVALID_ARGUMENT;
            break;
        }
        target->actual.minor_units = sum;
        target->has_actual = true;
    }

    if (status == APL_OK) {
        for (size_t index = 0; index < row_count; ++index) {
            apl_budget_report_row *const row = &rows[index];
            if (row->planned.minor_units == INT64_MIN || row->actual.minor_units == INT64_MIN) {
                apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                              "budget report amount is too large to invert");
                apl_error_set_field(error, APL_FIELD_AMOUNT);
                status = APL_ERR_INVALID_ARGUMENT;
                break;
            }
            row->actual.minor_units = -row->actual.minor_units;
            if (row->has_plan && row->has_actual) {
                int64_t variance = 0;
                if (ckd_sub(&variance, row->planned.minor_units, row->actual.minor_units)) {
                    apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "budget variance is too large");
                    apl_error_set_field(error, APL_FIELD_AMOUNT);
                    status = APL_ERR_INVALID_ARGUMENT;
                    break;
                }
                row->variance.minor_units = variance;
                row->over_plan = row->actual.minor_units > row->planned.minor_units;
            }
        }
    }

    apl_budget_repository_free_list(plans);
    apl_ledger_repository_free_list(entries);
    if (status != APL_OK) {
        free(rows);
        return apl_use_case_with_context(status, "cannot build budget report", error);
    }
    *out_rows = rows;
    *out_count = row_count;
    return APL_OK;
}

apl_status apl_budget_service_report(const apl_app *const app, const int64_t team_id,
                                     const char *const from_date, const char *const to_date,
                                     const char *const category_text,
                                     apl_budget_report_row **const out_rows,
                                     size_t *const out_count, apl_error *const error)
{
    return report_filtered(app, team_id, 0, false, from_date, to_date, category_text, out_rows,
                           out_count, error);
}

apl_status apl_budget_service_report_for_project(
    const apl_app *const app, const int64_t team_id, const int64_t project_id,
    const bool include_archived, const char *const from_date, const char *const to_date,
    const char *const category_text, apl_budget_report_row **const out_rows,
    size_t *const out_count, apl_error *const error)
{
    if (project_id < 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "budget report project filter is invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return report_filtered(app, team_id, project_id, include_archived, from_date, to_date,
                           category_text, out_rows, out_count, error);
}

void apl_budget_service_free_list(apl_budget_plan *const plans)
{
    apl_budget_repository_free_list(plans);
}

void apl_budget_service_free_report(apl_budget_report_row *const rows)
{
    free(rows);
}
