#include "tui/action.h"

#include <stdio.h>

#include "tui/text.h"

static void join_positionals(const apl_tui_command_request *const request, char *const out,
                             const size_t out_size)
{
    out[0] = '\0';
    for (unsigned index = 0; index < request->positional_count; ++index) {
        if (index > 0) {
            (void)apl_tui_text_append(out, out_size, " ");
        }
        (void)apl_tui_text_append(out, out_size, request->positionals[index]);
    }
}

apl_status apl_tui_action_from_request(const apl_tui_command_request *const request,
                                       apl_tui_action *const out_action, apl_error *const error)
{
    if (request == nullptr || out_action == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "command action arguments are nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_action = (apl_tui_action){};

    switch (request->id) {
    case APL_TUI_COMMAND_VIEW: {
        apl_tui_section section = APL_TUI_SECTION_COUNT;
        if (request->positional_count == 0 ||
            !apl_tui_section_from_name(request->positionals[0], &section)) {
            apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "unknown view target");
            return APL_ERR_INVALID_ARGUMENT;
        }
        out_action->kind = APL_TUI_ACTION_SWITCH_SECTION;
        out_action->section = section;
        return APL_OK;
    }
    case APL_TUI_COMMAND_OVERVIEW:
        if (request->positional_count > 0 &&
            apl_tui_text_equal_ci(request->positionals[0], "all")) {
            out_action->kind = APL_TUI_ACTION_OVERVIEW_ALL;
        } else {
            out_action->kind = APL_TUI_ACTION_SWITCH_SECTION;
            out_action->section = APL_TUI_SECTION_OVERVIEW;
        }
        return APL_OK;
    case APL_TUI_COMMAND_TEAM_LIST:
        out_action->kind = APL_TUI_ACTION_OPEN_TEAM_LIST;
        return APL_OK;
    case APL_TUI_COMMAND_TEAM_SWITCH:
        if (request->positional_count == 0) {
            out_action->kind = APL_TUI_ACTION_OPEN_TEAM_PICKER;
            return APL_OK;
        }
        out_action->kind = APL_TUI_ACTION_TEAM_SWITCH_TARGET;
        (void)snprintf(out_action->value, sizeof(out_action->value), "%s", request->positionals[0]);
        return APL_OK;
    case APL_TUI_COMMAND_TEAM_CREATE: {
        const char *const option_name = apl_tui_command_option_value(request, "name");
        const char *const name =
            (request->positional_count > 0) ? request->positionals[0] : option_name;
        if (name == nullptr || name[0] == '\0') {
            out_action->kind = APL_TUI_ACTION_OPEN_TEAM_CREATE_FORM;
            return APL_OK;
        }
        out_action->kind = APL_TUI_ACTION_CREATE_TEAM_DIRECT;
        (void)snprintf(out_action->value, sizeof(out_action->value), "%s", name);
        const char *const notes = apl_tui_command_option_value(request, "notes");
        if (notes != nullptr) {
            (void)snprintf(out_action->notes, sizeof(out_action->notes), "%s", notes);
        }
        return APL_OK;
    }
    case APL_TUI_COMMAND_DRIVER_LIST:
        out_action->kind = APL_TUI_ACTION_OPEN_DRIVER_LIST;
        return APL_OK;
    case APL_TUI_COMMAND_DRIVER_NEW: {
        const char *const name = apl_tui_command_option_value(request, "name");
        const char *const short_name = apl_tui_command_option_value(request, "short");
        const char *const notes = apl_tui_command_option_value(request, "notes");
        if (name == nullptr || name[0] == '\0') {
            out_action->kind = APL_TUI_ACTION_OPEN_DRIVER_CREATE_FORM;
            if (short_name != nullptr) {
                (void)snprintf(out_action->notes, sizeof(out_action->notes), "%s", short_name);
            }
            if (notes != nullptr) {
                (void)snprintf(out_action->role, sizeof(out_action->role), "%s", notes);
            }
            return APL_OK;
        }
        out_action->kind = APL_TUI_ACTION_CREATE_DRIVER_DIRECT;
        (void)snprintf(out_action->value, sizeof(out_action->value), "%s", name);
        if (short_name != nullptr) {
            (void)snprintf(out_action->notes, sizeof(out_action->notes), "%s", short_name);
        }
        if (notes != nullptr) {
            (void)snprintf(out_action->role, sizeof(out_action->role), "%s", notes);
        }
        return APL_OK;
    }
    case APL_TUI_COMMAND_DRIVER_SHOW:
        out_action->kind = APL_TUI_ACTION_OPEN_DRIVER_SHOW;
        (void)snprintf(out_action->value, sizeof(out_action->value), "%s", request->positionals[0]);
        return APL_OK;
    case APL_TUI_COMMAND_MEMBERSHIP_ADD: {
        const char *const driver = apl_tui_command_option_value(request, "driver");
        const char *const start = apl_tui_command_option_value(request, "start");
        const char *const end = apl_tui_command_option_value(request, "end");
        const char *const role = apl_tui_command_option_value(request, "role");
        const char *const notes = apl_tui_command_option_value(request, "notes");
        if (driver == nullptr || start == nullptr) {
            out_action->kind = APL_TUI_ACTION_OPEN_MEMBERSHIP_ADD_FORM;
            if (driver != nullptr) {
                (void)snprintf(out_action->driver, sizeof(out_action->driver), "%s", driver);
            }
            if (start != nullptr) {
                (void)snprintf(out_action->start, sizeof(out_action->start), "%s", start);
            }
            if (end != nullptr) {
                (void)snprintf(out_action->end, sizeof(out_action->end), "%s", end);
            }
            if (role != nullptr) {
                (void)snprintf(out_action->role, sizeof(out_action->role), "%s", role);
            }
            if (notes != nullptr) {
                (void)snprintf(out_action->notes, sizeof(out_action->notes), "%s", notes);
            }
            return APL_OK;
        }
        out_action->kind = APL_TUI_ACTION_CREATE_MEMBERSHIP_DIRECT;
        (void)snprintf(out_action->driver, sizeof(out_action->driver), "%s", driver);
        (void)snprintf(out_action->start, sizeof(out_action->start), "%s", start);
        if (end != nullptr) {
            (void)snprintf(out_action->end, sizeof(out_action->end), "%s", end);
        }
        if (role != nullptr) {
            (void)snprintf(out_action->role, sizeof(out_action->role), "%s", role);
        }
        if (notes != nullptr) {
            (void)snprintf(out_action->notes, sizeof(out_action->notes), "%s", notes);
        }
        return APL_OK;
    }
    case APL_TUI_COMMAND_MEMBERSHIP_END: {
        const char *const driver = apl_tui_command_option_value(request, "driver");
        const char *const end = apl_tui_command_option_value(request, "end");
        if (driver == nullptr || end == nullptr) {
            out_action->kind = APL_TUI_ACTION_OPEN_MEMBERSHIP_END_FORM;
            if (driver != nullptr) {
                (void)snprintf(out_action->driver, sizeof(out_action->driver), "%s", driver);
            }
            if (end != nullptr) {
                (void)snprintf(out_action->end, sizeof(out_action->end), "%s", end);
            }
            return APL_OK;
        }
        out_action->kind = APL_TUI_ACTION_END_MEMBERSHIP_DIRECT;
        (void)snprintf(out_action->driver, sizeof(out_action->driver), "%s", driver);
        (void)snprintf(out_action->end, sizeof(out_action->end), "%s", end);
        return APL_OK;
    }
    case APL_TUI_COMMAND_SEASON_LIST:
        out_action->kind = APL_TUI_ACTION_OPEN_SEASON_LIST;
        return APL_OK;
    case APL_TUI_COMMAND_SEASON_NEW: {
        const char *const label = apl_tui_command_option_value(request, "label");
        const char *const start = apl_tui_command_option_value(request, "start");
        const char *const end = apl_tui_command_option_value(request, "end");
        const char *const notes = apl_tui_command_option_value(request, "notes");
        if (label == nullptr || label[0] == '\0') {
            out_action->kind = APL_TUI_ACTION_OPEN_SEASON_CREATE_FORM;
            if (start != nullptr) {
                (void)snprintf(out_action->start, sizeof(out_action->start), "%s", start);
            }
            if (end != nullptr) {
                (void)snprintf(out_action->end, sizeof(out_action->end), "%s", end);
            }
            if (notes != nullptr) {
                (void)snprintf(out_action->notes, sizeof(out_action->notes), "%s", notes);
            }
            return APL_OK;
        }
        out_action->kind = APL_TUI_ACTION_CREATE_SEASON_DIRECT;
        (void)snprintf(out_action->value, sizeof(out_action->value), "%s", label);
        if (start != nullptr) {
            (void)snprintf(out_action->start, sizeof(out_action->start), "%s", start);
        }
        if (end != nullptr) {
            (void)snprintf(out_action->end, sizeof(out_action->end), "%s", end);
        }
        if (notes != nullptr) {
            (void)snprintf(out_action->notes, sizeof(out_action->notes), "%s", notes);
        }
        return APL_OK;
    }
    case APL_TUI_COMMAND_SEASON_SELECT:
        out_action->kind = APL_TUI_ACTION_SEASON_SELECT_TARGET;
        (void)snprintf(out_action->value, sizeof(out_action->value), "%s", request->positionals[0]);
        return APL_OK;
    case APL_TUI_COMMAND_CAR_LIST:
        out_action->kind = APL_TUI_ACTION_OPEN_CAR_LIST;
        return APL_OK;
    case APL_TUI_COMMAND_CAR_NEW: {
        const char *const label = apl_tui_command_option_value(request, "label");
        const char *const chassis = apl_tui_command_option_value(request, "chassis");
        const char *const notes = apl_tui_command_option_value(request, "notes");
        if (label == nullptr || label[0] == '\0') {
            out_action->kind = APL_TUI_ACTION_OPEN_CAR_CREATE_FORM;
            if (chassis != nullptr) {
                (void)snprintf(out_action->notes, sizeof(out_action->notes), "%s", chassis);
            }
            if (notes != nullptr) {
                (void)snprintf(out_action->role, sizeof(out_action->role), "%s", notes);
            }
            return APL_OK;
        }
        out_action->kind = APL_TUI_ACTION_CREATE_CAR_DIRECT;
        (void)snprintf(out_action->value, sizeof(out_action->value), "%s", label);
        if (chassis != nullptr) {
            (void)snprintf(out_action->notes, sizeof(out_action->notes), "%s", chassis);
        }
        if (notes != nullptr) {
            (void)snprintf(out_action->role, sizeof(out_action->role), "%s", notes);
        }
        return APL_OK;
    }
    case APL_TUI_COMMAND_CAR_SHOW:
        out_action->kind = APL_TUI_ACTION_OPEN_CAR_SHOW;
        (void)snprintf(out_action->value, sizeof(out_action->value), "%s", request->positionals[0]);
        return APL_OK;
    case APL_TUI_COMMAND_PROJECT_LIST:
        out_action->kind = APL_TUI_ACTION_OPEN_PROJECT_LIST;
        return APL_OK;
    case APL_TUI_COMMAND_PROJECT_NEW: {
        const char *const title = apl_tui_command_option_value(request, "title");
        const char *const category = apl_tui_command_option_value(request, "category");
        const char *const description = apl_tui_command_option_value(request, "description");
        const char *const status = apl_tui_command_option_value(request, "status");
        const char *const target = apl_tui_command_option_value(request, "target");
        const char *const actual = apl_tui_command_option_value(request, "actual");
        const char *const season = apl_tui_command_option_value(request, "season");
        const char *const car = apl_tui_command_option_value(request, "car");
        const char *const notes = apl_tui_command_option_value(request, "notes");
        if (title == nullptr || title[0] == '\0') {
            out_action->kind = APL_TUI_ACTION_OPEN_PROJECT_CREATE_FORM;
        } else {
            out_action->kind = APL_TUI_ACTION_CREATE_PROJECT_DIRECT;
            (void)snprintf(out_action->value, sizeof(out_action->value), "%s", title);
        }
        if (category != nullptr) {
            (void)snprintf(out_action->category, sizeof(out_action->category), "%s", category);
        }
        if (description != nullptr) {
            (void)snprintf(out_action->description, sizeof(out_action->description), "%s",
                           description);
        }
        if (status != nullptr) {
            (void)snprintf(out_action->status, sizeof(out_action->status), "%s", status);
        }
        if (target != nullptr) {
            (void)snprintf(out_action->start, sizeof(out_action->start), "%s", target);
        }
        if (actual != nullptr) {
            (void)snprintf(out_action->end, sizeof(out_action->end), "%s", actual);
        }
        if (season != nullptr) {
            (void)snprintf(out_action->season, sizeof(out_action->season), "%s", season);
        }
        if (car != nullptr) {
            (void)snprintf(out_action->car, sizeof(out_action->car), "%s", car);
        }
        if (notes != nullptr) {
            (void)snprintf(out_action->notes, sizeof(out_action->notes), "%s", notes);
        }
        return APL_OK;
    }
    case APL_TUI_COMMAND_PROJECT_SHOW:
        out_action->kind = APL_TUI_ACTION_OPEN_PROJECT_SHOW;
        (void)snprintf(out_action->value, sizeof(out_action->value), "%s", request->positionals[0]);
        return APL_OK;
    case APL_TUI_COMMAND_PROJECT_EDIT:
        out_action->kind = APL_TUI_ACTION_OPEN_PROJECT_EDIT_FORM;
        (void)snprintf(out_action->value, sizeof(out_action->value), "%s", request->positionals[0]);
        return APL_OK;
    case APL_TUI_COMMAND_PROJECT_ARCHIVE:
        out_action->kind = APL_TUI_ACTION_ARCHIVE_PROJECT_TARGET;
        (void)snprintf(out_action->value, sizeof(out_action->value), "%s", request->positionals[0]);
        return APL_OK;
    case APL_TUI_COMMAND_PROJECT_LOG: {
        const char *const date = apl_tui_command_option_value(request, "date");
        const char *const summary = apl_tui_command_option_value(request, "summary");
        const char *const status = apl_tui_command_option_value(request, "status");
        const char *const notes = apl_tui_command_option_value(request, "notes");
        (void)snprintf(out_action->value, sizeof(out_action->value), "%s", request->positionals[0]);
        if (date != nullptr) {
            (void)snprintf(out_action->start, sizeof(out_action->start), "%s", date);
        }
        if (summary != nullptr) {
            (void)snprintf(out_action->summary, sizeof(out_action->summary), "%s", summary);
        }
        if (status != nullptr) {
            (void)snprintf(out_action->status, sizeof(out_action->status), "%s", status);
        }
        if (notes != nullptr) {
            (void)snprintf(out_action->notes, sizeof(out_action->notes), "%s", notes);
        }
        out_action->kind =
            (date == nullptr || date[0] == '\0' || summary == nullptr || summary[0] == '\0')
                ? APL_TUI_ACTION_OPEN_PROJECT_LOG_FORM
                : APL_TUI_ACTION_LOG_PROJECT_UPDATE_DIRECT;
        return APL_OK;
    }
    case APL_TUI_COMMAND_BUDGET_LIST:
        out_action->kind = APL_TUI_ACTION_OPEN_BUDGET_LIST;
        return APL_OK;
    case APL_TUI_COMMAND_BUDGET_NEW:
        out_action->kind = APL_TUI_ACTION_OPEN_BUDGET_CREATE_FORM;
        return APL_OK;
    case APL_TUI_COMMAND_BUDGET_ARCHIVE:
        out_action->kind = APL_TUI_ACTION_ARCHIVE_BUDGET_SELECTED;
        return APL_OK;
    case APL_TUI_COMMAND_HELP:
        out_action->kind = APL_TUI_ACTION_OPEN_HELP;
        join_positionals(request, out_action->value, sizeof(out_action->value));
        return APL_OK;
    case APL_TUI_COMMAND_ABOUT:
        out_action->kind = APL_TUI_ACTION_OPEN_ABOUT;
        return APL_OK;
    case APL_TUI_COMMAND_VERSION:
        out_action->kind = APL_TUI_ACTION_OPEN_VERSION;
        return APL_OK;
    case APL_TUI_COMMAND_QUIT:
        out_action->kind = APL_TUI_ACTION_QUIT;
        return APL_OK;
    case APL_TUI_COMMAND_NONE:
    case APL_TUI_COMMAND_ID_COUNT:
        break;
    }
    apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "command has no action");
    return APL_ERR_INVALID_ARGUMENT;
}

