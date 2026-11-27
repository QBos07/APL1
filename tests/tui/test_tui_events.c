#include <criterion/criterion.h>
#include <string.h>

#include "app/outcome_service.h"
#include "db/driver_repository.h"
#include "db/membership_repository.h"
#include "db/team_repository.h"
#include "db_fixture.h"
#include "tui/shell_internal.h"

Test(tui_events, navigation_scopes_outcomes_and_correction_history)
{
    apl_test_db db = {};
    apl_error error;
    apl_error_init(&error);
    cr_assert_eq(apl_test_db_open(&db, &error), APL_OK);
    apl_app app = {.db = db.connection};
    apl_team team = {}, other = {};
    cr_assert_eq(apl_team_repository_create(&app.db, "Team A", "", &team, &error), APL_OK);
    cr_assert_eq(apl_team_repository_create(&app.db, "Team B", "", &other, &error), APL_OK);
    apl_event event = {};
    cr_assert_eq(apl_event_repository_create(&app.db, team.id, 0, "Event A", "2026-04-01", "", "",
                                             &event, &error),
                 APL_OK);
    apl_event_session session = {};
    cr_assert_eq(
        apl_session_repository_create(&app.db, team.id, event.id, "Race", &session, &error),
        APL_OK);
    apl_driver driver = {}, empty = {};
    cr_assert_eq(apl_driver_repository_create(&app.db, "Driver", "", "", &driver, &error), APL_OK);
    cr_assert_eq(apl_driver_repository_create(&app.db, "Empty", "", "", &empty, &error), APL_OK);
    apl_membership member = {};
    cr_assert_eq(apl_membership_repository_create(&app.db, team.id, driver.id, "2020-01-01", "", "",
                                                  "", &member, &error),
                 APL_OK);
    cr_assert_eq(apl_membership_repository_create(&app.db, team.id, empty.id, "2020-01-01", "", "",
                                                  "", &member, &error),
                 APL_OK);
    apl_outcome input = {
        .team_id = team.id, .event_id = event.id, .session_id = session.id, .driver_id = driver.id};
    apl_outcome saved = {};
    cr_assert_eq(apl_outcome_service_save(&app, APL_OUTCOME_CREATE, &input, &saved, &error),
                 APL_OK);
    input.id = saved.id;
    input.has_laps = true;
    input.laps = 0;
    cr_assert_eq(apl_outcome_service_save(&app, APL_OUTCOME_EDIT, &input, &saved, &error), APL_OK);

    shell_state state = {};
    state.app = &app;
    state.active_team = team;
    state.active_status = APL_ACTIVE_TEAM_ACTIVE;
    state.section = APL_TUI_SECTION_EVENTS;
    state.screen = SHELL_SCREEN_EVENT_LIST;
    apl_tui_list_init(&state.list, 8);
    apl_tui_focus_init(&state.focus);
    cr_assert_eq(load_race_events(&state, &error), APL_OK);
    cr_assert_eq(state.race_event_count, 1);
    handle_events_mouse(&state, NCKEY_BUTTON1, 0, 12);
    cr_assert_eq(state.screen, SHELL_SCREEN_EVENT_LIST);
    handle_events_mouse(&state, NCKEY_BUTTON1, 2, 12);
    cr_assert_eq(state.screen, SHELL_SCREEN_EVENT_DETAIL);
    cr_assert_eq(state.race_event_detail.session_count, 1);
    handle_events_mouse(&state, NCKEY_BUTTON1, 4, 12);
    cr_assert_eq(state.screen, SHELL_SCREEN_SESSION_DETAIL);
    cr_assert_eq(state.race_session_detail.row_count, 2);
    cr_assert(state.race_session_detail.rows[0].has_outcome);
    cr_assert_not(state.race_session_detail.rows[1].has_outcome);
    handle_events_mouse(&state, NCKEY_BUTTON1, 3, 12);
    cr_assert_eq(state.screen, SHELL_SCREEN_OUTCOME_HISTORY);
    cr_assert_eq(state.outcome_correction_count, 1);
    cr_assert_not(state.outcome_corrections[0].before.has_laps);
    cr_assert(state.outcome_corrections[0].after.has_laps);
    cr_assert_eq(state.outcome_corrections[0].after.laps, 0);
    handle_events_input(&state, 'l');
    cr_assert_eq(state.outcome_history_offset, 1);
    handle_events_input(&state, 'h');
    cr_assert_eq(state.outcome_history_offset, 0);
    handle_events_input(&state, NCKEY_ESC);
    cr_assert_eq(state.screen, SHELL_SCREEN_SESSION_DETAIL);
    handle_events_mouse(&state, NCKEY_BUTTON1, 4, 12);
    cr_assert_eq(state.dialog_context, SHELL_DIALOG_CONTEXT_OUTCOME_FORM);
    cr_assert_eq(state.dialog_driver_id, empty.id);
    ncinput cancel = {.id = NCKEY_ESC};
    handle_dialog_input(&state, &cancel);
    apl_tui_list_select(&state.list, 1);
    handle_events_input(&state, NCKEY_ENTER);
    cr_assert_eq(state.screen, SHELL_SCREEN_SESSION_DETAIL);
    handle_events_input(&state, NCKEY_ESC);
    cr_assert_eq(state.screen, SHELL_SCREEN_EVENT_DETAIL);
    handle_events_input(&state, NCKEY_ESC);
    cr_assert_eq(state.screen, SHELL_SCREEN_EVENT_LIST);
    perform_set_active(&state, other.id, &error);
    cr_assert_eq(state.screen, SHELL_SCREEN_EVENT_LIST);
    cr_assert_eq(state.race_event_count, 0);
    cr_assert_eq(state.race_session_detail.row_count, 0);
    free_reference_lists(&state);
    cr_assert_eq(apl_test_db_close(&db, &error), APL_OK);
}

