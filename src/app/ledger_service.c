#include "app/ledger_service.h"

#include <limits.h>
#include <stdckdint.h>
#include <stdio.h>
#include <string.h>

#include "app/audit.h"
#include "app/use_case.h"
#include "db/ledger_repository.h"
#include "db/team_repository.h"
#include "db/transaction.h"
#include "domain/record.h"

static apl_status load_team(const apl_app *const app, const int64_t team_id,
                            apl_team *const out_team, apl_error *const error)
{
    apl_status status = apl_team_repository_get(&app->db, team_id, out_team, error);
    if (status != APL_OK) {
        return apl_use_case_with_context(status, "cannot load team", error);
    }
    if (out_team->meta.archived) {
        apl_error_set(error, APL_ERR_CONFLICT,
                      "archived teams cannot receive money entries; choose an active team");
        return APL_ERR_CONFLICT;
    }
    if (out_team->currency[0] == '\0') {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                      "choose a reporting currency for team '%s' before posting money",
                      out_team->name);
        apl_error_set_field(error, APL_FIELD_CURRENCY);
        return APL_ERR_INVALID_ARGUMENT;
    }
    return APL_OK;
}

static apl_status validate_entry_fields(const char *const entry_date, const char *const category,
                                        const char *const description, char *const out_date,
                                        char *const out_category, char *const out_category_key,
                                        char *const out_description, apl_error *const error)
{
    apl_status status = apl_use_case_with_field(
        apl_ledger_date_normalize(entry_date, out_date, APL_REFERENCE_DATE_MAX, error),
        APL_FIELD_DATE, error);
    if (status == APL_OK) {
        status = apl_use_case_with_field(
            apl_ledger_category_normalize(category, out_category, APL_LEDGER_CATEGORY_MAX,
                                          out_category_key, APL_LEDGER_CATEGORY_MAX, error),
            APL_FIELD_CATEGORY, error);
    }
    if (status == APL_OK) {
        status = apl_use_case_with_field(
            apl_ledger_description_normalize(description, out_description,
                                             APL_LEDGER_DESCRIPTION_MAX, error),
            APL_FIELD_DESCRIPTION, error);
    }
    return status;
}

