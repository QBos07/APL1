#include <stdio.h>

#include "core/error.h"
#include "tui/lifecycle.h"
#include "tui/shell.h"

int main(void)
{
    apl_error error;
    apl_error_init(&error);

    apl_tui tui = {0};
    if (apl_tui_start(&tui, &error) != APL_OK) {
        fprintf(stderr, "apl1: %s\n", apl_error_message(&error));
        return 1;
    }

    const apl_status status = apl_tui_shell_run(&tui, &error);
    apl_tui_stop(&tui);

    if (status != APL_OK) {
        fprintf(stderr, "apl1: %s\n", apl_error_message(&error));
        return 1;
    }
    return 0;
}
