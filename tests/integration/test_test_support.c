#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <criterion/criterion.h>

#include "test_support.h"

/** Write a small file, failing the test on any filesystem error. */
static void write_file(const char *const path, const char *const contents)
{
    FILE *const file = fopen(path, "w");
    cr_assert_not_null(file, "cannot create %s", path);
    cr_assert_geq(fputs(contents, file), 0);
    cr_assert_eq(fclose(file), 0);
}

static void make_child(char *const out, const size_t out_size, const char *const base,
                       const char *const name)
{
    const int written = snprintf(out, out_size, "%s/%s", base, name);
    cr_assert_gt(written, 0);
    cr_assert_lt((size_t)written, out_size);
}

/** Append a directory component, create it, and update `path` in place. */
static void extend_path(char *const path, const size_t path_size, const char *const component)
{
    char next[APL_TEST_PATH_MAX];
    make_child(next, sizeof(next), path, component);
    cr_assert_eq(mkdir(next, 0777), 0, "cannot create %s", next);
    const int copied = snprintf(path, path_size, "%s", next);
    cr_assert_gt(copied, 0);
    cr_assert_lt((size_t)copied, path_size);
}

Test(test_support, custom_tmpdir_is_honored)
{
    apl_error err;
    apl_error_init(&err);

    char outer[APL_TEST_DIR_MAX];
    cr_assert_eq(apl_test_temp_dir(outer, sizeof(outer), &err), APL_OK);

    char base[APL_TEST_PATH_MAX];
    cr_assert_eq(apl_test_temp_base_ex(outer, base, sizeof(base), &err), APL_OK);
    cr_assert_str_eq(base, outer);

    char created[APL_TEST_PATH_MAX];
    cr_assert_eq(apl_test_temp_dir_in(outer, created, sizeof(created), &err), APL_OK);
    cr_assert_eq(strncmp(created, outer, strlen(outer)), 0);

    struct stat info;
    cr_assert_eq(stat(created, &info), 0);
    cr_assert(S_ISDIR(info.st_mode));

    cr_assert_eq(apl_test_remove_temp_dir(created, &err), APL_OK);
    cr_assert_eq(apl_test_remove_temp_dir(outer, &err), APL_OK);
}

Test(test_support, unset_or_invalid_tmpdir_falls_back_to_tmp)
{
    apl_error err;
    apl_error_init(&err);

    char base[APL_TEST_PATH_MAX];
    const char *const invalid[] = {
        nullptr,
        "",
        "relative/path",
        "/definitely/not/a/real/tmpdir-for-apl1-tests",
    };
    for (size_t index = 0; index < sizeof(invalid) / sizeof(invalid[0]); ++index) {
        cr_assert_eq(apl_test_temp_base_ex(invalid[index], base, sizeof(base), &err), APL_OK);
        cr_assert_str_eq(base, "/tmp");
    }
}

Test(test_support, non_directory_tmpdir_falls_back_to_tmp)
{
    apl_error err;
    apl_error_init(&err);

    char outer[APL_TEST_DIR_MAX];
    cr_assert_eq(apl_test_temp_dir(outer, sizeof(outer), &err), APL_OK);

    char file[APL_TEST_PATH_MAX];
    make_child(file, sizeof(file), outer, "not-a-directory");
    write_file(file, "x");

    char base[APL_TEST_PATH_MAX];
    cr_assert_eq(apl_test_temp_base_ex(file, base, sizeof(base), &err), APL_OK);
    cr_assert_str_eq(base, "/tmp");

    cr_assert_eq(apl_test_remove_temp_dir(outer, &err), APL_OK);
}

Test(test_support, created_directories_are_unique)
{
    apl_error err;
    apl_error_init(&err);

    char first[APL_TEST_DIR_MAX];
    char second[APL_TEST_DIR_MAX];
    cr_assert_eq(apl_test_temp_dir(first, sizeof(first), &err), APL_OK);
    cr_assert_eq(apl_test_temp_dir(second, sizeof(second), &err), APL_OK);
    cr_assert_str_neq(first, second);

    cr_assert_eq(apl_test_remove_temp_dir(first, &err), APL_OK);
    cr_assert_eq(apl_test_remove_temp_dir(second, &err), APL_OK);
}

