#include <stdio.h>
#include <ctype.h>

#include <criterion/criterion.h>

#include "db/part_type_repository.h"
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

static void lower_copy(char *const out, const size_t out_size, const char *const text)
{
    size_t index = 0;
    while (text[index] != '\0' && index + 1 < out_size) {
        out[index] = (char)tolower((unsigned char)text[index]);
        ++index;
    }
    out[index] = '\0';
}

static apl_part_type make_type(const int64_t team_id, const char *const category,
                               const char *const name)
{
    apl_part_type type = {.team_id = team_id};
    (void)snprintf(type.category, sizeof(type.category), "%s", category);
    lower_copy(type.category_key, sizeof(type.category_key), category);
    (void)snprintf(type.name, sizeof(type.name), "%s", name);
    lower_copy(type.name_key, sizeof(type.name_key), name);
    return type;
}

Test(part_type_repository, creates_reads_updates_and_archives_without_seeding)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);
    const apl_team team = make_team(&db, "Ferrari");

    const apl_part_type_filter filter = {.team_id = team.id};
    apl_part_type *types = nullptr;
    size_t count = 1;
    cr_assert_eq(apl_part_type_repository_list(&db.connection, &filter, &types, &count, &err),
                 APL_OK);
    cr_assert_eq(count, 0);
    cr_assert_null(types);

    apl_part_type draft = make_type(team.id, "  Aero  ", "  Front Wing  ");
    (void)snprintf(draft.description, sizeof(draft.description), "Nose condition");
    (void)snprintf(draft.notes, sizeof(draft.notes), "Used set");

    apl_part_type created = {};
    cr_assert_eq(apl_part_type_repository_create(&db.connection, &draft, &created, &err), APL_OK,
                 "part type create failed: %s", apl_error_message(&err));
    cr_assert_gt(created.id, 0);
    cr_assert_eq(created.team_id, team.id);
    cr_assert_str_eq(created.category, "  Aero  ");
    cr_assert_str_eq(created.name, "  Front Wing  ");
    cr_assert_not(created.meta.archived);
    cr_assert_neq(created.meta.created_at[0], '\0');

    apl_part_type fetched = {};
    cr_assert_eq(apl_part_type_repository_get(&db.connection, team.id, created.id, &fetched, &err),
                 APL_OK);
    cr_assert_str_eq(fetched.name, created.name);

    apl_part_type changed = fetched;
    (void)snprintf(changed.name, sizeof(changed.name), "Rear Wing");
    (void)snprintf(changed.name_key, sizeof(changed.name_key), "rear wing");
    (void)snprintf(changed.category, sizeof(changed.category), "Aero");
    (void)snprintf(changed.category_key, sizeof(changed.category_key), "aero");
    apl_part_type updated = {};
    cr_assert_eq(apl_part_type_repository_update(&db.connection, &changed, &updated, &err), APL_OK);
    cr_assert_eq(updated.id, created.id);
    cr_assert_str_eq(updated.name, "Rear Wing");

    apl_part_type archived = {};
    cr_assert_eq(
        apl_part_type_repository_archive(&db.connection, team.id, created.id, &archived, &err),
        APL_OK);
    cr_assert(archived.meta.archived);
    cr_assert_neq(archived.meta.archived_at[0], '\0');

    cr_assert_eq(apl_part_type_repository_list(&db.connection, &filter, &types, &count, &err),
                 APL_OK);
    cr_assert_eq(count, 0);
    apl_part_type_repository_free_list(types);

    apl_part_type_filter archived_filter = {.team_id = team.id, .include_archived = true};
    cr_assert_eq(apl_part_type_repository_list(&db.connection, &archived_filter, &types, &count,
                                               &err),
                 APL_OK);
    cr_assert_eq(count, 1);
    cr_assert_str_eq(types[0].name, "Rear Wing");
    apl_part_type_repository_free_list(types);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}

