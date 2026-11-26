#include "db/migrations.h"

#include "core/util.h"

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
    /* Version 9: one optional reporting currency per team (4.1). Existing
     * teams start unset; the application requires a currency before posting
     * any money. No currency is guessed or seeded. */
    {9, "ALTER TABLE teams ADD COLUMN currency TEXT "
        "CHECK (currency IS NULL OR "
        "(length(currency) = 3 AND currency GLOB '[A-Z][A-Z][A-Z]'));\n"},
    /* Version 10: immutable team-scoped ledger entries (4.2). Corrections are
     * separate rows linked to their original; there is no balance column and
     * no seeded entry/category. One opening balance per team is enforced, and
     * a correction can only point at an entry in the same team. */
    {10,
     "CREATE TABLE ledger_entries ("
     "id INTEGER PRIMARY KEY,"
     "team_id INTEGER NOT NULL REFERENCES teams(id) ON DELETE CASCADE,"
     "amount_minor INTEGER NOT NULL,"
     "currency TEXT NOT NULL CHECK (length(currency) = 3 AND currency GLOB '[A-Z][A-Z][A-Z]'),"
     "entry_date TEXT NOT NULL "
     "CHECK (entry_date GLOB '[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]'),"
     "kind TEXT NOT NULL CHECK (kind IN "
     "('opening_balance', 'income', 'expense', 'reversal', 'adjustment')),"
     "category TEXT NOT NULL,"
     "category_key TEXT NOT NULL,"
     "description TEXT NOT NULL DEFAULT '',"
     "corrects_entry_id INTEGER REFERENCES ledger_entries(id) ON DELETE RESTRICT,"
     "created_at TEXT NOT NULL,"
     "CHECK (length(trim(category)) > 0),"
     "CHECK (length(trim(category_key)) > 0),"
     "CHECK (corrects_entry_id IS NULL OR corrects_entry_id <> id),"
     "CHECK ((kind IN ('reversal', 'adjustment')) = (corrects_entry_id IS NOT NULL))"
     ");\n"
     "CREATE INDEX idx_ledger_team_date ON ledger_entries(team_id, entry_date, id);\n"
     "CREATE INDEX idx_ledger_team_category ON ledger_entries(team_id, category_key);\n"
     "CREATE INDEX idx_ledger_corrects ON ledger_entries(corrects_entry_id);\n"
     "CREATE UNIQUE INDEX idx_ledger_one_opening ON ledger_entries(team_id) "
     "WHERE kind = 'opening_balance';\n"
     "CREATE UNIQUE INDEX idx_ledger_one_reversal ON ledger_entries(corrects_entry_id) "
     "WHERE kind = 'reversal';\n"
     "CREATE TRIGGER ledger_correction_same_team BEFORE INSERT ON ledger_entries "
     "WHEN NEW.corrects_entry_id IS NOT NULL AND NOT EXISTS ("
     "SELECT 1 FROM ledger_entries original "
     "WHERE original.id = NEW.corrects_entry_id AND original.team_id = NEW.team_id) "
     "BEGIN SELECT RAISE(ABORT, 'correction must reference an entry in the same team'); END;\n"},
    /* Version 11: user-entered budget plans, kept separate from actual ledger
     * entries (4.11). Amounts are non-negative planned minor units; no plan,
     * category, or period is seeded. */
    {11,
     "CREATE TABLE budget_plans ("
     "id INTEGER PRIMARY KEY,"
     "team_id INTEGER NOT NULL REFERENCES teams(id) ON DELETE CASCADE,"
     "season_id INTEGER REFERENCES seasons(id) ON DELETE SET NULL,"
     "start_date TEXT NOT NULL "
     "CHECK (start_date GLOB '[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]'),"
     "end_date TEXT NOT NULL "
     "CHECK (end_date GLOB '[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]'),"
     "category TEXT NOT NULL,"
     "category_key TEXT NOT NULL,"
     "amount_minor INTEGER NOT NULL CHECK (amount_minor >= 0),"
     "currency TEXT NOT NULL CHECK (length(currency) = 3 AND currency GLOB '[A-Z][A-Z][A-Z]'),"
     "notes TEXT NOT NULL DEFAULT '',"
     "created_at TEXT NOT NULL,"
     "updated_at TEXT NOT NULL,"
     "CHECK (start_date <= end_date),"
     "CHECK (length(trim(category)) > 0),"
     "CHECK (length(trim(category_key)) > 0),"
     "UNIQUE (team_id, start_date, end_date, category_key)"
     ");\n"
     "CREATE INDEX idx_budget_team_dates ON budget_plans(team_id, start_date, end_date);\n"
     "CREATE INDEX idx_budget_team_category ON budget_plans(team_id, category_key);\n"
     "CREATE TRIGGER budget_season_same_team BEFORE INSERT ON budget_plans "
     "WHEN NEW.season_id IS NOT NULL AND NOT EXISTS ("
     "SELECT 1 FROM seasons WHERE seasons.id = NEW.season_id AND seasons.team_id = NEW.team_id) "
     "BEGIN SELECT RAISE(ABORT, 'budget season must belong to the same team'); END;\n"},
    /* Version 12: user-entered development projects (5.1). Titles, categories,
     * statuses, dates, and notes come from the user; no project row, status
     * vocabulary, or progress value is seeded. Optional season/car links are
     * same-team only and survive later archiving of the referenced record. */
    {12, "CREATE TABLE projects ("
         "id INTEGER PRIMARY KEY,"
         "team_id INTEGER NOT NULL REFERENCES teams(id) ON DELETE CASCADE,"
         "season_id INTEGER REFERENCES seasons(id) ON DELETE SET NULL,"
         "car_id INTEGER REFERENCES cars(id) ON DELETE SET NULL,"
         "title TEXT NOT NULL COLLATE NOCASE,"
         "category TEXT NOT NULL DEFAULT '',"
         "category_key TEXT NOT NULL DEFAULT '',"
         "description TEXT NOT NULL DEFAULT '',"
         "status TEXT NOT NULL DEFAULT '',"
         "status_key TEXT NOT NULL DEFAULT '',"
         "target_date TEXT,"
         "actual_date TEXT,"
         "notes TEXT NOT NULL DEFAULT '',"
         "archived INTEGER NOT NULL DEFAULT 0 CHECK (archived IN (0, 1)),"
         "created_at TEXT NOT NULL,"
         "updated_at TEXT NOT NULL,"
         "archived_at TEXT,"
         "CHECK (length(trim(title)) > 0),"
         "CHECK (target_date IS NULL OR "
         "target_date GLOB '[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]'),"
         "CHECK (actual_date IS NULL OR "
         "actual_date GLOB '[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]')"
         ");\n"
         "CREATE INDEX idx_projects_team_archived ON projects(team_id, archived);\n"
         "CREATE INDEX idx_projects_team_status ON projects(team_id, status_key);\n"
         "CREATE INDEX idx_projects_team_category ON projects(team_id, category_key);\n"
         "CREATE INDEX idx_projects_season ON projects(season_id);\n"
         "CREATE INDEX idx_projects_car ON projects(car_id);\n"
         "CREATE TRIGGER projects_season_same_team_insert BEFORE INSERT ON projects "
         "WHEN NEW.season_id IS NOT NULL AND NOT EXISTS ("
         "SELECT 1 FROM seasons WHERE seasons.id = NEW.season_id "
         "AND seasons.team_id = NEW.team_id) "
         "BEGIN SELECT RAISE(ABORT, 'project season must belong to the same team'); END;\n"
         "CREATE TRIGGER projects_season_same_team_update "
         "BEFORE UPDATE OF season_id, team_id ON projects "
         "WHEN NEW.season_id IS NOT NULL AND NOT EXISTS ("
         "SELECT 1 FROM seasons WHERE seasons.id = NEW.season_id "
         "AND seasons.team_id = NEW.team_id) "
         "BEGIN SELECT RAISE(ABORT, 'project season must belong to the same team'); END;\n"
         "CREATE TRIGGER projects_car_same_team_insert BEFORE INSERT ON projects "
         "WHEN NEW.car_id IS NOT NULL AND NOT EXISTS ("
         "SELECT 1 FROM cars WHERE cars.id = NEW.car_id AND cars.team_id = NEW.team_id) "
         "BEGIN SELECT RAISE(ABORT, 'project car must belong to the same team'); END;\n"
         "CREATE TRIGGER projects_car_same_team_update "
         "BEFORE UPDATE OF car_id, team_id ON projects "
         "WHEN NEW.car_id IS NOT NULL AND NOT EXISTS ("
         "SELECT 1 FROM cars WHERE cars.id = NEW.car_id AND cars.team_id = NEW.team_id) "
         "BEGIN SELECT RAISE(ABORT, 'project car must belong to the same team'); END;\n"},
    /* Version 13: append-only dated project updates (5.3). The entered update
     * date is separate from the system creation timestamp; no update rows,
     * status values, or progress data are seeded. */
    {13, "CREATE TABLE project_updates ("
         "id INTEGER PRIMARY KEY,"
         "project_id INTEGER NOT NULL REFERENCES projects(id) ON DELETE CASCADE,"
         "update_date TEXT NOT NULL "
         "CHECK (update_date GLOB '[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]'),"
         "summary TEXT NOT NULL,"
         "status_change TEXT NOT NULL DEFAULT '',"
         "status_change_key TEXT NOT NULL DEFAULT '',"
         "notes TEXT NOT NULL DEFAULT '',"
         "created_at TEXT NOT NULL,"
         "CHECK (length(trim(summary)) > 0)"
         ");\n"
         "CREATE INDEX idx_project_updates_project_date "
         "ON project_updates(project_id, update_date, id);\n"},
    /* Version 14: optional project links on actual money transactions (5.5).
     * The link is nullable and same-team; project actual cost stays a view of
     * the ledger rather than a second cost record. Existing entries keep no
     * link and no fabricated cost. */
    {14, "ALTER TABLE ledger_entries ADD COLUMN project_id INTEGER "
         "REFERENCES projects(id) ON DELETE SET NULL;\n"
         "CREATE INDEX idx_ledger_team_project ON ledger_entries(team_id, project_id);\n"
         "CREATE TRIGGER ledger_project_same_team_insert BEFORE INSERT ON ledger_entries "
         "WHEN NEW.project_id IS NOT NULL AND NOT EXISTS ("
         "SELECT 1 FROM projects WHERE projects.id = NEW.project_id "
         "AND projects.team_id = NEW.team_id) "
         "BEGIN SELECT RAISE(ABORT, 'ledger project must belong to the same team'); END;\n"},
    /* Version 15: project-linked, archivable budget plans. Rebuilds the table
     * to add optional project scope and soft archive state while preserving
     * existing rows and ids. Existing plans stay team-level and active. */
    {15,
     "CREATE TABLE budget_plans_new ("
     "id INTEGER PRIMARY KEY,"
     "team_id INTEGER NOT NULL REFERENCES teams(id) ON DELETE CASCADE,"
     "season_id INTEGER REFERENCES seasons(id) ON DELETE SET NULL,"
     "project_id INTEGER REFERENCES projects(id) ON DELETE SET NULL,"
     "start_date TEXT NOT NULL "
     "CHECK (start_date GLOB '[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]'),"
     "end_date TEXT NOT NULL "
     "CHECK (end_date GLOB '[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]'),"
     "category TEXT NOT NULL,"
     "category_key TEXT NOT NULL,"
     "amount_minor INTEGER NOT NULL CHECK (amount_minor >= 0),"
     "currency TEXT NOT NULL "
     "CHECK (length(currency) = 3 AND currency GLOB '[A-Z][A-Z][A-Z]'),"
     "notes TEXT NOT NULL DEFAULT '',"
     "archived INTEGER NOT NULL DEFAULT 0 CHECK (archived IN (0, 1)),"
     "archived_at TEXT,"
     "created_at TEXT NOT NULL,"
     "updated_at TEXT NOT NULL,"
     "CHECK (start_date <= end_date),"
     "CHECK (length(trim(category)) > 0),"
     "CHECK (length(trim(category_key)) > 0)"
     ");\n"
     "INSERT INTO budget_plans_new (id, team_id, season_id, project_id, start_date, end_date, "
     "category, category_key, amount_minor, currency, notes, archived, archived_at, created_at, "
     "updated_at) SELECT id, team_id, season_id, NULL, start_date, end_date, category, "
     "category_key, amount_minor, currency, notes, 0, NULL, created_at, updated_at "
     "FROM budget_plans;\n"
     "DROP TABLE budget_plans;\n"
     "ALTER TABLE budget_plans_new RENAME TO budget_plans;\n"
     "CREATE UNIQUE INDEX idx_budget_scope ON budget_plans("
     "team_id, COALESCE(project_id, 0), start_date, end_date, category_key);\n"
     "CREATE INDEX idx_budget_team_dates ON budget_plans(team_id, start_date, end_date);\n"
     "CREATE INDEX idx_budget_team_project ON budget_plans(team_id, project_id, archived);\n"
     "CREATE INDEX idx_budget_team_category ON budget_plans(team_id, category_key);\n"
     "CREATE TRIGGER budget_season_same_team BEFORE INSERT ON budget_plans "
     "WHEN NEW.season_id IS NOT NULL AND NOT EXISTS ("
     "SELECT 1 FROM seasons WHERE seasons.id = NEW.season_id AND seasons.team_id = NEW.team_id) "
     "BEGIN SELECT RAISE(ABORT, 'budget season must belong to the same team'); END;\n"
     "CREATE TRIGGER budget_season_same_team_update BEFORE UPDATE OF season_id, team_id "
     "ON budget_plans WHEN NEW.season_id IS NOT NULL AND NOT EXISTS ("
     "SELECT 1 FROM seasons WHERE seasons.id = NEW.season_id AND seasons.team_id = NEW.team_id) "
     "BEGIN SELECT RAISE(ABORT, 'budget season must belong to the same team'); END;\n"
     "CREATE TRIGGER budget_project_same_team BEFORE INSERT ON budget_plans "
     "WHEN NEW.project_id IS NOT NULL AND NOT EXISTS ("
     "SELECT 1 FROM projects WHERE projects.id = NEW.project_id "
     "AND projects.team_id = NEW.team_id) "
     "BEGIN SELECT RAISE(ABORT, 'budget project must belong to the same team'); END;\n"
     "CREATE TRIGGER budget_project_same_team_update BEFORE UPDATE OF project_id, team_id "
     "ON budget_plans WHEN NEW.project_id IS NOT NULL AND NOT EXISTS ("
     "SELECT 1 FROM projects WHERE projects.id = NEW.project_id "
     "AND projects.team_id = NEW.team_id) "
     "BEGIN SELECT RAISE(ABORT, 'budget project must belong to the same team'); END;\n"},
    /* Version 16: user-defined component types (6.1). Category and name are
     * user-entered labels; no catalogue, compatibility rule, or part type is
     * seeded. The unique index is partial on non-archived rows so archiving a
     * type frees its team/category/name for reuse without breaking history. */
    {16, "CREATE TABLE part_types ("
         "id INTEGER PRIMARY KEY,"
         "team_id INTEGER NOT NULL REFERENCES teams(id) ON DELETE CASCADE,"
         "category TEXT NOT NULL COLLATE NOCASE,"
         "category_key TEXT NOT NULL,"
         "name TEXT NOT NULL COLLATE NOCASE,"
         "name_key TEXT NOT NULL,"
         "description TEXT NOT NULL DEFAULT '',"
         "notes TEXT NOT NULL DEFAULT '',"
         "archived INTEGER NOT NULL DEFAULT 0 CHECK (archived IN (0, 1)),"
         "created_at TEXT NOT NULL,"
         "updated_at TEXT NOT NULL,"
         "archived_at TEXT,"
         "CHECK (length(trim(category)) > 0),"
         "CHECK (length(trim(name)) > 0)"
         ");\n"
         "CREATE INDEX idx_part_types_team_archived ON part_types(team_id, archived);\n"
         "CREATE INDEX idx_part_types_team_category ON part_types(team_id, category_key);\n"
         "CREATE UNIQUE INDEX uq_part_types_team_name ON part_types(team_id, category_key, "
         "name_key) WHERE archived = 0;\n"},
    /* Version 17: individual part/unit records (6.2). A unit is a distinct,
     * team-owned row linked to one part type. Serial and part numbers are
     * genuinely optional and stored as empty strings (not NULL); no stock or
     * availability state is created. */
    {17, "CREATE TABLE part_units ("
         "id INTEGER PRIMARY KEY,"
         "team_id INTEGER NOT NULL REFERENCES teams(id) ON DELETE CASCADE,"
         "type_id INTEGER NOT NULL REFERENCES part_types(id) ON DELETE RESTRICT,"
         "serial_number TEXT NOT NULL DEFAULT '',"
         "part_number TEXT NOT NULL DEFAULT '',"
         "condition_label TEXT NOT NULL DEFAULT '',"
         "condition_key TEXT NOT NULL DEFAULT '',"
         "notes TEXT NOT NULL DEFAULT '',"
         "archived INTEGER NOT NULL DEFAULT 0 CHECK (archived IN (0, 1)),"
         "created_at TEXT NOT NULL,"
         "updated_at TEXT NOT NULL,"
         "archived_at TEXT"
         ");\n"
         "CREATE INDEX idx_part_units_team_archived ON part_units(team_id, archived);\n"
         "CREATE INDEX idx_part_units_team_type ON part_units(team_id, type_id, archived);\n"
         "CREATE INDEX idx_part_units_team_serial ON part_units(team_id, serial_number);\n"
         "CREATE INDEX idx_part_units_team_part ON part_units(team_id, part_number);\n"
         "CREATE TRIGGER part_units_type_same_team BEFORE INSERT ON part_units "
         "WHEN NOT EXISTS (SELECT 1 FROM part_types WHERE part_types.id = NEW.type_id "
         "AND part_types.team_id = NEW.team_id) "
         "BEGIN SELECT RAISE(ABORT, 'part unit type must belong to the same team'); END;\n"
         "CREATE TRIGGER part_units_type_same_team_update "
         "BEFORE UPDATE OF type_id, team_id ON part_units "
         "WHEN NOT EXISTS (SELECT 1 FROM part_types WHERE part_types.id = NEW.type_id "
         "AND part_types.team_id = NEW.team_id) "
         "BEGIN SELECT RAISE(ABORT, 'part unit type must belong to the same team'); END;\n"},
    /* Version 18: append-only dated lifecycle events (6.3). Actions are
     * user-entered install/remove/replace/repair/dispose. Event date is entered
     * by the user and is separate from the system creation timestamp. No
     * lifecycle or fitment row is seeded and no mutable "current part" field is
     * stored. */
    {18, "CREATE TABLE lifecycle_events ("
         "id INTEGER PRIMARY KEY,"
         "team_id INTEGER NOT NULL REFERENCES teams(id) ON DELETE CASCADE,"
         "action TEXT NOT NULL CHECK (action IN "
         "('install', 'remove', 'replace', 'repair', 'dispose')),"
         "event_date TEXT NOT NULL "
         "CHECK (event_date GLOB '[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]'),"
         "unit_id INTEGER REFERENCES part_units(id) ON DELETE RESTRICT,"
         "old_unit_id INTEGER REFERENCES part_units(id) ON DELETE RESTRICT,"
         "new_unit_id INTEGER REFERENCES part_units(id) ON DELETE RESTRICT,"
         "car_id INTEGER REFERENCES cars(id) ON DELETE SET NULL,"
         "position TEXT NOT NULL DEFAULT '',"
         "event_label TEXT NOT NULL DEFAULT '',"
         "reason TEXT NOT NULL DEFAULT '',"
         "usage TEXT NOT NULL DEFAULT '',"
         "notes TEXT NOT NULL DEFAULT '',"
         "created_at TEXT NOT NULL,"
         "CHECK (length(trim(event_date)) > 0)"
         ");\n"
         "CREATE INDEX idx_lifecycle_team_date ON lifecycle_events(team_id, event_date, id);\n"
         "CREATE INDEX idx_lifecycle_car_date ON lifecycle_events(car_id, event_date);\n"
         "CREATE INDEX idx_lifecycle_unit_date ON lifecycle_events(unit_id, event_date);\n"
         "CREATE INDEX idx_lifecycle_old_unit ON lifecycle_events(old_unit_id, event_date);\n"
         "CREATE INDEX idx_lifecycle_new_unit ON lifecycle_events(new_unit_id, event_date);\n"
         "CREATE INDEX idx_lifecycle_action ON lifecycle_events(team_id, action, event_date);\n"
         "CREATE TRIGGER lifecycle_car_same_team BEFORE INSERT ON lifecycle_events "
         "WHEN NEW.car_id IS NOT NULL AND NOT EXISTS ("
         "SELECT 1 FROM cars WHERE cars.id = NEW.car_id AND cars.team_id = NEW.team_id) "
         "BEGIN SELECT RAISE(ABORT, 'lifecycle car must belong to the same team'); END;\n"
         "CREATE TRIGGER lifecycle_unit_same_team BEFORE INSERT ON lifecycle_events "
         "WHEN (NEW.unit_id IS NOT NULL AND NOT EXISTS ("
         "SELECT 1 FROM part_units WHERE part_units.id = NEW.unit_id "
         "AND part_units.team_id = NEW.team_id)) OR "
         "(NEW.old_unit_id IS NOT NULL AND NOT EXISTS ("
         "SELECT 1 FROM part_units WHERE part_units.id = NEW.old_unit_id "
         "AND part_units.team_id = NEW.team_id)) OR "
         "(NEW.new_unit_id IS NOT NULL AND NOT EXISTS ("
         "SELECT 1 FROM part_units WHERE part_units.id = NEW.new_unit_id "
         "AND part_units.team_id = NEW.team_id)) "
         "BEGIN SELECT RAISE(ABORT, 'lifecycle unit must belong to the same team'); END;\n"},
    /* Version 19: optional replacement-to-ledger cost link (6.7). Adds a
     * nullable transaction_id to lifecycle events using an explicit FK, not a
     * generic polymorphic link. In v1 a transaction is linked to at most one
     * lifecycle event, so a partial unique index on non-null rows enforces the
     * one-to-one direction. No cost amount is stored on the event. */
    {19, "ALTER TABLE lifecycle_events ADD COLUMN transaction_id INTEGER "
         "REFERENCES ledger_entries(id) ON DELETE SET NULL;\n"
         "CREATE INDEX idx_lifecycle_transaction ON lifecycle_events(transaction_id);\n"
         "CREATE UNIQUE INDEX uq_lifecycle_transaction ON lifecycle_events(transaction_id) "
         "WHERE transaction_id IS NOT NULL;\n"
         "CREATE TRIGGER lifecycle_transaction_same_team BEFORE INSERT ON lifecycle_events "
         "WHEN NEW.transaction_id IS NOT NULL AND NOT EXISTS ("
         "SELECT 1 FROM ledger_entries WHERE ledger_entries.id = NEW.transaction_id "
         "AND ledger_entries.team_id = NEW.team_id) "
         "BEGIN SELECT RAISE(ABORT, 'lifecycle transaction must belong to the same team'); END;\n"},
};

const apl_db_migration *apl_db_migrations_list(size_t *const out_count)
{
    if (out_count != nullptr) {
        *out_count = APL_COUNT_OF(apl_migrations);
    }
    return apl_migrations;
}
