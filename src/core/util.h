#ifndef APL_CORE_UTIL_H
#define APL_CORE_UTIL_H

#include <stddef.h>

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

#endif /* APL_CORE_UTIL_H */
