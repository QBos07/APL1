#include "domain/money.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdckdint.h>
#include <stdio.h>
#include <string.h>

#include "domain/reference.h"

static apl_status amount_error(apl_error *const error, const char *const message)
{
    apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "%s", message);
    apl_error_set_field(error, APL_FIELD_AMOUNT);
    return APL_ERR_INVALID_ARGUMENT;
}

apl_status apl_money_currency_normalize(const char *const text, char *const out,
                                        const size_t out_size, apl_error *const error)
{
    if (out == nullptr || out_size < APL_MONEY_CURRENCY_MAX) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "currency buffer is not usable");
        apl_error_set_field(error, APL_FIELD_CURRENCY);
        return APL_ERR_INVALID_ARGUMENT;
    }

    char trimmed[8];
    const apl_status status =
        apl_reference_optional_normalize(text, "currency", trimmed, sizeof(trimmed), error);
    if (status != APL_OK) {
        apl_error_set_field(error, APL_FIELD_CURRENCY);
        return status;
    }
    if (strlen(trimmed) != 3) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                      "currency must be a three-letter code such as EUR");
        apl_error_set_field(error, APL_FIELD_CURRENCY);
        return APL_ERR_INVALID_ARGUMENT;
    }

    for (size_t index = 0; index < 3; ++index) {
        if (!isalpha((unsigned char)trimmed[index])) {
            apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "currency must contain only letters");
            apl_error_set_field(error, APL_FIELD_CURRENCY);
            return APL_ERR_INVALID_ARGUMENT;
        }
        out[index] = (char)toupper((unsigned char)trimmed[index]);
    }
    out[3] = '\0';
    return APL_OK;
}

apl_status apl_money_parse(const char *const text, const char *const currency, apl_money *const out,
                           apl_error *const error)
{
    if (out == nullptr) {
        return amount_error(error, "money output is nullptr");
    }
    *out = (apl_money){};

    char amount[64];
    apl_status status =
        apl_reference_optional_normalize(text, "amount", amount, sizeof(amount), error);
    if (status != APL_OK) {
        apl_error_set_field(error, APL_FIELD_AMOUNT);
        return status;
    }
    if (amount[0] == '\0') {
        return amount_error(error, "amount is required");
    }

    const char *cursor = amount;
    bool negative = false;
    if (*cursor == '+' || *cursor == '-') {
        negative = *cursor == '-';
        ++cursor;
    }
    if (*cursor < '0' || *cursor > '9') {
        return amount_error(error, "amount must start with a digit after an optional sign");
    }

    int64_t whole = 0;
    while (*cursor >= '0' && *cursor <= '9') {
        const int64_t digit = *cursor - '0';
        if (ckd_mul(&whole, whole, 10) || ckd_add(&whole, whole, digit)) {
            return amount_error(error, "amount is too large");
        }
        ++cursor;
    }

    int64_t fraction = 0;
    unsigned fraction_digits = 0;
    if (*cursor == '.') {
        ++cursor;
        if (*cursor < '0' || *cursor > '9') {
            return amount_error(error, "amount must have digits after the decimal point");
        }
        while (*cursor >= '0' && *cursor <= '9') {
            if (fraction_digits == 2) {
                return amount_error(error, "amount supports at most two decimal places");
            }
            fraction = fraction * 10 + (*cursor - '0');
            ++fraction_digits;
            ++cursor;
        }
    }
    if (*cursor != '\0') {
        return amount_error(error, "amount contains unsupported characters");
    }
    if (fraction_digits == 1) {
        fraction *= 10;
    }
    if (ckd_mul(&whole, whole, 100) || ckd_add(&whole, whole, fraction)) {
        return amount_error(error, "amount is too large");
    }

    char normalized_currency[APL_MONEY_CURRENCY_MAX];
    status = apl_money_currency_normalize(currency, normalized_currency,
                                          sizeof(normalized_currency), error);
    if (status != APL_OK) {
        return status;
    }

    out->minor_units = negative ? -whole : whole;
    (void)snprintf(out->currency, sizeof(out->currency), "%s", normalized_currency);
    return APL_OK;
}

apl_status apl_money_add(const apl_money *const left, const apl_money *const right,
                         apl_money *const out, apl_error *const error)
{
    if (left == nullptr || right == nullptr || out == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "money add arguments are nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }
    if (strcmp(left->currency, right->currency) != 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                      "cannot add amounts in different currencies (%s and %s)", left->currency,
                      right->currency);
        apl_error_set_field(error, APL_FIELD_CURRENCY);
        return APL_ERR_INVALID_ARGUMENT;
    }

    int64_t sum = 0;
    if (ckd_add(&sum, left->minor_units, right->minor_units)) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "amount total is too large");
        apl_error_set_field(error, APL_FIELD_AMOUNT);
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out = *left;
    out->minor_units = sum;
    return APL_OK;
}

size_t apl_money_format(const apl_money *const money, char *const out, const size_t out_size)
{
    if (out == nullptr || out_size == 0) {
        return 0;
    }
    if (money == nullptr) {
        out[0] = '\0';
        return 0;
    }

    const bool negative = money->minor_units < 0;
    const uint64_t magnitude =
        negative ? (uint64_t)(-(money->minor_units + 1)) + 1U : (uint64_t)money->minor_units;
    const int written = snprintf(out, out_size, "%s %s%" PRIu64 ".%02" PRIu64, money->currency,
                                 negative ? "-" : "", magnitude / 100U, magnitude % 100U);
    if (written < 0) {
        out[0] = '\0';
        return 0;
    }
    return ((size_t)written < out_size) ? (size_t)written : out_size - 1;
}
