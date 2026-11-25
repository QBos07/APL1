#include <stdio.h>

#include <criterion/criterion.h>

#include "db/car_repository.h"
#include "db/project_repository.h"
#include "db/season_repository.h"
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

static apl_season make_season(apl_test_db *const db, const int64_t team_id, const char *const label)
{
    apl_error err;
    apl_error_init(&err);
    apl_season season = {};
    cr_assert_eq(
        apl_season_repository_create(&db->connection, team_id, label, "", "", "", &season, &err),
        APL_OK, "season create failed: %s", apl_error_message(&err));
    return season;
}

static apl_car make_car(apl_test_db *const db, const int64_t team_id, const char *const label)
{
    apl_error err;
    apl_error_init(&err);
    apl_car car = {};
    cr_assert_eq(apl_car_repository_create(&db->connection, team_id, label, "", "", &car, &err),
                 APL_OK, "car create failed: %s", apl_error_message(&err));
    return car;
}

static apl_project make_project(const int64_t team_id, const char *const title)
{
    apl_project project = {.team_id = team_id};
    (void)snprintf(project.title, sizeof(project.title), "%s", title);
    return project;
}

Test(project_repository, creates_reads_updates_and_archives_without_seeding)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);
    const apl_team team = make_team(&db, "Ferrari");
    const apl_season season = make_season(&db, team.id, "2026");
    const apl_car car = make_car(&db, team.id, "SF-26");

    const apl_project_filter filter = {.team_id = team.id};
    apl_project *projects = nullptr;
    size_t count = 1;
    cr_assert_eq(apl_project_repository_list(&db.connection, &filter, &projects, &count, &err),
                 APL_OK);
    cr_assert_eq(count, 0);
    cr_assert_null(projects);

    apl_project draft = make_project(team.id, "  Floor upgrade  ");
    draft.season_id = season.id;
    draft.car_id = car.id;
    (void)snprintf(draft.category, sizeof(draft.category), "Aero");
    (void)snprintf(draft.category_key, sizeof(draft.category_key), "aero");
    (void)snprintf(draft.description, sizeof(draft.description), "New floor concept");
    (void)snprintf(draft.status, sizeof(draft.status), "In progress");
    (void)snprintf(draft.status_key, sizeof(draft.status_key), "in progress");
    (void)snprintf(draft.target_date, sizeof(draft.target_date), "2026-03-01");
    (void)snprintf(draft.notes, sizeof(draft.notes), "Wind tunnel checked");

    apl_project created = {};
    cr_assert_eq(apl_project_repository_create(&db.connection, &draft, &created, &err), APL_OK,
                 "project create failed: %s", apl_error_message(&err));
    cr_assert_gt(created.id, 0);
    cr_assert_eq(created.team_id, team.id);
    cr_assert_eq(created.season_id, season.id);
    cr_assert_eq(created.car_id, car.id);
    cr_assert_str_eq(created.title, "  Floor upgrade  ");
    cr_assert_str_eq(created.description, "New floor concept");
    cr_assert_str_eq(created.target_date, "2026-03-01");
    cr_assert_str_eq(created.actual_date, "");
    cr_assert_not(created.meta.archived);
    cr_assert_neq(created.meta.created_at[0], '\0');

    apl_project fetched = {};
    cr_assert_eq(apl_project_repository_get(&db.connection, team.id, created.id, &fetched, &err),
                 APL_OK);
    cr_assert_str_eq(fetched.title, created.title);
    cr_assert_eq(fetched.season_id, season.id);

    apl_project changed = fetched;
    (void)snprintf(changed.title, sizeof(changed.title), "Floor upgrade v2");
    (void)snprintf(changed.status, sizeof(changed.status), "Paused");
    (void)snprintf(changed.status_key, sizeof(changed.status_key), "paused");
    (void)snprintf(changed.actual_date, sizeof(changed.actual_date), "2026-04-15");
    changed.season_id = 0;
    changed.car_id = 0;
    apl_project updated = {};
    cr_assert_eq(apl_project_repository_update(&db.connection, &changed, &updated, &err), APL_OK);
    cr_assert_eq(updated.id, created.id);
    cr_assert_str_eq(updated.title, "Floor upgrade v2");
    cr_assert_str_eq(updated.status, "Paused");
    cr_assert_str_eq(updated.actual_date, "2026-04-15");
    cr_assert_eq(updated.season_id, 0);
    cr_assert_eq(updated.car_id, 0);

    apl_project archived = {};
    cr_assert_eq(
        apl_project_repository_archive(&db.connection, team.id, created.id, &archived, &err),
        APL_OK);
    cr_assert(archived.meta.archived);
    cr_assert_neq(archived.meta.archived_at[0], '\0');

    apl_project hidden = {};
    cr_assert_eq(apl_project_repository_get(&db.connection, team.id, created.id, &hidden, &err),
                 APL_OK);
    cr_assert(hidden.meta.archived);

    cr_assert_eq(apl_project_repository_list(&db.connection, &filter, &projects, &count, &err),
                 APL_OK);
    cr_assert_eq(count, 0);
    apl_project_repository_free_list(projects);

    apl_project_filter archived_filter = {.team_id = team.id, .include_archived = true};
    cr_assert_eq(
        apl_project_repository_list(&db.connection, &archived_filter, &projects, &count, &err),
        APL_OK);
    cr_assert_eq(count, 1);
    cr_assert_str_eq(projects[0].title, "Floor upgrade v2");
    apl_project_repository_free_list(projects);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}

