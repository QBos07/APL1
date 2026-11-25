
#include <criterion/criterion.h>

#include "db/driver_repository.h"
#include "db/membership_repository.h"
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

static apl_driver make_driver(apl_test_db *const db, const char *const name)
{
    apl_error err;
    apl_error_init(&err);
    apl_driver driver = {};
    cr_assert_eq(apl_driver_repository_create(&db->connection, name, "", "", &driver, &err), APL_OK,
                 "driver create failed: %s", apl_error_message(&err));
    return driver;
}

Test(driver_repository, duplicate_names_remain_distinct_and_archive_round_trips)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);

    const apl_driver first = make_driver(&db, "Alex Driver");
    const apl_driver second = make_driver(&db, "Alex Driver");
    cr_assert_neq(first.id, second.id);
    cr_assert_str_eq(first.name, second.name);

    apl_driver updated = {};
    cr_assert_eq(apl_driver_repository_update(&db.connection, second.id, "Alex Driver Jr", "ADJ",
                                              "notes", &updated, &err),
                 APL_OK);
    cr_assert_eq(updated.id, second.id);
    cr_assert_str_eq(updated.name, "Alex Driver Jr");

    apl_driver archived = {};
    cr_assert_eq(apl_driver_repository_archive(&db.connection, first.id, &archived, &err), APL_OK);
    cr_assert(archived.meta.archived);

    size_t count = 1;
    apl_driver *drivers = nullptr;
    cr_assert_eq(apl_driver_repository_list(&db.connection, false, &drivers, &count, &err), APL_OK);
    cr_assert_eq(count, 1);
    cr_assert_eq(drivers[0].id, second.id);
    apl_driver_repository_free_list(drivers);

    cr_assert_eq(apl_driver_repository_list(&db.connection, true, &drivers, &count, &err), APL_OK);
    cr_assert_eq(count, 2);
    apl_driver_repository_free_list(drivers);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}

Test(driver_repository, team_listing_uses_dated_memberships_not_a_roster_name)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);
    const apl_team ferrari = make_team(&db, "Ferrari");
    const apl_team mclaren = make_team(&db, "McLaren");
    const apl_driver ferrari_driver = make_driver(&db, "Ferrari Driver");
    const apl_driver mclaren_driver = make_driver(&db, "McLaren Driver");
    (void)make_driver(&db, "Unlinked Driver");

    apl_membership membership = {};
    cr_assert_eq(apl_membership_repository_create(&db.connection, ferrari.id, ferrari_driver.id,
                                                  "2025-01-01", "2025-06-30", "", "", &membership,
                                                  &err),
                 APL_OK);
    cr_assert_eq(apl_membership_repository_create(&db.connection, mclaren.id, mclaren_driver.id,
                                                  "2025-01-01", "", "", "", &membership, &err),
                 APL_OK);

    size_t count = 0;
    apl_driver *drivers = nullptr;
    cr_assert_eq(apl_driver_repository_list_for_team(&db.connection, ferrari.id, false, &drivers,
                                                     &count, &err),
                 APL_OK);
    cr_assert_eq(count, 1);
    cr_assert_eq(drivers[0].id, ferrari_driver.id);
    apl_driver_repository_free_list(drivers);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}

