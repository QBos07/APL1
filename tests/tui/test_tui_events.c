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
    cr_assert_eq(load_race_events(&state, &error), APL_OK);
    cr_assert_eq(state.race_event_count, 1);
    handle_events_input(&state, NCKEY_ENTER);
    cr_assert_eq(state.screen, SHELL_SCREEN_EVENT_DETAIL);
    cr_assert_eq(state.race_event_detail.session_count, 1);
    handle_events_input(&state, NCKEY_ENTER);
    cr_assert_eq(state.screen, SHELL_SCREEN_SESSION_DETAIL);
    cr_assert_eq(state.race_session_detail.row_count, 2);
    cr_assert(state.race_session_detail.rows[0].has_outcome);
    cr_assert_not(state.race_session_detail.rows[1].has_outcome);
    handle_events_input(&state, NCKEY_ENTER);
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
    free_reference_lists(&state);
    cr_assert_eq(apl_test_db_close(&db, &error), APL_OK);
}
