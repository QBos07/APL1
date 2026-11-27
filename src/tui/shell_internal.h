#ifndef APL_TUI_SHELL_INTERNAL_H
#define APL_TUI_SHELL_INTERNAL_H

#include <limits.h>
#include <notcurses/notcurses.h>
#include <stdarg.h>
#include <stdckdint.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "app/activity_service.h"
#include "app/budget_service.h"
#include "app/car_service.h"
#include "app/driver_service.h"
#include "app/event_detail.h"
#include "app/ledger_service.h"
#include "app/lifecycle_service.h"
#include "app/membership_service.h"
#include "app/part_type_service.h"
#include "app/part_unit_service.h"
#include "app/project_service.h"
#include "app/season_service.h"
#include "app/team_service.h"
#include "app/version.h"
#include "db/outcome_correction_repository.h"
#include "core/util.h"
#include "domain/ledger.h"
#include "domain/money.h"
#include "domain/part.h"
#include "domain/record.h"
#include "tui/action.h"
#include "tui/command.h"
#include "tui/command_line.h"
#include "tui/dialog.h"
#include "tui/focus.h"
#include "tui/frame.h"
#include "tui/list.h"
#include "tui/menu.h"
#include "tui/navigation.h"
#include "tui/status.h"
#include "tui/text.h"
#include "tui/theme.h"

typedef enum shell_screen {
    SHELL_SCREEN_WELCOME = 0,
    SHELL_SCREEN_TEAM_LIST,
    SHELL_SCREEN_TEAM_DETAIL,
    SHELL_SCREEN_SEASON_LIST,
    SHELL_SCREEN_SEASON_DETAIL,
    SHELL_SCREEN_CAR_LIST,
    SHELL_SCREEN_CAR_DETAIL,
    SHELL_SCREEN_PEOPLE_LIST,
    SHELL_SCREEN_PERSON_DETAIL,
    SHELL_SCREEN_HISTORY_LIST,
    SHELL_SCREEN_MONEY_LIST,
    SHELL_SCREEN_MONEY_DETAIL,
    SHELL_SCREEN_BUDGET_LIST,
    SHELL_SCREEN_PROJECT_LIST,
    SHELL_SCREEN_PROJECT_DETAIL,
    SHELL_SCREEN_PARTS_FITMENT,
    SHELL_SCREEN_PART_TYPE_LIST,
    SHELL_SCREEN_PART_TYPE_DETAIL,
    SHELL_SCREEN_PART_UNIT_LIST,
    SHELL_SCREEN_PART_UNIT_DETAIL,
    SHELL_SCREEN_EVENT_LIST,
    SHELL_SCREEN_EVENT_DETAIL,
    SHELL_SCREEN_SESSION_DETAIL,
    SHELL_SCREEN_OUTCOME_HISTORY,
    SHELL_SCREEN_PLACEHOLDER,
} shell_screen;

typedef enum shell_pending_action {
    SHELL_PENDING_NONE = 0,
    SHELL_PENDING_ARCHIVE_TEAM,
    SHELL_PENDING_ARCHIVE_SEASON,
    SHELL_PENDING_ARCHIVE_CAR,
    SHELL_PENDING_ARCHIVE_DRIVER,
    SHELL_PENDING_ARCHIVE_PROJECT,
    SHELL_PENDING_ARCHIVE_BUDGET,
    SHELL_PENDING_ARCHIVE_PART_TYPE,
    SHELL_PENDING_ARCHIVE_PART_UNIT,
    SHELL_PENDING_APPLY_LIFECYCLE,
    SHELL_PENDING_APPLY_REPLACE,
} shell_pending_action;

