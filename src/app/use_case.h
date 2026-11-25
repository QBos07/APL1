#ifndef APL_APP_USE_CASE_H
#define APL_APP_USE_CASE_H

#include "app/app.h"
#include "core/error.h"
#include "db/transaction.h"

/*
 * Shared write-use-case plumbing.
 *
 * Feature services own their validation and repository calls but should not
 * reimplement transaction cleanup or error-context handling. These helpers
 * keep the commit/rollback contract consistent across use cases (work items
 * 4.6, 5.2, 5.4, 6.5, 6.6, 7.3, 7.7).
 */

/** Add context to a failed status; APL_OK is returned unchanged. */
apl_status apl_use_case_with_context(apl_status status, const char *context, apl_error *error);

/** Annotate a failed validation status with the field that caused it. */
apl_status apl_use_case_with_field(apl_status status, apl_field field, apl_error *error);

/** Begin a write transaction for `app`, prefixing failures with `context`. */
apl_status apl_use_case_begin(apl_db_transaction *transaction, const apl_app *app,
                              const char *context, apl_error *error);

/** Commit a write transaction.
 *
 * A failed commit is rolled back when the transaction is still active, then
 * reported with `context`. */
apl_status apl_use_case_commit(apl_db_transaction *transaction, const char *context,
                               apl_error *error);

/** Roll back after a failed use-case step, preserving the original status.
 *
 * A rollback failure is added as extra context. `status` is then reported with
 * the use-case `context`. */
apl_status apl_use_case_rollback(apl_db_transaction *transaction, apl_status status,
                                 const char *context, apl_error *error);

#endif /* APL_APP_USE_CASE_H */
