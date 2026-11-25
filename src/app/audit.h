#ifndef APL_APP_AUDIT_H
#define APL_APP_AUDIT_H

#include <stddef.h>
#include <stdint.h>

#include "app/app.h"
#include "core/error.h"
#include "domain/reference.h"

/* Small append helper shared by Phase 3 use cases. Callers own the transaction
 * boundary, so the business write and its audit event commit or roll back
 * together. */

apl_status apl_audit_append(const apl_app *app, int64_t team_id, const char *action,
                            const char *entity_type, int64_t entity_id, const char *summary,
                            const apl_activity_change *changes, size_t change_count,
                            apl_error *error);

/** Fill one change row. Missing old/new values are represented explicitly. */
void apl_audit_change_text(apl_activity_change *change, const char *field, const char *old_value,
                           bool old_missing, const char *new_value, bool new_missing);

/** Fill a boolean change row using normalized "false"/"true" text. */
void apl_audit_change_bool(apl_activity_change *change, const char *field, bool old_value,
                           bool new_value);

#endif /* APL_APP_AUDIT_H */