typedef enum shell_dialog_context {
    SHELL_DIALOG_CONTEXT_NONE = 0,
    SHELL_DIALOG_CONTEXT_TEAM_PICKER,
    SHELL_DIALOG_CONTEXT_TEAM_CREATE,
    SHELL_DIALOG_CONTEXT_TEAM_EDIT,
    SHELL_DIALOG_CONTEXT_SEASON_CREATE,
    SHELL_DIALOG_CONTEXT_SEASON_EDIT,
    SHELL_DIALOG_CONTEXT_CAR_CREATE,
    SHELL_DIALOG_CONTEXT_CAR_EDIT,
    SHELL_DIALOG_CONTEXT_DRIVER_CREATE,
    SHELL_DIALOG_CONTEXT_DRIVER_EDIT,
    SHELL_DIALOG_CONTEXT_MEMBERSHIP_ADD,
    SHELL_DIALOG_CONTEXT_MEMBERSHIP_END,
    SHELL_DIALOG_CONTEXT_DRIVER_PICKER,
    SHELL_DIALOG_CONTEXT_ACTIVITY_DETAIL,
    SHELL_DIALOG_CONTEXT_MONEY_CURRENCY,
    SHELL_DIALOG_CONTEXT_MONEY_ENTRY,
    SHELL_DIALOG_CONTEXT_MONEY_REVIEW,
    SHELL_DIALOG_CONTEXT_MONEY_CORRECT,
    SHELL_DIALOG_CONTEXT_MONEY_CORRECT_REVIEW,
    SHELL_DIALOG_CONTEXT_MONEY_FILTER,
    SHELL_DIALOG_CONTEXT_BUDGET_FORM,
    SHELL_DIALOG_CONTEXT_BUDGET_REVIEW,
    SHELL_DIALOG_CONTEXT_PROJECT_CREATE,
    SHELL_DIALOG_CONTEXT_PROJECT_EDIT,
    SHELL_DIALOG_CONTEXT_PROJECT_UPDATE,
    SHELL_DIALOG_CONTEXT_PROJECT_FILTER,
    SHELL_DIALOG_CONTEXT_PART_TYPE_CREATE,
    SHELL_DIALOG_CONTEXT_PART_TYPE_EDIT,
    SHELL_DIALOG_CONTEXT_PART_UNIT_CREATE,
    SHELL_DIALOG_CONTEXT_PART_UNIT_EDIT,
    SHELL_DIALOG_CONTEXT_PART_FILTER,
    SHELL_DIALOG_CONTEXT_PART_INSTALL,
    SHELL_DIALOG_CONTEXT_PART_REMOVE,
    SHELL_DIALOG_CONTEXT_PART_REPAIR,
    SHELL_DIALOG_CONTEXT_PART_DISPOSE,
    SHELL_DIALOG_CONTEXT_PART_REPLACE,
    SHELL_DIALOG_CONTEXT_PART_REPLACE_REVIEW,
} shell_dialog_context;

static constexpr int APL_TUI_SCROLL_STEP = 3;
static constexpr int64_t APL_TUI_DOUBLE_CLICK_MS = 500;

/* Keep this private, process-lifetime state grouped by feature; optimizing the
 * padding would make cross-screen state brittle for a negligible one-instance
 * memory saving. */
// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding)
typedef struct shell_state {
    const apl_app *app;
    apl_team *teams;
    size_t team_count;
    apl_season *seasons;
    size_t season_count;
    int64_t selected_season_id;
    apl_car *cars;
    size_t car_count;
    apl_driver *drivers;
    size_t driver_count;
    apl_membership *memberships;
    size_t membership_count;
    apl_activity_event *events;
    size_t event_count;
    apl_event *race_events;
    size_t race_event_count;
    apl_event_detail race_event_detail;
    apl_session_detail race_session_detail;
    apl_outcome_correction *outcome_corrections;
    size_t outcome_correction_count;
    size_t event_return_index, session_return_index, outcome_return_index;
    size_t outcome_history_offset;
    bool outcome_history_after;
    apl_ledger_entry *entries;
    size_t entry_count;
    apl_ledger_entry entry_detail;
    apl_budget_report_row *budget_rows;
    size_t budget_row_count;
    apl_activity_event *entry_events;
    size_t entry_event_count;
    int64_t dialog_team_id;
    int64_t dialog_record_id;
    int64_t dialog_driver_id;
    apl_team *picker_teams;
    size_t picker_team_count;
    int64_t pending_team_id;
    int64_t pending_record_id;
    int64_t pending_driver_id;
    int64_t last_click_team_id;
    int64_t last_click_ms;
    int64_t last_mouse_ms;
    apl_tui_list list;
    apl_tui_list section_lists[APL_TUI_SECTION_COUNT];
    apl_tui_list tabs;
    apl_activity_filter history_filter;
    apl_tui_frame frame;
    int64_t driver_choice_ids[APL_TUI_DIALOG_OPTIONS_MAX];
    apl_team active_team;
    apl_team detail;
    apl_season season_detail;
    apl_driver driver_detail;
    apl_car car_detail;
    apl_tui_section section;
    shell_screen screen;
    apl_active_team_status active_status;
    unsigned history_date_preset;
    unsigned history_action_filter;
    unsigned history_entity_filter;
    shell_dialog_context dialog_context;
    int driver_choice_mode;
    unsigned driver_choice_count;
    shell_pending_action pending;
    uint32_t last_mouse_id;
    int last_mouse_y;
    int last_mouse_x;
    apl_tui_focus focus;
    apl_tui_menu menu;
    apl_tui_status status;
    apl_tui_command_line command_line;
    apl_tui_dialog dialog;
    bool include_archived;
    bool any_teams;
    bool include_archived_seasons;
    bool include_archived_cars;
    bool include_archived_drivers;
    bool people_all_teams;
    bool history_all_teams;
    apl_tui_theme theme;
    bool picker_include_archived;
    bool team_focus;
    bool quit;
    unsigned money_date_preset;
    unsigned money_kind_filter;
    unsigned budget_date_preset;
    apl_ledger_kind pending_ledger_kind;
    int64_t pending_season_id;
    char money_category_filter[APL_LEDGER_CATEGORY_MAX];
    char budget_category_filter[APL_LEDGER_CATEGORY_MAX];
    char pending_amount[64];
    char pending_date[APL_REFERENCE_DATE_MAX];
    char pending_category[APL_LEDGER_CATEGORY_MAX];
    char pending_description[APL_LEDGER_DESCRIPTION_MAX];
    char pending_start[APL_TUI_COMMAND_WORD_MAX];
    char pending_end[APL_TUI_COMMAND_WORD_MAX];
    char pending_role[APL_TUI_COMMAND_WORD_MAX];
    char pending_notes[APL_TUI_COMMAND_WORD_MAX];
    char pending_currency[APL_MONEY_CURRENCY_MAX];
    char pending_project[APL_REFERENCE_LABEL_MAX];
    char pending_budget_project[APL_REFERENCE_LABEL_MAX];
    bool include_archived_budgets;
    int64_t budget_project_filter;
    apl_project *projects;
    size_t project_count;
    apl_project project_detail;
    apl_project_update *project_updates;
    size_t project_update_count;
    apl_ledger_entry *project_entries;
    size_t project_entry_count;
    apl_project_cost project_cost;
    bool include_archived_projects;
    char project_status_filter[APL_PROJECT_STATUS_MAX];
    char project_category_filter[APL_PROJECT_CATEGORY_MAX];
    int64_t project_season_filter;
    int64_t dialog_project_id;
    bool project_return_pending;
    apl_tui_section project_return_section;
    shell_screen project_return_screen;
    int64_t project_return_id;
    apl_part_type *part_types;
    size_t part_type_count;
    apl_part_unit *part_units;
    size_t part_unit_count;
    apl_fitment_row *fitment_rows;
    size_t fitment_count;
    apl_lifecycle_history_entry *unit_events;
    size_t unit_event_count;
    apl_part_type part_type_detail;
    apl_part_unit part_unit_detail;
    apl_fitment_row part_unit_fitment;
    bool part_unit_fitment_known;
    bool part_unit_is_fitted;
    int64_t dialog_part_type_id;
    int64_t dialog_part_unit_id;
    int64_t dialog_car_id;
    bool parts_return_pending;
    int64_t parts_return_unit_id;
    bool include_archived_part_types;
    bool include_archived_part_units;
    char part_type_category_filter[APL_PART_TYPE_CATEGORY_MAX];
    int64_t part_unit_type_filter;
    char parts_action_filter[APL_LIFECYCLE_EVENT_MAX];
    int64_t parts_car_filter;
    char parts_position_filter[APL_LIFECYCLE_POSITION_MAX];
    char parts_from_date[APL_REFERENCE_DATE_MAX];
    char parts_to_date[APL_REFERENCE_DATE_MAX];
    char parts_search[APL_PART_UNIT_SERIAL_MAX];
    char parts_type_filter[APL_PART_TYPE_CATEGORY_MAX];
    char replace_old_unit[APL_TUI_DIALOG_FIELD_VALUE_MAX];
    char replace_new_unit[APL_TUI_DIALOG_FIELD_VALUE_MAX];
    char replace_car[APL_REFERENCE_LABEL_MAX];
    char replace_position[APL_LIFECYCLE_POSITION_MAX];
    char replace_date[APL_REFERENCE_DATE_MAX];
    char replace_event[APL_LIFECYCLE_EVENT_MAX];
    char replace_reason[APL_LIFECYCLE_REASON_MAX];
    char replace_usage[APL_LIFECYCLE_USAGE_MAX];
    char replace_notes[APL_REFERENCE_NOTES_MAX];
    char replace_transaction[32];
    bool replacement_create_pending;
    bool replacement_unit_created_inline;
    apl_lifecycle_action pending_lifecycle_action;
    char pending_lifecycle_unit[APL_TUI_DIALOG_FIELD_VALUE_MAX];
    char pending_lifecycle_car[APL_REFERENCE_LABEL_MAX];
    char pending_lifecycle_position[APL_LIFECYCLE_POSITION_MAX];
    char pending_lifecycle_date[APL_REFERENCE_DATE_MAX];
    char pending_lifecycle_event[APL_LIFECYCLE_EVENT_MAX];
    char pending_lifecycle_reason[APL_LIFECYCLE_REASON_MAX];
    char pending_lifecycle_usage[APL_LIFECYCLE_USAGE_MAX];
    char pending_lifecycle_notes[APL_REFERENCE_NOTES_MAX];
} shell_state;

