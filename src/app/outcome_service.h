#ifndef APL_APP_OUTCOME_SERVICE_H
#define APL_APP_OUTCOME_SERVICE_H

#include "app/app.h"
#include "db/outcome_repository.h"

typedef enum apl_outcome_save_mode {
    APL_OUTCOME_CREATE,
    APL_OUTCOME_EDIT,
} apl_outcome_save_mode;

/* CREATE rejects an existing driver/session target with APL_ERR_CONFLICT.
 * EDIT requires input->id and the same team, event, session and driver as the
 * existing outcome. Optional blank text is converted to missing. */
apl_status apl_outcome_service_save(const apl_app *app, apl_outcome_save_mode mode,
                                    const apl_outcome *input, apl_outcome *out, apl_error *error);

#endif
