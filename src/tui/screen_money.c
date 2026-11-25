#include "tui/shell_internal.h"

apl_ledger_kind money_filter_kind(const unsigned index)
{
    switch (index) {
    case 1:
        return APL_LEDGER_KIND_OPENING_BALANCE;
    case 2:
        return APL_LEDGER_KIND_INCOME;
    case 3:
        return APL_LEDGER_KIND_EXPENSE;
    case 4:
        return APL_LEDGER_KIND_REVERSAL;
    case 5:
        return APL_LEDGER_KIND_ADJUSTMENT;
    default:
        return APL_LEDGER_KIND_NONE;
    }
}

const char *money_filter_kind_name(const unsigned index)
{
    return (index == 0) ? "all" : apl_ledger_kind_code(money_filter_kind(index));
}

const char *money_date_filter_name(const unsigned preset)
{
    switch (preset) {
    case 1:
        return "30d";
    case 2:
        return "90d";
    case 3:
        return "365d";
    default:
        return "all";
    }
}

void money_date_from(const unsigned preset, char *const out, const size_t out_size)
{
    out[0] = '\0';
    if (preset == 0) {
        return;
    }
    const int days = (preset == 1) ? 30 : ((preset == 2) ? 90 : 365);
    apl_reference_date_today_minus_days(days, out, out_size);
}

void money_cycle_date_filter(unsigned *const preset)
{
    *preset = (*preset + 1U) % 4U;
}

size_t find_ledger_index(const shell_state *const state, const int64_t id)
{
    for (size_t index = 0; index < state->entry_count; ++index) {
        if (state->entries[index].id == id) {
            return index;
        }
    }
    return SIZE_MAX;
}

apl_status load_ledger(shell_state *const state, const int64_t preferred_id, apl_error *const error)
{
    int64_t keep_id = preferred_id;
    if (keep_id == 0 && state->entry_count > 0 && state->list.selected < state->entry_count) {
        keep_id = state->entries[state->list.selected].id;
    }
    apl_ledger_entry *entries = nullptr;
    size_t count = 0;
    const int64_t team_id = active_team_id(state);
    apl_status status = APL_OK;
    if (team_id > 0) {
        apl_ledger_filter filter = {.team_id = team_id,
                                    .kind = money_filter_kind(state->money_kind_filter)};
        money_date_from(state->money_date_preset, filter.from_date, sizeof(filter.from_date));
        (void)snprintf(filter.category_key, sizeof(filter.category_key), "%s",
                       state->money_category_filter);
        status = apl_ledger_service_list(state->app, &filter, &entries, &count, error);
    }
    if (status != APL_OK) {
        return status;
    }
    apl_ledger_service_free_list(state->entries);
    state->entries = entries;
    state->entry_count = count;
    apl_tui_list_set_count(&state->list, count);
    if (keep_id != 0) {
        const size_t index = find_ledger_index(state, keep_id);
        if (index != SIZE_MAX) {
            apl_tui_list_select(&state->list, index);
        }
    }
    return APL_OK;
}

apl_status load_budget_report(shell_state *const state, apl_error *const error)
{
    apl_budget_report_row *rows = nullptr;
    size_t count = 0;
    const int64_t team_id = active_team_id(state);
    apl_status status = APL_OK;
    if (team_id > 0) {
        char from[APL_REFERENCE_DATE_MAX];
        money_date_from(state->budget_date_preset, from, sizeof(from));
        status = apl_budget_service_report(state->app, team_id, from, "",
                                           state->budget_category_filter, &rows, &count, error);
    }
    if (status != APL_OK) {
        return status;
    }
    apl_budget_service_free_report(state->budget_rows);
    state->budget_rows = rows;
    state->budget_row_count = count;
    apl_tui_list_set_count(&state->list, count);
    return APL_OK;
}

apl_status load_entry_activity(shell_state *const state, const int64_t entry_id,
                               apl_error *const error)
{
    apl_activity_service_free_events(state->entry_events);
    state->entry_events = nullptr;
    state->entry_event_count = 0;
    const int64_t team_id = active_team_id(state);
    if (team_id <= 0) {
        return APL_OK;
    }
    apl_activity_filter filter = {.team_id = team_id};
    (void)snprintf(filter.entity_type, sizeof(filter.entity_type), "%s",
                   apl_record_kind_code(APL_RECORD_LEDGER_ENTRY));
    apl_activity_event *events = nullptr;
    size_t count = 0;
    const apl_status status =
        apl_activity_service_list(state->app, &filter, &events, &count, error);
    if (status != APL_OK) {
        return status;
    }
    size_t kept = 0;
    for (size_t index = 0; index < count; ++index) {
        if (events[index].entity_id == entry_id) {
            events[kept++] = events[index];
        }
    }
    state->entry_events = events;
    state->entry_event_count = kept;
    return APL_OK;
}

void open_money_currency_dialog(shell_state *const state)
{
    if (active_team_id(state) == 0) {
        set_status_warning(state, "Select an active team before choosing a reporting currency.");
        return;
    }
    apl_tui_dialog_open_form(&state->dialog, "Reporting currency",
                             "Three-letter code, for example EUR.");
    (void)apl_tui_dialog_add_field(&state->dialog, "Currency (3 letters)",
                                   state->active_team.currency, true);
    state->dialog_context = SHELL_DIALOG_CONTEXT_MONEY_CURRENCY;
    state->dialog_team_id = state->active_team.id;
    apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
    set_status(state, "Enter a three-letter currency code such as EUR; Esc writes nothing.");
}