Test(membership_repository, periods_are_preserved_and_current_roster_has_no_end_date)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);
    const apl_team team = make_team(&db, "Ferrari");
    const apl_driver driver = make_driver(&db, "Driver One");

    size_t count = 1;
    apl_membership *memberships = nullptr;
    cr_assert_eq(apl_membership_repository_list_for_driver(&db.connection, driver.id, &memberships,
                                                           &count, &err),
                 APL_OK);
    cr_assert_eq(count, 0);

    apl_membership historical = {};
    cr_assert_eq(apl_membership_repository_create(&db.connection, team.id, driver.id, "2024-01-01",
                                                  "2024-12-31", "Race seat", "old", &historical,
                                                  &err),
                 APL_OK);
    apl_membership current = {};
    cr_assert_eq(apl_membership_repository_create(&db.connection, team.id, driver.id, "2025-01-01",
                                                  "", "Reserve", "", &current, &err),
                 APL_OK);
    cr_assert_neq(historical.id, current.id);

    cr_assert_eq(apl_membership_repository_list_for_team(&db.connection, team.id, false,
                                                         &memberships, &count, &err),
                 APL_OK);
    cr_assert_eq(count, 1);
    cr_assert_eq(memberships[0].id, current.id);
    cr_assert_str_eq(memberships[0].end_date, "");
    apl_membership_repository_free_list(memberships);

    cr_assert_eq(apl_membership_repository_list_for_team(&db.connection, team.id, true,
                                                         &memberships, &count, &err),
                 APL_OK);
    cr_assert_eq(count, 2);
    apl_membership_repository_free_list(memberships);

    apl_membership ended = {};
    cr_assert_eq(apl_membership_repository_end(&db.connection, team.id, current.id, "2025-06-30",
                                               &ended, &err),
                 APL_OK);
    cr_assert_str_eq(ended.end_date, "2025-06-30");
    cr_assert_eq(ended.id, current.id);

    cr_assert_eq(apl_membership_repository_list_for_team(&db.connection, team.id, false,
                                                         &memberships, &count, &err),
                 APL_OK);
    cr_assert_eq(count, 0);
    apl_membership_repository_free_list(memberships);

    apl_membership rejoined = {};
    cr_assert_eq(apl_membership_repository_create(&db.connection, team.id, driver.id, "2026-01-01",
                                                  "", "", "", &rejoined, &err),
                 APL_OK);
    cr_assert_neq(rejoined.id, current.id);
    cr_assert_neq(rejoined.id, historical.id);

    cr_assert_eq(apl_membership_repository_list_for_driver(&db.connection, driver.id, &memberships,
                                                           &count, &err),
                 APL_OK);
    cr_assert_eq(count, 3);
    cr_assert_str_eq(memberships[0].start_date, "2024-01-01");
    cr_assert_str_eq(memberships[1].start_date, "2025-01-01");
    cr_assert_str_eq(memberships[2].start_date, "2026-01-01");
    apl_membership_repository_free_list(memberships);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}

Test(membership_repository, rejects_exact_duplicates_invalid_dates_and_wrong_team_scope)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);
    const apl_team ferrari = make_team(&db, "Ferrari");
    const apl_team mclaren = make_team(&db, "McLaren");
    const apl_driver driver = make_driver(&db, "Driver One");

    apl_membership created = {};
    cr_assert_eq(apl_membership_repository_create(&db.connection, ferrari.id, driver.id,
                                                  "2025-01-01", "", "", "", &created, &err),
                 APL_OK);

    apl_membership duplicate = {};
    cr_assert_eq(apl_membership_repository_create(&db.connection, ferrari.id, driver.id,
                                                  "2025-01-01", "", "", "", &duplicate, &err),
                 APL_ERR_CONFLICT);

    apl_membership reversed = {};
    cr_assert_eq(apl_membership_repository_create(&db.connection, ferrari.id, driver.id,
                                                  "2025-06-01", "2025-01-01", "", "", &reversed,
                                                  &err),
                 APL_ERR_INVALID_ARGUMENT);

    cr_assert_eq(apl_membership_repository_create(&db.connection, ferrari.id, 999999, "2025-01-01",
                                                  "", "", "", &reversed, &err),
                 APL_ERR_NOT_FOUND);

    apl_membership hidden = {};
    cr_assert_eq(
        apl_membership_repository_get(&db.connection, mclaren.id, created.id, &hidden, &err),
        APL_ERR_NOT_FOUND);
    cr_assert_eq(apl_membership_repository_update(&db.connection, mclaren.id, created.id,
                                                  "2025-01-01", "2025-12-31", "", "", &hidden,
                                                  &err),
                 APL_ERR_NOT_FOUND);
    cr_assert_eq(apl_membership_repository_end(&db.connection, mclaren.id, created.id, "2025-12-31",
                                               &hidden, &err),
                 APL_ERR_NOT_FOUND);

    size_t count = 0;
    apl_membership *memberships = nullptr;
    cr_assert_eq(apl_membership_repository_list_for_team(&db.connection, mclaren.id, true,
                                                         &memberships, &count, &err),
                 APL_OK);
    cr_assert_eq(count, 0);
    apl_membership_repository_free_list(memberships);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}
