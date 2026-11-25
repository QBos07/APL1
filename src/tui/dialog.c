#include "tui/dialog.h"

#include <limits.h>

#include "core/text.h"
#include "core/util.h"
#include "tui/text.h"

void apl_tui_dialog_init(apl_tui_dialog *const dialog)
{
    if (dialog == nullptr) {
        return;
    }
    *dialog = (apl_tui_dialog){};
}

void apl_tui_dialog_open_confirm(apl_tui_dialog *const dialog, const char *const title,
                                 const char *const message)
{
    if (dialog == nullptr) {
        return;
    }
    *dialog = (apl_tui_dialog){.kind = APL_TUI_DIALOG_CONFIRM, .focus = 0};
    apl_text_copy(dialog->title, sizeof(dialog->title), title);
    apl_text_copy(dialog->message, sizeof(dialog->message), message);
}

void apl_tui_dialog_open_error(apl_tui_dialog *const dialog, const char *const title,
                               const char *const message)
{
    if (dialog == nullptr) {
        return;
    }
    *dialog = (apl_tui_dialog){.kind = APL_TUI_DIALOG_ERROR, .focus = 0};
    apl_text_copy(dialog->title, sizeof(dialog->title), title);
    apl_text_copy(dialog->message, sizeof(dialog->message), message);
}

void apl_tui_dialog_open_form(apl_tui_dialog *const dialog, const char *const title,
                              const char *const message)
{
    if (dialog == nullptr) {
        return;
    }
    *dialog = (apl_tui_dialog){
        .kind = APL_TUI_DIALOG_FORM,
        .focus = 0,
        .viewport_rows = 6,
    };
    apl_text_copy(dialog->title, sizeof(dialog->title), title);
    apl_text_copy(dialog->message, sizeof(dialog->message), message);
}

unsigned apl_tui_dialog_add_field(apl_tui_dialog *const dialog, const char *const label,
                                  const char *const value, const bool required)
{
    if (dialog == nullptr || dialog->kind != APL_TUI_DIALOG_FORM ||
        dialog->field_count >= APL_TUI_DIALOG_FIELDS_MAX) {
        return UINT_MAX;
    }
    const unsigned index = dialog->field_count++;
    apl_tui_dialog_field *const field = &dialog->fields[index];
    apl_text_copy(field->label, sizeof(field->label), label);
    apl_text_copy(field->value, sizeof(field->value), value);
    field->required = required;
    return index;
}

void apl_tui_dialog_set_field_value(apl_tui_dialog *const dialog, const unsigned index,
                                    const char *const value)
{
    if (dialog == nullptr || index >= dialog->field_count) {
        return;
    }
    apl_text_copy(dialog->fields[index].value, sizeof(dialog->fields[index].value), value);
}

void apl_tui_dialog_open_choice(apl_tui_dialog *const dialog, const char *const title,
                                const char *const message)
{
    if (dialog == nullptr) {
        return;
    }
    *dialog = (apl_tui_dialog){
        .kind = APL_TUI_DIALOG_CHOICE,
        .focus = 0,
        .viewport_rows = 6,
    };
    apl_text_copy(dialog->title, sizeof(dialog->title), title);
    apl_text_copy(dialog->message, sizeof(dialog->message), message);
}

static void keep_option_visible(apl_tui_dialog *const dialog)
{
    const unsigned visible = (dialog->viewport_rows > 0) ? dialog->viewport_rows : 1;
    if (dialog->focus < dialog->option_offset) {
        dialog->option_offset = dialog->focus;
    } else if (dialog->focus >= dialog->option_offset + visible) {
        dialog->option_offset = dialog->focus - visible + 1;
    }
    const unsigned max_offset =
        (dialog->option_count > visible) ? dialog->option_count - visible : 0;
    dialog->option_offset = apl_min_unsigned(dialog->option_offset, max_offset);
}

unsigned apl_tui_dialog_add_option(apl_tui_dialog *const dialog, const char *const label,
                                   const bool enabled, const bool active)
{
    if (dialog == nullptr || dialog->kind != APL_TUI_DIALOG_CHOICE ||
        dialog->option_count >= APL_TUI_DIALOG_OPTIONS_MAX) {
        return UINT_MAX;
    }
    const unsigned index = dialog->option_count++;
    apl_tui_dialog_option *const option = &dialog->options[index];
    apl_text_copy(option->label, sizeof(option->label), label);
    option->enabled = enabled;
    option->active = active;
    keep_option_visible(dialog);
    return index;
}

void apl_tui_dialog_select_option(apl_tui_dialog *const dialog, const unsigned index)
{
    if (dialog == nullptr || dialog->kind != APL_TUI_DIALOG_CHOICE ||
        index >= dialog->option_count) {
        return;
    }
    dialog->focus = index;
    keep_option_visible(dialog);
}

