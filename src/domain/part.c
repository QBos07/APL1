#include "domain/part.h"

#include <ctype.h>
#include <string.h>

/* Phase 6 domain rules for part types (work item 6.1). These advance the
 * user-entered label conventions already used by projects: required labels are
 * produced alongside a trimmed, lower-cased comparison key, and optional free
 * form keeps the same empty-string convention as other reference records. */

static apl_status label_normalize(const char *const text, const char *const label,
                                  char *const out_display, const size_t display_size,
                                  char *const out_key, const size_t key_size,
                                  apl_error *const error)
{
    if (out_display == nullptr || display_size == 0 || out_key == nullptr || key_size == 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "%s buffer is not usable", label);
        return APL_ERR_INVALID_ARGUMENT;
    }
    out_display[0] = '\0';
    out_key[0] = '\0';

    const char *cursor = (text != nullptr) ? text : "";
    while (*cursor != '\0' && isspace((unsigned char)*cursor)) {
        ++cursor;
    }
    if (*cursor == '\0') {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "%s is required", label);
        return APL_ERR_INVALID_ARGUMENT;
    }

    size_t display_length = 0;
    size_t key_length = 0;
    bool pending_space = false;
    while (*cursor != '\0') {
        if (isspace((unsigned char)*cursor)) {
            pending_space = display_length > 0;
            ++cursor;
            continue;
        }
        if (pending_space) {
            if (display_length + 1 >= display_size || key_length + 1 >= key_size) {
                break;
            }
            out_display[display_length++] = ' ';
            out_key[key_length++] = ' ';
            pending_space = false;
        }
        if (display_length + 1 >= display_size || key_length + 1 >= key_size) {
            break;
        }
        out_display[display_length++] = *cursor;
        out_key[key_length++] = (char)tolower((unsigned char)*cursor);
        ++cursor;
    }

    if (*cursor != '\0') {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "%s is too long (maximum %zu bytes)", label,
                      display_size - 1);
        out_display[0] = '\0';
        out_key[0] = '\0';
        return APL_ERR_INVALID_ARGUMENT;
    }

    out_display[display_length] = '\0';
    out_key[key_length] = '\0';
    return APL_OK;
}

apl_status apl_part_type_category_normalize(const char *const text, char *const out_display,
                                            const size_t display_size, char *const out_key,
                                            const size_t key_size, apl_error *const error)
{
    const apl_status status = label_normalize(text, "part category", out_display, display_size,
                                              out_key, key_size, error);
    if (status != APL_OK) {
        apl_error_set_field(error, APL_FIELD_CATEGORY);
    }
    return status;
}

apl_status apl_part_type_name_normalize(const char *const text, char *const out_display,
                                        const size_t display_size, char *const out_key,
                                        const size_t key_size, apl_error *const error)
{
    const apl_status status = label_normalize(text, "part type name", out_display, display_size,
                                              out_key, key_size, error);
    if (status != APL_OK) {
        apl_error_set_field(error, APL_FIELD_NAME);
    }
    return status;
}

apl_status apl_part_type_description_normalize(const char *const text, char *const out,
                                               const size_t out_size, apl_error *const error)
{
    const apl_status status =
        apl_reference_optional_normalize(text, "part type description", out, out_size, error);
    if (status != APL_OK) {
        apl_error_set_field(error, APL_FIELD_DESCRIPTION);
    }
    return status;
}

apl_status apl_part_type_notes_normalize(const char *const text, char *const out,
                                         const size_t out_size, apl_error *const error)
{
    const apl_status status =
        apl_reference_optional_normalize(text, "part type notes", out, out_size, error);
    if (status != APL_OK) {
        apl_error_set_field(error, APL_FIELD_NOTES);
    }
    return status;
}

apl_status apl_part_unit_serial_normalize(const char *const text, char *const out,
                                          const size_t out_size, apl_error *const error)
{
    const apl_status status =
        apl_reference_optional_normalize(text, "serial number", out, out_size, error);
    if (status != APL_OK) {
        apl_error_set_field(error, APL_FIELD_SERIAL_NUMBER);
    }
    return status;
}

apl_status apl_part_unit_number_normalize(const char *const text, char *const out,
                                          const size_t out_size, apl_error *const error)
{
    const apl_status status =
        apl_reference_optional_normalize(text, "part number", out, out_size, error);
    if (status != APL_OK) {
        apl_error_set_field(error, APL_FIELD_PART_NUMBER);
    }
    return status;
}

apl_status apl_part_unit_condition_normalize(const char *const text, const char *const label,
                                             char *const out_display, const size_t display_size,
                                             char *const out_key, const size_t key_size,
                                             apl_error *const error)
{
    if (out_display == nullptr || display_size == 0 || out_key == nullptr || key_size == 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "%s buffer is not usable", label);
        return APL_ERR_INVALID_ARGUMENT;
    }
    out_display[0] = '\0';
    out_key[0] = '\0';
    /* An empty condition label is allowed and stays empty. */
    if (text == nullptr || text[0] == '\0') {
        return APL_OK;
    }

    const char *cursor = text;
    while (*cursor != '\0' && isspace((unsigned char)*cursor)) {
        ++cursor;
    }
    size_t display_length = 0;
    size_t key_length = 0;
    bool pending_space = false;
    while (*cursor != '\0') {
        if (isspace((unsigned char)*cursor)) {
            pending_space = display_length > 0;
            ++cursor;
            continue;
        }
        if (pending_space) {
            if (display_length + 1 >= display_size || key_length + 1 >= key_size) {
                break;
            }
            out_display[display_length++] = ' ';
            out_key[key_length++] = ' ';
            pending_space = false;
        }
        if (display_length + 1 >= display_size || key_length + 1 >= key_size) {
            break;
        }
        out_display[display_length++] = *cursor;
        out_key[key_length++] = (char)tolower((unsigned char)*cursor);
        ++cursor;
    }
    if (*cursor != '\0') {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "%s is too long (maximum %zu bytes)", label,
                      display_size - 1);
        out_display[0] = '\0';
        out_key[0] = '\0';
        return APL_ERR_INVALID_ARGUMENT;
    }
    out_display[display_length] = '\0';
    out_key[key_length] = '\0';
    return APL_OK;
}

apl_status apl_part_unit_notes_normalize(const char *const text, char *const out,
                                         const size_t out_size, apl_error *const error)
{
    const apl_status status =
        apl_reference_optional_normalize(text, "part unit notes", out, out_size, error);
    if (status != APL_OK) {
        apl_error_set_field(error, APL_FIELD_NOTES);
    }
    return status;
}