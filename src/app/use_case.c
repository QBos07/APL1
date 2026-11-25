#include "app/use_case.h"

#include <stdio.h>

apl_status apl_use_case_with_context(const apl_status status, const char *const context,
                                     apl_error *const error)
{
    if (status != APL_OK) {
        apl_error_prefix(error, status, context);
    }
    return status;
}

apl_status apl_use_case_begin(apl_db_transaction *const transaction, const apl_app *const app,
                              const char *const context, apl_error *const error)
{
    return apl_use_case_with_context(apl_db_transaction_begin(transaction, &app->db, error),
                                     context, error);
}

apl_status apl_use_case_commit(apl_db_transaction *const transaction, const char *const context,
                               apl_error *const error)
{
    const apl_status status = apl_db_transaction_commit(transaction, error);
    if (status != APL_OK && transaction->active) {
        (void)apl_use_case_rollback(transaction, status, context, error);
    }
    return apl_use_case_with_context(status, context, error);
}

apl_status apl_use_case_rollback(apl_db_transaction *const transaction, const apl_status status,
                                 const char *const context, apl_error *const error)
{
    apl_error rollback_error;
    apl_error_init(&rollback_error);
    if (apl_db_transaction_rollback(transaction, &rollback_error) != APL_OK) {
        char detail[160];
        (void)snprintf(detail, sizeof(detail), "rollback failed: %s",
                       apl_error_message(&rollback_error));
        apl_error_prefix(error, status, detail);
    }
    return apl_use_case_with_context(status, context, error);
}