const char *apl_tui_action_name(const apl_tui_action_kind kind)
{
    switch (kind) {
    case APL_TUI_ACTION_NONE:
        return "none";
    case APL_TUI_ACTION_SWITCH_SECTION:
        return "switch-section";
    case APL_TUI_ACTION_OVERVIEW_ALL:
        return "overview-all";
    case APL_TUI_ACTION_OPEN_TEAM_LIST:
        return "open-team-list";
    case APL_TUI_ACTION_OPEN_TEAM_PICKER:
        return "open-team-picker";
    case APL_TUI_ACTION_OPEN_TEAM_CREATE_FORM:
        return "open-team-create-form";
    case APL_TUI_ACTION_CREATE_TEAM_DIRECT:
        return "create-team-direct";
    case APL_TUI_ACTION_TEAM_SWITCH_TARGET:
        return "team-switch-target";
    case APL_TUI_ACTION_OPEN_DRIVER_LIST:
        return "open-driver-list";
    case APL_TUI_ACTION_OPEN_DRIVER_CREATE_FORM:
        return "open-driver-create-form";
    case APL_TUI_ACTION_CREATE_DRIVER_DIRECT:
        return "create-driver-direct";
    case APL_TUI_ACTION_OPEN_DRIVER_SHOW:
        return "open-driver-show";
    case APL_TUI_ACTION_OPEN_MEMBERSHIP_ADD_FORM:
        return "open-membership-add-form";
    case APL_TUI_ACTION_CREATE_MEMBERSHIP_DIRECT:
        return "create-membership-direct";
    case APL_TUI_ACTION_OPEN_MEMBERSHIP_END_FORM:
        return "open-membership-end-form";
    case APL_TUI_ACTION_END_MEMBERSHIP_DIRECT:
        return "end-membership-direct";
    case APL_TUI_ACTION_OPEN_SEASON_LIST:
        return "open-season-list";
    case APL_TUI_ACTION_OPEN_SEASON_CREATE_FORM:
        return "open-season-create-form";
    case APL_TUI_ACTION_CREATE_SEASON_DIRECT:
        return "create-season-direct";
    case APL_TUI_ACTION_SEASON_SELECT_TARGET:
        return "season-select-target";
    case APL_TUI_ACTION_OPEN_CAR_LIST:
        return "open-car-list";
    case APL_TUI_ACTION_OPEN_CAR_CREATE_FORM:
        return "open-car-create-form";
    case APL_TUI_ACTION_CREATE_CAR_DIRECT:
        return "create-car-direct";
    case APL_TUI_ACTION_OPEN_CAR_SHOW:
        return "open-car-show";
    case APL_TUI_ACTION_OPEN_PROJECT_LIST:
        return "open-project-list";
    case APL_TUI_ACTION_OPEN_PROJECT_CREATE_FORM:
        return "open-project-create-form";
    case APL_TUI_ACTION_CREATE_PROJECT_DIRECT:
        return "create-project-direct";
    case APL_TUI_ACTION_OPEN_PROJECT_SHOW:
        return "open-project-show";
    case APL_TUI_ACTION_OPEN_PROJECT_EDIT_FORM:
        return "open-project-edit-form";
    case APL_TUI_ACTION_ARCHIVE_PROJECT_TARGET:
        return "archive-project-target";
    case APL_TUI_ACTION_OPEN_PROJECT_LOG_FORM:
        return "open-project-log-form";
    case APL_TUI_ACTION_LOG_PROJECT_UPDATE_DIRECT:
        return "log-project-update-direct";
    case APL_TUI_ACTION_OPEN_BUDGET_LIST:
        return "open-budget-list";
    case APL_TUI_ACTION_OPEN_BUDGET_CREATE_FORM:
        return "open-budget-create-form";
    case APL_TUI_ACTION_ARCHIVE_BUDGET_SELECTED:
        return "archive-budget-selected";
    case APL_TUI_ACTION_OPEN_HELP:
        return "open-help";
    case APL_TUI_ACTION_OPEN_ABOUT:
        return "open-about";
    case APL_TUI_ACTION_OPEN_VERSION:
        return "open-version";
    case APL_TUI_ACTION_QUIT:
        return "quit";
    }
    return "none";
}
