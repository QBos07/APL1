#ifndef APL_DOMAIN_MONEY_H
#define APL_DOMAIN_MONEY_H

#include <stddef.h>
#include <stdint.h>

#include "core/error.h"

/*
 * Exact money values (work item 4.1).
 *
 * Amounts are signed integer minor units at scale 2 (cents). No floating point
 * is involved. v1 keeps one reporting currency per team; values still carry
 * their currency so an accidental mixed-currency operation is rejected rather
 * than silently converted.
 */

static constexpr size_t APL_MONEY_CURRENCY_MAX = 4;

typedef struct apl_money {
    int64_t minor_units;
    char currency[APL_MONEY_CURRENCY_MAX];
} apl_money;

/** Normalize a three-letter ISO-style currency code to uppercase.
 *
 * Exactly three ASCII letters are accepted; shorter, longer, or non-letter
 * values are rejected. The output buffer must hold APL_MONEY_CURRENCY_MAX. */
apl_status apl_money_currency_normalize(const char *text, char *out, size_t out_size,
                                        apl_error *error);

/** Parse a user-entered decimal amount into exact minor units.
 *
 * Accepts an optional sign, digits, and an optional decimal point followed by
 * one or two digits. A comma, exponent, currency symbol, more than two
 * fractional digits, or trailing text is rejected. */
apl_status apl_money_parse(const char *text, const char *currency, apl_money *out,
                           apl_error *error);

/** Add two values with the same currency; rejects currency mismatch/overflow. */
apl_status apl_money_add(const apl_money *left, const apl_money *right, apl_money *out,
                         apl_error *error);

/** Format as "<CURRENCY> <sign><whole>.<fraction>", for example "EUR -12.50".
 *  Returns the written length. INT64_MIN is handled without signed overflow. */
size_t apl_money_format(const apl_money *money, char *out, size_t out_size);

#endif /* APL_DOMAIN_MONEY_H */