Test(tui_events, event_and_session_forms_validate_and_cancel_without_losing_input)
{
    apl_test_db db = {};
    apl_error error;
    apl_error_init(&error);
    cr_assert_eq(apl_test_db_open(&db, &error), APL_OK);
    apl_app app = {.db = db.connection};
    apl_team team = {};
    cr_assert_eq(apl_team_repository_create(&app.db, "Team", "", &team, &error), APL_OK);
    shell_state state = {};
    state.app = &app;
    state.active_team = team;
    state.active_status = APL_ACTIVE_TEAM_ACTIVE;
    state.section = APL_TUI_SECTION_EVENTS;
    state.screen = SHELL_SCREEN_EVENT_LIST;
    apl_tui_list_init(&state.list, 8);
    apl_tui_focus_init(&state.focus);
    handle_events_input(&state, 'n');
    cr_assert_eq(state.dialog_context, SHELL_DIALOG_CONTEXT_EVENT_CREATE);
    apl_tui_dialog_set_field_value(&state.dialog, 0, "Race weekend");
    apl_tui_dialog_set_field_value(&state.dialog, 1, "2026-02-30");
    apl_tui_dialog_set_field_value(&state.dialog, 3, "Circuit");
    state.dialog.focus = state.dialog.field_count;
    ncinput enter = {.id = NCKEY_ENTER};
    handle_dialog_input(&state, &enter);
    cr_assert_eq(state.dialog_context, SHELL_DIALOG_CONTEXT_EVENT_CREATE);
    cr_assert_str_eq(state.dialog.fields[0].value, "Race weekend");
    cr_assert_str_eq(state.dialog.fields[3].value, "Circuit");
    cr_assert_eq(state.race_event_count, 0);
    apl_tui_dialog_set_field_value(&state.dialog, 1, "2026-02-28");
    state.dialog.focus = state.dialog.field_count;
    handle_dialog_input(&state, &enter);
    cr_assert_eq(state.race_event_count, 1);
    cr_assert_str_eq(state.race_events[0].location, "Circuit");
    handle_events_input(&state, NCKEY_ENTER);
    cr_assert_eq(state.screen, SHELL_SCREEN_EVENT_DETAIL);
    handle_events_input(&state, 'n');
    apl_tui_dialog_set_field_value(&state.dialog, 0, "Practice");
    ncinput escape = {.id = NCKEY_ESC};
    handle_dialog_input(&state, &escape);
    cr_assert_eq(state.race_event_detail.session_count, 0);
    handle_events_input(&state, 'n');
    apl_tui_dialog_set_field_value(&state.dialog, 0, "Practice");
    state.dialog.focus = state.dialog.field_count;
    handle_dialog_input(&state, &enter);
    cr_assert_eq(state.race_event_detail.session_count, 1);
    handle_events_input(&state, 's');
    cr_assert_str_eq(state.dialog.fields[0].value, "Practice");
    apl_tui_dialog_set_field_value(&state.dialog, 0, "Qualifying");
    state.dialog.focus = state.dialog.field_count;
    handle_dialog_input(&state, &enter);
    cr_assert_str_eq(state.race_event_detail.sessions[0].label, "Qualifying");
    handle_events_input(&state, 'e');
    cr_assert_str_eq(state.dialog.fields[0].value, "Race weekend");
    apl_tui_dialog_set_field_value(&state.dialog, 0, "Revised event");
    state.dialog.focus = state.dialog.field_count;
    handle_dialog_input(&state, &enter);
    cr_assert_str_eq(state.race_event_detail.event.name, "Revised event");
    cr_assert_eq(state.list.count, 1);
    apl_driver driver = {};
    apl_membership membership = {};
    cr_assert_eq(apl_driver_repository_create(&app.db, "Driver", "", "", &driver, &error), APL_OK);
    cr_assert_eq(apl_membership_repository_create(&app.db, team.id, driver.id, "2020-01-01", "", "",
                                                  "", &membership, &error),
                 APL_OK);
    handle_events_input(&state, NCKEY_ENTER);
    cr_assert_eq(state.screen, SHELL_SCREEN_SESSION_DETAIL);
    handle_events_input(&state, 'a');
    cr_assert_eq(state.dialog_context, SHELL_DIALOG_CONTEXT_OUTCOME_FORM);
    apl_tui_dialog_set_field_value(&state.dialog, 0, "0");
    apl_tui_dialog_set_field_value(&state.dialog, 3, "Finished");
    state.dialog.focus = state.dialog.field_count;
    handle_dialog_input(&state, &enter);
    cr_assert_eq(state.dialog_context, SHELL_DIALOG_CONTEXT_OUTCOME_FORM);
    cr_assert_str_eq(state.dialog.fields[3].value, "Finished");
    cr_assert_not(state.race_session_detail.rows[0].has_outcome);
    handle_dialog_input(&state, &escape);
    cr_assert_not(state.race_session_detail.rows[0].has_outcome);
    handle_events_input(&state, 'a');
    apl_tui_dialog_set_field_value(&state.dialog, 2, "0");
    apl_tui_dialog_set_field_value(&state.dialog, 3, "Finished");
    state.dialog.focus = state.dialog.field_count;
    handle_dialog_input(&state, &enter);
    cr_assert(state.race_session_detail.rows[0].has_outcome);
    cr_assert(state.race_session_detail.rows[0].outcome.has_laps);
    cr_assert_eq(state.race_session_detail.rows[0].outcome.laps, 0);
    cr_assert_not(state.race_session_detail.rows[0].outcome.has_finish_position);
    handle_events_input(&state, 'a');
    cr_assert_str_eq(state.dialog.fields[2].value, "0");
    apl_tui_dialog_set_field_value(&state.dialog, 2, "");
    apl_tui_dialog_set_field_value(&state.dialog, 1, "3");
    state.dialog.focus = state.dialog.field_count;
    handle_dialog_input(&state, &enter);
    cr_assert_not(state.race_session_detail.rows[0].outcome.has_laps);
    cr_assert_eq(state.race_session_detail.rows[0].outcome.finish_position, 3);
    free_reference_lists(&state);
    cr_assert_eq(apl_test_db_close(&db, &error), APL_OK);
}

