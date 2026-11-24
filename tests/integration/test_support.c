#define _POSIX_C_SOURCE 200809L

#include "test_support.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TEMPLATE "/tmp/apl1-test-XXXXXX"

apl_status apl_test_temp_dir(char *const buf, const size_t buf_size, apl_error *const error)
{
    if (buf == nullptr || buf_size <= sizeof(TEMPLATE)) {
        return APL_ERR_INVALID_ARGUMENT;
    }
    memcpy(buf, TEMPLATE, sizeof(TEMPLATE));
    if (mkdtemp(buf) == nullptr) {
        apl_error_set(error, APL_ERR_IO, "cannot create temporary directory");
        return APL_ERR_IO;
    }
    return APL_OK;
}

apl_status apl_test_remove_temp_dir(const char *const dir, apl_error *const error)
{
    if (dir == nullptr) {
        return APL_ERR_INVALID_ARGUMENT;
    }

    /* Delete every regular file the test left in the directory, then remove
     * the directory itself. Tests only create flat directories of files. */
    DIR *const listing = opendir(dir);
    if (listing == nullptr) {
        apl_error_set(error, APL_ERR_IO, "cannot list temporary directory '%s'", dir);
        return APL_ERR_IO;
    }

    struct dirent *entry;
    while ((entry = readdir(listing)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char file[512];
        const int written = snprintf(file, sizeof(file), "%s/%s", dir, entry->d_name);
        if (written > 0 && (size_t)written < sizeof(file)) {
            (void)unlink(file);
        }
    }
    closedir(listing);

    if (rmdir(dir) != 0) {
        apl_error_set(error, APL_ERR_IO, "temporary directory '%s' could not be removed", dir);
        return APL_ERR_IO;
    }
    return APL_OK;
}
