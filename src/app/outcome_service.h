#ifndef APL_APP_OUTCOME_SERVICE_H
#define APL_APP_OUTCOME_SERVICE_H

#include "app/app.h"
#include "db/outcome_repository.h"

typedef enum apl_outcome_save_mode {
    APL_OUTCOME_CREATE,
    APL_OUTCOME_EDIT,
} apl_outcome_save_mode;

typedef struct apl_outcome_batch_row {
    apl_outcome_save_mode mode;
    apl_outcome input;
} apl_outcome_batch_row;

/* row_errors has count entries and is initialized for every call. Validation
 * reports each failing row; a write failure is attached to its row. On any
 * failure no outcomes or activity are committed and out is not modified.
 * Transaction-level failures are returned through error. count must be > 0. */
apl_status apl_outcome_service_save_batch(const apl_app *app, const apl_outcome_batch_row *rows,
                                          size_t count, apl_outcome *out, apl_error *row_errors,
                                          apl_error *error);

/* CREATE rejects an existing driver/session target with APL_ERR_CONFLICT.
 * EDIT requires input->id and the same team, event, session and driver as the
 * existing outcome. Optional blank text is converted to missing. */
apl_status apl_outcome_service_save(const apl_app *app, apl_outcome_save_mode mode,
                                    const apl_outcome *input, apl_outcome *out, apl_error *error);

#endif
