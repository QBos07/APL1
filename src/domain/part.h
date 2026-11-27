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
apl_status apl_part_unit_condition_normalize(const char *text, const char *label, char *out_display,
                                             size_t display_size, char *out_key, size_t key_size,
                                             apl_error *error);

/** Normalize optional free-form notes text for a unit. */
apl_status apl_part_unit_notes_normalize(const char *text, char *out, size_t out_size,
                                         apl_error *error);

/** User-entered lifecycle action identifiers (work item 6.3). */
typedef enum apl_lifecycle_action {
    APL_LIFECYCLE_NONE = 0,
    APL_LIFECYCLE_INSTALL,
    APL_LIFECYCLE_REMOVE,
    APL_LIFECYCLE_REPLACE,
    APL_LIFECYCLE_REPAIR,
    APL_LIFECYCLE_DISPOSE,
} apl_lifecycle_action;

/** Stable storage code ("install", "remove", …) or "" for invalid. */
const char *apl_lifecycle_action_code(apl_lifecycle_action action);

/** Display label ("Install", "Remove", …) or "Unknown" for invalid. */
const char *apl_lifecycle_action_label(apl_lifecycle_action action);

/** Resolve a stable storage code; unknown codes return APL_LIFECYCLE_NONE. */
apl_lifecycle_action apl_lifecycle_action_from_code(const char *code);

/** One append-only dated lifecycle event (work item 6.3).
 *
 * An install/remove/repair/dispose event identifies the affected `unit_id`. A
 * replace event references both `old_unit_id` and `new_unit_id` in one
 * operation and appears in both unit histories. `event_date` is entered by the
 * user and is separate from `created_at`. Position/event/reason/usage/notes and
 * the car link are optional.
 */
typedef struct apl_lifecycle_event {
    int64_t id;
    int64_t team_id;
    apl_lifecycle_action action;
    int64_t unit_id;        /* affected unit for install/remove/repair/dispose */
    int64_t old_unit_id;    /* old unit for a replace */
    int64_t new_unit_id;    /* new unit for a replace */
    int64_t car_id;         /* optional car/position link */
    int64_t transaction_id; /* optional replacement-to-ledger link (6.7) */
    char event_date[APL_REFERENCE_DATE_MAX];
    char position[APL_LIFECYCLE_POSITION_MAX];
    char event_label[APL_LIFECYCLE_EVENT_MAX];
    char reason[APL_LIFECYCLE_REASON_MAX];
    char usage[APL_LIFECYCLE_USAGE_MAX];
    char notes[APL_REFERENCE_NOTES_MAX];
    char created_at[APL_RECORD_TIMESTAMP_MAX];
} apl_lifecycle_event;

/** Read filters for lifecycle/history lists. Empty text/zero fields unset. */
typedef struct apl_lifecycle_filter {
    int64_t team_id;
    int64_t unit_id;
    int64_t car_id;
    apl_lifecycle_action action;
    char from_date[APL_REFERENCE_DATE_MAX];
    char to_date[APL_REFERENCE_DATE_MAX];
} apl_lifecycle_filter;

/** Normalize an entered required event date (ISO YYYY-MM-DD). */
apl_status apl_lifecycle_date_normalize(const char *text, char *out, size_t out_size,
                                        apl_error *error);

/** Normalize an optional position label. */
apl_status apl_lifecycle_position_normalize(const char *text, char *out, size_t out_size,
                                            apl_error *error);

/** Normalize an optional event label. */
apl_status apl_lifecycle_event_label_normalize(const char *text, char *out, size_t out_size,
                                               apl_error *error);

/** Normalize an optional reason text. */
apl_status apl_lifecycle_reason_normalize(const char *text, char *out, size_t out_size,
                                          apl_error *error);

/** Normalize an optional usage text. */
apl_status apl_lifecycle_usage_normalize(const char *text, char *out, size_t out_size,
                                         apl_error *error);

/** Normalize optional free-form notes text. */
apl_status apl_lifecycle_notes_normalize(const char *text, char *out, size_t out_size,
                                         apl_error *error);

/** One derived fitment slot (work item 6.4/6.8).
 *
 * Computed only from recorded lifecycle events. A positive `unit_id` means a
 * unit is currently fitted at `car_id`/`position`; zero means history explicitly
 * records that the slot is empty. A missing row means fitment is unknown.
 * `type_id` is included for display convenience and is zero for an empty slot.
 */
typedef struct apl_fitment_row {
    int64_t unit_id;
    int64_t type_id;
    int64_t car_id;
    char position[APL_LIFECYCLE_POSITION_MAX];
} apl_fitment_row;

/** One lifecycle event decorated with its derived current state (6.8). */
typedef struct apl_lifecycle_history_entry {
    apl_lifecycle_event event;
    bool current;
} apl_lifecycle_history_entry;

#endif /* APL_DOMAIN_PART_H */