void open_money_entry_dialog(shell_state *const state, const apl_ledger_kind kind)
{
    if (active_team_id(state) == 0) {
        set_status_warning(state, "Select an active team before recording money.");
        return;
    }
    if (state->active_team.currency[0] == '\0') {
        set_status_warning(state, "Set the team's reporting currency first (press u).");
        return;
    }
    const char *const title =
        (kind == APL_LEDGER_KIND_OPENING_BALANCE)
            ? "Record opening balance"
            : ((kind == APL_LEDGER_KIND_INCOME) ? "Record income" : "Record expense");
    const char *const message = (kind == APL_LEDGER_KIND_OPENING_BALANCE)
                                    ? "Amount may be positive, zero, or negative."
                                    : "Enter a positive amount; the sign is applied automatically.";
    apl_tui_dialog_open_form(&state->dialog, title, message);
    (void)apl_tui_dialog_add_field(&state->dialog, "Amount (e.g. 250.00)", "", true);
    (void)apl_tui_dialog_add_field(&state->dialog, "Date (YYYY-MM-DD)", "", true);
    (void)apl_tui_dialog_add_field(&state->dialog, "Category (user-entered)", "", true);
    (void)apl_tui_dialog_add_field(&state->dialog, "Description (required)", "", true);
    state->dialog_context = SHELL_DIALOG_CONTEXT_MONEY_ENTRY;
    state->pending_ledger_kind = kind;
    apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
    set_status(state, "Tab moves fields. Enter reviews before saving; Esc writes nothing.");
}

void store_money_entry_values(shell_state *const state)
{
    const apl_tui_dialog_field *const amount = apl_tui_dialog_field_at(&state->dialog, 0);
    const apl_tui_dialog_field *const date = apl_tui_dialog_field_at(&state->dialog, 1);
    const apl_tui_dialog_field *const category = apl_tui_dialog_field_at(&state->dialog, 2);
    const apl_tui_dialog_field *const description = apl_tui_dialog_field_at(&state->dialog, 3);
    (void)snprintf(state->pending_amount, sizeof(state->pending_amount), "%.63s",
                   amount != nullptr ? amount->value : "");
    (void)snprintf(state->pending_date, sizeof(state->pending_date), "%.10s",
                   date != nullptr ? date->value : "");
    (void)snprintf(state->pending_category, sizeof(state->pending_category), "%.127s",
                   category != nullptr ? category->value : "");
    (void)snprintf(state->pending_description, sizeof(state->pending_description), "%.511s",
                   description != nullptr ? description->value : "");
}

void open_money_review_dialog(shell_state *const state)
{
    const char *const kind = apl_ledger_kind_label(state->pending_ledger_kind);
    char amount_display[80];
    apl_error parse_error;
    apl_error_init(&parse_error);
    apl_money parsed = {};
    if (apl_money_parse(state->pending_amount, state->active_team.currency, &parsed,
                        &parse_error) == APL_OK) {
        if (state->pending_ledger_kind == APL_LEDGER_KIND_EXPENSE && parsed.minor_units > 0) {
            parsed.minor_units = -parsed.minor_units;
        }
        (void)apl_money_format(&parsed, amount_display, sizeof(amount_display));
    } else {
        (void)snprintf(amount_display, sizeof(amount_display), "%s", state->pending_amount);
    }
    char message[1024];
    (void)snprintf(message, sizeof(message),
                   "Team: %s\nCurrency: %s\nKind: %s\nAmount: %s\nDate: %s\nCategory: %s\n"
                   "Description: %s\n\nConfirm to save. Nothing is written before confirmation.",
                   state->active_team.name, state->active_team.currency, kind, amount_display,
                   state->pending_date, state->pending_category, state->pending_description);
    apl_tui_dialog_open_confirm(&state->dialog, "Review money entry", message);
    state->dialog.focus = 1;
    state->dialog_context = SHELL_DIALOG_CONTEXT_MONEY_REVIEW;
    apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
    set_status(state, "Confirm saves; Esc cancels without writing.");
}

void open_money_correct_dialog(shell_state *const state, const apl_ledger_entry *const entry,
                               const char *const replacement_amount, const char *const date,
                               const char *const category, const char *const reason)
{
    if (entry == nullptr || !apl_ledger_kind_is_postable(entry->kind)) {
        set_status_warning(state, "Only opening balances, income, and expenses can be corrected.");
        return;
    }
    char amount_text[64];
    (void)apl_money_format(&entry->amount, amount_text, sizeof(amount_text));
    char message[128];
    (void)snprintf(message, sizeof(message), "Original: %.60s on %s. Blank amount = full reversal.",
                   amount_text, entry->entry_date);
    apl_tui_dialog_open_form(&state->dialog, "Correct money entry", message);
    (void)apl_tui_dialog_add_field(&state->dialog, "New amount (blank = reverse)",
                                   replacement_amount, false);
    (void)apl_tui_dialog_add_field(&state->dialog, "Date (YYYY-MM-DD)", date, true);
    (void)apl_tui_dialog_add_field(&state->dialog, "Category", category, true);
    (void)apl_tui_dialog_add_field(&state->dialog, "Reason (required)", reason, true);
    state->dialog_context = SHELL_DIALOG_CONTEXT_MONEY_CORRECT;
    state->dialog_record_id = entry->id;
    apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
    set_status(state,
               "The original entry is kept. Enter reviews the reversal; Esc writes nothing.");
}

