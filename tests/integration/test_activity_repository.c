#include <stdio.h>
#include <string.h>

#include <criterion/criterion.h>

#include "db/activity_repository.h"
#include "db/team_repository.h"
#include "db/transaction.h"
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

static apl_activity_event make_event(const int64_t team_id, const char *const action,
                                     const char *const entity_type)
{
    apl_activity_event event = {};
    event.team_id = team_id;
    event.entity_id = team_id;
    (void)snprintf(event.action, sizeof(event.action), "%s", action);
    (void)snprintf(event.entity_type, sizeof(event.entity_type), "%s", entity_type);
    (void)snprintf(event.summary, sizeof(event.summary), "Summary for %s", action);
    return event;
}

Test(activity_repository, migration_seeds_no_activity)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);

    apl_activity_event *events = nullptr;
    size_t count = 1;
    cr_assert_eq(apl_activity_repository_list(&db.connection, nullptr, &events, &count, &err),
                 APL_OK);
    cr_assert_null(events);
    cr_assert_eq(count, 0);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}

Test(activity_repository, append_stores_event_and_field_changes_atomically)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);
    const apl_team team = make_team(&db, "Ferrari");

    apl_activity_event input = make_event(team.id, "team_created", "team");
    apl_activity_change changes[2] = {};
    (void)snprintf(changes[0].field, sizeof(changes[0].field), "name");
    changes[0].old_missing = true;
    (void)snprintf(changes[0].new_value, sizeof(changes[0].new_value), "Ferrari");
    (void)snprintf(changes[1].field, sizeof(changes[1].field), "notes");
    changes[1].old_missing = true;
    (void)snprintf(changes[1].new_value, sizeof(changes[1].new_value), "Maranello");

    apl_activity_event stored = {};
    cr_assert_eq(apl_activity_repository_append(&db.connection, &input, changes, 2, &stored, &err),
                 APL_OK);
    cr_assert_gt(stored.id, 0);
    cr_assert_eq(stored.team_id, team.id);
    cr_assert_str_eq(stored.action, "team_created");
    cr_assert_str_eq(stored.entity_type, "team");
    cr_assert_neq(stored.occurred_at[0], '\0');

    apl_activity_event fetched = {};
    cr_assert_eq(apl_activity_repository_get(&db.connection, stored.id, &fetched, &err), APL_OK);
    cr_assert_eq(fetched.id, stored.id);
    cr_assert_str_eq(fetched.summary, "Summary for team_created");

    apl_activity_change *loaded = nullptr;
    size_t loaded_count = 0;
    cr_assert_eq(apl_activity_repository_list_changes(&db.connection, stored.id, &loaded,
                                                      &loaded_count, &err),
                 APL_OK);
    cr_assert_eq(loaded_count, 2);
    cr_assert_str_eq(loaded[0].field, "name");
    cr_assert(loaded[0].old_missing);
    cr_assert_str_eq(loaded[0].new_value, "Ferrari");
    cr_assert_str_eq(loaded[1].field, "notes");
    apl_activity_repository_free_changes(loaded);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}

Test(activity_repository, list_orders_newest_first_and_filters_by_team_action_entity_and_date)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);
    const apl_team ferrari = make_team(&db, "Ferrari");
    const apl_team mclaren = make_team(&db, "McLaren");

    const apl_activity_event first = make_event(ferrari.id, "team_created", "team");
    cr_assert_eq(apl_activity_repository_append(&db.connection, &first, nullptr, 0, nullptr, &err),
                 APL_OK);
    const apl_activity_event second = make_event(mclaren.id, "team_created", "team");
    cr_assert_eq(apl_activity_repository_append(&db.connection, &second, nullptr, 0, nullptr, &err),
                 APL_OK);
    const apl_activity_event third = make_event(ferrari.id, "season_created", "season");
    cr_assert_eq(apl_activity_repository_append(&db.connection, &third, nullptr, 0, nullptr, &err),
                 APL_OK);

    apl_activity_filter filter = {};
    filter.team_id = ferrari.id;
    apl_activity_event *events = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_activity_repository_list(&db.connection, &filter, &events, &count, &err),
                 APL_OK);
    cr_assert_eq(count, 2);
    cr_assert_eq(events[0].id > events[1].id, true);
    cr_assert_str_eq(events[0].action, "season_created");
    apl_activity_repository_free_events(events);

    filter = (apl_activity_filter){.team_id = ferrari.id};
    (void)snprintf(filter.action, sizeof(filter.action), "team_created");
    cr_assert_eq(apl_activity_repository_list(&db.connection, &filter, &events, &count, &err),
                 APL_OK);
    cr_assert_eq(count, 1);
    cr_assert_eq(events[0].team_id, ferrari.id);
    apl_activity_repository_free_events(events);

    filter = (apl_activity_filter){};
    (void)snprintf(filter.entity_type, sizeof(filter.entity_type), "season");
    cr_assert_eq(apl_activity_repository_list(&db.connection, &filter, &events, &count, &err),
                 APL_OK);
    cr_assert_eq(count, 1);
    apl_activity_repository_free_events(events);

    filter = (apl_activity_filter){};
    (void)snprintf(filter.to_date, sizeof(filter.to_date), "1970-01-01");
    cr_assert_eq(apl_activity_repository_list(&db.connection, &filter, &events, &count, &err),
                 APL_OK);
    cr_assert_eq(count, 0);
    apl_activity_repository_free_events(events);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}

Test(activity_repository, failed_change_write_can_be_rolled_back_with_its_event)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);
    const apl_team team = make_team(&db, "Ferrari");

    apl_db_transaction transaction = {};
    cr_assert_eq(apl_db_transaction_begin(&transaction, &db.connection, &err), APL_OK);

    const apl_activity_event input = make_event(team.id, "team_created", "team");
    apl_activity_change invalid = {};
    invalid.new_missing = true;
    cr_assert_eq(apl_activity_repository_append(&db.connection, &input, &invalid, 1, nullptr, &err),
                 APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_db_transaction_rollback(&transaction, &err), APL_OK);

    apl_activity_event *events = nullptr;
    size_t count = 1;
    cr_assert_eq(apl_activity_repository_list(&db.connection, nullptr, &events, &count, &err),
                 APL_OK);
    cr_assert_eq(count, 0);
    apl_activity_repository_free_events(events);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}