void apl_tui_dialog_set_viewport_rows(apl_tui_dialog *const dialog, const unsigned rows)
{
    if (dialog == nullptr || dialog->kind != APL_TUI_DIALOG_CHOICE) {
        return;
    }
    dialog->viewport_rows = (rows > 0) ? rows : 1;
    keep_option_visible(dialog);
}

bool apl_tui_dialog_active(const apl_tui_dialog *const dialog)
{
    return dialog != nullptr && dialog->kind != APL_TUI_DIALOG_NONE;
}

static unsigned option_count_enabled(const apl_tui_dialog *const dialog)
{
    unsigned count = 0;
    for (unsigned index = 0; index < dialog->option_count; ++index) {
        if (dialog->options[index].enabled) {
            ++count;
        }
    }
    return count;
}

static void move_choice_focus(apl_tui_dialog *const dialog, const int delta)
{
    if (dialog->option_count == 0 || option_count_enabled(dialog) == 0) {
        return;
    }
    const int count = (int)dialog->option_count;
    int index = (int)dialog->focus;
    for (int step = 0; step < count; ++step) {
        index = (index + ((delta > 0) ? 1 : -1) + count) % count;
        if (dialog->options[index].enabled) {
            dialog->focus = (unsigned)index;
            keep_option_visible(dialog);
            return;
        }
    }
}

void apl_tui_dialog_move_focus(apl_tui_dialog *const dialog, const int delta)
{
    if (dialog == nullptr || delta == 0) {
        return;
    }
    if (dialog->kind == APL_TUI_DIALOG_CONFIRM) {
        dialog->focus = (delta < 0) ? 0 : 1;
        return;
    }
    if (dialog->kind == APL_TUI_DIALOG_FORM) {
        const int controls = (int)dialog->field_count + 2; /* fields + Save + Cancel */
        int index = (int)dialog->focus + delta;
        while (index < 0) {
            index += controls;
        }
        index %= controls;
        dialog->focus = (unsigned)index;
        return;
    }
    if (dialog->kind == APL_TUI_DIALOG_CHOICE) {
        move_choice_focus(dialog, delta);
    }
}

static bool validate_fields(apl_tui_dialog *const dialog)
{
    bool valid = true;
    unsigned first_invalid = UINT_MAX;
    for (unsigned index = 0; index < dialog->field_count; ++index) {
        apl_tui_dialog_field *const field = &dialog->fields[index];
        if (field->required && field->value[0] == '\0') {
            field->invalid = true;
            apl_text_copy(field->error, sizeof(field->error), "Required value.");
            if (first_invalid == UINT_MAX) {
                first_invalid = index;
            }
            valid = false;
        }
    }
    if (!valid) {
        apl_text_copy(dialog->validation, sizeof(dialog->validation),
                      "Fill in the required field marked above.");
        dialog->focus = first_invalid;
    }
    return valid;
}

apl_tui_dialog_result apl_tui_dialog_activate(apl_tui_dialog *const dialog)
{
    if (dialog == nullptr || dialog->kind == APL_TUI_DIALOG_NONE) {
        return APL_TUI_DIALOG_PENDING;
    }
    switch (dialog->kind) {
    case APL_TUI_DIALOG_CONFIRM:
        return (dialog->focus == 1) ? APL_TUI_DIALOG_ACCEPTED : APL_TUI_DIALOG_DISMISSED;
    case APL_TUI_DIALOG_ERROR:
        return APL_TUI_DIALOG_DISMISSED;
    case APL_TUI_DIALOG_FORM:
        if (dialog->focus == dialog->field_count + 1) {
            return APL_TUI_DIALOG_DISMISSED;
        }
        return validate_fields(dialog) ? APL_TUI_DIALOG_ACCEPTED : APL_TUI_DIALOG_PENDING;
    case APL_TUI_DIALOG_CHOICE:
        if (dialog->option_count == 0 || dialog->focus >= dialog->option_count ||
            !dialog->options[dialog->focus].enabled) {
            return APL_TUI_DIALOG_PENDING;
        }
        return APL_TUI_DIALOG_ACCEPTED;
    case APL_TUI_DIALOG_NONE:
        break;
    }
    return APL_TUI_DIALOG_PENDING;
}

apl_tui_dialog_result apl_tui_dialog_dismiss(apl_tui_dialog *const dialog)
{
    if (dialog == nullptr || dialog->kind == APL_TUI_DIALOG_NONE) {
        return APL_TUI_DIALOG_PENDING;
    }
    return APL_TUI_DIALOG_DISMISSED;
}

bool apl_tui_dialog_focus_is_field(const apl_tui_dialog *const dialog)
{
    return dialog != nullptr && dialog->kind == APL_TUI_DIALOG_FORM &&
           dialog->focus < dialog->field_count;
}

void apl_tui_dialog_insert_text(apl_tui_dialog *const dialog, const char *const text)
{
    if (!apl_tui_dialog_focus_is_field(dialog) || text == nullptr) {
        return;
    }
    apl_tui_dialog_field *const field = &dialog->fields[dialog->focus];
    (void)apl_tui_text_append(field->value, sizeof(field->value), text);
    field->invalid = false;
    field->error[0] = '\0';
    dialog->validation[0] = '\0';
}

