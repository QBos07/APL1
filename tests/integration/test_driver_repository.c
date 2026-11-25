
#include <criterion/criterion.h>

#include "db/driver_repository.h"
#include "db_fixture.h"

Test(driver_repository, create_get_update_archive_and_duplicate_names)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);

    apl_driver first = {};
    cr_assert_eq(
        apl_driver_repository_create(&db.connection, "Alex Driver", "AD", "notes", &first, &err),
        APL_OK);
    apl_driver second = {};
    cr_assert_eq(apl_driver_repository_create(&db.connection, "Alex Driver", "", "", &second, &err),
                 APL_OK);
    cr_assert_neq(first.id, second.id);

    apl_driver fetched = {};
    cr_assert_eq(apl_driver_repository_get(&db.connection, first.id, &fetched, &err), APL_OK);
    cr_assert_str_eq(fetched.name, "Alex Driver");
    cr_assert_str_eq(fetched.short_name, "AD");

    apl_driver updated = {};
    cr_assert_eq(apl_driver_repository_update(&db.connection, first.id, "Alex Driver Jr", "", "",
                                              &updated, &err),
                 APL_OK);
    cr_assert_eq(updated.id, first.id);
    cr_assert_str_eq(updated.name, "Alex Driver Jr");
    cr_assert_str_eq(updated.created_at, fetched.created_at);

    apl_driver archived = {};
    cr_assert_eq(apl_driver_repository_archive(&db.connection, second.id, &archived, &err), APL_OK);
    cr_assert(archived.archived);

    size_t count = 1;
    apl_driver *drivers = nullptr;
    cr_assert_eq(apl_driver_repository_list(&db.connection, false, &drivers, &count, &err), APL_OK);
    cr_assert_eq(count, 1);
    cr_assert_eq(drivers[0].id, first.id);
    apl_driver_repository_free_list(drivers);

    cr_assert_eq(apl_driver_repository_get(&db.connection, 999999, &fetched, &err),
                 APL_ERR_NOT_FOUND);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}
