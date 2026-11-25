#ifndef APL_DB_SQLITE_UTIL_H
#define APL_DB_SQLITE_UTIL_H

#include <sqlite3.h>
#include <stddef.h>

/** Copy a SQLite text column into a fixed-size buffer.
 *
 * SQL NULL becomes an empty string. The result is always NUL-terminated and
 * truncated to fit. */
void apl_db_copy_column_text(sqlite3_stmt *statement, int column, char *out, size_t out_size);

#endif /* APL_DB_SQLITE_UTIL_H */
