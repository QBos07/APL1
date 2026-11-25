#include <stdint.h>

#include <criterion/criterion.h>

#include "domain/money.h"

Test(money, parses_exact_minor_units)
{
    apl_error err;
    apl_error_init(&err);
    apl_money money = {};

    cr_assert_eq(apl_money_parse("123.45", "EUR", &money, &err), APL_OK);
    cr_assert_eq(money.minor_units, 12345);
    cr_assert_str_eq(money.currency, "EUR");

    cr_assert_eq(apl_money_parse("123", "eur", &money, &err), APL_OK);
    cr_assert_eq(money.minor_units, 12300);
    cr_assert_str_eq(money.currency, "EUR");

    cr_assert_eq(apl_money_parse("123.4", "EUR", &money, &err), APL_OK);
    cr_assert_eq(money.minor_units, 12340);

    cr_assert_eq(apl_money_parse("-0.01", "EUR", &money, &err), APL_OK);
    cr_assert_eq(money.minor_units, -1);

    cr_assert_eq(apl_money_parse("+5.00", "EUR", &money, &err), APL_OK);
    cr_assert_eq(money.minor_units, 500);
}

Test(money, rejects_invalid_amounts_and_currencies)
{
    apl_error err;
    apl_error_init(&err);
    apl_money money = {};

    cr_assert_eq(apl_money_parse("", "EUR", &money, &err), APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_error_field_id(&err), APL_FIELD_AMOUNT);
    cr_assert_eq(apl_money_parse("abc", "EUR", &money, &err), APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_money_parse("1.234", "EUR", &money, &err), APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_money_parse("1.", "EUR", &money, &err), APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_money_parse(".5", "EUR", &money, &err), APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_money_parse("1,00", "EUR", &money, &err), APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_money_parse("1e3", "EUR", &money, &err), APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_money_parse("1.00", "EU", &money, &err), APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_error_field_id(&err), APL_FIELD_CURRENCY);
    cr_assert_eq(apl_money_parse("1.00", "E1R", &money, &err), APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_money_parse("1.00", nullptr, &money, &err), APL_ERR_INVALID_ARGUMENT);
}

Test(money, currency_is_normalized_to_uppercase)
{
    char currency[APL_MONEY_CURRENCY_MAX];
    apl_error err;
    apl_error_init(&err);

    cr_assert_eq(apl_money_currency_normalize(" usd ", currency, sizeof(currency), &err), APL_OK);
    cr_assert_str_eq(currency, "USD");
    cr_assert_eq(apl_money_currency_normalize("US", currency, sizeof(currency), &err),
                 APL_ERR_INVALID_ARGUMENT);
}

Test(money, add_rejects_currency_mismatch_and_overflow)
{
    apl_error err;
    apl_error_init(&err);
    const apl_money ten = {.minor_units = 1000, .currency = "EUR"};
    const apl_money twenty = {.minor_units = 2000, .currency = "EUR"};
    const apl_money dollars = {.minor_units = 1000, .currency = "USD"};
    apl_money sum = {};

    cr_assert_eq(apl_money_add(&ten, &twenty, &sum, &err), APL_OK);
    cr_assert_eq(sum.minor_units, 3000);
    cr_assert_str_eq(sum.currency, "EUR");

    cr_assert_eq(apl_money_add(&ten, &dollars, &sum, &err), APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_error_field_id(&err), APL_FIELD_CURRENCY);

    const apl_money max = {.minor_units = INT64_MAX, .currency = "EUR"};
    const apl_money one = {.minor_units = 1, .currency = "EUR"};
    cr_assert_eq(apl_money_add(&max, &one, &sum, &err), APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_error_field_id(&err), APL_FIELD_AMOUNT);
}

Test(money, formats_exact_values)
{
    char out[32];
    const apl_money positive = {.minor_units = 12345, .currency = "EUR"};
    const apl_money negative = {.minor_units = -1250, .currency = "EUR"};
    const apl_money minimum = {.minor_units = INT64_MIN, .currency = "EUR"};

    cr_assert_eq(apl_money_format(&positive, out, sizeof(out)), 10);
    cr_assert_str_eq(out, "EUR 123.45");
    cr_assert_eq(apl_money_format(&negative, out, sizeof(out)), 10);
    cr_assert_str_eq(out, "EUR -12.50");
    (void)apl_money_format(&minimum, out, sizeof(out));
    cr_assert_str_eq(out, "EUR -92233720368547758.08");
}
