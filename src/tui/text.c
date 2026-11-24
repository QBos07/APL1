#include "tui/text.h"

#include <string.h>

static char ascii_lower(const char value)
{
    return (value >= 'A' && value <= 'Z') ? (char)(value - 'A' + 'a') : value;
}

size_t apl_tui_text_append(char *const destination, const size_t capacity, const char *const source)
{
    if (destination == nullptr || capacity == 0) {
        return 0;
    }
    const size_t length = strlen(destination);
    if (source == nullptr) {
        return length;
    }
    const size_t addition = strlen(source);
    if (length + addition >= capacity) {
        return length;
    }
    memcpy(destination + length, source, addition + 1);
    return length + addition;
}

size_t apl_tui_text_backspace(char *const value)
{
    if (value == nullptr) {
        return 0;
    }
    size_t length = strlen(value);
    if (length == 0) {
        return 0;
    }
    --length;
    while (length > 0 && ((unsigned char)value[length] & 0xC0U) == 0x80U) {
        --length;
    }
    value[length] = '\0';
    return length;
}

size_t apl_tui_text_truncate_utf8(char *const value, size_t max_bytes)
{
    if (value == nullptr) {
        return 0;
    }
    size_t length = strlen(value);
    if (length <= max_bytes) {
        return length;
    }

    /* Back up to the start of the code point that would be cut, then drop the
     * whole sequence. A leading byte never starts with the 10xxxxxx mask. */
    size_t cut = max_bytes;
    while (cut > 0 && ((unsigned char)value[cut] & 0xC0U) == 0x80U) {
        --cut;
    }
    value[cut] = '\0';
    return cut;
}

bool apl_tui_text_equal_ci(const char *const left, const char *const right)
{
    if (left == nullptr || right == nullptr) {
        return left == right;
    }
    const char *left_cursor = left;
    const char *right_cursor = right;
    while (*left_cursor != '\0' && *right_cursor != '\0') {
        if (ascii_lower(*left_cursor) != ascii_lower(*right_cursor)) {
            return false;
        }
        ++left_cursor;
        ++right_cursor;
    }
    return *left_cursor == '\0' && *right_cursor == '\0';
}

bool apl_tui_text_starts_with_ci(const char *const text, const char *const prefix)
{
    if (text == nullptr || prefix == nullptr) {
        return false;
    }
    const char *text_cursor = text;
    const char *prefix_cursor = prefix;
    while (*prefix_cursor != '\0') {
        if (*text_cursor == '\0' || ascii_lower(*text_cursor) != ascii_lower(*prefix_cursor)) {
            return false;
        }
        ++text_cursor;
        ++prefix_cursor;
    }
    return true;
}