void store_money_correction_values(shell_state *const state)
{
    const apl_tui_dialog_field *const amount = apl_tui_dialog_field_at(&state->dialog, 0);
    const apl_tui_dialog_field *const date = apl_tui_dialog_field_at(&state->dialog, 1);
    const apl_tui_dialog_field *const category = apl_tui_dialog_field_at(&state->dialog, 2);
    const apl_tui_dialog_field *const reason = apl_tui_dialog_field_at(&state->dialog, 3);
    (void)snprintf(state->pending_amount, sizeof(state->pending_amount), "%.63s",
                   amount != nullptr ? amount->value : "");
    (void)snprintf(state->pending_date, sizeof(state->pending_date), "%.10s",
                   date != nullptr ? date->value : "");
    (void)snprintf(state->pending_category, sizeof(state->pending_category), "%.127s",
                   category != nullptr ? category->value : "");
    (void)snprintf(state->pending_description, sizeof(state->pending_description), "%.511s",
                   reason != nullptr ? reason->value : "");
}

void open_money_correction_review(shell_state *const state)
{
    const size_t index = find_ledger_index(state, state->dialog_record_id);
    const apl_ledger_entry *const original =
        (index != SIZE_MAX) ? &state->entries[index] : &state->entry_detail;
    char original_amount[64];
    (void)apl_money_format(&original->amount, original_amount, sizeof(original_amount));
    char balance_text[160] = "not available";
    apl_error totals_error;
    apl_error_init(&totals_error);
    apl_ledger_totals totals = {};
    if (apl_ledger_service_totals(state->app, active_team_id(state), "", "", "", &totals,
                                  &totals_error) == APL_OK) {
        int64_t projected = totals.balance.minor_units;
        int64_t reversal = 0;
        if (!ckd_sub(&reversal, 0, original->amount.minor_units) &&
            !ckd_add(&projected, projected, reversal)) {
            const bool has_replacement = state->pending_amount[0] != '\0';
            apl_money replacement = {};
            if (has_replacement &&
                apl_money_parse(state->pending_amount, state->active_team.currency, &replacement,
                                &totals_error) == APL_OK) {
                if (original->classification == APL_LEDGER_KIND_EXPENSE) {
                    replacement.minor_units = -replacement.minor_units;
                }
                (void)ckd_add(&projected, projected, replacement.minor_units);
            }
            apl_money projected_money = {.minor_units = projected};
            (void)snprintf(projected_money.currency, sizeof(projected_money.currency), "%s",
                           state->active_team.currency);
            char projected_text[64];
            char balance_before[64];
            (void)apl_money_format(&projected_money, projected_text, sizeof(projected_text));
            (void)apl_money_format(&totals.balance, balance_before, sizeof(balance_before));
            (void)snprintf(balance_text, sizeof(balance_text), "%s (was %s)", projected_text,
                           balance_before);
        }
    }
    char message[1024];
    (void)snprintf(
        message, sizeof(message),
        "Original: %.63s #%lld\nReplacement: %.63s\nDate: %.10s\nCategory: %.100s\n"
        "Reason: %.300s\nBalance after correction: %.159s\n\nConfirm to append the "
        "correction entries.",
        original_amount, (long long)original->id,
        state->pending_amount[0] != '\0' ? state->pending_amount : "(none: reversal only)",
        state->pending_date, state->pending_category, state->pending_description, balance_text);
    apl_tui_dialog_open_confirm(&state->dialog, "Review correction", message);
    state->dialog.focus = 1;
    state->dialog_context = SHELL_DIALOG_CONTEXT_MONEY_CORRECT_REVIEW;
    apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
    set_status(state, "Confirm appends the correction; Esc cancels without writing.");
}

bool perform_money_post(shell_state *const state, apl_error *const error)
{
    apl_ledger_entry created = {};
    apl_status status = APL_OK;
    switch (state->pending_ledger_kind) {
    case APL_LEDGER_KIND_OPENING_BALANCE:
        status = apl_ledger_service_post_opening(
            state->app, active_team_id(state), state->pending_amount, state->pending_date,
            state->pending_category, state->pending_description, &created, error);
        break;
    case APL_LEDGER_KIND_INCOME:
        status = apl_ledger_service_post_income(
            state->app, active_team_id(state), state->pending_amount, state->pending_date,
            state->pending_category, state->pending_description, &created, error);
        break;
    case APL_LEDGER_KIND_EXPENSE:
        status = apl_ledger_service_post_expense(
            state->app, active_team_id(state), state->pending_amount, state->pending_date,
            state->pending_category, state->pending_description, &created, error);
        break;
    default:
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "unknown money entry kind");
        return false;
    }
    if (status != APL_OK) {
        set_status_from_error(state, "Cannot save money entry", error);
        return false;
    }
    state->section = APL_TUI_SECTION_MONEY;
    state->entry_detail = created;
    if (load_ledger(state, created.id, error) != APL_OK ||
        load_entry_activity(state, created.id, error) != APL_OK) {
        show_error_dialog(state, error);
        return false;
    }
    state->screen = SHELL_SCREEN_MONEY_DETAIL;
    sync_status_context(state);
    char amount_text[64];
    (void)apl_money_format(&created.amount, amount_text, sizeof(amount_text));
    set_status_success(state, "Saved %s %s.", apl_ledger_kind_label(created.kind), amount_text);
    return true;
}