static apl_status post_entry(const apl_app *const app, const int64_t team_id,
                             const apl_ledger_kind kind, const char *const amount_text,
                             const char *const entry_date, const char *const category,
                             const char *const description, apl_ledger_entry *const out_entry,
                             apl_error *const error)
{
    if (app == nullptr || out_entry == nullptr || team_id <= 0 || amount_text == nullptr ||
        !apl_ledger_kind_is_postable(kind)) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "money entry arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_entry = (apl_ledger_entry){};

    apl_team team = {};
    apl_status status = load_team(app, team_id, &team, error);
    if (status != APL_OK) {
        return status;
    }

    apl_money amount = {};
    status = apl_use_case_with_field(apl_money_parse(amount_text, team.currency, &amount, error),
                                     APL_FIELD_AMOUNT, error);
    if (status != APL_OK) {
        return apl_use_case_with_context(status, "cannot post money entry", error);
    }
    if (kind == APL_LEDGER_KIND_INCOME || kind == APL_LEDGER_KIND_EXPENSE) {
        if (amount.minor_units <= 0) {
            apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                          "income and expense amounts must be greater than zero");
            apl_error_set_field(error, APL_FIELD_AMOUNT);
            return apl_use_case_with_context(APL_ERR_INVALID_ARGUMENT, "cannot post money entry",
                                             error);
        }
        if (kind == APL_LEDGER_KIND_EXPENSE) {
            amount.minor_units = -amount.minor_units;
        }
    }

    char date[APL_REFERENCE_DATE_MAX];
    char category_label[APL_LEDGER_CATEGORY_MAX];
    char category_key[APL_LEDGER_CATEGORY_MAX];
    char description_text[APL_LEDGER_DESCRIPTION_MAX];
    status = validate_entry_fields(entry_date, category, description, date, category_label,
                                   category_key, description_text, error);
    if (status != APL_OK) {
        return apl_use_case_with_context(status, "cannot post money entry", error);
    }

    apl_db_transaction transaction = {};
    status = apl_use_case_begin(&transaction, app, "cannot post money entry", error);
    if (status != APL_OK) {
        return status;
    }

    if (kind == APL_LEDGER_KIND_OPENING_BALANCE) {
        const apl_ledger_filter filter = {.team_id = team_id,
                                          .kind = APL_LEDGER_KIND_OPENING_BALANCE};
        apl_ledger_entry *existing = nullptr;
        size_t existing_count = 0;
        status = apl_ledger_repository_list(&app->db, &filter, &existing, &existing_count, error);
        apl_ledger_repository_free_list(existing);
        if (status == APL_OK && existing_count > 0) {
            apl_error_set(error, APL_ERR_CONFLICT,
                          "this team already has an opening balance; correct that entry instead");
            status = APL_ERR_CONFLICT;
        }
    }

    apl_ledger_entry entry = {};
    entry.team_id = team_id;
    entry.amount = amount;
    entry.kind = kind;
    entry.classification = kind;
    (void)snprintf(entry.entry_date, sizeof(entry.entry_date), "%s", date);
    (void)snprintf(entry.category, sizeof(entry.category), "%s", category_label);
    (void)snprintf(entry.category_key, sizeof(entry.category_key), "%s", category_key);
    (void)snprintf(entry.description, sizeof(entry.description), "%s", description_text);
    if (status == APL_OK) {
        status = apl_ledger_repository_create(&app->db, &entry, out_entry, error);
    }
    if (status == APL_OK) {
        char amount_text_display[64];
        (void)apl_money_format(&out_entry->amount, amount_text_display,
                               sizeof(amount_text_display));
        apl_activity_change changes[5] = {};
        apl_audit_change_text(&changes[0], "kind", nullptr, true,
                              apl_ledger_kind_code(out_entry->kind), false);
        apl_audit_change_text(&changes[1], "amount", nullptr, true, amount_text_display, false);
        apl_audit_change_text(&changes[2], "date", nullptr, true, out_entry->entry_date, false);
        apl_audit_change_text(&changes[3], "category", nullptr, true, out_entry->category, false);
        apl_audit_change_text(&changes[4], "description", nullptr, true, out_entry->description,
                              false);
        char summary[APL_ACTIVITY_SUMMARY_MAX];
        (void)snprintf(summary, sizeof(summary), "Recorded %s %s (%s)",
                       apl_ledger_kind_label(out_entry->kind), amount_text_display,
                       out_entry->category);
        const char *const action =
            (kind == APL_LEDGER_KIND_OPENING_BALANCE)
                ? "ledger_opening_balance"
                : ((kind == APL_LEDGER_KIND_INCOME) ? "ledger_income" : "ledger_expense");
        status =
            apl_audit_append(app, team_id, action, apl_record_kind_code(APL_RECORD_LEDGER_ENTRY),
                             out_entry->id, summary, changes, 5, error);
    }
    if (status != APL_OK) {
        return apl_use_case_rollback(&transaction, status, "cannot post money entry", error);
    }
    return apl_use_case_commit(&transaction, "cannot post money entry", error);
}

apl_status apl_ledger_service_post_opening(const apl_app *const app, const int64_t team_id,
                                           const char *const amount_text,
                                           const char *const entry_date, const char *const category,
                                           const char *const description,
                                           apl_ledger_entry *const out_entry,
                                           apl_error *const error)
{
    return post_entry(app, team_id, APL_LEDGER_KIND_OPENING_BALANCE, amount_text, entry_date,
                      category, description, out_entry, error);
}

apl_status apl_ledger_service_post_income(const apl_app *const app, const int64_t team_id,
                                          const char *const amount_text,
                                          const char *const entry_date, const char *const category,
                                          const char *const description,
                                          apl_ledger_entry *const out_entry, apl_error *const error)
{
    return post_entry(app, team_id, APL_LEDGER_KIND_INCOME, amount_text, entry_date, category,
                      description, out_entry, error);
}

apl_status apl_ledger_service_post_expense(const apl_app *const app, const int64_t team_id,
                                           const char *const amount_text,
                                           const char *const entry_date, const char *const category,
                                           const char *const description,
                                           apl_ledger_entry *const out_entry,
                                           apl_error *const error)
{
    return post_entry(app, team_id, APL_LEDGER_KIND_EXPENSE, amount_text, entry_date, category,
                      description, out_entry, error);
}

apl_status apl_ledger_service_get(const apl_app *const app, const int64_t team_id, const int64_t id,
                                  apl_ledger_entry *const out_entry, apl_error *const error)
{
    if (app == nullptr || out_entry == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "ledger get arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return apl_use_case_with_context(
        apl_ledger_repository_get(&app->db, team_id, id, out_entry, error),
        "cannot load ledger entry", error);
}

