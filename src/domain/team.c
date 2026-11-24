#include "domain/team.h"

#include <ctype.h>
#include <string.h>

/** True when a byte is trimmed from the outside of user-entered text. */
static bool is_trimmed_whitespace(const unsigned char byte)
{
    return isspace(byte) != 0;
}

/** Locate the trimmed span of `text`; null text is an empty span. */
static void trimmed_span(const char *const text, const char **const out_begin,
                         const char **const out_end)
{
    if (text == nullptr) {
        *out_begin = "";
        *out_end = "";
        return;
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
    *out_end = end;
}

/** Copy a trimmed span into `out`, rejecting values that do not fit. */
static apl_status copy_trimmed(const char *const begin, const char *const end, char *const out,
                               const size_t out_size, const char *const label,
                               apl_error *const error)
{
    if (out == nullptr || out_size == 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "%s buffer is not usable", label);
        return APL_ERR_INVALID_ARGUMENT;
    }

    const size_t length = (size_t)(end - begin);
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
    const char *end = nullptr;
    trimmed_span(name, &begin, &end);

    if (begin == end) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "team name is required");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return copy_trimmed(begin, end, out, out_size, "team name", error);
}

apl_status apl_team_notes_normalize(const char *const notes, char *const out, const size_t out_size,
                                    apl_error *const error)
{
    const char *begin = nullptr;
    const char *end = nullptr;
    trimmed_span(notes, &begin, &end);
    return copy_trimmed(begin, end, out, out_size, "team notes", error);
}
