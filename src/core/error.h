#ifndef APL_CORE_ERROR_H
#define APL_CORE_ERROR_H

#include <stdarg.h>
#include <stddef.h>

/*
 * Application error reporting for apl1.
 *
 * Conventions (see CONTRIBUTING.md):
 * - Fallible functions return an apl_status and may accept a caller-supplied
 *   apl_error* (optional: nullptr is allowed) that is only written on failure.
 * - No hidden global "last error" state: every error is a value the caller owns.
 * - apl_error owns its message as a fixed-size buffer, so no dynamic allocation
 *   is involved and callers need no destructor.
 */

/** Stable application-wide status codes. Values are part of the public
 *  contract: append new codes at the end and never renumber. */
typedef enum apl_status {
    APL_OK = 0,                   /**< Success. */
    APL_ERR_UNKNOWN = 1,          /**< Failure without a more specific cause. */
    APL_ERR_INVALID_ARGUMENT = 2, /**< Caller passed an invalid value. */
    APL_ERR_NOT_FOUND = 3,        /**< A requested record/path does not exist. */
    APL_ERR_IO = 4,               /**< Filesystem/path failure (permissions, missing directory). */
    APL_ERR_OUT_OF_MEMORY = 5,    /**< Allocation failed. */
    APL_ERR_DB_OPEN = 6,          /**< SQLite connection could not be opened/initialized. */
    APL_ERR_DB_QUERY = 7,         /**< SQLite statement execution failed. */
    APL_ERR_DB_FOREIGN_KEYS = 8,  /**< Foreign-key enforcement could not be enabled/verified. */
    APL_ERR_DB_MIGRATION = 9,     /**< A schema migration failed or the list is invalid. */
    APL_ERR_DB_SCHEMA_TOO_NEW = 10, /**< Database was written by a newer application. */
    APL_ERR_TUI_INIT = 11,          /**< Notcurses initialization failed. */
    APL_ERR_TUI_RENDER = 12,        /**< A screen frame could not be drawn. */
    APL_ERR_TUI_INPUT = 13,         /**< Terminal input became unavailable. */
    APL_ERR_CONFLICT = 14,          /**< A user-entered value conflicts with existing data. */
} apl_status;

/** Maximum length of an apl_error message, including the terminating NUL. */
constexpr size_t APL_ERROR_MESSAGE_MAX = 256;

/** Optional form-field identity for validation failures.
 *
 * Values are part of the public contract: append new fields at the end and
 * never renumber. APL_FIELD_NONE means the error is not tied to one field. */
typedef enum apl_field {
    APL_FIELD_NONE = 0,
    APL_FIELD_NAME,
    APL_FIELD_SHORT_NAME,
    APL_FIELD_LABEL,
    APL_FIELD_CHASSIS,
    APL_FIELD_NOTES,
    APL_FIELD_ROLE,
    APL_FIELD_START_DATE,
    APL_FIELD_END_DATE,
} apl_field;

/** A caller-owned error value. On success paths code is APL_OK and message is
 *  empty; on failure code is one of the APL_ERR_* values and message is a
 *  concise, NUL-terminated human-readable description (truncated to fit). */
typedef struct apl_error {
    apl_status code;
    apl_field field;
    char message[APL_ERROR_MESSAGE_MAX];
} apl_error;

/** Initialize an error value to success (APL_OK, empty message). */
void apl_error_init(apl_error *error);

/** Reset an error value to the success state. Safe on initialized or cleared
 *  values; no memory is owned beyond the embedded buffer. */
void apl_error_clear(apl_error *error);

/** Fill an error value with a failure code and a printf-formatted message
 *  (truncated to APL_ERROR_MESSAGE_MAX). Ignored if error is nullptr. */
[[gnu::format(printf, 3, 4)]]
void apl_error_set(apl_error *error, apl_status code, const char *fmt, ...);

/** Fill an error value using an argument list (see apl_error_set). */
void apl_error_setv(apl_error *error, apl_status code, const char *fmt, va_list args);

/** Annotate an existing error with the form field that caused it.
 *  A null error pointer is a no-op. */
void apl_error_set_field(apl_error *error, apl_field field);

/** Return the field annotation, or APL_FIELD_NONE when absent. */
apl_field apl_error_field_id(const apl_error *error);

/** Add context in front of an existing failure message without losing the
 *  original detail.
 *
 * The current message is copied before the error value is rewritten, so the
 * source and destination buffers never overlap. On a truncated result the
 * combined message is cut safely according to APL_ERROR_MESSAGE_MAX. A null
 * error pointer is a no-op; a null or empty context keeps the existing message
 * (only the status code is updated). */
void apl_error_prefix(apl_error *error, apl_status code, const char *context);

/** Return the stored status code (APL_OK on a cleared/initialized value). */
apl_status apl_error_code(const apl_error *error);

/** Return the stored message; empty string when the error reports success. */
const char *apl_error_message(const apl_error *error);

/** True when the error value reports success. */
bool apl_error_is_success(const apl_error *error);

/** Stable short name of a status code, for logs and tests. Unknown values
 *  return "APL_STATUS_INVALID" rather than dereferencing out-of-range data. */
const char *apl_status_string(apl_status status);

#endif /* APL_CORE_ERROR_H */
