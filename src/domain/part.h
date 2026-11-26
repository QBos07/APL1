#ifndef APL_DOMAIN_PART_H
#define APL_DOMAIN_PART_H

#include <stddef.h>
#include <stdint.h>

#include "core/error.h"
#include "domain/record.h"
#include "domain/reference.h"

/*
 * Team-scoped component types, individual units, and their lifecycle events
 * (Phase 6).
 *
 * A part type is a user-maintained category/name grouping; it is not a
 * catalogue or a stock record. An individual part/unit belongs to one type and
 * is addressable by a stable ID even when it has no serial number. Lifecycle
 * events are append-only dated records that derive current fitment; no editable
 * "current part" field or inferred stock state is stored anywhere.
 */

static constexpr size_t APL_PART_TYPE_CATEGORY_MAX = 128;
static constexpr size_t APL_PART_TYPE_NAME_MAX = 128;
static constexpr size_t APL_PART_TYPE_DESCRIPTION_MAX = 1024;

static constexpr size_t APL_PART_UNIT_SERIAL_MAX = 128;
static constexpr size_t APL_PART_UNIT_NUMBER_MAX = 128;
static constexpr size_t APL_PART_UNIT_CONDITION_MAX = 64;

static constexpr size_t APL_LIFECYCLE_POSITION_MAX = 128;
static constexpr size_t APL_LIFECYCLE_EVENT_MAX = 128;
static constexpr size_t APL_LIFECYCLE_REASON_MAX = 1024;
static constexpr size_t APL_LIFECYCLE_USAGE_MAX = 128;

/** One persisted, team-owned component type (work item 6.1).
 *
 * Category and name are required, user-entered text. Description and notes are
 * optional free form. The display values preserve the user's case; the `*_key`
 * fields are trimmed, lower-cased comparison keys used for ordering/filtering.
 */
typedef struct apl_part_type {
    int64_t id;
    int64_t team_id;
    char category[APL_PART_TYPE_CATEGORY_MAX];
    char category_key[APL_PART_TYPE_CATEGORY_MAX];
    char name[APL_PART_TYPE_NAME_MAX];
    char name_key[APL_PART_TYPE_NAME_MAX];
    char description[APL_PART_TYPE_DESCRIPTION_MAX];
    char notes[APL_REFERENCE_NOTES_MAX];
    apl_record_meta meta;
} apl_part_type;

/** Read filters for part-type lists. Empty text/zero fields are unfiltered. */
typedef struct apl_part_type_filter {
    int64_t team_id;
    bool include_archived;
    char category_key[APL_PART_TYPE_CATEGORY_MAX];
} apl_part_type_filter;

/** Normalize a required, trimmed part-type category and its comparison key. */
apl_status apl_part_type_category_normalize(const char *text, char *out_display,
                                            size_t display_size, char *out_key, size_t key_size,
                                            apl_error *error);

/** Normalize a required, trimmed part-type name and its comparison key. */
apl_status apl_part_type_name_normalize(const char *text, char *out_display, size_t display_size,
                                        char *out_key, size_t key_size, apl_error *error);

/** Normalize optional free-form description text. */
apl_status apl_part_type_description_normalize(const char *text, char *out, size_t out_size,
                                               apl_error *error);

/** Normalize optional free-form notes text. */
apl_status apl_part_type_notes_normalize(const char *text, char *out, size_t out_size,
                                         apl_error *error);

/** One persisted, team-owned individual part/unit (work item 6.2).
 *
 * A unit belongs to one part type and is addressable by a stable ID even when
 * it has no serial or part number. Serial/part numbers and the condition label
 * are genuinely optional user-entered text. No stock or availability state is
 * inferred.
 */
typedef struct apl_part_unit {
    int64_t id;
    int64_t team_id;
    int64_t type_id;
    char serial_number[APL_PART_UNIT_SERIAL_MAX];
    char part_number[APL_PART_UNIT_NUMBER_MAX];
    char condition[APL_PART_UNIT_CONDITION_MAX];
    char condition_key[APL_PART_UNIT_CONDITION_MAX];
    char notes[APL_REFERENCE_NOTES_MAX];
    apl_record_meta meta;
} apl_part_unit;

/** Read filters for part-unit lists. Empty text/zero fields are unfiltered. */
typedef struct apl_part_unit_filter {
    int64_t team_id;
    bool include_archived;
    int64_t type_id;
} apl_part_unit_filter;

/** Normalize an optional serial number (empty string stays empty). */
apl_status apl_part_unit_serial_normalize(const char *text, char *out, size_t out_size,
                                          apl_error *error);

/** Normalize an optional part number (empty string stays empty). */
apl_status apl_part_unit_number_normalize(const char *text, char *out, size_t out_size,
                                          apl_error *error);

/** Normalize an optional condition/status label and its comparison key. */
apl_status apl_part_unit_condition_normalize(const char *text, const char *label,
                                             char *out_display, size_t display_size,
                                             char *out_key, size_t key_size, apl_error *error);

/** Normalize optional free-form notes text for a unit. */
apl_status apl_part_unit_notes_normalize(const char *text, char *out, size_t out_size,
                                         apl_error *error);

#endif /* APL_DOMAIN_PART_H */