void handle_dialog_input(shell_state *state, const ncinput *input);
void set_section(shell_state *state, apl_tui_section section);
void switch_section(shell_state *state, bool forward);
void shell_save_view_state(shell_state *state, apl_tui_section section);
void shell_restore_view_state(shell_state *state, apl_tui_section section);
void set_status_level(shell_state *state, apl_tui_status_level level, const char *format,
                      va_list args);
[[gnu::format(printf, 2, 3)]] void set_status(shell_state *state, const char *format, ...);
[[gnu::format(printf, 2, 3)]] void set_status_success(shell_state *state, const char *format, ...);
[[gnu::format(printf, 2, 3)]] void set_status_warning(shell_state *state, const char *format, ...);
[[gnu::format(printf, 2, 3)]] void set_status_error(shell_state *state, const char *format, ...);
void set_status_from_error(shell_state *state, const char *context, const apl_error *error);
size_t find_team_index(const shell_state *state, int64_t id);
const apl_team *selected_team(const shell_state *state);
bool active_status_has_team(apl_active_team_status status);
size_t non_archived_count(const shell_state *state);
bool can_archive_team(const shell_state *state, const apl_team *team);
unsigned form_error_field(const apl_error *error);
unsigned reference_form_error_field(shell_dialog_context context, const apl_error *error);
unsigned money_form_error_field(const apl_error *error);
unsigned budget_form_error_field(const apl_error *error);
apl_status refresh_active(shell_state *state, apl_error *error);
void sync_status_context(shell_state *state);
apl_status detect_any_teams(shell_state *state, apl_error *error);
int64_t active_team_id(const shell_state *state);
apl_status refresh_teams(shell_state *state, int64_t preferred_id, apl_error *error);
void show_error_dialog(shell_state *state, const apl_error *error);
void show_info_dialog(shell_state *state, const char *title, const char *message);
void close_shell_dialog(shell_state *state);
bool handle_body_list_navigation(shell_state *state, uint32_t id);
apl_status load_race_events(shell_state *state, apl_error *error);
void draw_events_screen(shell_state *state);
void handle_events_input(shell_state *state, uint32_t id);
void append_filter_button(char *out, size_t out_size, unsigned *cursor, unsigned starts[4],
                          unsigned ends[4], unsigned index, const char *label);
bool is_enter_key(uint32_t id);
bool text_is_numeric(const char *text);
int64_t now_ms(void);
bool point_in_rect(apl_tui_rect rect, int y, int x);
void free_reference_lists(shell_state *state);
void open_create_dialog(shell_state *state, const char *name, const char *notes);
void open_create_form(shell_state *state);
void open_edit_form(shell_state *state);
bool perform_create(shell_state *state, const char *name, const char *notes, apl_error *error);
bool perform_update(shell_state *state, int64_t team_id, const char *name, const char *notes,
                    apl_error *error);
