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
#include "app/ledger_service.h"
#include "app/membership_service.h"
#include "app/season_service.h"
#include "app/team_service.h"
#include "app/version.h"
#include "core/util.h"
#include "domain/ledger.h"
#include "domain/money.h"
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
    SHELL_SCREEN_PLACEHOLDER,
} shell_screen;

typedef enum shell_pending_action {
    SHELL_PENDING_NONE = 0,
    SHELL_PENDING_ARCHIVE_TEAM,
    SHELL_PENDING_ARCHIVE_SEASON,
    SHELL_PENDING_ARCHIVE_CAR,
    SHELL_PENDING_ARCHIVE_DRIVER,
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
} shell_dialog_context;

constexpr int APL_TUI_SCROLL_STEP = 3;
constexpr int64_t APL_TUI_DOUBLE_CLICK_MS = 500;

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
} shell_state;

void handle_dialog_input(shell_state *const state, const ncinput *const input);
void set_section(shell_state *const state, const apl_tui_section section);
void switch_section(shell_state *const state, const bool forward);
void shell_save_view_state(shell_state *const state, apl_tui_section section);
void shell_restore_view_state(shell_state *const state, apl_tui_section section);
void set_status_level(shell_state *const state, const apl_tui_status_level level,
                      const char *const format, va_list args);
[[gnu::format(printf, 2, 3)]] void set_status(shell_state *const state, const char *const format,
                                              ...);
[[gnu::format(printf, 2, 3)]] void set_status_success(shell_state *const state,
                                                      const char *const format, ...);
[[gnu::format(printf, 2, 3)]] void set_status_warning(shell_state *const state,
                                                      const char *const format, ...);
[[gnu::format(printf, 2, 3)]] void set_status_error(shell_state *const state,
                                                    const char *const format, ...);
void set_status_from_error(shell_state *const state, const char *const context,
                           const apl_error *const error);
size_t find_team_index(const shell_state *const state, const int64_t id);
const apl_team *selected_team(const shell_state *const state);
bool active_status_has_team(const apl_active_team_status status);
size_t non_archived_count(const shell_state *const state);
bool can_archive_team(const shell_state *const state, const apl_team *const team);
unsigned form_error_field(const apl_error *const error);
unsigned reference_form_error_field(const shell_dialog_context context,
                                    const apl_error *const error);
unsigned money_form_error_field(const apl_error *const error);
unsigned budget_form_error_field(const apl_error *const error);
apl_status refresh_active(shell_state *const state, apl_error *const error);
void sync_status_context(shell_state *const state);
apl_status detect_any_teams(shell_state *const state, apl_error *const error);
int64_t active_team_id(const shell_state *const state);
apl_status refresh_teams(shell_state *const state, const int64_t preferred_id,
                         apl_error *const error);
void show_error_dialog(shell_state *const state, const apl_error *const error);
void show_info_dialog(shell_state *const state, const char *const title, const char *const message);
void close_shell_dialog(shell_state *const state);
bool handle_body_list_navigation(shell_state *const state, const uint32_t id);
void append_filter_button(char *const out, const size_t out_size, unsigned *const cursor,
                          unsigned starts[4], unsigned ends[4], const unsigned index,
                          const char *const label);
bool is_enter_key(const uint32_t id);
bool text_is_numeric(const char *const text);
int64_t now_ms(void);
bool point_in_rect(const apl_tui_rect rect, const int y, const int x);
void free_reference_lists(shell_state *const state);
void open_create_dialog(shell_state *const state, const char *const name, const char *const notes);
void open_create_form(shell_state *const state);
void open_edit_form(shell_state *const state);
bool perform_create(shell_state *const state, const char *const name, const char *const notes,
                    apl_error *const error);
bool perform_update(shell_state *const state, const int64_t team_id, const char *const name,
                    const char *const notes, apl_error *const error);
