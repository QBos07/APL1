#include "domain/reference.h"

#include <ctype.h>
#include <string.h>
#include <time.h>

/** True for the whitespace trimmed from user-entered reference text. */
static bool is_trimmed_whitespace(const unsigned char byte)
{
    return isspace(byte) != 0;
}

/** Locate the trimmed span of `text`; null text is an empty span. */
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

/** Copy a trimmed span, rejecting values that do not fit. */
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

apl_status apl_reference_label_normalize(const char *const text, const char *const label,
                                         char *const out, const size_t out_size,
                                         apl_error *const error)
{
    const char *begin = nullptr;
    const size_t length = trimmed_span(text, &begin);
    if (length == 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "%s is required", label);
        return APL_ERR_INVALID_ARGUMENT;
    }
    return copy_trimmed(begin, length, out, out_size, label, error);
}

apl_status apl_reference_optional_normalize(const char *const text, const char *const label,
                                            char *const out, const size_t out_size,
                                            apl_error *const error)
{
    const char *begin = nullptr;
    const size_t length = trimmed_span(text, &begin);
    return copy_trimmed(begin, length, out, out_size, label, error);
}

/** Return the number of days in a Gregorian month. */
static int days_in_month(const int year, const int month)
{
    static constexpr int DAYS[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) {
        return 0;
    }
    if (month == 2) {
        const bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        return leap ? 29 : 28;
    }
    return DAYS[month - 1];
}

/** Parse one exactly-ten-character ISO date into year/month/day. */
static bool parse_iso_date(const char *const text, int *const out_year, int *const out_month,
                           int *const out_day)
{
    if (text == nullptr || strlen(text) != 10 || text[4] != '-' || text[7] != '-') {
        return false;
    }
    for (size_t index = 0; index < 10; ++index) {
        if (index == 4 || index == 7) {
            continue;
        }
        if (text[index] < '0' || text[index] > '9') {
            return false;
        }
    }
    const int year =
        (text[0] - '0') * 1000 + (text[1] - '0') * 100 + (text[2] - '0') * 10 + (text[3] - '0');
    const int month = (text[5] - '0') * 10 + (text[6] - '0');
    const int day = (text[8] - '0') * 10 + (text[9] - '0');
    if (year < 1 || month < 1 || month > 12 || day < 1 || day > days_in_month(year, month)) {
        return false;
    }
    *out_year = year;
    *out_month = month;
    *out_day = day;
    return true;
}

apl_status apl_reference_date_normalize(const char *const text, const char *const label,
                                        char *const out, const size_t out_size,
                                        apl_error *const error)
{
    const char *begin = nullptr;
    const size_t length = trimmed_span(text, &begin);
    if (length == 0) {
        return copy_trimmed(begin, 0, out, out_size, label, error);
    }
    if (length != 10) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "%s must use YYYY-MM-DD or be left blank",
                      label);
        return APL_ERR_INVALID_ARGUMENT;
    }
    char date[APL_REFERENCE_DATE_MAX];
    const apl_status status = copy_trimmed(begin, length, date, sizeof(date), label, error);
    if (status != APL_OK) {
        return status;
    }
    int year = 0;
    int month = 0;
    int day = 0;
    if (!parse_iso_date(date, &year, &month, &day)) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                      "%s is not a valid calendar date (use YYYY-MM-DD)", label);
        return APL_ERR_INVALID_ARGUMENT;
    }
    return copy_trimmed(begin, length, out, out_size, label, error);
}

apl_status apl_reference_date_range_validate(const char *const start_date,
                                             const char *const end_date, const char *const label,
                                             apl_error *const error)
{
    const bool has_start = start_date != nullptr && start_date[0] != '\0';
    const bool has_end = end_date != nullptr && end_date[0] != '\0';
    if (has_start && has_end && strcmp(start_date, end_date) > 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                      "%s start date must not be after its end date", label);
        return APL_ERR_INVALID_ARGUMENT;
    }
    return APL_OK;
}

void apl_reference_date_today_minus_days(const int days, char *const out, const size_t out_size)
{
    if (out == nullptr || out_size == 0) {
        return;
    }
    const time_t now = time(nullptr);
    const time_t shifted = now - (time_t)days * 24 * 60 * 60;
    struct tm value = {};
    if (gmtime_r(&shifted, &value) == nullptr || strftime(out, out_size, "%Y-%m-%d", &value) == 0) {
        out[0] = '\0';
    }
}

apl_status apl_membership_start_date_normalize(const char *const text, char *const out,
                                               const size_t out_size, apl_error *const error)
{
    const apl_status status =
        apl_reference_date_normalize(text, "membership start date", out, out_size, error);
    if (status == APL_OK && out[0] == '\0') {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "membership start date is required");
        apl_error_set_field(error, APL_FIELD_START_DATE);
        return APL_ERR_INVALID_ARGUMENT;
    }
    if (status != APL_OK) {
        apl_error_set_field(error, APL_FIELD_START_DATE);
    }
    return status;
}

apl_status apl_membership_dates_normalize(const char *const start, const char *const end,
                                          char *const out_start, const size_t out_start_size,
                                          char *const out_end, const size_t out_end_size,
                                          apl_error *const error)
{
    apl_status status =
        apl_membership_start_date_normalize(start, out_start, out_start_size, error);
    if (status == APL_OK) {
        status =
            apl_reference_date_normalize(end, "membership end date", out_end, out_end_size, error);
        if (status != APL_OK) {
            apl_error_set_field(error, APL_FIELD_END_DATE);
        }
    }
    if (status == APL_OK) {
        status = apl_reference_date_range_validate(out_start, out_end, "membership", error);
        if (status != APL_OK) {
            apl_error_set_field(error, APL_FIELD_START_DATE);
        }
    }
    return status;
}
