#ifndef APL_CORE_UTIL_H
#define APL_CORE_UTIL_H

#include <stdckdint.h>
#include <stddef.h>
#include <stdlib.h>

/*
 * Small shared helpers.
 *
 * `APL_COUNT_OF` must be given a real array, not a pointer; a pointer would
 * produce a meaningless count. The min/max helpers are deliberately per-type:
 * they avoid the multiple-evaluation and signedness pitfalls of a generic
 * macro.
 */

/** Number of elements in an array. Callers must pass a real array, not a
 *  pointer. */
#define APL_COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

/** Smaller/larger of two unsigned values. */
static inline unsigned apl_min_unsigned(const unsigned left, const unsigned right)
{
    return (left < right) ? left : right;
}

static inline unsigned apl_max_unsigned(const unsigned left, const unsigned right)
{
    return (left > right) ? left : right;
}

/** Smaller/larger of two size_t values. */
static inline size_t apl_min_size(const size_t left, const size_t right)
{
    return (left < right) ? left : right;
}

static inline size_t apl_max_size(const size_t left, const size_t right)
{
    return (left > right) ? left : right;
}

/** Compute the next capacity: `initial` from empty, otherwise double.
 *
 * Returns false instead of wrapping when doubling would overflow `size_t`. */
static inline bool apl_grow_capacity(const size_t current, const size_t initial,
                                     size_t *const out_next)
{
    size_t next = initial;
    if (current != 0 && ckd_mul(&next, current, 2)) {
        return false;
    }
    *out_next = next;
    return true;
}

/** Grow an allocated array, updating `capacity` on success.
 *
 * Starts at `initial_capacity` items and doubles from there. Returns the
 * reallocated array, or nullptr when the capacity or byte size would overflow
 * or allocation fails. On failure the caller's original array and capacity
 * remain unchanged. */
static inline void *apl_grow_array(void *const items, size_t *const capacity,
                                   const size_t initial_capacity, const size_t item_size)
{
    if (capacity == nullptr) {
        return nullptr;
    }
    size_t next = 0;
    if (!apl_grow_capacity(*capacity, initial_capacity, &next)) {
        return nullptr;
    }
    size_t bytes = 0;
    if (ckd_mul(&bytes, next, item_size)) {
        return nullptr;
    }
    void *const grown = realloc(items, bytes);
    if (grown == nullptr) {
        return nullptr;
    }
    *capacity = next;
    return grown;
}

#endif /* APL_CORE_UTIL_H */
