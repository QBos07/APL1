#include "db/migrations.h"

/*
 * Compiled-in migration list for apl1.
 *
 * Feature schemas are added here as ordered migrations, each registered with
 * the feature work that needs it. The list must stay contiguous and ascending
 * from version 1 (the runner rejects duplicates and gaps).
 */

static const apl_db_migration apl_migrations[] = {
    /* Version 1 is the empty schema baseline: it intentionally creates no
     * domain tables and inserts no data. Teams, drivers, seasons, events,
     * financial records, and other domain schemas arrive as later versions. */
    {1, "-- apl1 initial schema baseline (version 1): empty by design.\n"
        "-- Domain schemas are introduced as later, ordered migrations.\n"},
};

const apl_db_migration *apl_db_migrations_list(size_t *const out_count)
{
    if (out_count != nullptr) {
        *out_count = sizeof(apl_migrations) / sizeof(apl_migrations[0]);
    }
    return apl_migrations;
}
