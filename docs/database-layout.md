# Database layout, schema version 26

Versions 24–26 normalize relationships that have a mandatory parent. Previous
migrations remain unchanged so existing databases can upgrade on startup.

## One stored ownership path

| Table | Stored parent | Derived fields (not stored) |
| --- | --- | --- |
| `part_units` | `type_id → part_types.id` | `team_id` from the type |
| `event_sessions` | `event_id → events.id` | `team_id` from the event |
| `driver_outcomes` | `session_id → event_sessions.id` | `event_id` from the session; `team_id` from its event |
| `outcome_corrections` | `outcome_id → driver_outcomes.id` | `team_id` through outcome → session → event |

Primary keys remain stable identities, not redundant ownership facts.
`driver_outcomes` is unique on `(session_id, driver_id)`; team scope no longer
needs to be repeated in that key or in a composite session foreign key.

Read-only views expose the derived fields for queries:
`part_units_scoped`, `event_sessions_scoped`, `driver_outcomes_scoped`, and
`outcome_corrections_scoped`. These are joins, not extra stored copies.
Application structs still include resolved IDs for display and team-scoped
operations. Repositories check the requested scope against the parent before
writing, rather than trusting or storing caller-supplied derived IDs.

## Links intentionally retained

- Seasons, cars, part types, projects, events, ledger entries, and budget plans
  own their team association. Optional links do not replace that ownership:
  an event may have no season, a project may have no season/car, and a ledger
  entry or budget may have no project. A project's season also need not be a
  budget plan's reporting season.
- Lifecycle events retain their team scope: the existing schema permits all
  unit/car/transaction links to be absent. Removing it would require a separate
  domain change to require a canonical parent and handle existing unitless
  history. Multiple unit links describe different roles, not duplicate facts.
- Activity-event team scope and correction snapshots are historical context,
  not caches of current entity values. Polymorphic activity subjects also do
  not provide a mandatory foreign-key ownership path.
- Driver identities are shared; memberships represent dated associations,
  not a driver's single permanent team.

## Upgrade and integrity

The migrations preserve row IDs, values, timestamps, nullable outcome fields,
archive state, and correction history. Foreign keys remain enabled throughout.
Existing contradictory ownership values cause the relevant migration to roll
back rather than silently choosing an owner. Earlier successful migrations
remain applied; after explicitly resolving the conflict, startup can retry.

Parent edits cannot move units or sessions across teams, move an event with
sessions to another team, or move a session with outcomes to another event.
Corrected outcomes cannot change session/driver identity. Outcome membership
checks, foreign-key restrictions, and append-only correction protections remain.

Back up a database before upgrading; older application builds cannot open
schema version 26.
