#define _POSIX_C_SOURCE 200809L

#include "tui/lifecycle.h"

#include <notcurses/notcurses.h>
#include <time.h>
#include <unistd.h>

/** A usable interactive terminal is required: stdin and stdout must both be
 *  terminal devices. Without one, Notcurses would initialize headless and
 *  the shell could never be operated (input EOFs immediately), so refuse
 *  here with a clear error instead. */
static bool interactive_terminal_available(void)
{
    return isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
}

/** Discard late terminal query replies before interactive input begins.
 *
 * Notcurses probes the terminal at startup (device attributes, dynamic colors,
 * XTVERSION, ...). A slow or asynchronous terminal, notably Windows Terminal
 * over SSH, can deliver a reply after initialization has stopped waiting. The
 * reply arrives on stdin and would otherwise be decoded as typed keys (for
 * example the ":d7d7/d7d7/5f5f" tail of an OSC 11 color response). Wait for a
 * short quiet period, with a bounded total, and drop whatever arrived.
 */
static void drain_terminal_replies(struct notcurses *const context)
{
    struct timespec quiet = {.tv_sec = 0, .tv_nsec = 200000000};
    struct timespec started = {};
    (void)clock_gettime(CLOCK_MONOTONIC, &started);
    for (;;) {
        ncinput input = {};
        const uint32_t id = notcurses_get(context, &quiet, &input);
        if (id == 0 || id == (uint32_t)-1) {
            break;
        }
        struct timespec now = {};
        (void)clock_gettime(CLOCK_MONOTONIC, &now);
        const int64_t elapsed_ms = (int64_t)(now.tv_sec - started.tv_sec) * 1000 +
                                   (int64_t)((now.tv_nsec - started.tv_nsec) / 1000000);
        if (elapsed_ms >= 750) {
            break;
        }
    }
}

apl_status apl_tui_start(apl_tui *const tui, apl_error *const error)
{
    if (tui == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "TUI context is nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }
    if (tui->notcurses != nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "TUI is already started");
        return APL_ERR_INVALID_ARGUMENT;
    }
    if (!interactive_terminal_available()) {
        apl_error_set(error, APL_ERR_TUI_INIT,
                      "Notcurses initialization failed; a usable interactive terminal is required");
        return APL_ERR_TUI_INIT;
    }

    const notcurses_options options = {};
    struct notcurses *const context = notcurses_init(&options, nullptr);
    if (context == nullptr) {
        /* Partial startup: there is no context to stop, and the value stays
         * uninitialized so cleanup paths remain safe. */
        tui->notcurses = nullptr;
        apl_error_set(error, APL_ERR_TUI_INIT,
                      "Notcurses initialization failed; a usable interactive terminal is required");
        return APL_ERR_TUI_INIT;
    }

    tui->notcurses = context;
    drain_terminal_replies(context);
    return APL_OK;
}

void apl_tui_stop(apl_tui *const tui)
{
    if (tui == nullptr || tui->notcurses == nullptr) {
        return;
    }
    notcurses_stop(tui->notcurses);
    tui->notcurses = nullptr;
}
