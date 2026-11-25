#include <stdint.h>
#include <stdlib.h>

#include <criterion/criterion.h>

#include "core/util.h"

Test(util, count_of_counts_array_elements)
{
    const int values[] = {1, 2, 3};

    cr_assert_eq(APL_COUNT_OF(values), 3);
}

Test(util, min_and_max_helpers_choose_the_expected_value)
{
    cr_assert_eq(apl_min_unsigned(4U, 9U), 4U);
    cr_assert_eq(apl_max_unsigned(4U, 9U), 9U);
    cr_assert_eq(apl_min_size(4U, 9U), 4U);
    cr_assert_eq(apl_max_size(4U, 9U), 9U);
}

Test(util, grow_capacity_starts_and_doubles)
{
    size_t next = 0;

    cr_assert(apl_grow_capacity(0, 8, &next));
    cr_assert_eq(next, 8);
    cr_assert(apl_grow_capacity(8, 8, &next));
    cr_assert_eq(next, 16);
}

Test(util, grow_capacity_rejects_overflow)
{
    size_t next = 0;

    cr_assert_not(apl_grow_capacity(SIZE_MAX / 2 + 1, 8, &next));
}

Test(util, grow_array_allocates_and_updates_capacity)
{
    size_t capacity = 0;
    int *items = nullptr;

    items = apl_grow_array(items, &capacity, 4, sizeof(*items));
    cr_assert_not_null(items);
    cr_assert_eq(capacity, 4);

    int *const grown = apl_grow_array(items, &capacity, 4, sizeof(*items));
    cr_assert_not_null(grown);
    cr_assert_eq(capacity, 8);

    free(grown);
}
