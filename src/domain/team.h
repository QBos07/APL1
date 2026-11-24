#ifndef APL_DOMAIN_TEAM_H
#define APL_DOMAIN_TEAM_H

#include <stddef.h>
#include <stdint.h>

#include "core/error.h"

/*
 * Team identity and validation (work items 2.5/2.7).
 *
 * These rules are deliberately independent of SQLite and Notcurses. The
 * repository stores an already-normalized name/notes; the application use case
 * applies these helpers before touching the database. Duplicate detection is a
 * persistence concern and is reported as APL_ERR_CONFLICT by the repository.
 */

/** Maximum team-name buffer size, including the terminating NUL. */
constexpr size_t APL_TEAM_NAME_MAX = 128;

/** Maximum optional-notes buffer size, including the terminating NUL. */
constexpr size_t APL_TEAM_NOTES_MAX = 1024;

/** Maximum stored timestamp length, including the terminating NUL. */
constexpr size_t APL_TEAM_TIMESTAMP_MAX = 32;

/** A persisted team row. Text fields are NUL-terminated and owned by the
 *  value; optional text is an empty string, never a null pointer. */
typedef struct apl_team {
    int64_t id;
    char name[APL_TEAM_NAME_MAX];
    char notes[APL_TEAM_NOTES_MAX];
    bool archived;
    char created_at[APL_TEAM_TIMESTAMP_MAX];
    char updated_at[APL_TEAM_TIMESTAMP_MAX];
    /** Empty when the team is not archived. */
    char archived_at[APL_TEAM_TIMESTAMP_MAX];
} apl_team;

/** Trim surrounding whitespace from a user-entered team name into `out`.
 *
 * A null, empty, or whitespace-only name is rejected with
 * APL_ERR_INVALID_ARGUMENT. Values that do not fit `out` are rejected rather
 * than silently truncated. On success `out` holds the trimmed name. */
apl_status apl_team_name_normalize(const char *name, char *out, size_t out_size, apl_error *error);

/** Trim surrounding whitespace from optional notes into `out`.
 *
 * Null is treated as empty notes. Values that do not fit `out` are rejected
 * rather than silently truncated. */
apl_status apl_team_notes_normalize(const char *notes, char *out, size_t out_size,
                                    apl_error *error);

#endif /* APL_DOMAIN_TEAM_H */
