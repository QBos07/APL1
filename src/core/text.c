#include "core/text.h"

#include <stdio.h>

void apl_text_copy(char *const out, const size_t out_size, const char *const text)
{
    if (out == nullptr || out_size == 0) {
        return;
    }
    (void)snprintf(out, out_size, "%s", (text != nullptr) ? text : "");
}
