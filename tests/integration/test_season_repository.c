
#include <criterion/criterion.h>

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

Test(season_repository, create_list_get_update_and_archive)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);
    const apl_team team = make_team(&db, "Ferrari");

    apl_season *empty = nullptr;
    size_t empty_count = 1;
    cr_assert_eq(
        apl_season_repository_list(&db.connection, team.id, false, &empty, &empty_count, &err),
        APL_OK);
    cr_assert_null(empty);
    cr_assert_eq(empty_count, 0);

    apl_season created = {};
    cr_assert_eq(apl_season_repository_create(&db.connection, team.id, "2026", "2026-01-01",
                                              "2026-12-31", "Main season", &created, &err),
                 APL_OK);
    cr_assert_gt(created.id, 0);
    cr_assert_eq(created.team_id, team.id);
    cr_assert_str_eq(created.label, "2026");
    cr_assert_str_eq(created.start_date, "2026-01-01");
    cr_assert_str_eq(created.end_date, "2026-12-31");
    cr_assert_str_eq(created.notes, "Main season");
    cr_assert_not(created.archived);

    apl_season fetched = {};
    cr_assert_eq(apl_season_repository_get(&db.connection, team.id, created.id, &fetched, &err),
                 APL_OK);
    cr_assert_eq(fetched.id, created.id);

    apl_season updated = {};
    cr_assert_eq(apl_season_repository_update(&db.connection, team.id, created.id, "2026 season",
                                              "", "", "", &updated, &err),
                 APL_OK);
    cr_assert_eq(updated.id, created.id);
    cr_assert_str_eq(updated.label, "2026 season");
    cr_assert_str_eq(updated.start_date, "");
    cr_assert_str_eq(updated.end_date, "");
    cr_assert_str_eq(updated.created_at, created.created_at);

    apl_season archived = {};
    cr_assert_eq(
        apl_season_repository_archive(&db.connection, team.id, created.id, &archived, &err),
        APL_OK);
    cr_assert(archived.archived);
    cr_assert_neq(archived.archived_at[0], '\0');

    apl_season *active = nullptr;
    size_t active_count = 1;
    cr_assert_eq(
        apl_season_repository_list(&db.connection, team.id, false, &active, &active_count, &err),
        APL_OK);
    cr_assert_eq(active_count, 0);
    apl_season_repository_free_list(active);

    apl_season *all = nullptr;
    size_t all_count = 0;
    cr_assert_eq(apl_season_repository_list(&db.connection, team.id, true, &all, &all_count, &err),
                 APL_OK);
    cr_assert_eq(all_count, 1);
    cr_assert(all[0].archived);
    apl_season_repository_free_list(all);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}

Test(season_repository, duplicate_labels_are_case_insensitive_and_team_scoped)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);
    const apl_team ferrari = make_team(&db, "Ferrari");
    const apl_team mclaren = make_team(&db, "McLaren");

    apl_season first = {};
    cr_assert_eq(
        apl_season_repository_create(&db.connection, ferrari.id, "2026", "", "", "", &first, &err),
        APL_OK);

    apl_season duplicate = {};
    cr_assert_eq(apl_season_repository_create(&db.connection, ferrari.id, "2026", "", "", "",
                                              &duplicate, &err),
                 APL_ERR_CONFLICT);

    apl_season other_team = {};
    cr_assert_eq(apl_season_repository_create(&db.connection, mclaren.id, "2026", "", "", "",
                                              &other_team, &err),
                 APL_OK);
    cr_assert_neq(other_team.id, first.id);

    size_t ferrari_count = 0;
    apl_season *ferrari_seasons = nullptr;
    cr_assert_eq(apl_season_repository_list(&db.connection, ferrari.id, true, &ferrari_seasons,
                                            &ferrari_count, &err),
                 APL_OK);
    cr_assert_eq(ferrari_count, 1);
    cr_assert_str_eq(ferrari_seasons[0].label, "2026");
    apl_season_repository_free_list(ferrari_seasons);

    apl_season wrong_team = {};
    cr_assert_eq(apl_season_repository_get(&db.connection, mclaren.id, first.id, &wrong_team, &err),
                 APL_ERR_NOT_FOUND);
    cr_assert_eq(apl_season_repository_update(&db.connection, mclaren.id, first.id, "Hijack", "",
                                              "", "", &wrong_team, &err),
                 APL_ERR_NOT_FOUND);
    cr_assert_eq(
        apl_season_repository_archive(&db.connection, mclaren.id, first.id, &wrong_team, &err),
        APL_ERR_NOT_FOUND);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}

Test(season_repository, reversed_dates_are_rejected_by_the_database)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);
    const apl_team team = make_team(&db, "Ferrari");

    apl_season season = {};
    cr_assert_eq(apl_season_repository_create(&db.connection, team.id, "bad", "2026-12-31",
                                              "2026-01-01", "", &season, &err),
                 APL_ERR_INVALID_ARGUMENT);

    size_t count = 1;
    apl_season *seasons = nullptr;
    cr_assert_eq(apl_season_repository_list(&db.connection, team.id, true, &seasons, &count, &err),
                 APL_OK);
    cr_assert_eq(count, 0);
    apl_season_repository_free_list(seasons);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}