bool perform_money_correction(shell_state *const state, apl_error *const error)
{
    apl_ledger_entry reversal = {};
    apl_ledger_entry replacement = {};
    const apl_status status = apl_ledger_service_correct(
        state->app, active_team_id(state), state->dialog_record_id, state->pending_amount,
        state->pending_date, state->pending_category, state->pending_description, &reversal,
        &replacement, error);
    if (status != APL_OK) {
        set_status_from_error(state, "Cannot correct money entry", error);
        return false;
    }
    const int64_t show_id = replacement.id > 0 ? replacement.id : reversal.id;
    state->entry_detail = replacement.id > 0 ? replacement : reversal;
    if (load_ledger(state, show_id, error) != APL_OK ||
        load_entry_activity(state, show_id, error) != APL_OK) {
        show_error_dialog(state, error);
        return false;
    }
    state->section = APL_TUI_SECTION_MONEY;
    state->screen = SHELL_SCREEN_MONEY_DETAIL;
    sync_status_context(state);
    set_status_success(state, "Correction saved. Original entry #%lld remains unchanged.",
                       (long long)reversal.corrects_entry_id);
    return true;
}

void open_budget_form(shell_state *const state, const bool edit)
{
    if (active_team_id(state) == 0) {
        set_status_warning(state, "Select an active team before planning a budget.");
        return;
    }
    if (state->active_team.currency[0] == '\0') {
        set_status_warning(state, "Set the team's reporting currency before planning a budget.");
        return;
    }
    const apl_budget_report_row *const row = (state->list.selected < state->budget_row_count)
                                                 ? &state->budget_rows[state->list.selected]
                                                 : nullptr;
    const bool has_plan = edit && row != nullptr && row->plan_id > 0;
    char start[APL_REFERENCE_DATE_MAX] = "";
    char end[APL_REFERENCE_DATE_MAX] = "";
    char category[APL_LEDGER_CATEGORY_MAX] = "";
    char amount[64] = "";
    char notes[APL_REFERENCE_NOTES_MAX] = "";
    if (has_plan) {
        (void)snprintf(start, sizeof(start), "%s", row->start_date);
        (void)snprintf(end, sizeof(end), "%s", row->end_date);
        (void)snprintf(category, sizeof(category), "%s", row->category);
        (void)snprintf(amount, sizeof(amount), "%lld.%02lld",
                       (long long)(row->planned.minor_units / 100),
                       (long long)(row->planned.minor_units % 100));
        state->pending_season_id = row->season_id;
        (void)snprintf(notes, sizeof(notes), "%s", row->notes);
    } else {
        state->pending_season_id = 0;
        if (row != nullptr) {
            (void)snprintf(category, sizeof(category), "%s", row->category);
        }
    }
    apl_tui_dialog_open_form(&state->dialog, has_plan ? "Edit budget plan" : "Create budget plan",
                             "Planned amount cannot be negative.");
    (void)apl_tui_dialog_add_field(&state->dialog, "Start date (YYYY-MM-DD)", start, true);
    (void)apl_tui_dialog_add_field(&state->dialog, "End date (YYYY-MM-DD)", end, true);
    (void)apl_tui_dialog_add_field(&state->dialog, "Category (user-entered)", category, true);
    (void)apl_tui_dialog_add_field(&state->dialog, "Planned (e.g. 300.00)", amount, true);
    (void)apl_tui_dialog_add_field(&state->dialog, "Notes (optional)", notes, false);
    state->dialog_context = SHELL_DIALOG_CONTEXT_BUDGET_FORM;
    state->dialog_record_id = has_plan ? row->plan_id : 0;
    apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
    set_status(state, "Tab moves fields. Enter reviews the plan; Esc writes nothing.");
}

void store_budget_values(shell_state *const state)
{
    const apl_tui_dialog_field *const start = apl_tui_dialog_field_at(&state->dialog, 0);
    const apl_tui_dialog_field *const end = apl_tui_dialog_field_at(&state->dialog, 1);
    const apl_tui_dialog_field *const category = apl_tui_dialog_field_at(&state->dialog, 2);
    const apl_tui_dialog_field *const amount = apl_tui_dialog_field_at(&state->dialog, 3);
    const apl_tui_dialog_field *const notes = apl_tui_dialog_field_at(&state->dialog, 4);
    (void)snprintf(state->pending_start, sizeof(state->pending_start), "%.10s",
                   start != nullptr ? start->value : "");
    (void)snprintf(state->pending_end, sizeof(state->pending_end), "%.10s",
                   end != nullptr ? end->value : "");
    (void)snprintf(state->pending_category, sizeof(state->pending_category), "%.127s",
                   category != nullptr ? category->value : "");
    (void)snprintf(state->pending_amount, sizeof(state->pending_amount), "%.63s",
                   amount != nullptr ? amount->value : "");
    (void)snprintf(state->pending_notes, sizeof(state->pending_notes), "%.1023s",
                   notes != nullptr ? notes->value : "");
    return;
}

