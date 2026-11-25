#ifndef APL_CORE_TEXT_H
#define APL_CORE_TEXT_H

#include <stddef.h>

/** Copy `text` into a fixed-size buffer.
 *
 * A null source is treated as an empty string. The result is always
 * NUL-terminated and truncated to fit. A null/zero-size destination is a
 * no-op. */
void apl_text_copy(char *out, size_t out_size, const char *text);

#endif /* APL_CORE_TEXT_H */