Test(tui_events, staged_batch_review_and_failed_save_are_atomic)
{
    apl_test_db db = {};
    apl_error error;
    apl_error_init(&error);
    cr_assert_eq(apl_test_db_open(&db, &error), APL_OK);
    apl_app app = {.db = db.connection};
    apl_team team = {};
    cr_assert_eq(apl_team_repository_create(&app.db, "Team", "", &team, &error), APL_OK);
    apl_event event = {};
    apl_event_session session = {};
    cr_assert_eq(apl_event_repository_create(&app.db, team.id, 0, "Event", "2026-01-01", "", "",
                                             &event, &error),
                 APL_OK);
    cr_assert_eq(
        apl_session_repository_create(&app.db, team.id, event.id, "Race", &session, &error),
        APL_OK);
    apl_driver drivers[2] = {};
    apl_membership member = {};
    cr_assert_eq(apl_driver_repository_create(&app.db, "First", "", "", &drivers[0], &error),
                 APL_OK);
    cr_assert_eq(apl_driver_repository_create(&app.db, "Second", "", "", &drivers[1], &error),
                 APL_OK);
    for (size_t i = 0; i < 2; ++i)
        cr_assert_eq(apl_membership_repository_create(&app.db, team.id, drivers[i].id, "2020-01-01",
                                                      "", "", "", &member, &error),
                     APL_OK);
    shell_state state = {};
    state.app = &app;
    state.active_team = team;
    state.active_status = APL_ACTIVE_TEAM_ACTIVE;
    state.section = APL_TUI_SECTION_EVENTS;
    state.screen = SHELL_SCREEN_SESSION_DETAIL;
    apl_tui_list_init(&state.list, 8);
    apl_tui_focus_init(&state.focus);
    cr_assert_eq(apl_session_detail_load(&app, team.id, event.id, session.id,
                                         &state.race_session_detail, &error),
                 APL_OK);
    apl_tui_list_set_count(&state.list, state.race_session_detail.row_count);
    handle_events_input(&state, 'b');
    cr_assert(state.outcome_batch_active);
    ncinput enter = {.id = NCKEY_ENTER};
    handle_events_input(&state, 'a');
    apl_tui_dialog_set_field_value(&state.dialog, 2, "0");
    apl_tui_dialog_set_field_value(&state.dialog, 3, "Finished");
    state.dialog.focus = state.dialog.field_count;
    handle_dialog_input(&state, &enter);
    apl_tui_list_select(&state.list, 1);
    handle_events_input(&state, 'a');
    apl_tui_dialog_set_field_value(&state.dialog, 1, "2");
    apl_tui_dialog_set_field_value(&state.dialog, 4, "manual note");
    state.dialog.focus = state.dialog.field_count;
    handle_dialog_input(&state, &enter);
    cr_assert_eq(state.outcome_batch_count, 2);
    apl_outcome *records = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_outcome_repository_list(&app.db, team.id, event.id, session.id, &records,
                                             &count, &error),
                 APL_OK);
    cr_assert_eq(count, 0);
    apl_outcome_repository_free_list(records);
    handle_events_input(&state, 'r');
    cr_assert_eq(state.screen, SHELL_SCREEN_OUTCOME_REVIEW);
    cr_assert_str_eq(state.outcome_batch[1].input.notes, "manual note");
    handle_events_mouse(&state, NCKEY_BUTTON1, 1, 12);
    cr_assert_eq(state.screen, SHELL_SCREEN_SESSION_DETAIL);
    cr_assert_eq(state.dialog_context, SHELL_DIALOG_CONTEXT_OUTCOME_FORM);
    cr_assert_str_eq(state.dialog.fields[4].value, "manual note");
    ncinput cancel_review_edit = {.id = NCKEY_ESC};
    handle_dialog_input(&state, &cancel_review_edit);
    handle_events_input(&state, 'r');
    cr_assert_eq(state.screen, SHELL_SCREEN_OUTCOME_REVIEW);
    cr_assert_eq(sqlite3_exec(app.db.handle,
                              "CREATE TRIGGER fail_batch_ui BEFORE INSERT ON driver_outcomes "
                              "WHEN NEW.driver_id=2 BEGIN SELECT RAISE(ABORT,'failure'); END;",
                              nullptr, nullptr, nullptr),
                 SQLITE_OK);
    handle_events_input(&state, 's');
    cr_assert_eq(state.screen, SHELL_SCREEN_OUTCOME_REVIEW);
    cr_assert_eq(state.outcome_batch_count, 2);
    cr_assert_eq(apl_outcome_repository_list(&app.db, team.id, event.id, session.id, &records,
                                             &count, &error),
                 APL_OK);
    cr_assert_eq(count, 0);
    apl_outcome_repository_free_list(records);
    cr_assert_eq(
        sqlite3_exec(app.db.handle, "DROP TRIGGER fail_batch_ui", nullptr, nullptr, nullptr),
        SQLITE_OK);
    handle_events_input(&state, 's');
    cr_assert_eq(state.screen, SHELL_SCREEN_SESSION_DETAIL);
    cr_assert_not(state.outcome_batch_active);
    cr_assert_eq(apl_outcome_repository_list(&app.db, team.id, event.id, session.id, &records,
                                             &count, &error),
                 APL_OK);
    cr_assert_eq(count, 2);
    cr_assert(records[0].has_laps);
    cr_assert_eq(records[0].laps, 0);
    cr_assert_not(records[1].has_laps);
    cr_assert_str_eq(records[1].notes, "manual note");
    apl_outcome_repository_free_list(records);
    handle_events_input(&state, 'u');
    cr_assert_eq(state.dialog_context, SHELL_DIALOG_CONTEXT_OUTCOME_SUMMARY);
    apl_tui_dialog_set_field_value(&state.dialog, 1, "2026-02-30");
    state.dialog.focus = state.dialog.field_count;
    handle_dialog_input(&state, &enter);
    cr_assert_eq(state.dialog_context, SHELL_DIALOG_CONTEXT_OUTCOME_SUMMARY);
    cr_assert_str_eq(state.dialog.fields[1].value, "2026-02-30");
    apl_tui_dialog_set_field_value(&state.dialog, 1, "");
    state.dialog.focus = state.dialog.field_count;
    handle_dialog_input(&state, &enter);
    cr_assert_eq(state.dialog_context, SHELL_DIALOG_CONTEXT_ACTIVITY_DETAIL);
    cr_assert_not_null(strstr(state.dialog.message, "Outcome records: 2"));
    cr_assert_not_null(strstr(state.dialog.message, "Laps total: 0"));
    cr_assert_not_null(strstr(state.dialog.message, "Outcomes with laps missing: 1"));
    cr_assert_not_null(strstr(state.dialog.message, "Season filter: all"));
    ncinput escape = {.id = NCKEY_ESC};
    handle_dialog_input(&state, &escape);
    handle_events_input(&state, 'b');
    handle_events_input(&state, 'a');
    apl_tui_dialog_set_field_value(&state.dialog, 4, "not saved");
    state.dialog.focus = state.dialog.field_count;
    handle_dialog_input(&state, &enter);
    handle_events_input(&state, 'x');
    cr_assert_not(state.outcome_batch_active);
    cr_assert_eq(apl_outcome_repository_list(&app.db, team.id, event.id, session.id, &records,
                                             &count, &error),
                 APL_OK);
    cr_assert_eq(count, 2);
    apl_outcome_repository_free_list(records);
    free_reference_lists(&state);
    cr_assert_eq(apl_test_db_close(&db, &error), APL_OK);
}