void perform_archive(shell_state *state, int64_t team_id, apl_error *error);
void perform_set_active(shell_state *state, int64_t team_id, apl_error *error);
void toggle_archived_view(shell_state *state, apl_error *error);
void free_picker_teams(shell_state *state);
void build_team_picker_dialog(shell_state *state);
apl_status load_picker_teams(shell_state *state, apl_error *error);
void open_team_picker(shell_state *state);
void rebuild_team_picker(shell_state *state, bool include_archived);
void finish_team_picker(shell_state *state, apl_tui_dialog_result result);
void draw_welcome(shell_state *state);
void draw_team_list(shell_state *state);
void draw_team_detail(shell_state *state);
void open_selected_detail(shell_state *state);
void handle_list_input(shell_state *state, uint32_t id);
void handle_detail_input(shell_state *state, uint32_t id);
void handle_welcome_input(shell_state *state, uint32_t id);
apl_status load_people(shell_state *state, int64_t preferred_id, apl_error *error);
apl_status load_driver_memberships(shell_state *state, int64_t driver_id, apl_error *error);
size_t find_driver_index(const shell_state *state, int64_t id);
void open_driver_dialog(shell_state *state, bool edit, const char *name, const char *short_name,
                        const char *notes);
void open_membership_add_dialog(shell_state *state, int64_t driver_id, const char *start_date,
                                const char *end_date, const char *role, const char *notes);
void open_membership_end_dialog(shell_state *state, int64_t membership_id, const char *start_date,
                                const char *end_date);
bool perform_driver_create(shell_state *state, const char *name, const char *short_name,
                           const char *notes, apl_error *error);
bool perform_driver_update(shell_state *state, const char *name, const char *short_name,
                           const char *notes, apl_error *error);
void perform_driver_archive(shell_state *state, int64_t id, apl_error *error);
bool perform_membership_add(shell_state *state, const char *start_date, const char *end_date,
                            const char *role, const char *notes, apl_error *error);
bool perform_membership_end(shell_state *state, const char *end_date, apl_error *error);
void open_person_detail(shell_state *state, apl_driver driver);
void finish_driver_picker(shell_state *state, apl_tui_dialog_result result);
void open_driver_choice(shell_state *state, apl_driver *drivers, size_t count, int mode);
void draw_people_list(shell_state *state);
void draw_person_detail(shell_state *state);
void open_selected_person(shell_state *state);
void handle_people_list_input(shell_state *state, uint32_t id);
void handle_person_detail_input(shell_state *state, uint32_t id);
size_t find_season_index(const shell_state *state, int64_t id);
size_t find_car_index(const shell_state *state, int64_t id);
const char *selected_season_label(const shell_state *state);
apl_status load_seasons(shell_state *state, int64_t preferred_id, apl_error *error);
apl_status load_cars(shell_state *state, int64_t preferred_id, apl_error *error);
void open_season_dialog(shell_state *state, bool edit, const char *label, const char *start_date,
                        const char *end_date, const char *notes);
void open_car_dialog(shell_state *state, bool edit, const char *label, const char *chassis,
                     const char *notes);
bool perform_season_create(shell_state *state, const char *label, const char *start_date,
                           const char *end_date, const char *notes, apl_error *error);
bool perform_season_update(shell_state *state, const char *label, const char *start_date,
                           const char *end_date, const char *notes, apl_error *error);
void perform_season_archive(shell_state *state, int64_t id, apl_error *error);
bool perform_car_create(shell_state *state, const char *label, const char *chassis,
                        const char *notes, apl_error *error);
bool perform_car_update(shell_state *state, const char *label, const char *chassis,
                        const char *notes, apl_error *error);
void perform_car_archive(shell_state *state, int64_t id, apl_error *error);
void draw_season_list(shell_state *state);
void draw_season_detail(shell_state *state);
void draw_car_list(shell_state *state);
void draw_car_detail(shell_state *state);
void open_selected_season(shell_state *state);
void open_selected_car(shell_state *state);
void handle_season_list_input(shell_state *state, uint32_t id);
void handle_season_detail_input(shell_state *state, uint32_t id);
void handle_car_list_input(shell_state *state, uint32_t id);
void handle_car_detail_input(shell_state *state, uint32_t id);
const char *history_entity_filter_name(unsigned index);
const char *history_action_filter_name(unsigned index);
unsigned history_entity_filter_count(void);
unsigned history_action_filter_count(void);
apl_status load_history(shell_state *state, apl_error *error);
const char *history_entity_filter_short_name(unsigned index);
const char *history_action_filter_short_name(unsigned index);
const char *history_date_filter_short_name(unsigned preset);
void append_history_filter_button(char *out, size_t out_size, unsigned *cursor, unsigned starts[5],
                                  unsigned ends[5], unsigned index, const char *label);
