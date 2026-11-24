#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <criterion/criterion.h>

#include "db/path.h"
#include "test_support.h"

Test(db_path, explicit_override_wins)
{
    char resolved[APL_DB_PATH_MAX];

    apl_error err;
    apl_error_init(&err);

    cr_assert_eq(apl_db_resolve_path_ex("/tmp/explicit/custom.sqlite3", "/tmp/xdg", "/home/user",
                                        resolved, sizeof(resolved), &err), APL_OK);
    cr_assert_str_eq(resolved, "/tmp/explicit/custom.sqlite3");
}

Test(db_path, absolute_xdg_data_home_is_used)
{
    char resolved[APL_DB_PATH_MAX];

    apl_error err;
    apl_error_init(&err);

    cr_assert_eq(apl_db_resolve_path_ex(nullptr, "/data", "/home/user", resolved, sizeof(resolved),
                                        &err), APL_OK);
    cr_assert_str_eq(resolved, "/data/apl1/apl1.sqlite3");
}

Test(db_path, relative_xdg_falls_back_to_home)
{
    char resolved[APL_DB_PATH_MAX];

    apl_error err;
    apl_error_init(&err);

    /* XDG spec violation: a relative XDG_DATA_HOME is ignored. */
    cr_assert_eq(apl_db_resolve_path_ex(nullptr, "relative/data", "/home/user", resolved,
                                        sizeof(resolved), &err), APL_OK);
    cr_assert_str_eq(resolved, "/home/user/.local/share/apl1/apl1.sqlite3");
}

Test(db_path, home_fallback_without_xdg)
{
    char resolved[APL_DB_PATH_MAX];

    apl_error err;
    apl_error_init(&err);

    cr_assert_eq(apl_db_resolve_path_ex(nullptr, nullptr, "/home/user", resolved, sizeof(resolved),
                                        &err), APL_OK);
    cr_assert_str_eq(resolved, "/home/user/.local/share/apl1/apl1.sqlite3");
}

Test(db_path, empty_environment_values_are_ignored)
{
    char resolved[APL_DB_PATH_MAX];

    apl_error err;
    apl_error_init(&err);

    cr_assert_eq(apl_db_resolve_path_ex(nullptr, "", "", resolved, sizeof(resolved), &err),
                 APL_ERR_NOT_FOUND);
    cr_assert_not(apl_error_is_success(&err));
    cr_assert_neq(strstr(apl_error_message(&err), "XDG_DATA_HOME"), nullptr);
}

Test(db_path, missing_environment_fails_instead_of_creating_cwd_database)
{
    char resolved[APL_DB_PATH_MAX];

    apl_error err;
    apl_error_init(&err);

    cr_assert_eq(apl_db_resolve_path_ex(nullptr, nullptr, nullptr, resolved, sizeof(resolved), &err),
                 APL_ERR_NOT_FOUND);
}

Test(db_path, too_small_buffer_is_rejected_not_truncated)
{
    char tiny[8];

    apl_error err;
    apl_error_init(&err);

    cr_assert_eq(apl_db_resolve_path_ex("/tmp/explicit/custom.sqlite3", nullptr, nullptr, tiny,
                                        sizeof(tiny), &err), APL_ERR_INVALID_ARGUMENT);
}

Test(db_path, ensure_parent_directory_creates_missing_directories)
{
    char root[128];
    char path[192];
    char leaf[192];

    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_temp_dir(root, sizeof(root), &err), APL_OK);

    snprintf(path, sizeof(path), "%s/one/two/data.sqlite3", root);
    cr_assert_eq(apl_db_ensure_parent_directory(path, &err), APL_OK);

    snprintf(leaf, sizeof(leaf), "%s/one/two", root);
    struct stat created;
    cr_assert_eq(stat(leaf, &created), 0);
    cr_assert(S_ISDIR(created.st_mode));

    /* No file is created by path preparation itself. */
    cr_assert_eq(stat(path, &created), -1);

    cr_assert_eq(rmdir(leaf), 0);
    cr_assert_eq(rmdir(strcat(strcpy(leaf, root), "/one")), 0);
    cr_assert_eq(apl_test_remove_temp_dir(root, &err), APL_OK);
}

Test(db_path, ensure_parent_directory_fails_on_file_collision)
{
    char root[128];
    char blocker_path[192];
    char path[192];

    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_temp_dir(root, sizeof(root), &err), APL_OK);

    /* A file where a directory component would need to be created. */
    snprintf(blocker_path, sizeof(blocker_path), "%s/blocking-file", root);
    FILE *const blocker = fopen(blocker_path, "w");
    cr_assert_not_null(blocker);
    cr_assert_eq(fclose(blocker), 0);

    snprintf(path, sizeof(path), "%s/blocking-file/deeper/data.sqlite3", root);
    cr_assert_eq(apl_db_ensure_parent_directory(path, &err), APL_ERR_IO);
    cr_assert_not(apl_error_is_success(&err));

    cr_assert_eq(unlink(blocker_path), 0);
    cr_assert_eq(apl_test_remove_temp_dir(root, &err), APL_OK);
}

Test(db_path, ensure_parent_directory_rejects_empty_path)
{
    apl_error err;
    apl_error_init(&err);

    cr_assert_eq(apl_db_ensure_parent_directory("", &err), APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_db_ensure_parent_directory(nullptr, &err), APL_ERR_INVALID_ARGUMENT);
}