Test(test_support, overlong_tmpdir_falls_back_to_tmp)
{
    static const char *const suffix = "/apl1-test-XXXXXX";

    apl_error err;
    apl_error_init(&err);

    char root[APL_TEST_DIR_MAX];
    cr_assert_eq(apl_test_temp_dir(root, sizeof(root), &err), APL_OK);

    char component[128];
    memset(component, 'd', sizeof(component) - 1);
    component[sizeof(component) - 1] = '\0';

    char long_path[APL_TEST_PATH_MAX];
    const int copied = snprintf(long_path, sizeof(long_path), "%s", root);
    cr_assert_gt(copied, 0);
    cr_assert_lt((size_t)copied, sizeof(long_path));

    while (strlen(long_path) + strlen(suffix) < APL_TEST_DIR_MAX) {
        extend_path(long_path, sizeof(long_path), component);
    }
    /* One more component pushes the base past the documented limit. */
    extend_path(long_path, sizeof(long_path), component);
    cr_assert_geq(strlen(long_path) + strlen(suffix), APL_TEST_DIR_MAX);

    char base[APL_TEST_PATH_MAX];
    cr_assert_eq(apl_test_temp_base_ex(long_path, base, sizeof(base), &err), APL_OK);
    cr_assert_str_eq(base, "/tmp");

    cr_assert_eq(apl_test_remove_temp_dir(root, &err), APL_OK);
}

Test(test_support, overlong_base_is_rejected_by_temp_dir_in)
{
    apl_error err;
    apl_error_init(&err);

    char base[APL_TEST_PATH_MAX];
    memset(base, 'a', APL_TEST_DIR_MAX);
    base[0] = '/';
    base[APL_TEST_DIR_MAX] = '\0';

    char created[APL_TEST_PATH_MAX];
    cr_assert_eq(apl_test_temp_dir_in(base, created, sizeof(created), &err),
                 APL_ERR_INVALID_ARGUMENT);
    cr_assert_neq(strstr(apl_error_message(&err), "too long"), nullptr);
}

Test(test_support, cleanup_removes_nested_files_and_directories)
{
    apl_error err;
    apl_error_init(&err);

    char root[APL_TEST_DIR_MAX];
    cr_assert_eq(apl_test_temp_dir(root, sizeof(root), &err), APL_OK);

    char nested[APL_TEST_DIR_MAX];
    make_child(nested, sizeof(nested), root, "one");
    cr_assert_eq(mkdir(nested, 0777), 0);
    make_child(nested, sizeof(nested), root, "one/two");
    cr_assert_eq(mkdir(nested, 0777), 0);

    char file[APL_TEST_PATH_MAX];
    make_child(file, sizeof(file), root, "top-level.txt");
    write_file(file, "top");
    make_child(file, sizeof(file), nested, "nested.txt");
    write_file(file, "nested");

    char deeper[APL_TEST_DIR_MAX];
    make_child(deeper, sizeof(deeper), root, "one/two/three");
    cr_assert_eq(mkdir(deeper, 0777), 0);
    make_child(file, sizeof(file), deeper, "deep.txt");
    write_file(file, "deep");

    cr_assert_eq(apl_test_remove_temp_dir(root, &err), APL_OK);
    cr_assert_eq(access(root, F_OK), -1);
}

Test(test_support, cleanup_unlinks_symlinks_without_touching_their_targets)
{
    apl_error err;
    apl_error_init(&err);

    char victim[APL_TEST_DIR_MAX];
    cr_assert_eq(apl_test_temp_dir(victim, sizeof(victim), &err), APL_OK);

    char victim_file[APL_TEST_PATH_MAX];
    make_child(victim_file, sizeof(victim_file), victim, "keep-me.txt");
    write_file(victim_file, "must survive");

    char root[APL_TEST_DIR_MAX];
    cr_assert_eq(apl_test_temp_dir(root, sizeof(root), &err), APL_OK);
    char link[APL_TEST_PATH_MAX];
    make_child(link, sizeof(link), root, "escape");
    cr_assert_eq(symlink(victim, link), 0);

    cr_assert_eq(apl_test_remove_temp_dir(root, &err), APL_OK);
    cr_assert_eq(access(root, F_OK), -1);
    cr_assert_eq(access(victim_file, F_OK), 0);

    cr_assert_eq(apl_test_remove_temp_dir(victim, &err), APL_OK);
}

Test(test_support, cleanup_of_missing_directory_is_success)
{
    apl_error err;
    apl_error_init(&err);

    char root[APL_TEST_DIR_MAX];
    cr_assert_eq(apl_test_temp_dir(root, sizeof(root), &err), APL_OK);
    cr_assert_eq(apl_test_remove_temp_dir(root, &err), APL_OK);
    cr_assert_eq(apl_test_remove_temp_dir(root, &err), APL_OK);
    cr_assert(apl_error_is_success(&err));
}
