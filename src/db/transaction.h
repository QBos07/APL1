#ifndef APL_DB_TRANSACTION_H
#define APL_DB_TRANSACTION_H

#include "core/error.h"
#include "db/connection.h"

/*
 * Small transaction helper for use-case-owned multi-write operations
 * (work item 2.0.3).
 *
 * The helper borrows the connection; it never opens or closes it. It issues
 * BEGIN IMMEDIATE so the write lock is taken before the first mutation, rejects
 * nested transactions, and leaves no transaction open after commit attempts.
 * Repositories do not use this directly for partial work: the application use
 * case owns the boundary and calls the repository primitives between these
 * calls.
 */

typedef struct apl_db_transaction {
    const apl_db_connection *connection; /* borrowed, never owned */
    bool active;
} apl_db_transaction;

/** Start a write transaction. Fails if a transaction is already active. */
apl_status apl_db_transaction_begin(apl_db_transaction *transaction,
                                    const apl_db_connection *connection, apl_error *error);

/** Commit the active transaction.
 *
 * On a commit failure the helper attempts a best-effort rollback, leaves the
 * transaction inactive, and reports the commit failure. */
apl_status apl_db_transaction_commit(apl_db_transaction *transaction, apl_error *error);

/** Roll back the active transaction. Safe/idempotent when inactive. */
apl_status apl_db_transaction_rollback(apl_db_transaction *transaction, apl_error *error);

#endif /* APL_DB_TRANSACTION_H */