apl_status apl_ledger_service_list(const apl_app *const app, const apl_ledger_filter *const filter,
                                   apl_ledger_entry **const out_entries, size_t *const out_count,
                                   apl_error *const error)
{
    if (app == nullptr || filter == nullptr || filter->team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "ledger list arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return apl_use_case_with_context(
        apl_ledger_repository_list(&app->db, filter, out_entries, out_count, error),
        "cannot list ledger entries", error);
}

apl_status apl_ledger_service_totals(const apl_app *const app, const int64_t team_id,
                                     const char *const from_date, const char *const to_date,
                                     const char *const category_text,
                                     apl_ledger_totals *const out_totals, apl_error *const error)
{
    if (app == nullptr || out_totals == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "ledger total arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_totals = (apl_ledger_totals){};

    apl_team team = {};
    apl_status status = load_team(app, team_id, &team, error);
    if (status != APL_OK) {
        return status;
    }
    out_totals->balance = (apl_money){.minor_units = 0};
    out_totals->income = (apl_money){.minor_units = 0};
    out_totals->expense = (apl_money){.minor_units = 0};
    (void)snprintf(out_totals->balance.currency, sizeof(out_totals->balance.currency), "%s",
                   team.currency);
    (void)snprintf(out_totals->income.currency, sizeof(out_totals->income.currency), "%s",
                   team.currency);
    (void)snprintf(out_totals->expense.currency, sizeof(out_totals->expense.currency), "%s",
                   team.currency);

    char from[APL_REFERENCE_DATE_MAX];
    char to[APL_REFERENCE_DATE_MAX];
    char category_key[APL_LEDGER_CATEGORY_MAX] = "";
    status = apl_use_case_with_field(
        apl_reference_date_normalize(from_date, "start date", from, sizeof(from), error),
        APL_FIELD_START_DATE, error);
    if (status == APL_OK) {
        status = apl_use_case_with_field(
            apl_reference_date_normalize(to_date, "end date", to, sizeof(to), error),
            APL_FIELD_END_DATE, error);
    }
    if (status == APL_OK) {
        status = apl_reference_date_range_validate(from, to, "ledger total", error);
        if (status != APL_OK) {
            apl_error_set_field(error, APL_FIELD_START_DATE);
        }
    }
    if (status == APL_OK && category_text != nullptr && category_text[0] != '\0') {
        char category_label[APL_LEDGER_CATEGORY_MAX];
        status =
            apl_ledger_category_normalize(category_text, category_label, sizeof(category_label),
                                          category_key, sizeof(category_key), error);
    }
    if (status != APL_OK) {
        return apl_use_case_with_context(status, "cannot calculate totals", error);
    }

    const apl_ledger_filter filter = {.team_id = team_id};
    apl_ledger_entry *entries = nullptr;
    size_t count = 0;
    status = apl_ledger_repository_list(&app->db, &filter, &entries, &count, error);
    if (status != APL_OK) {
        return apl_use_case_with_context(status, "cannot calculate totals", error);
    }
    out_totals->entry_count = count;

    for (size_t index = 0; index < count; ++index) {
        const apl_ledger_entry *const entry = &entries[index];
        if (strcmp(entry->amount.currency, team.currency) != 0) {
            apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                          "ledger entry %lld uses %s but the team reports in %s",
                          (long long)entry->id, entry->amount.currency, team.currency);
            apl_error_set_field(error, APL_FIELD_CURRENCY);
            status = APL_ERR_INVALID_ARGUMENT;
            break;
        }
        status = apl_money_add(&out_totals->balance, &entry->amount, &out_totals->balance, error);
        if (status != APL_OK) {
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
        if (entry->classification == APL_LEDGER_KIND_INCOME) {
            status = apl_money_add(&out_totals->income, &entry->amount, &out_totals->income, error);
        } else if (entry->classification == APL_LEDGER_KIND_EXPENSE) {
            status =
                apl_money_add(&out_totals->expense, &entry->amount, &out_totals->expense, error);
        }
        if (status != APL_OK) {
            break;
        }
    }
    apl_ledger_repository_free_list(entries);
    if (status != APL_OK) {
        return apl_use_case_with_context(status, "cannot calculate totals", error);
    }
    return APL_OK;
}

apl_status
apl_ledger_service_correct(const apl_app *const app, const int64_t team_id,
                           const int64_t original_id, const char *const replacement_amount_text,
                           const char *const replacement_date, const char *const category,
                           const char *const description, apl_ledger_entry *const out_reversal,
                           apl_ledger_entry *const out_replacement, apl_error *const error)
{
    if (app == nullptr || out_reversal == nullptr || team_id <= 0 || original_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "correction arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_reversal = (apl_ledger_entry){};
    if (out_replacement != nullptr) {
        *out_replacement = (apl_ledger_entry){};
    }

    apl_team team = {};
    apl_status status = load_team(app, team_id, &team, error);
    if (status != APL_OK) {
        return status;
    }
    apl_ledger_entry original = {};
    status = apl_ledger_repository_get(&app->db, team_id, original_id, &original, error);
    if (status != APL_OK) {
        return apl_use_case_with_context(status, "cannot correct ledger entry", error);
    }
    if (!apl_ledger_kind_is_postable(original.kind)) {
        apl_error_set(error, APL_ERR_CONFLICT,
                      "only opening balances, income, and expenses can be corrected; correction "
                      "entries cannot be corrected again");
        return apl_use_case_with_context(APL_ERR_CONFLICT, "cannot correct ledger entry", error);
    }

    char date[APL_REFERENCE_DATE_MAX];
    char category_label[APL_LEDGER_CATEGORY_MAX];
    char category_key[APL_LEDGER_CATEGORY_MAX];
    char description_text[APL_LEDGER_DESCRIPTION_MAX];
    status = validate_entry_fields(replacement_date, category, description, date, category_label,
                                   category_key, description_text, error);
    if (status != APL_OK) {
        return apl_use_case_with_context(status, "cannot correct ledger entry", error);
    }

    char normalized_amount[64];
    status = apl_reference_optional_normalize(replacement_amount_text, "replacement amount",
                                              normalized_amount, sizeof(normalized_amount), error);
    if (status != APL_OK) {
        apl_error_set_field(error, APL_FIELD_AMOUNT);
        return apl_use_case_with_context(status, "cannot correct ledger entry", error);
    }
    const bool has_replacement = normalized_amount[0] != '\0';
    apl_money replacement = {};
    if (has_replacement) {
        status = apl_money_parse(normalized_amount, team.currency, &replacement, error);
        if (status != APL_OK) {
            return apl_use_case_with_context(status, "cannot correct ledger entry", error);
        }
        if (original.classification == APL_LEDGER_KIND_INCOME ||
            original.classification == APL_LEDGER_KIND_EXPENSE) {
            if (replacement.minor_units <= 0) {
                apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                              "a replacement income or expense must be greater than zero");
                apl_error_set_field(error, APL_FIELD_AMOUNT);
                return apl_use_case_with_context(APL_ERR_INVALID_ARGUMENT,
                                                 "cannot correct ledger entry", error);
            }
            if (original.classification == APL_LEDGER_KIND_EXPENSE) {
                replacement.minor_units = -replacement.minor_units;
            }
        }
    }
    if (original.amount.minor_units == INT64_MIN) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "the reversal amount is too large");
        apl_error_set_field(error, APL_FIELD_AMOUNT);
        return apl_use_case_with_context(APL_ERR_INVALID_ARGUMENT, "cannot correct ledger entry",
                                         error);
    }

    apl_db_transaction transaction = {};
    status = apl_use_case_begin(&transaction, app, "cannot correct ledger entry", error);
    if (status != APL_OK) {
        return status;
    }
    apl_ledger_entry existing_reversal = {};
    bool has_reversal = false;
    status = apl_ledger_repository_find_reversal(&app->db, team_id, original_id, &existing_reversal,
                                                 &has_reversal, error);
    if (status == APL_OK && has_reversal) {
        apl_error_set(error, APL_ERR_CONFLICT, "ledger entry %lld already has a reversal",
                      (long long)original_id);
        status = APL_ERR_CONFLICT;
    }

    apl_ledger_entry reversal = {};
    reversal.team_id = team_id;
    reversal.amount = original.amount;
    reversal.amount.minor_units = -original.amount.minor_units;
    reversal.kind = APL_LEDGER_KIND_REVERSAL;
    reversal.classification = original.classification;
    (void)snprintf(reversal.entry_date, sizeof(reversal.entry_date), "%s", date);
    (void)snprintf(reversal.category, sizeof(reversal.category), "%s", original.category);
    (void)snprintf(reversal.category_key, sizeof(reversal.category_key), "%s",
                   original.category_key);
    (void)snprintf(reversal.description, sizeof(reversal.description), "%s", description_text);
    reversal.corrects_entry_id = original_id;
    if (status == APL_OK) {
        status = apl_ledger_repository_create(&app->db, &reversal, out_reversal, error);
    }

    apl_ledger_entry replacement_entry = {};
    apl_ledger_entry replacement_stored = {};
    bool replacement_created = false;
    if (status == APL_OK && has_replacement) {
        replacement_entry.team_id = team_id;
        replacement_entry.amount = replacement;
        replacement_entry.kind = APL_LEDGER_KIND_ADJUSTMENT;
        replacement_entry.classification = original.classification;
        (void)snprintf(replacement_entry.entry_date, sizeof(replacement_entry.entry_date), "%s",
                       date);
        (void)snprintf(replacement_entry.category, sizeof(replacement_entry.category), "%s",
                       category_label);
        (void)snprintf(replacement_entry.category_key, sizeof(replacement_entry.category_key), "%s",
                       category_key);
        (void)snprintf(replacement_entry.description, sizeof(replacement_entry.description), "%s",
                       description_text);
        replacement_entry.corrects_entry_id = original_id;
        status =
            apl_ledger_repository_create(&app->db, &replacement_entry, &replacement_stored, error);
        if (status == APL_OK) {
            replacement_entry = replacement_stored;
            replacement_created = true;
        }
    }

    if (status == APL_OK) {
        char original_amount[64];
        char replacement_amount[64];
        (void)apl_money_format(&original.amount, original_amount, sizeof(original_amount));
        apl_activity_change changes[6] = {};
        size_t count = 0;
        char reversal_id[32];
        (void)snprintf(reversal_id, sizeof(reversal_id), "#%lld", (long long)out_reversal->id);
        apl_audit_change_text(&changes[count++], "reversed_entry", nullptr, true, reversal_id,
                              false);
        if (replacement_created) {
            (void)apl_money_format(&replacement_entry.amount, replacement_amount,
                                   sizeof(replacement_amount));
            char replacement_id[32];
            (void)snprintf(replacement_id, sizeof(replacement_id), "#%lld",
                           (long long)replacement_entry.id);
            apl_audit_change_text(&changes[count++], "replacement_entry", nullptr, true,
                                  replacement_id, false);
            apl_audit_change_text(&changes[count++], "amount", original_amount, false,
                                  replacement_amount, false);
        } else {
            apl_audit_change_text(&changes[count++], "amount", original_amount, false, "reversed",
                                  false);
        }
        apl_audit_change_text(&changes[count++], "date", original.entry_date, false, date, false);
        apl_audit_change_text(&changes[count++], "category", original.category, false,
                              category_label, false);
        apl_audit_change_text(&changes[count++], "description", nullptr, true, description_text,
                              false);
        char summary[APL_ACTIVITY_SUMMARY_MAX];
        if (replacement_created) {
            char formatted_replacement[64];
            (void)apl_money_format(&replacement_entry.amount, formatted_replacement,
                                   sizeof(formatted_replacement));
            (void)snprintf(summary, sizeof(summary),
                           "Corrected %s #%lld to %s with reversal #%lld and adjustment #%lld",
                           apl_ledger_kind_label(original.kind), (long long)original.id,
                           formatted_replacement, (long long)out_reversal->id,
                           (long long)replacement_entry.id);
        } else {
            (void)snprintf(summary, sizeof(summary), "Reversed %s #%lld with reversal #%lld",
                           apl_ledger_kind_label(original.kind), (long long)original.id,
                           (long long)out_reversal->id);
        }
        status = apl_audit_append(app, team_id, "ledger_corrected",
                                  apl_record_kind_code(APL_RECORD_LEDGER_ENTRY), original_id,
                                  summary, changes, count, error);
    }
    if (status != APL_OK) {
        return apl_use_case_rollback(&transaction, status, "cannot correct ledger entry", error);
    }
    status = apl_use_case_commit(&transaction, "cannot correct ledger entry", error);
    if (status == APL_OK && out_replacement != nullptr && replacement_created) {
        *out_replacement = replacement_entry;
    }
    return status;
}

void apl_ledger_service_free_list(apl_ledger_entry *const entries)
{
    apl_ledger_repository_free_list(entries);
}
