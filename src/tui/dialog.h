#ifndef APL_TUI_DIALOG_H
#define APL_TUI_DIALOG_H

#include <stddef.h>

/*
 * Reusable modal dialog controller (work items 2.4 and 2.23).
 *
 * Supports confirmation, information/error, short form, and choice/list
 * dialogs. State transitions are terminal-free; drawing and mouse hit regions
 * are derived from the exported row constants. Form values live in the dialog
 * until explicit submit; Cancel/Escape never commits them. Business validation
 * and persistence remain in the application use cases.
 */

typedef enum apl_tui_dialog_kind {
    APL_TUI_DIALOG_NONE = 0,
    APL_TUI_DIALOG_CONFIRM,
    APL_TUI_DIALOG_ERROR,
    APL_TUI_DIALOG_FORM,
    APL_TUI_DIALOG_CHOICE,
} apl_tui_dialog_kind;

typedef enum apl_tui_dialog_result {
    APL_TUI_DIALOG_PENDING = 0,
    APL_TUI_DIALOG_ACCEPTED,
    APL_TUI_DIALOG_DISMISSED,
} apl_tui_dialog_result;

static constexpr size_t APL_TUI_DIALOG_TITLE_MAX = 96;
static constexpr size_t APL_TUI_DIALOG_MESSAGE_MAX = 4096;
static constexpr size_t APL_TUI_DIALOG_FIELDS_MAX = 10;
static constexpr size_t APL_TUI_DIALOG_FIELD_LABEL_MAX = 48;
static constexpr size_t APL_TUI_DIALOG_FIELD_VALUE_MAX = 256;
static constexpr size_t APL_TUI_DIALOG_FIELD_ERROR_MAX = 160;
static constexpr size_t APL_TUI_DIALOG_VALIDATION_MAX = 256;
static constexpr size_t APL_TUI_DIALOG_OPTIONS_MAX = 48;
static constexpr size_t APL_TUI_DIALOG_OPTION_LABEL_MAX = 160;
static constexpr unsigned APL_TUI_DIALOG_CONTENT_ROW = 3;

typedef struct apl_tui_dialog_field {
    char label[APL_TUI_DIALOG_FIELD_LABEL_MAX];
    char value[APL_TUI_DIALOG_FIELD_VALUE_MAX];
    bool required;
    bool invalid;
    char error[APL_TUI_DIALOG_FIELD_ERROR_MAX];
} apl_tui_dialog_field;

typedef struct apl_tui_dialog_option {
    char label[APL_TUI_DIALOG_OPTION_LABEL_MAX];
    bool enabled;
    bool active;
} apl_tui_dialog_option;

typedef struct apl_tui_dialog {
    apl_tui_dialog_kind kind;
    char title[APL_TUI_DIALOG_TITLE_MAX];
    char message[APL_TUI_DIALOG_MESSAGE_MAX];
    char validation[APL_TUI_DIALOG_VALIDATION_MAX];
    /** Confirm: 0 Cancel/OK, 1 Confirm. Form: field or Save/Cancel control.
     *  Choice: selected option index. */
    unsigned focus;
    unsigned field_count;
    apl_tui_dialog_field fields[APL_TUI_DIALOG_FIELDS_MAX];
    unsigned option_count;
    unsigned option_offset;
    unsigned viewport_rows;
    apl_tui_dialog_option options[APL_TUI_DIALOG_OPTIONS_MAX];
} apl_tui_dialog;

/** Reset to no dialog. */
void apl_tui_dialog_init(apl_tui_dialog *dialog);

/** Open a confirmation dialog with Cancel focused. */
void apl_tui_dialog_open_confirm(apl_tui_dialog *dialog, const char *title, const char *message);

/** Open an information/error dialog with one OK target. */
void apl_tui_dialog_open_error(apl_tui_dialog *dialog, const char *title, const char *message);

/** Open a short-form dialog. Add fields with add_field before display. */
void apl_tui_dialog_open_form(apl_tui_dialog *dialog, const char *title, const char *message);

/** Add a form field; returns its index or UINT_MAX when full. */
unsigned apl_tui_dialog_add_field(apl_tui_dialog *dialog, const char *label, const char *value,
                                  bool required);

/** Replace a field value (for example a prefilled edit form). */
void apl_tui_dialog_set_field_value(apl_tui_dialog *dialog, unsigned index, const char *value);

/** Open a choice/list dialog. Add options with add_option before display. */
void apl_tui_dialog_open_choice(apl_tui_dialog *dialog, const char *title, const char *message);

/** Add a choice option; returns its index or UINT_MAX when full. */
unsigned apl_tui_dialog_add_option(apl_tui_dialog *dialog, const char *label, bool enabled,
                                   bool active);

/** Select an option and adjust the visible window. */
void apl_tui_dialog_select_option(apl_tui_dialog *dialog, unsigned index);

/** Update the number of choice rows currently visible after resize. */
void apl_tui_dialog_set_viewport_rows(apl_tui_dialog *dialog, unsigned rows);

/** True when a dialog is active. */
bool apl_tui_dialog_active(const apl_tui_dialog *dialog);

/** Move focus/selection with wrap (clamped for confirmations). */
void apl_tui_dialog_move_focus(apl_tui_dialog *dialog, int delta);

/** Activate the focused control.
 *
 * Form: required fields are checked; failures return PENDING, keep every value,
 * mark the invalid field(s), and keep focus inside the dialog. Cancel returns
 * DISMISSED. Choice: a disabled option cannot be activated. */
apl_tui_dialog_result apl_tui_dialog_activate(apl_tui_dialog *dialog);

/** Dismiss/cancel the dialog; no values are committed. */
apl_tui_dialog_result apl_tui_dialog_dismiss(apl_tui_dialog *dialog);

/** Append typed UTF-8 text to the focused form field. */
void apl_tui_dialog_insert_text(apl_tui_dialog *dialog, const char *text);

/** Remove one UTF-8 code point from the focused form field. */
void apl_tui_dialog_backspace(apl_tui_dialog *dialog);

/** True when focus is on a form field (not Save/Cancel). */
bool apl_tui_dialog_focus_is_field(const apl_tui_dialog *dialog);

/** Mark a field invalid after business validation and focus it. */
void apl_tui_dialog_set_field_error(apl_tui_dialog *dialog, unsigned index, const char *message);

/** Clear all field/validation errors. */
void apl_tui_dialog_clear_errors(apl_tui_dialog *dialog);

/** Focused form field index, or UINT_MAX when not a field. */
unsigned apl_tui_dialog_focused_field(const apl_tui_dialog *dialog);

/** Selected choice option index, or UINT_MAX when empty. */
unsigned apl_tui_dialog_selected_option(const apl_tui_dialog *dialog);

/** Option at `index`, or nullptr. */
const apl_tui_dialog_option *apl_tui_dialog_option_at(const apl_tui_dialog *dialog, unsigned index);

/** Field at `index`, or nullptr. */
const apl_tui_dialog_field *apl_tui_dialog_field_at(const apl_tui_dialog *dialog, unsigned index);

/** Number of choice options visible in a dialog `rows` high. */
unsigned apl_tui_dialog_visible_options(const apl_tui_dialog *dialog, unsigned rows);

/** Mouse hit test in overlay-local coordinates.
 *
 * Returns the focused control index: form field index, Save
 * (`field_count`), Cancel (`field_count + 1`), choice option index, or
 * UINT_MAX for no control. */
unsigned apl_tui_dialog_control_at(const apl_tui_dialog *dialog, unsigned rows, unsigned cols,
                                   unsigned local_row, unsigned local_column);

#endif /* APL_TUI_DIALOG_H */
