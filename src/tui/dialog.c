#include "tui/dialog.h"

#include <stdio.h>

/** Copy a title/message, rejecting null and truncating safely. */
static void copy_text(char *const out, const size_t out_size, const char *const text)
{
    (void)snprintf(out, out_size, "%s", (text != nullptr) ? text : "");
}

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
    copy_text(dialog->title, sizeof(dialog->title), title);
    copy_text(dialog->message, sizeof(dialog->message), message);
}

void apl_tui_dialog_open_error(apl_tui_dialog *const dialog, const char *const title,
                               const char *const message)
{
    if (dialog == nullptr) {
        return;
    }
    *dialog = (apl_tui_dialog){.kind = APL_TUI_DIALOG_ERROR, .focus = 0};
    copy_text(dialog->title, sizeof(dialog->title), title);
    copy_text(dialog->message, sizeof(dialog->message), message);
}

bool apl_tui_dialog_active(const apl_tui_dialog *const dialog)
{
    return dialog != nullptr && dialog->kind != APL_TUI_DIALOG_NONE;
}

void apl_tui_dialog_move_focus(apl_tui_dialog *const dialog, const int delta)
{
    if (dialog == nullptr || dialog->kind != APL_TUI_DIALOG_CONFIRM) {
        return;
    }
    if (delta < 0) {
        dialog->focus = 0;
    } else if (delta > 0) {
        dialog->focus = 1;
    }
}

apl_tui_dialog_result apl_tui_dialog_activate(apl_tui_dialog *const dialog)
{
    if (dialog == nullptr || dialog->kind == APL_TUI_DIALOG_NONE) {
        return APL_TUI_DIALOG_PENDING;
    }
    if (dialog->kind == APL_TUI_DIALOG_CONFIRM && dialog->focus == 1) {
        return APL_TUI_DIALOG_ACCEPTED;
    }
    return APL_TUI_DIALOG_DISMISSED;
}

apl_tui_dialog_result apl_tui_dialog_dismiss(apl_tui_dialog *const dialog)
{
    if (dialog == nullptr || dialog->kind == APL_TUI_DIALOG_NONE) {
        return APL_TUI_DIALOG_PENDING;
    }
    return APL_TUI_DIALOG_DISMISSED;
}
