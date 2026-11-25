#include "domain/team.h"

#include <ctype.h>
#include <string.h>

/** True when a byte is trimmed from the outside of user-entered text. */
static bool is_trimmed_whitespace(const unsigned char byte)
{
    return isspace(byte) != 0;
}

/** Locate the trimmed span of `text`; null text is an empty span. Returns the
 *  number of bytes in the span and writes its start to `out_begin`. */
static size_t trimmed_span(const char *const text, const char **const out_begin)
{
    if (text == nullptr) {
        *out_begin = "";
        return 0;
    }

    const char *begin = text;
    while (*begin != '\0' && is_trimmed_whitespace((unsigned char)*begin)) {
        ++begin;
    }
    const char *end = begin + strlen(begin);
    while (end > begin && is_trimmed_whitespace((unsigned char)end[-1])) {
        --end;
    }
    *out_begin = begin;
    return (size_t)(end - begin);
}

/** Copy a trimmed span into `out`, rejecting values that do not fit. */
static apl_status copy_trimmed(const char *const begin, const size_t length, char *const out,
                               const size_t out_size, const char *const label,
                               apl_error *const error)
{
    if (out == nullptr || out_size == 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "%s buffer is not usable", label);
        return APL_ERR_INVALID_ARGUMENT;
    }

    if (length >= out_size) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                      "%s is too long (maximum %zu bytes); shorten it and try again", label,
                      out_size - 1);
        return APL_ERR_INVALID_ARGUMENT;
    }

    memcpy(out, begin, length);
    out[length] = '\0';
    return APL_OK;
}

apl_status apl_team_name_normalize(const char *const name, char *const out, const size_t out_size,
                                   apl_error *const error)
{
    const char *begin = nullptr;
    const size_t length = trimmed_span(name, &begin);

    if (length == 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "team name is required");
        apl_error_set_field(error, APL_FIELD_NAME);
        return APL_ERR_INVALID_ARGUMENT;
    }
    const apl_status status = copy_trimmed(begin, length, out, out_size, "team name", error);
    if (status != APL_OK) {
        apl_error_set_field(error, APL_FIELD_NAME);
    }
    return status;
}

apl_status apl_team_notes_normalize(const char *const notes, char *const out, const size_t out_size,
                                    apl_error *const error)
{
    const char *begin = nullptr;
    const size_t length = trimmed_span(notes, &begin);
    const apl_status status = copy_trimmed(begin, length, out, out_size, "team notes", error);
    if (status != APL_OK) {
        apl_error_set_field(error, APL_FIELD_NOTES);
    }
    return status;
}
