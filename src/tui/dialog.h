#ifndef APL_TUI_DIALOG_H
#define APL_TUI_DIALOG_H

#include <stddef.h>

/*
 * Reusable confirmation/error dialog state (work item 2.4).
 *
 * Keys are mapped by the screen controller to these small state transitions;
 * drawing lives in the TUI frame/screen code. Confirmation dialogs start with
 * Cancel focused so Enter cannot accidentally confirm a destructive action.
 */

typedef enum apl_tui_dialog_kind {
    APL_TUI_DIALOG_NONE = 0,
    APL_TUI_DIALOG_CONFIRM,
    APL_TUI_DIALOG_ERROR,
} apl_tui_dialog_kind;

typedef enum apl_tui_dialog_result {
    APL_TUI_DIALOG_PENDING = 0,
    APL_TUI_DIALOG_ACCEPTED,
    APL_TUI_DIALOG_DISMISSED,
} apl_tui_dialog_result;

constexpr size_t APL_TUI_DIALOG_TITLE_MAX = 96;
constexpr size_t APL_TUI_DIALOG_MESSAGE_MAX = 256;

typedef struct apl_tui_dialog {
    apl_tui_dialog_kind kind;
    char title[APL_TUI_DIALOG_TITLE_MAX];
    char message[APL_TUI_DIALOG_MESSAGE_MAX];
    /** 0 = Cancel/OK, 1 = Confirm (confirmation dialogs only). */
    unsigned focus;
} apl_tui_dialog;

/** Reset to no dialog. */
void apl_tui_dialog_init(apl_tui_dialog *dialog);

/** Open a confirmation dialog with Cancel focused. */
void apl_tui_dialog_open_confirm(apl_tui_dialog *dialog, const char *title, const char *message);

/** Open an informational error dialog with one OK target. */
void apl_tui_dialog_open_error(apl_tui_dialog *dialog, const char *title, const char *message);

/** True when a dialog is active. */
bool apl_tui_dialog_active(const apl_tui_dialog *dialog);

/** Move the confirm/cancel focus (clamped; no-op on error dialogs). */
void apl_tui_dialog_move_focus(apl_tui_dialog *dialog, int delta);

/** Activate the focused target: Accept for Confirm, Dismiss for Cancel/OK. */
apl_tui_dialog_result apl_tui_dialog_activate(apl_tui_dialog *dialog);

/** Dismiss/cancel the dialog. */
apl_tui_dialog_result apl_tui_dialog_dismiss(apl_tui_dialog *dialog);

#endif /* APL_TUI_DIALOG_H */
