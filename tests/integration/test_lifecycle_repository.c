#include <stdio.h>
#include <string.h>

#include <criterion/criterion.h>

#include "db/car_repository.h"
#include "db/lifecycle_repository.h"
#include "db/part_type_repository.h"
#include "db/part_unit_repository.h"
#include "db/team_repository.h"
#include "db_fixture.h"
#include "domain/part.h"

static apl_team make_team(apl_test_db *const db, const char *const name)
{
    apl_error err;
    apl_error_init(&err);
    apl_team team = {};
    cr_assert_eq(apl_team_repository_create(&db->connection, name, "", &team, &err), APL_OK,
                 "team create failed: %s", apl_error_message(&err));
    return team;
}

static int64_t make_unit(apl_test_db *const db, const int64_t team_id, const char *const serial)
{
    apl_error err;
    apl_error_init(&err);
    apl_part_type type = {.team_id = team_id};
    (void)snprintf(type.category, sizeof(type.category), "Aero");
    (void)snprintf(type.category_key, sizeof(type.category_key), "aero");
    (void)snprintf(type.name, sizeof(type.name), "Wing %s", serial);
    (void)snprintf(type.name_key, sizeof(type.name_key), "wing %s", serial);
    apl_part_type created_type = {};
    cr_assert_eq(apl_part_type_repository_create(&db->connection, &type, &created_type, &err),
                 APL_OK);
    apl_part_unit unit = {.team_id = team_id, .type_id = created_type.id};
    (void)snprintf(unit.serial_number, sizeof(unit.serial_number), "%s", serial);
    apl_part_unit created = {};
    cr_assert_eq(apl_part_unit_repository_create(&db->connection, &unit, &created, &err), APL_OK);
    return created.id;
}

static int64_t make_car(apl_test_db *const db, const int64_t team_id, const char *const label)
{
    apl_error err;
    apl_error_init(&err);
    apl_car car = {};
    cr_assert_eq(apl_car_repository_create(&db->connection, team_id, label, "", "", &car, &err),
                 APL_OK);
    return car.id;
}

static apl_lifecycle_event install(int64_t team_id, int64_t unit_id, int64_t car_id,
                                   const char *position, const char *date)
{
    apl_lifecycle_event event = {
        .team_id = team_id, .action = APL_LIFECYCLE_INSTALL, .unit_id = unit_id, .car_id = car_id};
    (void)snprintf(event.event_date, sizeof(event.event_date), "%s", date);
    (void)snprintf(event.position, sizeof(event.position), "%s", position);
    return event;
}

static apl_status append_cmp(apl_test_db *const db, apl_lifecycle_event event)
{
    apl_error err;
    apl_error_init(&err);
    apl_lifecycle_event out = {};
    const apl_status status =
        apl_lifecycle_repository_append(&db->connection, event.team_id, &event, &out, &err);
    (void)err;
    return status;
}

Test(lifecycle_repository, derives_current_fitment_from_events)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);
    const apl_team team = make_team(&db, "Ferrari");
    const int64_t car = make_car(&db, team.id, "SF-26");
    const int64_t wing = make_unit(&db, team.id, "FW-100");
    const int64_t wing2 = make_unit(&db, team.id, "FW-200");

    /* No events -> no fitment (unknown, not "no part fitted"). */
    apl_fitment_row *rows = nullptr;
    size_t count = 1;
    cr_assert_eq(
        apl_lifecycle_repository_current_fitment(&db.connection, team.id, &rows, &count, &err),
        APL_OK);
    cr_assert_eq(count, 0);
    cr_assert_null(rows);

    /* Install wing at front -> current. */
    cr_assert_eq(append_cmp(&db, install(team.id, wing, car, "front", "2026-01-01")), APL_OK);
    cr_assert_eq(
        apl_lifecycle_repository_current_fitment(&db.connection, team.id, &rows, &count, &err),
        APL_OK);
    cr_assert_eq(count, 1);
    cr_assert_eq(rows[0].unit_id, wing);
    cr_assert_eq(rows[0].car_id, car);
    cr_assert_str_eq(rows[0].position, "front");
    apl_lifecycle_repository_free_fitment(rows);

    /* Install wing2 at same position -> replaces existing without a replace op. */
    cr_assert_eq(append_cmp(&db, install(team.id, wing2, car, "front", "2026-01-05")), APL_OK);
    cr_assert_eq(
        apl_lifecycle_repository_current_fitment(&db.connection, team.id, &rows, &count, &err),
        APL_OK);
    cr_assert_eq(count, 1);
    cr_assert_eq(rows[0].unit_id, wing2);
    apl_lifecycle_repository_free_fitment(rows);

    /* Remove wing2 -> an explicit empty slot, distinct from no event history. */
    apl_lifecycle_event remove = {
        .team_id = team.id, .action = APL_LIFECYCLE_REMOVE, .unit_id = wing2, .car_id = car};
    (void)snprintf(remove.event_date, sizeof(remove.event_date), "2026-02-01");
    (void)snprintf(remove.position, sizeof(remove.position), "front");
    cr_assert_eq(append_cmp(&db, remove), APL_OK);
    cr_assert_eq(
        apl_lifecycle_repository_current_fitment(&db.connection, team.id, &rows, &count, &err),
        APL_OK);
    cr_assert_eq(count, 1);
    cr_assert_eq(rows[0].unit_id, 0);
    cr_assert_eq(rows[0].car_id, car);
    cr_assert_str_eq(rows[0].position, "front");
    apl_lifecycle_repository_free_fitment(rows);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}

