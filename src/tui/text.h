#ifndef APL_TUI_TEXT_H
#define APL_TUI_TEXT_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Small text helpers shared by the terminal-free TUI state models.
 *
 * These functions never allocate and always leave a NUL-terminated result.
 * UTF-8 is treated as an opaque byte stream except where a helper explicitly
 * avoids splitting a multi-byte sequence.
 */

/** Append `source` to `destination` when it fits completely.
 *
 * Returns the resulting length. A partial append is never performed; the
 * destination is unchanged when the value would overflow. */
size_t apl_tui_text_append(char *destination, size_t capacity, const char *source);

/** Remove the last UTF-8 code point from a NUL-terminated string.
 *
 * Returns the resulting length. Invalid trailing bytes are removed one byte at
 * a time so the value stays NUL-terminated. */
size_t apl_tui_text_backspace(char *value);

/** Truncate `value` in place to at most `max_bytes` while keeping complete
 *  UTF-8 sequences. Returns the resulting length. */
size_t apl_tui_text_truncate_utf8(char *value, size_t max_bytes);

/** True when both strings are equal ignoring ASCII case. */
bool apl_tui_text_equal_ci(const char *left, const char *right);

/** True when `text` starts with `prefix`, ignoring ASCII case. */
bool apl_tui_text_starts_with_ci(const char *text, const char *prefix);

#endif /* APL_TUI_TEXT_H */