void build_history_filter_bar(const shell_state *state, char *out, size_t out_size,
                              unsigned starts[5], unsigned ends[5]);
void fit_text_with_ellipsis(char *out, size_t out_size, const char *text, size_t width);
void draw_history_list(shell_state *state);
void reload_history_with_status(shell_state *state, const char *message);
void handle_history_input(shell_state *state, uint32_t id);
apl_ledger_kind money_filter_kind(unsigned index);
const char *money_filter_kind_name(unsigned index);
const char *money_date_filter_name(unsigned preset);
void money_date_from(unsigned preset, char *out, size_t out_size);
void money_cycle_date_filter(unsigned *preset);
size_t find_ledger_index(const shell_state *state, int64_t id);
apl_status load_ledger(shell_state *state, int64_t preferred_id, apl_error *error);
apl_status load_budget_report(shell_state *state, apl_error *error);
apl_status load_entry_activity(shell_state *state, int64_t entry_id, apl_error *error);
void open_money_currency_dialog(shell_state *state);
void open_money_entry_dialog(shell_state *state, apl_ledger_kind kind);
void store_money_entry_values(shell_state *state);
void open_money_review_dialog(shell_state *state);
void open_money_correct_dialog(shell_state *state, const apl_ledger_entry *entry,
                               const char *replacement_amount, const char *date,
                               const char *category, const char *reason);
void store_money_correction_values(shell_state *state);
void open_money_correction_review(shell_state *state);
bool perform_money_post(shell_state *state, apl_error *error);
bool perform_money_correction(shell_state *state, apl_error *error);
void open_budget_form(shell_state *state, bool edit);
void store_budget_values(shell_state *state);
void open_budget_review_dialog(shell_state *state);
bool perform_budget_save(shell_state *state, apl_error *error);
void build_money_filter_bar(const shell_state *state, char *out, size_t out_size,
                            unsigned starts[4], unsigned ends[4]);
void build_budget_filter_bar(const shell_state *state, char *out, size_t out_size,
                             unsigned starts[4], unsigned ends[4]);
void draw_money_list(shell_state *state);
void draw_money_detail(shell_state *state);
void draw_budget_list(shell_state *state);
void open_money_filter_dialog(shell_state *state);
void open_selected_money_entry(shell_state *state);
void open_selected_budget(shell_state *state);
void select_first_budget_plan(shell_state *state);
void perform_budget_archive(shell_state *state, int64_t id, apl_error *error);
void open_money_entry_dialog_for_project(shell_state *state, apl_ledger_kind kind,
                                         const char *project_title);
void handle_money_list_input(shell_state *state, uint32_t id);
void handle_money_detail_input(shell_state *state, uint32_t id);
void handle_budget_list_input(shell_state *state, uint32_t id);

size_t find_project_index(const shell_state *state, int64_t id);
apl_status load_projects(shell_state *state, int64_t preferred_id, apl_error *error);
apl_status load_project_detail(shell_state *state, int64_t project_id, apl_error *error);
void open_project_detail(shell_state *state, const apl_project project);
void open_selected_project(shell_state *state);
void draw_project_list(shell_state *state);
void draw_project_detail(shell_state *state);
void handle_project_list_input(shell_state *state, uint32_t id);
void handle_project_detail_input(shell_state *state, uint32_t id);
void build_project_filter_bar(const shell_state *state, char *out, size_t out_size,
                              unsigned starts[5], unsigned ends[5]);

void open_project_dialog(shell_state *state, bool edit);
/** Open the create form with partial values from a future command route. */
void open_project_create_form_prefilled(shell_state *state, const char *title, const char *category,
                                        const char *description, const char *status,
                                        const char *target_date, const char *actual_date,
                                        const char *season_name, const char *car_name,
                                        const char *notes);
void open_project_update_dialog(shell_state *state);
void open_project_filter_dialog(shell_state *state);
unsigned project_form_error_field(const apl_error *error);
unsigned project_update_form_error_field(const apl_error *error);
bool perform_project_create(shell_state *state, const char *title, const char *category,
                            const char *description, const char *status, const char *target_date,
                            const char *actual_date, const char *season_name, const char *car_name,
                            const char *notes, apl_error *error);
bool perform_project_update(shell_state *state, const char *title, const char *category,
                            const char *description, const char *status, const char *target_date,
                            const char *actual_date, const char *season_name, const char *car_name,
                            const char *notes, apl_error *error);