Test(project_repository, rejects_cross_team_links_and_hides_other_team_rows)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);
    const apl_team ferrari = make_team(&db, "Ferrari");
    const apl_team mclaren = make_team(&db, "McLaren");
    const apl_season ferrari_season = make_season(&db, ferrari.id, "2026");
    const apl_car mclaren_car = make_car(&db, mclaren.id, "MCL-26");

    apl_project cross_season = make_project(ferrari.id, "Cross season");
    cross_season.season_id = ferrari_season.id + 1000;
    apl_project rejected = {};
    cr_assert_eq(apl_project_repository_create(&db.connection, &cross_season, &rejected, &err),
                 APL_ERR_INVALID_ARGUMENT);

    apl_project cross_car = make_project(ferrari.id, "Cross car");
    cross_car.car_id = mclaren_car.id;
    cr_assert_eq(apl_project_repository_create(&db.connection, &cross_car, &rejected, &err),
                 APL_ERR_INVALID_ARGUMENT);

    const apl_project_filter filter = {.team_id = ferrari.id};
    apl_project *projects = nullptr;
    size_t count = 1;
    cr_assert_eq(apl_project_repository_list(&db.connection, &filter, &projects, &count, &err),
                 APL_OK);
    cr_assert_eq(count, 0);
    apl_project_repository_free_list(projects);

    apl_project own = make_project(ferrari.id, "Own season");
    own.season_id = ferrari_season.id;
    apl_project created = {};
    cr_assert_eq(apl_project_repository_create(&db.connection, &own, &created, &err), APL_OK);

    apl_project hidden = {};
    cr_assert_eq(apl_project_repository_get(&db.connection, mclaren.id, created.id, &hidden, &err),
                 APL_ERR_NOT_FOUND);
    apl_project foreign_update = created;
    foreign_update.team_id = mclaren.id;
    cr_assert_eq(apl_project_repository_update(&db.connection, &foreign_update, &hidden, &err),
                 APL_ERR_NOT_FOUND);
    cr_assert_eq(
        apl_project_repository_archive(&db.connection, mclaren.id, created.id, &hidden, &err),
        APL_ERR_NOT_FOUND);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}

Test(project_repository, orders_by_title_and_applies_filters)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);
    const apl_team team = make_team(&db, "Ferrari");
    const apl_season season = make_season(&db, team.id, "2026");

    const char *const titles[] = {"Rear wing", "brake ducts", "Aero floor"};
    for (size_t index = 0; index < 3; ++index) {
        apl_project project = make_project(team.id, titles[index]);
        if (index == 1) {
            project.season_id = season.id;
            (void)snprintf(project.status, sizeof(project.status), "Open");
            (void)snprintf(project.status_key, sizeof(project.status_key), "open");
            (void)snprintf(project.category, sizeof(project.category), "Aero");
            (void)snprintf(project.category_key, sizeof(project.category_key), "aero");
        }
        apl_project created = {};
        cr_assert_eq(apl_project_repository_create(&db.connection, &project, &created, &err),
                     APL_OK);
    }

    const apl_project_filter all = {.team_id = team.id};
    apl_project *projects = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_project_repository_list(&db.connection, &all, &projects, &count, &err),
                 APL_OK);
    cr_assert_eq(count, 3);
    cr_assert_str_eq(projects[0].title, "Aero floor");
    cr_assert_str_eq(projects[1].title, "brake ducts");
    cr_assert_str_eq(projects[2].title, "Rear wing");
    apl_project_repository_free_list(projects);

    apl_project_filter status = {.team_id = team.id};
    (void)snprintf(status.status_key, sizeof(status.status_key), "open");
    cr_assert_eq(apl_project_repository_list(&db.connection, &status, &projects, &count, &err),
                 APL_OK);
    cr_assert_eq(count, 1);
    cr_assert_str_eq(projects[0].title, "brake ducts");
    apl_project_repository_free_list(projects);

    apl_project_filter season_filter = {.team_id = team.id, .season_id = season.id};
    cr_assert_eq(
        apl_project_repository_list(&db.connection, &season_filter, &projects, &count, &err),
        APL_OK);
    cr_assert_eq(count, 1);
    cr_assert_str_eq(projects[0].title, "brake ducts");
    apl_project_repository_free_list(projects);

    apl_project_filter category = {.team_id = team.id};
    (void)snprintf(category.category_key, sizeof(category.category_key), "aero");
    cr_assert_eq(apl_project_repository_list(&db.connection, &category, &projects, &count, &err),
                 APL_OK);
    cr_assert_eq(count, 1);
    cr_assert_str_eq(projects[0].title, "brake ducts");
    apl_project_repository_free_list(projects);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}
