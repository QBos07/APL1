#include <stdio.h>

#include <criterion/criterion.h>

#include "db/part_type_repository.h"
#include "db/part_unit_repository.h"
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

static apl_part_type make_type(const int64_t team_id, const char *const name)
{
    apl_part_type type = {.team_id = team_id};
    (void)snprintf(type.category, sizeof(type.category), "Aero");
    (void)snprintf(type.category_key, sizeof(type.category_key), "aero");
    (void)snprintf(type.name, sizeof(type.name), "%s", name);
    (void)snprintf(type.name_key, sizeof(type.name_key), "%s", name);
    return type;
}

static int64_t create_type(apl_test_db *const db, const int64_t team_id, const char *const name)
{
    apl_error err;
    apl_error_init(&err);
    apl_part_type type = make_type(team_id, name);
    apl_part_type created = {};
    cr_assert_eq(apl_part_type_repository_create(&db->connection, &type, &created, &err), APL_OK);
    return created.id;
}

Test(part_unit_repository, lists_by_serial_and_scopes_by_type)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);
    const apl_team team = make_team(&db, "Ferrari");
    const int64_t front = create_type(&db, team.id, "Front Wing");
    const int64_t rear = create_type(&db, team.id, "Rear Wing");

    const char *serials[] = {"FW-003", "FW-001", "FW-002"};
    for (size_t index = 0; index < 3; ++index) {
        apl_part_unit unit = {.team_id = team.id, .type_id = front};
        (void)snprintf(unit.serial_number, sizeof(unit.serial_number), "%s", serials[index]);
        apl_part_unit created = {};
        cr_assert_eq(apl_part_unit_repository_create(&db.connection, &unit, &created, &err),
                     APL_OK);
    }
    apl_part_unit rear_unit = {.team_id = team.id, .type_id = rear};
    (void)snprintf(rear_unit.serial_number, sizeof(rear_unit.serial_number), "RW-001");
    apl_part_unit created_rear = {};
    cr_assert_eq(
        apl_part_unit_repository_create(&db.connection, &rear_unit, &created_rear, &err), APL_OK);

    const apl_part_unit_filter all = {.team_id = team.id};
    apl_part_unit *units = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_part_unit_repository_list(&db.connection, &all, &units, &count, &err), APL_OK);
    cr_assert_eq(count, 4);
    cr_assert_str_eq(units[0].serial_number, "FW-001");
    cr_assert_str_eq(units[1].serial_number, "FW-002");
    cr_assert_str_eq(units[2].serial_number, "FW-003");
    cr_assert_str_eq(units[3].serial_number, "RW-001");
    apl_part_unit_repository_free_list(units);

    const apl_part_unit_filter type_filter = {.team_id = team.id, .type_id = front};
    cr_assert_eq(apl_part_unit_repository_list(&db.connection, &type_filter, &units, &count, &err),
                 APL_OK);
    cr_assert_eq(count, 3);
    apl_part_unit_repository_free_list(units);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}

Test(part_unit_repository, rejects_cross_team_type_via_trigger)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);
    const apl_team ferrari = make_team(&db, "Ferrari");
    const apl_team mclaren = make_team(&db, "McLaren");
    const int64_t mclaren_type = create_type(&db, mclaren.id, "Front Wing");

    apl_part_unit unit = {.team_id = ferrari.id, .type_id = mclaren_type};
    (void)snprintf(unit.serial_number, sizeof(unit.serial_number), "FW-001");
    apl_part_unit created = {};
    cr_assert_eq(apl_part_unit_repository_create(&db.connection, &unit, &created, &err),
                 APL_ERR_INVALID_ARGUMENT);

    const apl_part_unit_filter filter = {.team_id = ferrari.id};
    apl_part_unit *units = nullptr;
    size_t count = 1;
    cr_assert_eq(apl_part_unit_repository_list(&db.connection, &filter, &units, &count, &err),
                 APL_OK);
    cr_assert_eq(count, 0);
    apl_part_unit_repository_free_list(units);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}