void perform_archive(shell_state *const state, const int64_t team_id, apl_error *const error);
void perform_set_active(shell_state *const state, const int64_t team_id, apl_error *const error);
void toggle_archived_view(shell_state *const state, apl_error *const error);
void free_picker_teams(shell_state *const state);
void build_team_picker_dialog(shell_state *const state);
apl_status load_picker_teams(shell_state *const state, apl_error *const error);
void open_team_picker(shell_state *const state);
void rebuild_team_picker(shell_state *const state, const bool include_archived);
void finish_team_picker(shell_state *const state, const apl_tui_dialog_result result);
void draw_welcome(shell_state *const state);
void draw_team_list(shell_state *const state);
void draw_team_detail(shell_state *const state);
void open_selected_detail(shell_state *const state);
void handle_list_input(shell_state *const state, const uint32_t id);
void handle_detail_input(shell_state *const state, const uint32_t id);
void handle_welcome_input(shell_state *const state, const uint32_t id);
apl_status load_people(shell_state *const state, const int64_t preferred_id,
                       apl_error *const error);
apl_status load_driver_memberships(shell_state *const state, const int64_t driver_id,
                                   apl_error *const error);
size_t find_driver_index(const shell_state *const state, const int64_t id);
void open_driver_dialog(shell_state *const state, const bool edit, const char *const name,
                        const char *const short_name, const char *const notes);
void open_membership_add_dialog(shell_state *const state, const int64_t driver_id,
                                const char *const start_date, const char *const end_date,
                                const char *const role, const char *const notes);
void open_membership_end_dialog(shell_state *const state, const int64_t membership_id,
                                const char *const start_date, const char *const end_date);
bool perform_driver_create(shell_state *const state, const char *const name,
                           const char *const short_name, const char *const notes,
                           apl_error *const error);
bool perform_driver_update(shell_state *const state, const char *const name,
                           const char *const short_name, const char *const notes,
                           apl_error *const error);
void perform_driver_archive(shell_state *const state, const int64_t id, apl_error *const error);
bool perform_membership_add(shell_state *const state, const char *const start_date,
                            const char *const end_date, const char *const role,
                            const char *const notes, apl_error *const error);
bool perform_membership_end(shell_state *const state, const char *const end_date,
                            apl_error *const error);
void open_person_detail(shell_state *const state, const apl_driver driver);
void finish_driver_picker(shell_state *const state, const apl_tui_dialog_result result);
void open_driver_choice(shell_state *const state, apl_driver *const drivers, const size_t count,
                        const int mode);
void draw_people_list(shell_state *const state);
void draw_person_detail(shell_state *const state);
void open_selected_person(shell_state *const state);
void handle_people_list_input(shell_state *const state, const uint32_t id);
void handle_person_detail_input(shell_state *const state, const uint32_t id);
size_t find_season_index(const shell_state *const state, const int64_t id);
size_t find_car_index(const shell_state *const state, const int64_t id);
const char *selected_season_label(const shell_state *const state);
apl_status load_seasons(shell_state *const state, const int64_t preferred_id,
                        apl_error *const error);
apl_status load_cars(shell_state *const state, const int64_t preferred_id, apl_error *const error);
void open_season_dialog(shell_state *const state, const bool edit, const char *const label,
                        const char *const start_date, const char *const end_date,
                        const char *const notes);
void open_car_dialog(shell_state *const state, const bool edit, const char *const label,
                     const char *const chassis, const char *const notes);
bool perform_season_create(shell_state *const state, const char *const label,
                           const char *const start_date, const char *const end_date,
                           const char *const notes, apl_error *const error);
bool perform_season_update(shell_state *const state, const char *const label,
                           const char *const start_date, const char *const end_date,
                           const char *const notes, apl_error *const error);
void perform_season_archive(shell_state *const state, const int64_t id, apl_error *const error);
bool perform_car_create(shell_state *const state, const char *const label,
                        const char *const chassis, const char *const notes, apl_error *const error);
bool perform_car_update(shell_state *const state, const char *const label,
                        const char *const chassis, const char *const notes, apl_error *const error);
