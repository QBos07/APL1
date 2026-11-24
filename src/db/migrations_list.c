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
    /* Version 2: team workspaces. No team rows are seeded. The active-team
     * setting is a later migration (2.9) so this slice owns only team data. */
    {2, "CREATE TABLE teams ("
        "id INTEGER PRIMARY KEY,"
        "name TEXT NOT NULL COLLATE NOCASE UNIQUE,"
        "notes TEXT NOT NULL DEFAULT '',"
        "archived INTEGER NOT NULL DEFAULT 0 CHECK (archived IN (0, 1)),"
        "created_at TEXT NOT NULL,"
        "updated_at TEXT NOT NULL,"
        "archived_at TEXT,"
        "CHECK (length(trim(name)) > 0)"
        ");\n"},
    /* Version 3: the persisted active-team setting. The singleton row starts
     * with no selection; no team or sample domain data is created. */
    {3, "CREATE TABLE app_settings ("
        "id INTEGER PRIMARY KEY CHECK (id = 1),"
        "active_team_id INTEGER REFERENCES teams(id) ON DELETE SET NULL"
        ");\n"
        "INSERT INTO app_settings (id, active_team_id) VALUES (1, NULL);\n"},
};

const apl_db_migration *apl_db_migrations_list(size_t *const out_count)
{
    if (out_count != nullptr) {
        *out_count = sizeof(apl_migrations) / sizeof(apl_migrations[0]);
    }
    return apl_migrations;
}
