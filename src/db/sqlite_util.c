#include "db/sqlite_util.h"

#include "core/text.h"

void apl_db_copy_column_text(sqlite3_stmt *const statement, const int column, char *const out,
                             const size_t out_size)
{
    const unsigned char *const text = sqlite3_column_text(statement, column);
    apl_text_copy(out, out_size, (const char *)text);
}