Test(tui_events, duplicate_choices_preserve_single_and_batch_values)
{
    apl_test_db db = {};
    apl_error error;
    apl_error_init(&error);
    cr_assert_eq(apl_test_db_open(&db, &error), APL_OK);
    apl_app app = {.db = db.connection};
    apl_team team = {};
    apl_event event = {};
    apl_event_session session = {};
    cr_assert_eq(apl_team_repository_create(&app.db, "Team", "", &team, &error), APL_OK);
    cr_assert_eq(apl_event_repository_create(&app.db, team.id, 0, "Event", "2026-01-01", "", "",
                                             &event, &error),
                 APL_OK);
    cr_assert_eq(
        apl_session_repository_create(&app.db, team.id, event.id, "Race", &session, &error),
        APL_OK);
    apl_driver drivers[2] = {};
    apl_membership member = {};
    cr_assert_eq(apl_driver_repository_create(&app.db, "First", "", "", &drivers[0], &error),
                 APL_OK);
    cr_assert_eq(apl_driver_repository_create(&app.db, "Second", "", "", &drivers[1], &error),
                 APL_OK);
    for (size_t i = 0; i < 2; ++i)
        cr_assert_eq(apl_membership_repository_create(&app.db, team.id, drivers[i].id, "2020-01-01",
                                                      "", "", "", &member, &error),
                     APL_OK);
    shell_state state = {};
    state.app = &app;
    state.active_team = team;
    state.active_status = APL_ACTIVE_TEAM_ACTIVE;
    state.section = APL_TUI_SECTION_EVENTS;
    state.screen = SHELL_SCREEN_SESSION_DETAIL;
    apl_tui_list_init(&state.list, 8);
    apl_tui_focus_init(&state.focus);
    cr_assert_eq(apl_session_detail_load(&app, team.id, event.id, session.id,
                                         &state.race_session_detail, &error),
                 APL_OK);
    apl_tui_list_set_count(&state.list, 2);
    ncinput enter = {.id = NCKEY_ENTER};
    ncinput escape = {.id = NCKEY_ESC};
    handle_events_input(&state, 'a');
    apl_tui_dialog_set_field_value(&state.dialog, 2, "5");
    apl_tui_dialog_set_field_value(&state.dialog, 4, "single preserved");
    apl_outcome concurrent = {.team_id = team.id,
                              .event_id = event.id,
                              .session_id = session.id,
                              .driver_id = drivers[0].id};
    apl_outcome existing = {};
    cr_assert_eq(apl_outcome_repository_create(&app.db, &concurrent, &existing, &error), APL_OK);
    int64_t first_id = existing.id;
    state.dialog.focus = state.dialog.field_count;
    handle_dialog_input(&state, &enter);
    cr_assert_eq(state.dialog_context, SHELL_DIALOG_CONTEXT_OUTCOME_CONFLICT);
    handle_dialog_input(&state, &escape);
    cr_assert_eq(state.dialog_context, SHELL_DIALOG_CONTEXT_OUTCOME_FORM);
    cr_assert_str_eq(state.dialog.fields[2].value, "5");
    cr_assert_str_eq(state.dialog.fields[4].value, "single preserved");
    cr_assert_eq(state.dialog_record_id, 0);
    state.dialog.focus = state.dialog.field_count;
    handle_dialog_input(&state, &enter);
    cr_assert_eq(state.dialog_context, SHELL_DIALOG_CONTEXT_OUTCOME_CONFLICT);
    apl_tui_dialog_select_option(&state.dialog, 1);
    handle_dialog_input(&state, &enter);
    cr_assert_eq(state.dialog_context, SHELL_DIALOG_CONTEXT_OUTCOME_FORM);
    cr_assert_eq(state.dialog_record_id, first_id);
    cr_assert_str_eq(state.dialog.fields[4].value, "single preserved");
    cr_assert_eq(apl_outcome_repository_get(&app.db, team.id, first_id, &existing, &error), APL_OK);
    cr_assert_not(existing.has_laps);
    state.dialog.focus = state.dialog.field_count;
    handle_dialog_input(&state, &enter);
    cr_assert_eq(apl_outcome_repository_get(&app.db, team.id, first_id, &existing, &error), APL_OK);
    cr_assert_eq(existing.laps, 5);
    cr_assert_str_eq(existing.notes, "single preserved");
    apl_outcome_correction *history = nullptr;
    size_t history_count = 0;
    cr_assert_eq(
        apl_outcome_correction_list(&app.db, team.id, first_id, &history, &history_count, &error),
        APL_OK);
    cr_assert_eq(history_count, 1);
    cr_assert_not(history[0].before.has_laps);
    cr_assert_eq(history[0].after.laps, 5);
    apl_outcome_correction_free_list(history);

    handle_events_input(&state, 'b');
    handle_events_input(&state, 'a');
    apl_tui_dialog_set_field_value(&state.dialog, 2, "7");
    state.dialog.focus = state.dialog.field_count;
    handle_dialog_input(&state, &enter);
    apl_tui_list_select(&state.list, 1);
    handle_events_input(&state, 'a');
    apl_tui_dialog_set_field_value(&state.dialog, 4, "batch preserved");
    state.dialog.focus = state.dialog.field_count;
    handle_dialog_input(&state, &enter);
    concurrent.driver_id = drivers[1].id;
    cr_assert_eq(apl_outcome_repository_create(&app.db, &concurrent, &existing, &error), APL_OK);
    int64_t second_id = existing.id;
    handle_events_input(&state, 'r');
    handle_events_input(&state, 's');
    cr_assert_eq(state.dialog_context, SHELL_DIALOG_CONTEXT_OUTCOME_CONFLICT);
    handle_dialog_input(&state, &escape);
    cr_assert_eq(state.screen, SHELL_SCREEN_OUTCOME_REVIEW);
    cr_assert_eq(state.outcome_batch[1].mode, APL_OUTCOME_CREATE);
    cr_assert_str_eq(state.outcome_batch[1].input.notes, "batch preserved");
    cr_assert_eq(apl_outcome_repository_get(&app.db, team.id, first_id, &existing, &error), APL_OK);
    cr_assert_eq(existing.laps, 5);
    handle_events_input(&state, 's');
    cr_assert_eq(state.dialog_context, SHELL_DIALOG_CONTEXT_OUTCOME_CONFLICT);
    apl_tui_dialog_select_option(&state.dialog, 1);
    handle_dialog_input(&state, &enter);
    cr_assert_eq(state.screen, SHELL_SCREEN_OUTCOME_REVIEW);
    cr_assert_eq(state.outcome_batch[1].mode, APL_OUTCOME_EDIT);
    cr_assert_eq(state.outcome_batch[1].input.id, second_id);
    cr_assert_str_eq(state.outcome_batch[1].input.notes, "batch preserved");
    cr_assert_eq(apl_outcome_repository_get(&app.db, team.id, second_id, &existing, &error),
                 APL_OK);
    cr_assert_not(existing.has_notes);
    handle_events_input(&state, 's');
    cr_assert_eq(state.screen, SHELL_SCREEN_SESSION_DETAIL);
    cr_assert_eq(apl_outcome_repository_get(&app.db, team.id, first_id, &existing, &error), APL_OK);
    cr_assert_eq(existing.laps, 7);
    cr_assert_eq(apl_outcome_repository_get(&app.db, team.id, second_id, &existing, &error),
                 APL_OK);
    cr_assert_str_eq(existing.notes, "batch preserved");
    cr_assert_eq(
        apl_outcome_correction_list(&app.db, team.id, second_id, &history, &history_count, &error),
        APL_OK);
    cr_assert_eq(history_count, 1);
    cr_assert_not(history[0].before.has_notes);
    cr_assert_str_eq(history[0].after.notes, "batch preserved");
    apl_outcome_correction_free_list(history);
    free_reference_lists(&state);
    cr_assert_eq(apl_test_db_close(&db, &error), APL_OK);
}
