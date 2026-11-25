#include <stdio.h>

#include <criterion/criterion.h>

#include "db/project_repository.h"
#include "db/project_update_repository.h"
#include "db/team_repository.h"
#include "db_fixture.h"

static apl_team make_team(apl_test_db *const db, const char *const name)
{
    apl_error err;
    apl_error_init(&err);
    apl_team team = {};
    cr_assert_eq(apl_team_repository_create(&db->connection, name, "", &team, &err), APL_OK,
                 "team create failed: %s", apl_error_message(&err));
    return team;
}

static apl_project make_project(apl_test_db *const db, const int64_t team_id,
                                const char *const title)
{
    apl_error err;
    apl_error_init(&err);
    apl_project project = {.team_id = team_id};
    (void)snprintf(project.title, sizeof(project.title), "%s", title);
    apl_project created = {};
    cr_assert_eq(apl_project_repository_create(&db->connection, &project, &created, &err), APL_OK,
                 "project create failed: %s", apl_error_message(&err));
    return created;
}

static apl_project_update make_update(const int64_t project_id, const char *const date,
                                      const char *const summary)
{
    apl_project_update update = {.project_id = project_id};
    (void)snprintf(update.update_date, sizeof(update.update_date), "%s", date);
    (void)snprintf(update.summary, sizeof(update.summary), "%s", summary);
    return update;
}

Test(project_update_repository, appends_and_lists_in_entered_date_order)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);
    const apl_team team = make_team(&db, "Ferrari");
    const apl_project project = make_project(&db, team.id, "Aero");

    apl_project_update *updates = nullptr;
    size_t count = 1;
    cr_assert_eq(apl_project_update_repository_list(&db.connection, team.id, project.id, &updates,
                                                    &count, &err),
                 APL_OK);
    cr_assert_eq(count, 0);
    cr_assert_null(updates);

    apl_project_update later = make_update(project.id, "2026-05-01", "Second");
    (void)snprintf(later.notes, sizeof(later.notes), "Late note");
    apl_project_update stored_later = {};
    cr_assert_eq(
        apl_project_update_repository_append(&db.connection, team.id, &later, &stored_later, &err),
        APL_OK);
    cr_assert_gt(stored_later.id, 0);
    cr_assert_str_eq(stored_later.update_date, "2026-05-01");
    cr_assert_str_eq(stored_later.summary, "Second");
    cr_assert_neq(stored_later.created_at[0], '\0');

    apl_project_update first = make_update(project.id, "2026-04-01", "First");
    (void)snprintf(first.status_change, sizeof(first.status_change), "In progress");
    (void)snprintf(first.status_change_key, sizeof(first.status_change_key), "in progress");
    apl_project_update stored_first = {};
    cr_assert_eq(
        apl_project_update_repository_append(&db.connection, team.id, &first, &stored_first, &err),
        APL_OK);

    apl_project_update same_day = make_update(project.id, "2026-04-01", "Same day");
    apl_project_update stored_same = {};
    cr_assert_eq(apl_project_update_repository_append(&db.connection, team.id, &same_day,
                                                      &stored_same, &err),
                 APL_OK);

    cr_assert_eq(apl_project_update_repository_list(&db.connection, team.id, project.id, &updates,
                                                    &count, &err),
                 APL_OK);
    cr_assert_eq(count, 3);
    cr_assert_str_eq(updates[0].summary, "First");
    cr_assert_str_eq(updates[0].status_change, "In progress");
    cr_assert_str_eq(updates[1].summary, "Same day");
    cr_assert_str_eq(updates[2].summary, "Second");
    /* The two same-date entries keep insertion order; the later date is last
     * even though its row was created first. */
    cr_assert_lt(updates[0].id, updates[1].id);
    apl_project_update_repository_free_list(updates);

    apl_project_update fetched = {};
    cr_assert_eq(
        apl_project_update_repository_get(&db.connection, team.id, stored_first.id, &fetched, &err),
        APL_OK);
    cr_assert_str_eq(fetched.update_date, "2026-04-01");
    cr_assert_str_eq(fetched.summary, "First");

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}

Test(project_update_repository, rejects_missing_projects_and_hides_other_teams)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);
    const apl_team ferrari = make_team(&db, "Ferrari");
    const apl_team mclaren = make_team(&db, "McLaren");
    const apl_project project = make_project(&db, ferrari.id, "Aero");

    apl_project_update missing = make_update(project.id + 1000, "2026-04-01", "Missing parent");
    apl_project_update stored = {};
    cr_assert_eq(
        apl_project_update_repository_append(&db.connection, ferrari.id, &missing, &stored, &err),
        APL_ERR_NOT_FOUND);

    apl_project_update update = make_update(project.id, "2026-04-01", "Visible");
    cr_assert_eq(
        apl_project_update_repository_append(&db.connection, ferrari.id, &update, &stored, &err),
        APL_OK);
    const int64_t stored_id = stored.id;

    cr_assert_eq(
        apl_project_update_repository_append(&db.connection, mclaren.id, &update, &stored, &err),
        APL_ERR_NOT_FOUND);

    apl_project_update hidden = {};
    cr_assert_eq(
        apl_project_update_repository_get(&db.connection, mclaren.id, stored_id, &hidden, &err),
        APL_ERR_NOT_FOUND);

    apl_project_update *updates = nullptr;
    size_t count = 1;
    cr_assert_eq(apl_project_update_repository_list(&db.connection, mclaren.id, project.id,
                                                    &updates, &count, &err),
                 APL_OK);
    cr_assert_eq(count, 0);
    apl_project_update_repository_free_list(updates);

    cr_assert_eq(apl_project_update_repository_list(&db.connection, ferrari.id, project.id,
                                                    &updates, &count, &err),
                 APL_OK);
    cr_assert_eq(count, 1);
    apl_project_update_repository_free_list(updates);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}

Test(project_update_repository, archived_projects_keep_their_timeline_readable)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);
    const apl_team team = make_team(&db, "Ferrari");
    const apl_project project = make_project(&db, team.id, "Aero");

    apl_project_update update = make_update(project.id, "2026-04-01", "Work");
    apl_project_update stored = {};
    cr_assert_eq(
        apl_project_update_repository_append(&db.connection, team.id, &update, &stored, &err),
        APL_OK);

    apl_project archived = {};
    cr_assert_eq(
        apl_project_repository_archive(&db.connection, team.id, project.id, &archived, &err),
        APL_OK);

    apl_project_update *updates = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_project_update_repository_list(&db.connection, team.id, project.id, &updates,
                                                    &count, &err),
                 APL_OK);
    cr_assert_eq(count, 1);
    cr_assert_str_eq(updates[0].summary, "Work");
    apl_project_update_repository_free_list(updates);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}