void open_budget_review_dialog(shell_state *const state)
{
    char message[1024];
    (void)snprintf(message, sizeof(message),
                   "Team: %.100s\nCurrency: %.3s\nPeriod: %.10s to %.10s\nCategory: %.100s\n"
                   "Planned: %.63s\nNotes: %.400s\n\nConfirm to save the plan.",
                   state->active_team.name, state->active_team.currency, state->pending_start,
                   state->pending_end, state->pending_category, state->pending_amount,
                   state->pending_notes);
    apl_tui_dialog_open_confirm(&state->dialog, "Review budget plan", message);
    state->dialog.focus = 1;
    state->dialog_context = SHELL_DIALOG_CONTEXT_BUDGET_REVIEW;
    apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
    set_status(state, "Confirm saves the plan; Esc cancels without writing.");
}

bool perform_budget_save(shell_state *const state, apl_error *const error)
{
    apl_budget_plan saved = {};
    const apl_status status =
        apl_budget_service_save(state->app, active_team_id(state), state->pending_start,
                                state->pending_end, state->pending_amount, state->pending_category,
                                state->pending_season_id, state->pending_notes, &saved, error);
    if (status != APL_OK) {
        set_status_from_error(state, "Cannot save budget plan", error);
        return false;
    }
    state->section = APL_TUI_SECTION_MONEY;
    state->screen = SHELL_SCREEN_BUDGET_LIST;
    if (load_budget_report(state, error) != APL_OK) {
        show_error_dialog(state, error);
        return false;
    }
    sync_status_context(state);
    set_status_success(state, "Budget plan for %s saved. Plans do not change the balance.",
                       saved.category);
    return true;
}

void build_money_filter_bar(const shell_state *const state, char *const out, const size_t out_size,
                            unsigned starts[4], unsigned ends[4])
{
    out[0] = '\0';
    unsigned cursor = 0;
    char label[160];
    (void)snprintf(label, sizeof(label), "Date:%s",
                   money_date_filter_name(state->money_date_preset));
    append_filter_button(out, out_size, &cursor, starts, ends, 0, label);
    (void)snprintf(label, sizeof(label), "Kind:%s",
                   money_filter_kind_name(state->money_kind_filter));
    append_filter_button(out, out_size, &cursor, starts, ends, 1, label);
    (void)snprintf(label, sizeof(label), "Category:%s",
                   state->money_category_filter[0] != '\0' ? state->money_category_filter : "all");
    append_filter_button(out, out_size, &cursor, starts, ends, 2, label);
    append_filter_button(out, out_size, &cursor, starts, ends, 3, "Clear");
}

void build_budget_filter_bar(const shell_state *const state, char *const out, const size_t out_size,
                             unsigned starts[4], unsigned ends[4])
{
    out[0] = '\0';
    unsigned cursor = 0;
    char label[160];
    (void)snprintf(label, sizeof(label), "Date:%s",
                   money_date_filter_name(state->budget_date_preset));
    append_filter_button(out, out_size, &cursor, starts, ends, 0, label);
    (void)snprintf(label, sizeof(label), "Category:%s",
                   state->budget_category_filter[0] != '\0' ? state->budget_category_filter
                                                            : "all");
    append_filter_button(out, out_size, &cursor, starts, ends, 1, label);
    append_filter_button(out, out_size, &cursor, starts, ends, 2, "Clear");
}

void draw_money_list(shell_state *const state)
{
    struct ncplane *const body = apl_tui_frame_plane(&state->frame, APL_TUI_REGION_BODY);
    if (body == nullptr) {
        return;
    }
    if (active_team_id(state) == 0) {
        apl_tui_frame_draw_line(
            body, 0,
            "No active team. Select one, or press A in People for an explicit all-teams "
            "view.");
        return;
    }
    char balance_text[64] = "";
    apl_error error;
    apl_error_init(&error);
    apl_ledger_totals totals = {};
    if (apl_ledger_service_totals(state->app, active_team_id(state), "", "", "", &totals, &error) ==
        APL_OK) {
        (void)apl_money_format(&totals.balance, balance_text, sizeof(balance_text));
    }
    char header[256];
    (void)snprintf(
        header, sizeof(header), "Money: %s   Currency: %s   Balance: %s", state->active_team.name,
        state->active_team.currency[0] != '\0' ? state->active_team.currency : "(not chosen)",
        balance_text[0] != '\0' ? balance_text : "not available");
    apl_tui_frame_draw_line(body, 0, header);

    char filters[320];
    unsigned starts[4] = {};
    unsigned ends[4] = {};
    build_money_filter_bar(state, filters, sizeof(filters), starts, ends);
    apl_tui_theme_apply(&state->theme, body, APL_TUI_ROLE_BODY);
    apl_tui_frame_draw_line(body, 1, filters);
    apl_tui_theme_reset(body);

    if (state->active_team.currency[0] == '\0') {
        apl_tui_frame_draw_line(body, 3,
                                "No reporting currency chosen. Press u to set one before "
                                "recording money.");
        return;
    }
    if (state->entry_count == 0) {
        apl_tui_frame_draw_line(
            body, 3, "No ledger entries match. i income, x expense, o opening balance, b budgets.");
        return;
    }

    char header_line[256];
    (void)snprintf(header_line, sizeof(header_line), "  %-10.10s %-15.15s %18s %-16.16s %s", "Date",
                   "Kind", "Amount", "Category", "Description");
    apl_tui_theme_apply(&state->theme, body, APL_TUI_ROLE_MUTED);
    apl_tui_frame_draw_line(body, 2, header_line);
    apl_tui_theme_reset(body);

    const unsigned rows = apl_tui_frame_layout(&state->frame)->body.rows;
    const size_t visible = apl_tui_list_visible_count(&state->list);
    for (size_t offset = 0; offset < visible && offset + 3 < rows; ++offset) {
        const size_t index = state->list.offset + offset;
        const apl_ledger_entry *const entry = &state->entries[index];
        const bool focused = index == state->list.selected;
        char amount_text[64];
        (void)apl_money_format(&entry->amount, amount_text, sizeof(amount_text));
        char line[512];
        const int written =
            snprintf(line, sizeof(line), "%c %-10.10s %-15.15s %18s %-16.16s %.120s",
                     focused ? '>' : ' ', entry->entry_date, apl_ledger_kind_label(entry->kind),
                     amount_text, entry->category, entry->description);
        if (written > 0 && (size_t)written < sizeof(line) && entry->corrects_entry_id > 0) {
            (void)snprintf(line + written, sizeof(line) - (size_t)written, "  [corrects #%lld]",
                           (long long)entry->corrects_entry_id);
        }
        apl_tui_theme_apply(&state->theme, body,
                            focused ? APL_TUI_ROLE_SELECTION : APL_TUI_ROLE_BODY);
        apl_tui_frame_draw_line(body, (int)(offset + 3), line);
        apl_tui_theme_reset(body);
    }
}

