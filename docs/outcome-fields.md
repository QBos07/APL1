# Initial manually entered outcome fields

This is the Phase 7 initial field set for one driver's outcome in one
user-created session. The driver, event, and session identify the target; they
are not result values. Every field below is optional. An outcome record with no
values is still distinct from the absence of an outcome record.

| Field | Meaning and format | Validation | Missing versus entered value |
| --- | --- | --- | --- |
| Start position | Position on the starting grid, as an integer. | 1–2,147,483,647. | Missing means not recorded; zero is invalid. |
| Finish position | Classified finishing position, as an integer. | 1–2,147,483,647. | Missing means not recorded; zero is invalid. |
| Status | User-entered description of the outcome, such as `finished` or `DNF`. | Non-blank UTF-8 text, at most 255 bytes. No predefined vocabulary. | Missing means not recorded; no status is inferred from another field. |
| Laps | Number of laps completed, as an integer. | 0–2,147,483,647. | Missing means not recorded; zero is a recorded count. |
| Notes | User-entered free-form context about the outcome. | Non-blank UTF-8 text, at most 4096 bytes. | Missing means not recorded; entered text is not a default. |

Blank optional text input means missing, not an entered empty string. Non-blank
text is retained as entered, including its case and surrounding whitespace.
Text containing only whitespace is blank. Implementations must not silently
truncate input; invalid values must be reported before saving. Missing values
are stored as SQL NULL, never as zero, an empty string, a status, or a computed
value. Interfaces should show missing values as “not recorded”.

These fields deliberately do not enforce relationships between positions,
status, or laps: the application does not infer whether a driver started,
finished, or retired. They also do not define points, scoring rules, penalty
calculations, or generated results. Additional typed nullable fields can be
introduced through later migrations; configurable custom fields are not part
of this initial set.

For recorded-value summaries, a count of recorded outcomes and a sum of
recorded laps have clear meanings. The sum must exclude NULL laps and report
the number of outcomes with and without a recorded lap count; if none have a
lap count, the sum is not recorded rather than zero. Any summary must identify
its team and active event, session, season, and date filters. Positions and
free-form status/notes are not summed or used to infer rankings.

The session's **u summary** action shows this recorded-value count and lap sum.
It fixes the team, event, and session to the current view, and accepts optional
season and inclusive event-date filters. Its output repeats the active scope and
reports both recorded and missing lap counts. An empty or entirely missing-laps
scope displays the lap total as “not recorded”; explicitly entered zero laps
remains a recorded zero.
