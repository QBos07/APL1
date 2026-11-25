#include "domain/ledger.h"

#include <ctype.h>
#include <string.h>

const char *apl_ledger_kind_code(const apl_ledger_kind kind)
{
    switch (kind) {
    case APL_LEDGER_KIND_OPENING_BALANCE:
        return "opening_balance";
    case APL_LEDGER_KIND_INCOME:
        return "income";
    case APL_LEDGER_KIND_EXPENSE:
        return "expense";
    case APL_LEDGER_KIND_REVERSAL:
        return "reversal";
    case APL_LEDGER_KIND_ADJUSTMENT:
        return "adjustment";
    case APL_LEDGER_KIND_NONE:
        return "";
    }
    return "";
}

const char *apl_ledger_kind_label(const apl_ledger_kind kind)
{
    switch (kind) {
    case APL_LEDGER_KIND_OPENING_BALANCE:
        return "Opening balance";
    case APL_LEDGER_KIND_INCOME:
        return "Income";
    case APL_LEDGER_KIND_EXPENSE:
        return "Expense";
    case APL_LEDGER_KIND_REVERSAL:
        return "Reversal";
    case APL_LEDGER_KIND_ADJUSTMENT:
        return "Adjustment";
    case APL_LEDGER_KIND_NONE:
        return "Unknown";
    }
    return "Unknown";
}

apl_ledger_kind apl_ledger_kind_from_code(const char *const code)
{
    if (code == nullptr) {
        return APL_LEDGER_KIND_NONE;
    }
    for (int kind = APL_LEDGER_KIND_OPENING_BALANCE; kind <= APL_LEDGER_KIND_ADJUSTMENT; ++kind) {
        if (strcmp(code, apl_ledger_kind_code((apl_ledger_kind)kind)) == 0) {
            return (apl_ledger_kind)kind;
        }
    }
    return APL_LEDGER_KIND_NONE;
}

bool apl_ledger_kind_is_postable(const apl_ledger_kind kind)
{
    return kind == APL_LEDGER_KIND_OPENING_BALANCE || kind == APL_LEDGER_KIND_INCOME ||
           kind == APL_LEDGER_KIND_EXPENSE;
}

apl_status apl_ledger_category_normalize(const char *const text, char *const out_display,
                                         const size_t display_size, char *const out_key,
                                         const size_t key_size, apl_error *const error)
{
    if (out_display == nullptr || display_size == 0 || out_key == nullptr || key_size == 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "category buffer is not usable");
        apl_error_set_field(error, APL_FIELD_CATEGORY);
        return APL_ERR_INVALID_ARGUMENT;
    }
    out_display[0] = '\0';
    out_key[0] = '\0';

    const char *cursor = (text != nullptr) ? text : "";
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

    if (display_length == 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "category is required");
        apl_error_set_field(error, APL_FIELD_CATEGORY);
        return APL_ERR_INVALID_ARGUMENT;
    }
    if (*cursor != '\0') {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "category is too long (maximum %zu bytes)",
                      display_size - 1);
        apl_error_set_field(error, APL_FIELD_CATEGORY);
        return APL_ERR_INVALID_ARGUMENT;
    }

    out_display[display_length] = '\0';
    out_key[key_length] = '\0';
    return APL_OK;
}

apl_status apl_ledger_date_normalize(const char *const text, char *const out, const size_t out_size,
                                     apl_error *const error)
{
    apl_status status =
        apl_reference_date_normalize(text, "transaction date", out, out_size, error);
    if (status != APL_OK) {
        apl_error_set_field(error, APL_FIELD_DATE);
        return status;
    }
    if (out[0] == '\0') {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "transaction date is required");
        apl_error_set_field(error, APL_FIELD_DATE);
        return APL_ERR_INVALID_ARGUMENT;
    }
    return APL_OK;
}

apl_status apl_ledger_description_normalize(const char *const text, char *const out,
                                            const size_t out_size, apl_error *const error)
{
    apl_status status = apl_reference_label_normalize(text, "description", out, out_size, error);
    if (status != APL_OK) {
        apl_error_set_field(error, APL_FIELD_DESCRIPTION);
    }
    return status;
}