void draw_money_detail(shell_state *const state)
{
    struct ncplane *const body = apl_tui_frame_plane(&state->frame, APL_TUI_REGION_BODY);
    if (body == nullptr) {
        return;
    }
    const unsigned cols = apl_tui_frame_layout(&state->frame)->body.cols;
    const apl_ledger_entry *const entry = &state->entry_detail;
    int row = 0;
    char line[512];
    char amount_text[64];
    (void)apl_money_format(&entry->amount, amount_text, sizeof(amount_text));
    (void)snprintf(line, sizeof(line), "Amount: %s", amount_text);
    apl_tui_frame_draw_line(body, row++, line);
    (void)snprintf(line, sizeof(line), "Kind: %s (%s)", apl_ledger_kind_label(entry->kind),
                   apl_ledger_kind_label(entry->classification));
    apl_tui_frame_draw_line(body, row++, line);
    (void)snprintf(line, sizeof(line), "Date: %s", entry->entry_date);
    apl_tui_frame_draw_line(body, row++, line);
    (void)snprintf(line, sizeof(line), "Category: %s", entry->category);
    apl_tui_frame_draw_line(body, row++, line);
    (void)snprintf(line, sizeof(line), "Team: %s   Currency: %s", state->active_team.name,
                   state->active_team.currency);
    apl_tui_frame_draw_line(body, row++, line);
    apl_tui_frame_draw_line(body, row++, "Description:");
    const unsigned note_rows =
        apl_tui_frame_draw_wrapped(body, row, 2, cols > 2 ? cols - 2 : cols, entry->description);
    row += (int)(note_rows > 0 ? note_rows : 1);
    ++row;
    (void)snprintf(line, sizeof(line), "Created: %s", entry->created_at);
    apl_tui_frame_draw_line(body, row++, line);
    if (entry->corrects_entry_id > 0) {
        (void)snprintf(line, sizeof(line), "Corrects entry #%lld",
                       (long long)entry->corrects_entry_id);
        apl_tui_frame_draw_line(body, row++, line);
    }
    for (size_t index = 0; index < state->entry_count; ++index) {
        if (state->entries[index].corrects_entry_id == entry->id) {
            char linked_amount[64];
            (void)apl_money_format(&state->entries[index].amount, linked_amount,
                                   sizeof(linked_amount));
            (void)snprintf(line, sizeof(line), "Correction: #%lld %s %s on %s",
                           (long long)state->entries[index].id,
                           apl_ledger_kind_label(state->entries[index].kind), linked_amount,
                           state->entries[index].entry_date);
            apl_tui_frame_draw_line(body, row++, line);
        }
    }
    ++row;
    apl_tui_frame_draw_line(body, row++, "Activity:");
    if (state->entry_event_count == 0) {
        apl_tui_frame_draw_line(body, row++, "  No activity recorded for this entry.");
    } else {
        for (size_t index = 0; index < state->entry_event_count; ++index) {
            if (row >= (int)apl_tui_frame_layout(&state->frame)->body.rows - 2) {
                break;
            }
            const apl_activity_event *const event = &state->entry_events[index];
            (void)snprintf(line, sizeof(line), "  %.16s %.200s", event->occurred_at,
                           event->summary);
            apl_tui_frame_draw_line(body, row++, line);
        }
    }
    ++row;
    if (apl_ledger_kind_is_postable(entry->kind)) {
        apl_tui_frame_draw_line(body, row++, "c correct   Esc back to ledger");
    } else {
        apl_tui_frame_draw_line(body, row++, "Corrections cannot be corrected again. Esc back.");
    }
}