Test(part_type_repository, enforces_duplicate_name_within_team_and_category)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);
    const apl_team team = make_team(&db, "Ferrari");

    apl_part_type first = make_type(team.id, "Aero", "Front Wing");
    apl_part_type a = {};
    cr_assert_eq(apl_part_type_repository_create(&db.connection, &first, &a, &err), APL_OK);

    /* Exact duplicate in same team/category is rejected. */
    apl_part_type dup = make_type(team.id, "Aero", "front wing");
    apl_part_type rejected = {};
    cr_assert_eq(apl_part_type_repository_create(&db.connection, &dup, &rejected, &err),
                 APL_ERR_CONFLICT);

    /* Same name in a different category is allowed. */
    apl_part_type other_cat = make_type(team.id, "Aero Fittings", "Front Wing");
    apl_part_type ok = {};
    cr_assert_eq(apl_part_type_repository_create(&db.connection, &other_cat, &ok, &err), APL_OK);

    /* Archiving frees the first name without erasing history. */
    apl_part_type archived = {};
    cr_assert_eq(
        apl_part_type_repository_archive(&db.connection, team.id, a.id, &archived, &err), APL_OK);
    apl_part_type reuse = make_type(team.id, "Aero", "Front Wing");
    apl_part_type created = {};
    cr_assert_eq(apl_part_type_repository_create(&db.connection, &reuse, &created, &err), APL_OK);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}

Test(part_type_repository, rejects_cross_team_and_hides_other_team_rows)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);
    const apl_team ferrari = make_team(&db, "Ferrari");
    const apl_team mclaren = make_team(&db, "McLaren");

    apl_part_type own = make_type(ferrari.id, "Aero", "Front Wing");
    apl_part_type created = {};
    cr_assert_eq(apl_part_type_repository_create(&db.connection, &own, &created, &err), APL_OK);

    apl_part_type hidden = {};
    cr_assert_eq(
        apl_part_type_repository_get(&db.connection, mclaren.id, created.id, &hidden, &err),
        APL_ERR_NOT_FOUND);
    apl_part_type foreign_update = created;
    foreign_update.team_id = mclaren.id;
    cr_assert_eq(apl_part_type_repository_update(&db.connection, &foreign_update, &hidden, &err),
                 APL_ERR_NOT_FOUND);
    cr_assert_eq(
        apl_part_type_repository_archive(&db.connection, mclaren.id, created.id, &hidden, &err),
        APL_ERR_NOT_FOUND);

    const apl_part_type_filter filter = {.team_id = mclaren.id};
    apl_part_type *types = nullptr;
    size_t count = 1;
    cr_assert_eq(apl_part_type_repository_list(&db.connection, &filter, &types, &count, &err),
                 APL_OK);
    cr_assert_eq(count, 0);
    apl_part_type_repository_free_list(types);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}

Test(part_type_repository, orders_by_category_then_name)
{
    apl_test_db db = {};
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_db_open(&db, &err), APL_OK);
    const apl_team team = make_team(&db, "Ferrari");

    const char *pairs[][2] = {{"Aero", "Rear Wing"}, {"Aero", "Front Wing"}, {"Engine", "PU"}};
    for (size_t index = 0; index < 3; ++index) {
        apl_part_type type = make_type(team.id, pairs[index][0], pairs[index][1]);
        apl_part_type created = {};
        cr_assert_eq(apl_part_type_repository_create(&db.connection, &type, &created, &err),
                     APL_OK);
    }

    const apl_part_type_filter all = {.team_id = team.id};
    apl_part_type *types = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_part_type_repository_list(&db.connection, &all, &types, &count, &err), APL_OK);
    cr_assert_eq(count, 3);
    cr_assert_str_eq(types[0].name, "Front Wing");
    cr_assert_str_eq(types[1].name, "Rear Wing");
    cr_assert_str_eq(types[2].name, "PU");
    apl_part_type_repository_free_list(types);

    apl_part_type_filter category = {.team_id = team.id};
    (void)snprintf(category.category_key, sizeof(category.category_key), "aero");
    cr_assert_eq(
        apl_part_type_repository_list(&db.connection, &category, &types, &count, &err), APL_OK);
    cr_assert_eq(count, 2);
    apl_part_type_repository_free_list(types);

    cr_assert_eq(apl_test_db_close(&db, &err), APL_OK);
}