void apl_tui_dialog_backspace(apl_tui_dialog *const dialog)
{
    if (!apl_tui_dialog_focus_is_field(dialog)) {
        return;
    }
    apl_tui_dialog_field *const field = &dialog->fields[dialog->focus];
    (void)apl_tui_text_backspace(field->value);
    field->invalid = false;
    field->error[0] = '\0';
    dialog->validation[0] = '\0';
}

void apl_tui_dialog_set_field_error(apl_tui_dialog *const dialog, const unsigned index,
                                    const char *const message)
{
    if (dialog == nullptr || index >= dialog->field_count) {
        return;
    }
    apl_tui_dialog_field *const field = &dialog->fields[index];
    field->invalid = true;
    apl_text_copy(field->error, sizeof(field->error), message);
    apl_text_copy(dialog->validation, sizeof(dialog->validation), message);
    dialog->focus = index;
}

void apl_tui_dialog_clear_errors(apl_tui_dialog *const dialog)
{
    if (dialog == nullptr) {
        return;
    }
    dialog->validation[0] = '\0';
    for (unsigned index = 0; index < dialog->field_count; ++index) {
        dialog->fields[index].invalid = false;
        dialog->fields[index].error[0] = '\0';
    }
}

unsigned apl_tui_dialog_focused_field(const apl_tui_dialog *const dialog)
{
    return apl_tui_dialog_focus_is_field(dialog) ? dialog->focus : UINT_MAX;
}

unsigned apl_tui_dialog_selected_option(const apl_tui_dialog *const dialog)
{
    if (dialog == nullptr || dialog->kind != APL_TUI_DIALOG_CHOICE || dialog->option_count == 0 ||
        dialog->focus >= dialog->option_count) {
        return UINT_MAX;
    }
    return dialog->focus;
}

const apl_tui_dialog_option *apl_tui_dialog_option_at(const apl_tui_dialog *const dialog,
                                                      const unsigned index)
{
    if (dialog == nullptr || dialog->kind != APL_TUI_DIALOG_CHOICE ||
        index >= dialog->option_count) {
        return nullptr;
    }
    return &dialog->options[index];
}

const apl_tui_dialog_field *apl_tui_dialog_field_at(const apl_tui_dialog *const dialog,
                                                    const unsigned index)
{
    if (dialog == nullptr || dialog->kind != APL_TUI_DIALOG_FORM || index >= dialog->field_count) {
        return nullptr;
    }
    return &dialog->fields[index];
}

unsigned apl_tui_dialog_visible_options(const apl_tui_dialog *const dialog, const unsigned rows)
{
    (void)dialog;
    if (rows <= 5) {
        return 0;
    }
    return rows - 5;
}

unsigned apl_tui_dialog_control_at(const apl_tui_dialog *const dialog, const unsigned rows,
                                   const unsigned cols, const unsigned local_row,
                                   const unsigned local_column)
{
    if (dialog == nullptr || rows < 4 || cols < 4) {
        return UINT_MAX;
    }
    const unsigned button_row = rows - 2;
    switch (dialog->kind) {
    case APL_TUI_DIALOG_CONFIRM:
        if (local_row != button_row) {
            return UINT_MAX;
        }
        if (local_column >= 2 && local_column < 12) {
            return 0;
        }
        if (local_column >= 15 && local_column < 26) {
            return 1;
        }
        return UINT_MAX;
    case APL_TUI_DIALOG_ERROR:
        if (local_row == button_row && local_column >= 2 && local_column < 8) {
            return 0;
        }
        return UINT_MAX;
    case APL_TUI_DIALOG_FORM: {
        if (local_row >= APL_TUI_DIALOG_CONTENT_ROW &&
            local_row < APL_TUI_DIALOG_CONTENT_ROW + dialog->field_count) {
            return local_row - APL_TUI_DIALOG_CONTENT_ROW;
        }
        if (local_row == button_row) {
            if (local_column >= 2 && local_column < 12) {
                return dialog->field_count;
            }
            if (local_column >= 14 && local_column < 24) {
                return dialog->field_count + 1;
            }
        }
        return UINT_MAX;
    }
    case APL_TUI_DIALOG_CHOICE: {
        if (local_row < APL_TUI_DIALOG_CONTENT_ROW) {
            return UINT_MAX;
        }
        const unsigned visible = apl_tui_dialog_visible_options(dialog, rows);
        const unsigned row = local_row - APL_TUI_DIALOG_CONTENT_ROW;
        if (row < visible) {
            const unsigned index = dialog->option_offset + row;
            return (index < dialog->option_count) ? index : UINT_MAX;
        }
        return UINT_MAX;
    }
    case APL_TUI_DIALOG_NONE:
        break;
    }
    return UINT_MAX;
}