void draw_budget_list(shell_state *const state)
{
    struct ncplane *const body = apl_tui_frame_plane(&state->frame, APL_TUI_REGION_BODY);
    if (body == nullptr) {
        return;
    }
    if (active_team_id(state) == 0) {
        apl_tui_frame_draw_line(body, 0, "No active team. Select one before browsing budgets.");
        return;
    }
    char header[512];
    (void)snprintf(
        header, sizeof(header), "Budget report: %s   Currency: %s   Date:%s   Category:%s",
        state->active_team.name,
        state->active_team.currency[0] != '\0' ? state->active_team.currency : "(not chosen)",
        money_date_filter_name(state->budget_date_preset),
        state->budget_category_filter[0] != '\0' ? state->budget_category_filter : "all");
    apl_tui_frame_draw_line(body, 0, header);
    char filters[320];
    unsigned starts[4] = {};
    unsigned ends[4] = {};
    build_budget_filter_bar(state, filters, sizeof(filters), starts, ends);
    apl_tui_theme_apply(&state->theme, body, APL_TUI_ROLE_BODY);
    apl_tui_frame_draw_line(body, 1, filters);
    apl_tui_theme_reset(body);

    if (state->budget_row_count == 0) {
        apl_tui_frame_draw_line(body, 3,
                                "No budget plans or matching expenses. Press n to create a plan.");
        return;
    }
    char header_line[256];
    (void)snprintf(header_line, sizeof(header_line), "  %-23.23s %-16.16s %14s %14s %s", "Period",
                   "Category", "Planned", "Actual", "Variance");
    apl_tui_theme_apply(&state->theme, body, APL_TUI_ROLE_MUTED);
    apl_tui_frame_draw_line(body, 2, header_line);
    apl_tui_theme_reset(body);
    const unsigned rows = apl_tui_frame_layout(&state->frame)->body.rows;
    const size_t visible = apl_tui_list_visible_count(&state->list);
    for (size_t offset = 0; offset < visible && offset + 3 < rows; ++offset) {
        const size_t index = state->list.offset + offset;
        const apl_budget_report_row *const row = &state->budget_rows[index];
        const bool focused = index == state->list.selected;
        char planned[64] = "no plan";
        char actual[64] = "no actual";
        char variance[80] = "";
        if (row->has_plan) {
            (void)apl_money_format(&row->planned, planned, sizeof(planned));
        }
        if (row->has_actual) {
            (void)apl_money_format(&row->actual, actual, sizeof(actual));
        }
        if (row->has_plan && row->has_actual) {
            if (row->over_plan) {
                char over[64];
                (void)apl_money_format(&row->variance, over, sizeof(over));
                (void)snprintf(variance, sizeof(variance), "OVER PLAN (%s)", over);
            } else {
                char remaining[64];
                (void)apl_money_format(&row->variance, remaining, sizeof(remaining));
                (void)snprintf(variance, sizeof(variance), "remaining %.63s", remaining);
            }
        } else if (row->has_plan) {
            (void)snprintf(variance, sizeof(variance), "no actual recorded");
        } else {
            (void)snprintf(variance, sizeof(variance), "unplanned actual");
        }
        const char *const period =
            row->start_date[0] != '\0' ? row->start_date : "(no plan period)";
        const char *const period_end = row->end_date[0] != '\0' ? row->end_date : "?";
        char period_text[32];
        (void)snprintf(period_text, sizeof(period_text), "%s..%s", period, period_end);
        char line[512];
        (void)snprintf(line, sizeof(line), "%c %-23.23s %-16.16s %14s %14s %.80s",
                       focused ? '>' : ' ', period_text, row->category, planned, actual, variance);
        apl_tui_theme_apply(&state->theme, body,
                            focused ? APL_TUI_ROLE_SELECTION : APL_TUI_ROLE_BODY);
        apl_tui_frame_draw_line(body, (int)(offset + 3), line);
        apl_tui_theme_reset(body);
    }
}

void open_money_filter_dialog(shell_state *const state)
{
    const bool budgets = state->screen == SHELL_SCREEN_BUDGET_LIST;
    apl_tui_dialog_open_form(&state->dialog, "Filter by category",
                             "Blank includes every category.");
    (void)apl_tui_dialog_add_field(
        &state->dialog, "Category (blank = all)",
        budgets ? state->budget_category_filter : state->money_category_filter, false);
    state->dialog_context = SHELL_DIALOG_CONTEXT_MONEY_FILTER;
    apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
    set_status(state, "Enter applies the filter; Esc leaves it unchanged.");
}

void open_selected_money_entry(shell_state *const state)
{
    if (state->entry_count == 0 || state->list.selected >= state->entry_count) {
        return;
    }
    apl_error error;
    apl_error_init(&error);
    const int64_t id = state->entries[state->list.selected].id;
    if (apl_ledger_service_get(state->app, active_team_id(state), id, &state->entry_detail,
                               &error) != APL_OK ||
        load_entry_activity(state, id, &error) != APL_OK) {
        show_error_dialog(state, &error);
        return;
    }
    state->section = APL_TUI_SECTION_MONEY;
    state->screen = SHELL_SCREEN_MONEY_DETAIL;
    sync_status_context(state);
    set_status(state, "c correct, Esc back. Posted entries cannot be edited or deleted silently.");
}

void open_selected_budget(shell_state *const state)
{
    if (state->budget_row_count == 0 || state->list.selected >= state->budget_row_count) {
        open_budget_form(state, false);
        return;
    }
    open_budget_form(state, true);
}

