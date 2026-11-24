#define _POSIX_C_SOURCE 200809L

#include "test_support.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *const APL_TEST_FALLBACK_TMPDIR = "/tmp";
static const char *const APL_TEST_DIR_TEMPLATE = "/apl1-test-XXXXXX";

/** True when `path` is absolute. */
static bool is_absolute_path(const char *const path)
{
    return path != nullptr && path[0] == '/';
}

/** True when `path` names an existing directory the process can write to. */
static bool is_writable_directory(const char *const path)
{
    struct stat info;
    if (stat(path, &info) != 0 || !S_ISDIR(info.st_mode)) {
        return false;
    }
    return access(path, W_OK | X_OK) == 0;
}

/** True when a unique suffix still fits in the documented directory buffer. */
static bool base_fits_template(const char *const base)
{
    return strlen(base) + strlen(APL_TEST_DIR_TEMPLATE) < APL_TEST_DIR_MAX;
}

/** Copy `source` into `out`, reporting an error instead of truncating. */
static apl_status copy_path(char *const out, const size_t out_size, const char *const source,
                            apl_error *const error)
{
    if (out == nullptr || out_size < 2 || source == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "temporary path buffer is not usable");
        return APL_ERR_INVALID_ARGUMENT;
    }
    const int written = snprintf(out, out_size, "%s", source);
    if (written < 0 || (size_t)written >= out_size) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "temporary path does not fit in %zu bytes",
                      out_size);
        return APL_ERR_INVALID_ARGUMENT;
    }
    return APL_OK;
}

apl_status apl_test_temp_base_ex(const char *const tmpdir, char *const out, const size_t out_size,
                                 apl_error *const error)
{
    if (tmpdir != nullptr && is_absolute_path(tmpdir) && is_writable_directory(tmpdir) &&
        base_fits_template(tmpdir)) {
        return copy_path(out, out_size, tmpdir, error);
    }

    /* Unset, empty, relative, missing, unwritable, or over-long TMPDIR: use
     * the documented fallback. A /tmp that is itself unusable is a hard,
     * deterministic error. */
    if (!is_writable_directory(APL_TEST_FALLBACK_TMPDIR)) {
        apl_error_set(error, APL_ERR_IO,
                      "no usable temporary directory: TMPDIR is not usable and '%s' is not a "
                      "writable directory",
                      APL_TEST_FALLBACK_TMPDIR);
        return APL_ERR_IO;
    }
    return copy_path(out, out_size, APL_TEST_FALLBACK_TMPDIR, error);
}

apl_status apl_test_temp_dir_in(const char *const base, char *const buf, const size_t buf_size,
                                apl_error *const error)
{
    if (base == nullptr || !is_absolute_path(base)) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                      "temporary-directory base must be an absolute path");
        return APL_ERR_INVALID_ARGUMENT;
    }
    if (!base_fits_template(base)) {
        const size_t max_base = APL_TEST_DIR_MAX - strlen(APL_TEST_DIR_TEMPLATE) - 1;
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                      "temporary-directory base is too long (maximum %zu bytes with the "
                      "per-test suffix)",
                      max_base);
        return APL_ERR_INVALID_ARGUMENT;
    }

    char template_path[APL_TEST_PATH_MAX];
    const int written =
        snprintf(template_path, sizeof(template_path), "%s%s", base, APL_TEST_DIR_TEMPLATE);
    if (written < 0 || (size_t)written >= sizeof(template_path)) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                      "temporary-directory template does not fit in %zu bytes",
                      sizeof(template_path));
        return APL_ERR_INVALID_ARGUMENT;
    }
    if (buf == nullptr || buf_size <= strlen(template_path)) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                      "temporary-directory buffer is too small for '%s'", template_path);
        return APL_ERR_INVALID_ARGUMENT;
    }

    if (mkdtemp(template_path) == nullptr) {
        apl_error_set(error, APL_ERR_IO, "cannot create temporary directory beneath '%s': %s", base,
                      strerror(errno));
        return APL_ERR_IO;
    }

    return copy_path(buf, buf_size, template_path, error);
}

apl_status apl_test_temp_dir(char *const buf, const size_t buf_size, apl_error *const error)
{
    char base[APL_TEST_PATH_MAX];
    apl_status status = apl_test_temp_base_ex(getenv("TMPDIR"), base, sizeof(base), error);
    if (status != APL_OK) {
        return status;
    }
    return apl_test_temp_dir_in(base, buf, buf_size, error);
}

/** Append `name` to `directory`; reports an error instead of truncating. */
static apl_status join_path(char *const out, const size_t out_size, const char *const directory,
                            const char *const name, apl_error *const error)
{
    const int written = snprintf(out, out_size, "%s/%s", directory, name);
    if (written < 0 || (size_t)written >= out_size) {
        apl_error_set(error, APL_ERR_IO, "path '%s/%s' is too long to clean up", directory, name);
        return APL_ERR_IO;
    }
    return APL_OK;
}

/** Remove one directory tree. `lstat` is used so a symlink is unlinked rather
 *  than followed, which keeps cleanup inside the caller's unique directory. */
static apl_status remove_tree(const char *const path, apl_error *const error)
{
    struct stat info;
    if (lstat(path, &info) != 0) {
        if (errno == ENOENT) {
            return APL_OK;
        }
        apl_error_set(error, APL_ERR_IO, "cannot inspect '%s': %s", path, strerror(errno));
        return APL_ERR_IO;
    }

    if (!S_ISDIR(info.st_mode)) {
        if (unlink(path) != 0) {
            apl_error_set(error, APL_ERR_IO, "cannot remove '%s': %s", path, strerror(errno));
            return APL_ERR_IO;
        }
        return APL_OK;
    }

    DIR *const listing = opendir(path);
    if (listing == nullptr) {
        apl_error_set(error, APL_ERR_IO, "cannot list '%s': %s", path, strerror(errno));
        return APL_ERR_IO;
    }

    apl_status status = APL_OK;
    struct dirent *entry;
    while (status == APL_OK && (entry = readdir(listing)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char child[APL_TEST_PATH_MAX];
        status = join_path(child, sizeof(child), path, entry->d_name, error);
        if (status == APL_OK) {
            status = remove_tree(child, error);
        }
    }
    closedir(listing);
    if (status != APL_OK) {
        return status;
    }

    if (rmdir(path) != 0) {
        apl_error_set(error, APL_ERR_IO, "cannot remove directory '%s': %s", path, strerror(errno));
        return APL_ERR_IO;
    }
    return APL_OK;
}

apl_status apl_test_remove_temp_dir(const char *const dir, apl_error *const error)
{
    if (dir == nullptr || dir[0] == '\0') {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "temporary directory path is empty");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return remove_tree(dir, error);
}
