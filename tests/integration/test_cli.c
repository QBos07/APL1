#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include <criterion/criterion.h>

#include "app/exit_code.h"
#include "test_support.h"

/* Process-level smoke checks for the interactive launch (FH-11/FH-12). They
 * deliberately avoid a terminal driver: the help and usage paths must work
 * headless, and the no-terminal path must fail before creating a database.
 * The `--database` success path is exercised by the opt-in tmux harness
 * (scripts/smoke_tui.py) because Notcurses needs a real terminal. */

#ifndef APL1_BINARY_PATH
#error "APL1_BINARY_PATH must point at the built apl1 executable"
#endif

/** Run the app with stdio redirected to /dev/null and return the wait status. */
static int run_headless(char *const argv[], const int stdin_fd)
{
    const int devnull = open("/dev/null", O_RDWR);
    cr_assert_geq(devnull, 0);

    const pid_t pid = fork();
    cr_assert_geq(pid, 0, "fork failed: %s", strerror(errno));

    if (pid == 0) {
        if (dup2(stdin_fd, STDIN_FILENO) < 0 || dup2(devnull, STDOUT_FILENO) < 0 ||
            dup2(devnull, STDERR_FILENO) < 0) {
            _exit(127);
        }
        close(devnull);
        execv(APL1_BINARY_PATH, argv);
        _exit(127);
    }

    close(devnull);
    int status = 0;
    cr_assert_eq(waitpid(pid, &status, 0), pid);
    return status;
}

/**
 * Run the app with the given extra arguments. stdin is /dev/null so the child
 * is not accidentally attached to the test runner's terminal.
 */
static int run_app(char *const argv[])
{
    const int devnull = open("/dev/null", O_RDONLY);
    cr_assert_geq(devnull, 0);
    const int status = run_headless(argv, devnull);
    close(devnull);
    return status;
}

Test(cli, help_exits_successfully_without_a_database)
{
    char *const argv[] = {"apl1", "--help", nullptr};

    const int status = run_app(argv);

    cr_assert(WIFEXITED(status), "app did not exit normally");
    cr_assert_eq(WEXITSTATUS(status), APL_EXIT_SUCCESS);
}

Test(cli, unknown_option_exits_with_usage_code)
{
    char *const argv[] = {"apl1", "--bogus", nullptr};

    const int status = run_app(argv);

    cr_assert(WIFEXITED(status), "app did not exit normally");
    cr_assert_eq(WEXITSTATUS(status), APL_EXIT_USAGE);
}

Test(cli, missing_database_value_exits_with_usage_code)
{
    char *const argv[] = {"apl1", "--database", nullptr};

    const int status = run_app(argv);

    cr_assert(WIFEXITED(status), "app did not exit normally");
    cr_assert_eq(WEXITSTATUS(status), APL_EXIT_USAGE);
}

Test(cli, no_terminal_fails_before_creating_the_database)
{
    apl_error err;
    apl_error_init(&err);

    char dir[APL_TEST_DIR_MAX];
    cr_assert_eq(apl_test_temp_dir(dir, sizeof(dir), &err), APL_OK);

    char path[APL_TEST_PATH_MAX];
    const int written = snprintf(path, sizeof(path), "%s/never-created.sqlite3", dir);
    cr_assert_gt(written, 0);
    cr_assert_lt((size_t)written, sizeof(path));

    char *const argv[] = {"apl1", "--database", path, nullptr};
    const int status = run_app(argv);

    cr_assert(WIFEXITED(status), "app did not exit normally");
    cr_assert_eq(WEXITSTATUS(status), APL_EXIT_TERMINAL_FAILURE);
    cr_assert_eq(access(path, F_OK), -1, "database was created before terminal verification");

    cr_assert_eq(apl_test_remove_temp_dir(dir, &err), APL_OK);
}