void handle_money_list_input(shell_state *const state, const uint32_t id)
{
    apl_error error;
    apl_error_init(&error);
    switch (id) {
    case NCKEY_UP:
        apl_tui_list_move(&state->list, -1);
        return;
    case NCKEY_DOWN:
        apl_tui_list_move(&state->list, 1);
        return;
    case NCKEY_PGUP:
        apl_tui_list_page(&state->list, -1);
        return;
    case NCKEY_PGDOWN:
        apl_tui_list_page(&state->list, 1);
        return;
    case NCKEY_HOME:
        apl_tui_list_home(&state->list);
        return;
    case NCKEY_END:
        apl_tui_list_end(&state->list);
        return;
    default:
        break;
    }
    if (is_enter_key(id)) {
        open_selected_money_entry(state);
        return;
    }
    switch (id) {
    case 'i':
    case 'I':
        open_money_entry_dialog(state, APL_LEDGER_KIND_INCOME);
        return;
    case 'x':
    case 'X':
        open_money_entry_dialog(state, APL_LEDGER_KIND_EXPENSE);
        return;
    case 'o':
    case 'O':
        open_money_entry_dialog(state, APL_LEDGER_KIND_OPENING_BALANCE);
        return;
    case 'u':
    case 'U':
        open_money_currency_dialog(state);
        return;
    case 'b':
    case 'B':
        state->screen = SHELL_SCREEN_BUDGET_LIST;
        if (load_budget_report(state, &error) != APL_OK) {
            show_error_dialog(state, &error);
        } else {
            set_status(state, "Budget report. n creates/edits a plan; Esc returns to the ledger.");
        }
        return;
    case 'd':
    case 'D':
        money_cycle_date_filter(&state->money_date_preset);
        if (load_ledger(state, 0, &error) != APL_OK) {
            show_error_dialog(state, &error);
        } else {
            set_status(state, "Date filter: %s.", money_date_filter_name(state->money_date_preset));
        }
        return;
    case 'g':
    case 'G':
        state->money_kind_filter = (state->money_kind_filter + 1U) % 6U;
        if (load_ledger(state, 0, &error) != APL_OK) {
            show_error_dialog(state, &error);
        } else {
            set_status(state, "Kind filter: %s.", money_filter_kind_name(state->money_kind_filter));
        }
        return;
    case 'F':
        open_money_filter_dialog(state);
        return;
    case 'C':
        state->money_date_preset = 0;
        state->money_kind_filter = 0;
        state->money_category_filter[0] = '\0';
        if (load_ledger(state, 0, &error) != APL_OK) {
            show_error_dialog(state, &error);
        } else {
            set_status(state, "Money filters cleared.");
        }
        return;
    case 'q':
    case 'Q':
        state->quit = true;
        return;
    default:
        break;
    }
}

void handle_money_detail_input(shell_state *const state, const uint32_t id)
{
    switch (id) {
    case NCKEY_UP:
    case NCKEY_DOWN: {
        apl_tui_list_move(&state->list, id == NCKEY_UP ? -1 : 1);
        open_selected_money_entry(state);
        return;
    }
    case 'c':
    case 'C':
        open_money_correct_dialog(state, &state->entry_detail, "", state->entry_detail.entry_date,
                                  state->entry_detail.category, "");
        return;
    case NCKEY_ESC:
        state->screen = SHELL_SCREEN_MONEY_LIST;
        set_status(state, "Ledger entries for the active team.");
        return;
    case 'q':
    case 'Q':
        state->quit = true;
        return;
    default:
        break;
    }
}

void handle_budget_list_input(shell_state *const state, const uint32_t id)
{
    apl_error error;
    apl_error_init(&error);
    switch (id) {
    case NCKEY_UP:
        apl_tui_list_move(&state->list, -1);
        return;
    case NCKEY_DOWN:
        apl_tui_list_move(&state->list, 1);
        return;
    case NCKEY_PGUP:
        apl_tui_list_page(&state->list, -1);
        return;
    case NCKEY_PGDOWN:
        apl_tui_list_page(&state->list, 1);
        return;
    case NCKEY_HOME:
        apl_tui_list_home(&state->list);
        return;
    case NCKEY_END:
        apl_tui_list_end(&state->list);
        return;
    default:
        break;
    }
    if (is_enter_key(id)) {
        open_selected_budget(state);
        return;
    }
    switch (id) {
    case 'n':
    case 'N':
        open_budget_form(state, false);
        return;
    case 'u':
    case 'U':
        open_money_currency_dialog(state);
        return;
    case 'd':
    case 'D':
        money_cycle_date_filter(&state->budget_date_preset);
        if (load_budget_report(state, &error) != APL_OK) {
            show_error_dialog(state, &error);
        } else {
            set_status(state, "Report date filter: %s.",
                       money_date_filter_name(state->budget_date_preset));
        }
        return;
    case 'F':
        open_money_filter_dialog(state);
        return;
    case 'C':
        state->budget_date_preset = 0;
        state->budget_category_filter[0] = '\0';
        if (load_budget_report(state, &error) != APL_OK) {
            show_error_dialog(state, &error);
        } else {
            set_status(state, "Report filters cleared.");
        }
        return;
    case NCKEY_ESC:
        state->screen = SHELL_SCREEN_MONEY_LIST;
        if (load_ledger(state, 0, &error) != APL_OK) {
            show_error_dialog(state, &error);
        } else {
            set_status(state, "Money ledger for the active team.");
        }
        return;
    case 'q':
    case 'Q':
        state->quit = true;
        return;
    default:
        break;
    }
}
