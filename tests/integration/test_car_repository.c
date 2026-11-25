
#include <criterion/criterion.h>

#include "db/car_repository.h"
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

Test(car_repository, create_update_archive_and_optional_fields)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);
    const apl_team team = make_team(&db, "Ferrari");

    apl_car *empty = nullptr;
    size_t empty_count = 1;
    cr_assert_eq(
        apl_car_repository_list(&db.connection, team.id, false, &empty, &empty_count, &err),
        APL_OK);
    cr_assert_eq(empty_count, 0);

    apl_car created = {};
    cr_assert_eq(
        apl_car_repository_create(&db.connection, team.id, "SF-26", "", "", &created, &err),
        APL_OK);
    cr_assert_str_eq(created.label, "SF-26");
    cr_assert_str_eq(created.chassis, "");
    cr_assert_str_eq(created.notes, "");

    apl_car updated = {};
    cr_assert_eq(apl_car_repository_update(&db.connection, team.id, created.id, "SF-26B", "CH-001",
                                           "Spare monocoque", &updated, &err),
                 APL_OK);
    cr_assert_eq(updated.id, created.id);
    cr_assert_str_eq(updated.label, "SF-26B");
    cr_assert_str_eq(updated.chassis, "CH-001");
    cr_assert_str_eq(updated.notes, "Spare monocoque");

    apl_car archived = {};
    cr_assert_eq(apl_car_repository_archive(&db.connection, team.id, created.id, &archived, &err),
                 APL_OK);
    cr_assert(archived.meta.archived);
    cr_assert_neq(archived.meta.archived_at[0], '\0');

    size_t count = 1;
    apl_car *cars = nullptr;
    cr_assert_eq(apl_car_repository_list(&db.connection, team.id, false, &cars, &count, &err),
                 APL_OK);
    cr_assert_eq(count, 0);
    apl_car_repository_free_list(cars);

    cr_assert_eq(apl_car_repository_list(&db.connection, team.id, true, &cars, &count, &err),
                 APL_OK);
    cr_assert_eq(count, 1);
    cr_assert(cars[0].meta.archived);
    apl_car_repository_free_list(cars);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}

Test(car_repository, duplicate_labels_are_case_insensitive_and_team_isolated)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);
    const apl_team ferrari = make_team(&db, "Ferrari");
    const apl_team mclaren = make_team(&db, "McLaren");

    apl_car first = {};
    cr_assert_eq(
        apl_car_repository_create(&db.connection, ferrari.id, "Car 1", "", "", &first, &err),
        APL_OK);
    apl_car duplicate = {};
    cr_assert_eq(
        apl_car_repository_create(&db.connection, ferrari.id, "car 1", "", "", &duplicate, &err),
        APL_ERR_CONFLICT);

    apl_car other = {};
    cr_assert_eq(
        apl_car_repository_create(&db.connection, mclaren.id, "Car 1", "", "", &other, &err),
        APL_OK);

    apl_car hidden = {};
    cr_assert_eq(apl_car_repository_get(&db.connection, mclaren.id, first.id, &hidden, &err),
                 APL_ERR_NOT_FOUND);
    cr_assert_eq(apl_car_repository_update(&db.connection, mclaren.id, first.id, "Hijack", "", "",
                                           &hidden, &err),
                 APL_ERR_NOT_FOUND);

    size_t count = 0;
    apl_car *cars = nullptr;
    cr_assert_eq(apl_car_repository_list(&db.connection, ferrari.id, true, &cars, &count, &err),
                 APL_OK);
    cr_assert_eq(count, 1);
    cr_assert_str_eq(cars[0].label, "Car 1");
    apl_car_repository_free_list(cars);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}