bool perform_project_log_update(shell_state *state, const char *update_date, const char *summary,
                                const char *status_change, const char *notes, apl_error *error);
void perform_project_archive(shell_state *state, int64_t id, apl_error *error);
apl_status project_resolve_season(shell_state *state, const char *text, int64_t *out,
                                  apl_error *error);
apl_status project_resolve_car(shell_state *state, const char *text, int64_t *out,
                               apl_error *error);
apl_status project_resolve_title(shell_state *state, const char *text, int64_t *out,
                                 apl_error *error);

/* --- Parts (Phase 6) --- */
apl_status load_parts_fitment(shell_state *state, apl_error *error);
bool reload_parts_with_filters(shell_state *state, apl_error *error);
apl_status load_part_units(shell_state *state, int64_t preferred_id, apl_error *error);
apl_status load_part_types(shell_state *state, int64_t preferred_id, apl_error *error);
apl_status load_part_unit_detail(shell_state *state, int64_t unit_id, apl_error *error);
void open_part_unit_detail(shell_state *state, apl_part_unit unit);
void open_part_type_detail(shell_state *state, apl_part_type type);
void open_selected_part_unit(shell_state *state);
void open_linked_part_transaction(shell_state *state);
void draw_parts_fitment(shell_state *state);
void draw_part_type_list(shell_state *state);
void draw_part_type_detail(shell_state *state);
void draw_part_unit_list(shell_state *state);
void draw_part_unit_detail(shell_state *state);
void handle_parts_fitment_input(shell_state *state, uint32_t id);
void handle_part_type_list_input(shell_state *state, uint32_t id);
void handle_part_type_detail_input(shell_state *state, uint32_t id);
void handle_part_unit_list_input(shell_state *state, uint32_t id);
void handle_part_unit_detail_input(shell_state *state, uint32_t id);
void build_parts_filter_bar(const shell_state *state, char *out, size_t out_size,
                            unsigned starts[6], unsigned ends[6]);
void open_part_type_dialog(shell_state *state, bool edit);
void open_part_unit_dialog(shell_state *state, bool edit);
void open_lifecycle_dialog(shell_state *state, apl_lifecycle_action action);
void open_replace_dialog_direct(shell_state *state);
void open_parts_filter_dialog(shell_state *state);
bool perform_part_type_create(shell_state *state, const char *category, const char *name,
                              const char *description, const char *notes, apl_error *error);
bool perform_part_type_update(shell_state *state, const char *category, const char *name,
                              const char *description, const char *notes, apl_error *error);
void perform_part_type_archive(shell_state *state, int64_t id, apl_error *error);
bool perform_part_unit_create(shell_state *state, const char *type_name, const char *serial,
                              const char *part_number, const char *condition, const char *notes,
                              apl_error *error);
bool perform_part_unit_update(shell_state *state, int64_t type_id, const char *serial,
                              const char *part_number, const char *condition, const char *notes,
                              apl_error *error);
void perform_part_unit_archive(shell_state *state, int64_t id, apl_error *error);
bool perform_lifecycle_action(shell_state *state, apl_lifecycle_action action, const char *unit,
                              const char *car, const char *position, const char *event_date,
                              const char *event_label, const char *reason, const char *usage,
                              const char *notes, apl_error *error);
bool perform_replace(shell_state *state, const char *old_unit, const char *new_unit,
                     const char *car, const char *position, const char *event_date,
                     const char *event_label, const char *reason, const char *usage,
                     const char *notes, const char *transaction, apl_error *error);
bool apply_parts_filter(shell_state *state, const char *action, const char *car,
                        const char *position, const char *from_date, const char *to_date,
                        const char *search, const char *type, apl_error *error);
unsigned part_type_form_error_field(const apl_error *error);
unsigned part_unit_form_error_field(const apl_error *error);
unsigned lifecycle_form_error_field(const apl_error *error);
apl_status part_resolve_unit_by_id(shell_state *state, const char *text, int64_t *out,
                                   apl_error *error);
apl_status part_resolve_car_by_name(shell_state *state, const char *text, int64_t *out,
                                    apl_error *error);
apl_status part_resolve_type_id_for_dialog(shell_state *state, const char *text, int64_t *out,
                                           apl_error *error);

#endif /*APL_TUI_SHELL_INTERNAL_H */