void perform_car_archive(shell_state *const state, const int64_t id, apl_error *const error);
void draw_season_list(shell_state *const state);
void draw_season_detail(shell_state *const state);
void draw_car_list(shell_state *const state);
void draw_car_detail(shell_state *const state);
void open_selected_season(shell_state *const state);
void open_selected_car(shell_state *const state);
void handle_season_list_input(shell_state *const state, const uint32_t id);
void handle_season_detail_input(shell_state *const state, const uint32_t id);
void handle_car_list_input(shell_state *const state, const uint32_t id);
void handle_car_detail_input(shell_state *const state, const uint32_t id);
const char *history_entity_filter_name(const unsigned index);
const char *history_action_filter_name(const unsigned index);
unsigned history_entity_filter_count(void);
unsigned history_action_filter_count(void);
apl_status load_history(shell_state *const state, apl_error *const error);
const char *history_entity_filter_short_name(const unsigned index);
const char *history_action_filter_short_name(const unsigned index);
const char *history_date_filter_short_name(const unsigned preset);
void append_history_filter_button(char *const out, const size_t out_size, unsigned *const cursor,
                                  unsigned starts[5], unsigned ends[5], const unsigned index,
                                  const char *const label);
void build_history_filter_bar(const shell_state *const state, char *const out,
                              const size_t out_size, unsigned starts[5], unsigned ends[5]);
void fit_text_with_ellipsis(char *const out, const size_t out_size, const char *const text,
                            const size_t width);
void draw_history_list(shell_state *const state);
void reload_history_with_status(shell_state *const state, const char *const message);
void handle_history_input(shell_state *const state, const uint32_t id);
apl_ledger_kind money_filter_kind(const unsigned index);
const char *money_filter_kind_name(const unsigned index);
const char *money_date_filter_name(const unsigned preset);
void money_date_from(const unsigned preset, char *const out, const size_t out_size);
void money_cycle_date_filter(unsigned *const preset);
size_t find_ledger_index(const shell_state *const state, const int64_t id);
apl_status load_ledger(shell_state *const state, const int64_t preferred_id,
                       apl_error *const error);
apl_status load_budget_report(shell_state *const state, apl_error *const error);
apl_status load_entry_activity(shell_state *const state, const int64_t entry_id,
                               apl_error *const error);
void open_money_currency_dialog(shell_state *const state);
void open_money_entry_dialog(shell_state *const state, const apl_ledger_kind kind);
void store_money_entry_values(shell_state *const state);
void open_money_review_dialog(shell_state *const state);
void open_money_correct_dialog(shell_state *const state, const apl_ledger_entry *const entry,
                               const char *const replacement_amount, const char *const date,
                               const char *const category, const char *const reason);
void store_money_correction_values(shell_state *const state);
void open_money_correction_review(shell_state *const state);
bool perform_money_post(shell_state *const state, apl_error *const error);
bool perform_money_correction(shell_state *const state, apl_error *const error);
void open_budget_form(shell_state *const state, const bool edit);
void store_budget_values(shell_state *const state);
void open_budget_review_dialog(shell_state *const state);
bool perform_budget_save(shell_state *const state, apl_error *const error);
void build_money_filter_bar(const shell_state *const state, char *const out, const size_t out_size,
                            unsigned starts[4], unsigned ends[4]);
void build_budget_filter_bar(const shell_state *const state, char *const out, const size_t out_size,
                             unsigned starts[4], unsigned ends[4]);
void draw_money_list(shell_state *const state);
void draw_money_detail(shell_state *const state);
void draw_budget_list(shell_state *const state);
void open_money_filter_dialog(shell_state *const state);
void open_selected_money_entry(shell_state *const state);
void open_selected_budget(shell_state *const state);
void handle_money_list_input(shell_state *const state, const uint32_t id);
void handle_money_detail_input(shell_state *const state, const uint32_t id);
void handle_budget_list_input(shell_state *const state, const uint32_t id);

#endif /* APL_TUI_SHELL_INTERNAL_H */
