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
    /* Version 4: user-created seasons/periods (3.1). No rows are seeded and no
     * calendar year is assumed. Optional dates are stored as NULL. */
    {4,
     "CREATE TABLE seasons ("
     "id INTEGER PRIMARY KEY,"
     "team_id INTEGER NOT NULL REFERENCES teams(id) ON DELETE CASCADE,"
     "label TEXT NOT NULL COLLATE NOCASE,"
     "start_date TEXT,"
     "end_date TEXT,"
     "notes TEXT NOT NULL DEFAULT '',"
     "archived INTEGER NOT NULL DEFAULT 0 CHECK (archived IN (0, 1)),"
     "created_at TEXT NOT NULL,"
     "updated_at TEXT NOT NULL,"
     "archived_at TEXT,"
     "CHECK (length(trim(label)) > 0),"
     "CHECK (start_date IS NULL OR start_date GLOB '[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]'),"
     "CHECK (end_date IS NULL OR end_date GLOB '[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]'),"
     "CHECK (start_date IS NULL OR end_date IS NULL OR start_date <= end_date),"
     "UNIQUE (team_id, label)"
     ");\n"
     "CREATE INDEX idx_seasons_team_archived ON seasons(team_id, archived);\n"},
    /* Version 5: shared driver identities (3.2). Duplicate display names are
     * intentionally allowed; duplicates are not identities. No drivers seeded. */
    {5, "CREATE TABLE drivers ("
        "id INTEGER PRIMARY KEY,"
        "name TEXT NOT NULL COLLATE NOCASE,"
        "short_name TEXT NOT NULL DEFAULT '',"
        "notes TEXT NOT NULL DEFAULT '',"
        "archived INTEGER NOT NULL DEFAULT 0 CHECK (archived IN (0, 1)),"
        "created_at TEXT NOT NULL,"
        "updated_at TEXT NOT NULL,"
        "archived_at TEXT,"
        "CHECK (length(trim(name)) > 0)"
        ");\n"
        "CREATE INDEX idx_drivers_name ON drivers(name COLLATE NOCASE);\n"},
    /* Version 6: dated team-driver membership periods (3.3). A rejoining driver
     * gets a new period. The expression index rejects only an exact duplicate,
     * including rows whose end date is NULL. */
    {6, "CREATE TABLE team_driver_memberships ("
        "id INTEGER PRIMARY KEY,"
        "team_id INTEGER NOT NULL REFERENCES teams(id) ON DELETE CASCADE,"
        "driver_id INTEGER NOT NULL REFERENCES drivers(id) ON DELETE RESTRICT,"
        "start_date TEXT NOT NULL,"
        "end_date TEXT,"
        "role TEXT NOT NULL DEFAULT '',"
        "notes TEXT NOT NULL DEFAULT '',"
        "created_at TEXT NOT NULL,"
        "updated_at TEXT NOT NULL,"
        "CHECK (start_date GLOB '[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]'),"
        "CHECK (end_date IS NULL OR end_date GLOB '[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]'),"
        "CHECK (end_date IS NULL OR end_date >= start_date)"
        ");\n"
        "CREATE UNIQUE INDEX idx_memberships_exact ON team_driver_memberships("
        "team_id, driver_id, start_date, COALESCE(end_date, ''));\n"
        "CREATE INDEX idx_memberships_team_dates ON team_driver_memberships(team_id, start_date);\n"
        "CREATE INDEX idx_memberships_driver_dates ON team_driver_memberships(driver_id, "
        "start_date);\n"},
    /* Version 7: team-owned car/chassis references (3.4). No default car is
     * created by migrations or by team creation. */
    {7, "CREATE TABLE cars ("
        "id INTEGER PRIMARY KEY,"
        "team_id INTEGER NOT NULL REFERENCES teams(id) ON DELETE CASCADE,"
        "label TEXT NOT NULL COLLATE NOCASE,"
        "chassis TEXT NOT NULL DEFAULT '',"
        "notes TEXT NOT NULL DEFAULT '',"
        "archived INTEGER NOT NULL DEFAULT 0 CHECK (archived IN (0, 1)),"
        "created_at TEXT NOT NULL,"
        "updated_at TEXT NOT NULL,"
        "archived_at TEXT,"
        "CHECK (length(trim(label)) > 0),"
        "UNIQUE (team_id, label)"
        ");\n"
        "CREATE INDEX idx_cars_team_archived ON cars(team_id, archived);\n"},
    /* Version 8: append-only activity events and field changes (3.5/3.7). No
     * rows are seeded; only application actions create activity. */
    {8, "CREATE TABLE activity_events ("
        "id INTEGER PRIMARY KEY,"
        "team_id INTEGER REFERENCES teams(id) ON DELETE SET NULL,"
        "occurred_at TEXT NOT NULL,"
        "action TEXT NOT NULL,"
        "entity_type TEXT NOT NULL,"
        "entity_id INTEGER,"
        "summary TEXT NOT NULL DEFAULT '',"
        "CHECK (length(trim(action)) > 0),"
        "CHECK (length(trim(entity_type)) > 0)"
        ");\n"
        "CREATE INDEX idx_activity_team_time ON activity_events(team_id, occurred_at DESC);\n"
        "CREATE INDEX idx_activity_entity ON activity_events(entity_type, entity_id);\n"
        "CREATE INDEX idx_activity_time ON activity_events(occurred_at DESC);\n"
        "CREATE TABLE activity_changes ("
        "id INTEGER PRIMARY KEY,"
        "event_id INTEGER NOT NULL REFERENCES activity_events(id) ON DELETE CASCADE,"
        "field_key TEXT NOT NULL,"
        "old_value TEXT,"
        "new_value TEXT,"
        "CHECK (length(trim(field_key)) > 0)"
        ");\n"
        "CREATE INDEX idx_activity_changes_event ON activity_changes(event_id);\n"},
};

const apl_db_migration *apl_db_migrations_list(size_t *const out_count)
{
    if (out_count != nullptr) {
        *out_count = sizeof(apl_migrations) / sizeof(apl_migrations[0]);
    }
    return apl_migrations;
}
