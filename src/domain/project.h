#ifndef APL_DOMAIN_PROJECT_H
#define APL_DOMAIN_PROJECT_H

#include <stddef.h>
#include <stdint.h>

#include "core/error.h"
#include "domain/record.h"
#include "domain/reference.h"

/*
 * Team-scoped development projects (Phase 5).
 *
 * A project is a user-maintained record, not a simulated workflow. Title,
 * category, description, status, dates, notes, and optional season/car links
 * are all entered by the user; no status vocabulary, progress percentage, or
 * completion date is generated. Optional dates use the same empty-string
 * convention as other reference records and are stored as NULL.
 */

static constexpr size_t APL_PROJECT_TITLE_MAX = 128;
static constexpr size_t APL_PROJECT_CATEGORY_MAX = 128;
static constexpr size_t APL_PROJECT_STATUS_MAX = 64;
static constexpr size_t APL_PROJECT_DESCRIPTION_MAX = 1024;
static constexpr size_t APL_PROJECT_UPDATE_SUMMARY_MAX = 512;

/** One persisted development project. */
typedef struct apl_project {
    int64_t id;
    int64_t team_id;
    int64_t season_id; /* 0 means no link */
    int64_t car_id;    /* 0 means no link */
    char title[APL_PROJECT_TITLE_MAX];
    char category[APL_PROJECT_CATEGORY_MAX];
    char category_key[APL_PROJECT_CATEGORY_MAX];
    char description[APL_PROJECT_DESCRIPTION_MAX];
    char status[APL_PROJECT_STATUS_MAX];
    char status_key[APL_PROJECT_STATUS_MAX];
    char target_date[APL_REFERENCE_DATE_MAX];
    char actual_date[APL_REFERENCE_DATE_MAX];
    char notes[APL_REFERENCE_NOTES_MAX];
    apl_record_meta meta;
} apl_project;

/** One append-only dated work update on a project.
 *
 * `update_date` is entered by the user and may differ from `created_at`, the
 * system timestamp. `status_change`/`status_change_key` are empty when the
 * update does not change the project's current status. There is intentionally
 * no update/delete operation for a stored update; corrections are new dated
 * entries.
 */
typedef struct apl_project_update {
    int64_t id;
    int64_t project_id;
    char update_date[APL_REFERENCE_DATE_MAX];
    char summary[APL_PROJECT_UPDATE_SUMMARY_MAX];
    char status_change[APL_PROJECT_STATUS_MAX];
    char status_change_key[APL_PROJECT_STATUS_MAX];
    char notes[APL_REFERENCE_NOTES_MAX];
    char created_at[APL_RECORD_TIMESTAMP_MAX];
} apl_project_update;

/** Read filters for project lists. Empty text/zero fields are unfiltered. */
typedef struct apl_project_filter {
    int64_t team_id;
    bool include_archived;
    char status_key[APL_PROJECT_STATUS_MAX];
    int64_t season_id;
    char category_key[APL_PROJECT_CATEGORY_MAX];
} apl_project_filter;

/** Normalize a required, trimmed project title. */
apl_status apl_project_title_normalize(const char *text, char *out, size_t out_size,
                                       apl_error *error);

/** Normalize an optional category/status label and its lower-case comparison key.
 *
 * The display value preserves the user's case; the key is trimmed, collapses
 * internal whitespace, and is lower-cased. Empty input produces two empty
 * strings. Both output buffers must be at least the corresponding project
 * label size.
 */
apl_status apl_project_label_normalize(const char *text, const char *label, char *out_display,
                                       size_t display_size, char *out_key, size_t key_size,
                                       apl_error *error);

/** Normalize optional free-form description text. */
apl_status apl_project_description_normalize(const char *text, char *out, size_t out_size,
                                             apl_error *error);

/** Normalize optional free-form notes text. */
apl_status apl_project_notes_normalize(const char *text, char *out, size_t out_size,
                                       apl_error *error);

#endif /* APL_DOMAIN_PROJECT_H */
