#ifndef APL_APP_PROJECT_SERVICE_H
#define APL_APP_PROJECT_SERVICE_H

#include <stddef.h>
#include <stdint.h>

#include "app/app.h"
#include "core/error.h"
#include "domain/project.h"

/*
 * Development-project use cases (work item 5.2).
 *
 * All writes validate user-entered text and optional same-team season/car
 * references, then save the project and its audit event in one transaction.
 * Category and status remain user-entered labels; no workflow vocabulary or
 * progress value is imposed here.
 */

apl_status apl_project_service_create(const apl_app *app, int64_t team_id, const char *title,
                                      const char *category, const char *description,
                                      const char *status_text, const char *target_date,
                                      const char *actual_date, int64_t season_id, int64_t car_id,
                                      const char *notes, apl_project *out_project,
                                      apl_error *error);

apl_status apl_project_service_get(const apl_app *app, int64_t team_id, int64_t id,
                                   apl_project *out_project, apl_error *error);

apl_status apl_project_service_update(const apl_app *app, int64_t team_id, int64_t id,
                                      const char *title, const char *category,
                                      const char *description, const char *status_text,
                                      const char *target_date, const char *actual_date,
                                      int64_t season_id, int64_t car_id, const char *notes,
                                      apl_project *out_project, apl_error *error);

apl_status apl_project_service_archive(const apl_app *app, int64_t team_id, int64_t id,
                                       apl_project *out_project, apl_error *error);

apl_status apl_project_service_list(const apl_app *app, const apl_project_filter *filter,
                                    apl_project **out_projects, size_t *out_count,
                                    apl_error *error);

/** Append one dated work update and, when `status_change` is nonblank, update
 *  the project's current status in the same transaction. Archived projects
 *  reject new updates. */
apl_status apl_project_service_log_update(const apl_app *app, int64_t team_id, int64_t project_id,
                                          const char *update_date, const char *summary,
                                          const char *status_change, const char *notes,
                                          apl_project_update *out_update, apl_project *out_project,
                                          apl_error *error);

apl_status apl_project_service_get_update(const apl_app *app, int64_t team_id, int64_t id,
                                          apl_project_update *out_update, apl_error *error);

apl_status apl_project_service_list_updates(const apl_app *app, int64_t team_id, int64_t project_id,
                                            apl_project_update **out_updates, size_t *out_count,
                                            apl_error *error);

void apl_project_service_free_list(apl_project *projects);
void apl_project_service_free_updates(apl_project_update *updates);

#endif /* APL_APP_PROJECT_SERVICE_H */