Test(lifecycle_repository, lists_chronologically_and_filters)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);
    const apl_team team = make_team(&db, "Ferrari");
    const int64_t car = make_car(&db, team.id, "SF-26");
    const int64_t wing = make_unit(&db, team.id, "FW-100");

    cr_assert_eq(append_cmp(&db, install(team.id, wing, car, "front", "2026-03-01")), APL_OK);
    apl_lifecycle_event repair = {
        .team_id = team.id, .action = APL_LIFECYCLE_REPAIR, .unit_id = wing, .car_id = car};
    (void)snprintf(repair.event_date, sizeof(repair.event_date), "2026-03-10");
    (void)snprintf(repair.position, sizeof(repair.position), "front");
    cr_assert_eq(append_cmp(&db, repair), APL_OK);

    apl_lifecycle_filter filter = {.team_id = team.id};
    apl_lifecycle_event *events = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_lifecycle_repository_list(&db.connection, &filter, &events, &count, &err),
                 APL_OK);
    cr_assert_eq(count, 2);
    cr_assert_eq(events[0].action, APL_LIFECYCLE_INSTALL);
    cr_assert_eq(events[1].action, APL_LIFECYCLE_REPAIR);
    apl_lifecycle_repository_free_events(events);

    filter.action = APL_LIFECYCLE_REPAIR;
    cr_assert_eq(apl_lifecycle_repository_list(&db.connection, &filter, &events, &count, &err),
                 APL_OK);
    cr_assert_eq(count, 1);
    cr_assert_eq(events[0].action, APL_LIFECYCLE_REPAIR);
    apl_lifecycle_repository_free_events(events);

    filter = (apl_lifecycle_filter){.team_id = team.id, .unit_id = wing};
    cr_assert_eq(apl_lifecycle_repository_list(&db.connection, &filter, &events, &count, &err),
                 APL_OK);
    cr_assert_eq(count, 2);
    apl_lifecycle_repository_free_events(events);

    apl_lifecycle_history_entry *entries = nullptr;
    cr_assert_eq(
        apl_lifecycle_repository_list_unit(&db.connection, team.id, wing, &entries, &count, &err),
        APL_OK);
    cr_assert_eq(count, 2);
    apl_lifecycle_repository_free_history(entries);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}

Test(lifecycle_repository, rejects_cross_team_and_unknown_lifecycle_arguments)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);
    const apl_team ferrari = make_team(&db, "Ferrari");
    const apl_team mclaren = make_team(&db, "McLaren");
    const int64_t wing = make_unit(&db, mclaren.id, "FW-100");

    /* A cross-team unit reference is rejected by the schema trigger. */
    apl_lifecycle_event bad = install(ferrari.id, wing, 0, "front", "2026-01-01");
    cr_assert_eq(append_cmp(&db, bad), APL_ERR_INVALID_ARGUMENT);

    /* An unknown action is rejected. */
    apl_lifecycle_event none = {.team_id = mclaren.id, .action = APL_LIFECYCLE_NONE};
    cr_assert_eq(append_cmp(&db, none), APL_ERR_INVALID_ARGUMENT);

    /* A requested event from a foreign team is not found. */
    apl_lifecycle_event stored = {};
    cr_assert_eq(append_cmp(&db, install(mclaren.id, wing, 0, "front", "2026-01-02")), APL_OK);
    cr_assert_eq(apl_lifecycle_repository_get(&db.connection, ferrari.id, stored.id, &stored, &err),
                 APL_ERR_NOT_FOUND);

    const apl_lifecycle_filter filter = {.team_id = ferrari.id};
    apl_lifecycle_event *events = nullptr;
    size_t count = 1;
    cr_assert_eq(apl_lifecycle_repository_list(&db.connection, &filter, &events, &count, &err),
                 APL_OK);
    cr_assert_eq(count, 0);
    apl_lifecycle_repository_free_events(events);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}
