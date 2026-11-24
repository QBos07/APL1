#include "tui/command.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "tui/text.h"

#define TOKEN_COUNT_MAX 32

typedef struct token_list {
    char tokens[TOKEN_COUNT_MAX][APL_TUI_COMMAND_WORD_MAX];
    unsigned count;
} token_list;

static const char *const VIEW_CHOICES[] = {
    "overview", "teams", "people", "events", "money", "development", "parts", "history", "settings",
};

static const char *const OVERVIEW_CHOICES[] = {"active", "all"};
static const char *const TEAM_CREATE_OPTIONS[] = {"name", "notes"};

static const apl_tui_command_spec COMMANDS[] = {
    {"view", "v", "view <overview|teams|people|events|money|development|parts|history|settings>",
     "Switch the active view without changing teams.", VIEW_CHOICES, nullptr, APL_TUI_COMMAND_VIEW,
     1, 1, sizeof(VIEW_CHOICES) / sizeof(VIEW_CHOICES[0]), 0},
    {"overview", "ov", "overview [active|all]", "Show the active-team or all-teams overview.",
     OVERVIEW_CHOICES, nullptr, APL_TUI_COMMAND_OVERVIEW, 0, 1,
     sizeof(OVERVIEW_CHOICES) / sizeof(OVERVIEW_CHOICES[0]), 0},
    {"team list", "tl", "team list", "Open the Teams view and its team list.", nullptr, nullptr,
     APL_TUI_COMMAND_TEAM_LIST, 0, 0, 0, 0},
    {"team switch", "ts", "team switch <name-or-id>",
     "Switch the active team by unique name or id.", nullptr, nullptr, APL_TUI_COMMAND_TEAM_SWITCH,
     1, 1, 0, 0},
    {"team create", "tc", "team create [--name <name>] [--notes <notes>]",
     "Create a team; without a name the form opens prefilled.", nullptr, TEAM_CREATE_OPTIONS,
     APL_TUI_COMMAND_TEAM_CREATE, 0, 1, 0,
     sizeof(TEAM_CREATE_OPTIONS) / sizeof(TEAM_CREATE_OPTIONS[0])},
    {"help", "h", "help [command ...]", "Show command help.", nullptr, nullptr,
     APL_TUI_COMMAND_HELP, 0, 8, 0, 0},
    {"about", "ab", "about", "Show application information.", nullptr, nullptr,
     APL_TUI_COMMAND_ABOUT, 0, 0, 0, 0},
    {"version", "ver", "version", "Show version and build information.", nullptr, nullptr,
     APL_TUI_COMMAND_VERSION, 0, 0, 0, 0},
    {"quit", "q", "quit", "Quit the application.", nullptr, nullptr, APL_TUI_COMMAND_QUIT, 0, 0, 0,
     0},
};

constexpr size_t COMMAND_COUNT = sizeof(COMMANDS) / sizeof(COMMANDS[0]);

static bool is_space(const char value)
{
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

static bool word_equal_ci(const char *const left, const char *const start, const size_t length)
{
    if (strlen(left) != length) {
        return false;
    }
    for (size_t index = 0; index < length; ++index) {
        char left_char = left[index];
        char right_char = start[index];
        if (left_char >= 'A' && left_char <= 'Z') {
            left_char = (char)(left_char - 'A' + 'a');
        }
        if (right_char >= 'A' && right_char <= 'Z') {
            right_char = (char)(right_char - 'A' + 'a');
        }
        if (left_char != right_char) {
            return false;
        }
    }
    return true;
}

/** Append text to a bounded output buffer; returns false when it did not fit. */
static bool append_output(char *const out, const size_t out_size, const char *const format, ...)
{
    if (out == nullptr || out_size == 0) {
        return false;
    }
    va_list args;
    __builtin_va_start(args, format);
    const size_t length = strlen(out);
    const bool fits = length < out_size - 1;
    const int written = fits ? vsnprintf(out + length, out_size - length, format, args) : -1;
    __builtin_va_end(args);
    return fits && written >= 0 && (size_t)written < out_size - length;
}

static bool tokenize(const char *line, token_list *const out, char *const error,
                     const size_t error_size)
{
    out->count = 0;
    if (line == nullptr) {
        return true;
    }
    const char *cursor = line;
    while (is_space(*cursor)) {
        ++cursor;
    }
    if (*cursor == ':') {
        ++cursor;
    }

    while (*cursor != '\0') {
        while (is_space(*cursor)) {
            ++cursor;
        }
        if (*cursor == '\0') {
            break;
        }
        if (out->count >= TOKEN_COUNT_MAX) {
            (void)snprintf(error, error_size, "Too many words in the command.");
            return false;
        }

        char *const token = out->tokens[out->count];
        size_t length = 0;
        while (*cursor != '\0' && !is_space(*cursor)) {
            if (*cursor == '"' || *cursor == '\'') {
                const char quote = *cursor++;
                bool closed = false;
                while (*cursor != '\0') {
                    char character = *cursor++;
                    if (character == '\\' && *cursor != '\0') {
                        character = *cursor++;
                    } else if (character == quote) {
                        closed = true;
                        break;
                    }
                    if (length + 1 >= APL_TUI_COMMAND_WORD_MAX) {
                        (void)snprintf(error, error_size, "A command value is too long.");
                        return false;
                    }
                    token[length++] = character;
                }
                if (!closed) {
                    (void)snprintf(error, error_size, "Unterminated quoted value.");
                    return false;
                }
            } else {
                char character = *cursor++;
                if (character == '\\' && *cursor != '\0') {
                    character = *cursor++;
                }
                if (length + 1 >= APL_TUI_COMMAND_WORD_MAX) {
                    (void)snprintf(error, error_size, "A command value is too long.");
                    return false;
                }
                token[length++] = character;
            }
        }
        token[length] = '\0';
        ++out->count;
    }
    return true;
}

static unsigned match_phrase(const token_list *const tokens, const char *const phrase)
{
    if (phrase == nullptr || phrase[0] == '\0') {
        return 0;
    }
    unsigned matched = 0;
    const char *cursor = phrase;
    while (*cursor != '\0') {
        while (*cursor == ' ') {
            ++cursor;
        }
        if (*cursor == '\0') {
            break;
        }
        const char *const start = cursor;
        while (*cursor != '\0' && *cursor != ' ') {
            ++cursor;
        }
        const size_t length = (size_t)(cursor - start);
        if (matched >= tokens->count) {
            return 0;
        }
        if (!word_equal_ci(tokens->tokens[matched], start, length)) {
            return 0;
        }
        ++matched;
    }
    return matched;
}

static const apl_tui_command_spec *find_matching_spec(const token_list *const tokens,
                                                      unsigned *const out_matched)
{
    const apl_tui_command_spec *best = nullptr;
    unsigned best_words = 0;
    for (size_t index = 0; index < COMMAND_COUNT; ++index) {
        const unsigned name_words = match_phrase(tokens, COMMANDS[index].name);
        const unsigned alias_words = match_phrase(tokens, COMMANDS[index].alias);
        const unsigned words = (name_words > alias_words) ? name_words : alias_words;
        if (words > best_words) {
            best_words = words;
            best = &COMMANDS[index];
        }
    }
    *out_matched = best_words;
    return best;
}

static const apl_tui_command_spec *find_incomplete_spec(const token_list *const tokens)
{
    char joined[APL_TUI_COMMAND_WORD_MAX] = "";
    for (unsigned index = 0; index < tokens->count; ++index) {
        if (index > 0) {
            (void)apl_tui_text_append(joined, sizeof(joined), " ");
        }
        (void)apl_tui_text_append(joined, sizeof(joined), tokens->tokens[index]);
    }
    for (size_t index = 0; index < COMMAND_COUNT; ++index) {
        if (apl_tui_text_starts_with_ci(COMMANDS[index].name, joined) ||
            apl_tui_text_starts_with_ci(COMMANDS[index].alias, joined)) {
            return &COMMANDS[index];
        }
    }
    return nullptr;
}

static const apl_tui_command_spec *find_option_spec(const apl_tui_command_spec *const spec,
                                                    const char *const name)
{
    for (unsigned index = 0; index < spec->option_count; ++index) {
        if (apl_tui_text_equal_ci(spec->option_names[index], name)) {
            return spec;
        }
    }
    return nullptr;
}

static bool option_present(const apl_tui_command_request *const request, const char *const name)
{
    for (unsigned index = 0; index < request->option_count; ++index) {
        if (apl_tui_text_equal_ci(request->options[index].name, name)) {
            return true;
        }
    }
    return false;
}

static bool choice_present(const apl_tui_command_spec *const spec, const char *const value)
{
    for (unsigned index = 0; index < spec->choice_count; ++index) {
        if (apl_tui_text_equal_ci(spec->choices[index], value)) {
            return true;
        }
    }
    return false;
}

static void build_choice_list(const apl_tui_command_spec *const spec, char *const out,
                              const size_t out_size)
{
    out[0] = '\0';
    for (unsigned index = 0; index < spec->choice_count; ++index) {
        (void)append_output(out, out_size, "%s%s", (index > 0) ? ", " : "", spec->choices[index]);
    }
}

size_t apl_tui_command_count(void)
{
    return COMMAND_COUNT;
}

const apl_tui_command_spec *apl_tui_command_at(const size_t index)
{
    return (index < COMMAND_COUNT) ? &COMMANDS[index] : nullptr;
}

const apl_tui_command_spec *apl_tui_command_by_id(const apl_tui_command_id id)
{
    for (size_t index = 0; index < COMMAND_COUNT; ++index) {
        if (COMMANDS[index].id == id) {
            return &COMMANDS[index];
        }
    }
    return nullptr;
}

const apl_tui_command_spec *apl_tui_command_lookup(const char *const words)
{
    token_list tokens = {};
    char error[APL_TUI_COMMAND_ERROR_MAX] = "";
    if (!tokenize(words, &tokens, error, sizeof(error)) || tokens.count == 0) {
        return nullptr;
    }
    unsigned matched = 0;
    const apl_tui_command_spec *const spec = find_matching_spec(&tokens, &matched);
    if (spec == nullptr) {
        return nullptr;
    }
    /* A lookup is exact only when the whole input spells the command name/alias. */
    const unsigned name_words = match_phrase(&tokens, spec->name);
    const unsigned alias_words = match_phrase(&tokens, spec->alias);
    const unsigned phrase_words = (name_words > alias_words) ? name_words : alias_words;
    return (phrase_words == tokens.count) ? spec : nullptr;
}

const char *apl_tui_command_status_name(const apl_tui_command_status status)
{
    switch (status) {
    case APL_TUI_COMMAND_OK:
        return "ok";
    case APL_TUI_COMMAND_EMPTY:
        return "empty";
    case APL_TUI_COMMAND_INCOMPLETE:
        return "incomplete";
    case APL_TUI_COMMAND_INVALID:
        return "invalid";
    case APL_TUI_COMMAND_UNKNOWN:
        return "unknown";
    }
    return "unknown";
}

const char *apl_tui_command_option_value(const apl_tui_command_request *const request,
                                         const char *const name)
{
    if (request == nullptr || name == nullptr) {
        return nullptr;
    }
    for (unsigned index = 0; index < request->option_count; ++index) {
        if (apl_tui_text_equal_ci(request->options[index].name, name)) {
            return request->options[index].value;
        }
    }
    return nullptr;
}

apl_tui_command_parse_result apl_tui_command_parse(const char *const line)
{
    apl_tui_command_parse_result result = {.status = APL_TUI_COMMAND_EMPTY};
    token_list tokens = {};
    if (!tokenize(line, &tokens, result.error, sizeof(result.error))) {
        result.status = APL_TUI_COMMAND_INVALID;
        return result;
    }
    if (tokens.count == 0) {
        return result;
    }

    unsigned matched = 0;
    const apl_tui_command_spec *const spec = find_matching_spec(&tokens, &matched);
    if (spec == nullptr || matched == 0) {
        const apl_tui_command_spec *const incomplete = find_incomplete_spec(&tokens);
        result.status =
            (incomplete != nullptr) ? APL_TUI_COMMAND_INCOMPLETE : APL_TUI_COMMAND_UNKNOWN;
        if (incomplete != nullptr) {
            (void)snprintf(result.error, sizeof(result.error), "Incomplete command.");
            (void)snprintf(result.suggestion, sizeof(result.suggestion), "Try: %s",
                           incomplete->usage);
        } else {
            (void)snprintf(result.error, sizeof(result.error), "Unknown command '%.180s'.",
                           tokens.tokens[0]);
            (void)snprintf(result.suggestion, sizeof(result.suggestion),
                           "Type :help for the command list.");
        }
        return result;
    }

    result.spec = spec;
    result.request.id = spec->id;
    (void)snprintf(result.request.command, sizeof(result.request.command), "%s", spec->name);

    for (unsigned index = matched; index < tokens.count; ++index) {
        const char *const token = tokens.tokens[index];
        if (token[0] == '-' && token[1] == '-' && token[2] != '\0') {
            const char *name = token + 2;
            const char *value = nullptr;
            char name_buffer[APL_TUI_COMMAND_OPTION_MAX];
            const char *const equals = strchr(name, '=');
            if (equals != nullptr) {
                const size_t length = (size_t)(equals - name);
                if (length == 0 || length >= sizeof(name_buffer)) {
                    (void)snprintf(result.error, sizeof(result.error), "Invalid option '%.180s'.",
                                   token);
                    result.status = APL_TUI_COMMAND_INVALID;
                    return result;
                }
                memcpy(name_buffer, name, length);
                name_buffer[length] = '\0';
                name = name_buffer;
                value = equals + 1;
            }
            if (find_option_spec(spec, name) == nullptr) {
                (void)snprintf(result.error, sizeof(result.error), "Unknown option '--%s'.", name);
                result.status = APL_TUI_COMMAND_INVALID;
                return result;
            }
            if (option_present(&result.request, name)) {
                (void)snprintf(result.error, sizeof(result.error),
                               "Option '--%s' was given more than once.", name);
                result.status = APL_TUI_COMMAND_INVALID;
                return result;
            }
            if (value == nullptr) {
                if (index + 1 >= tokens.count || tokens.tokens[index + 1][0] == '-' ||
                    tokens.tokens[index + 1][1] == '-') {
                    (void)snprintf(result.error, sizeof(result.error),
                                   "Option '--%s' requires a value.", name);
                    result.status = APL_TUI_COMMAND_INVALID;
                    return result;
                }
                value = tokens.tokens[++index];
            }
            if (value[0] == '\0') {
                (void)snprintf(result.error, sizeof(result.error),
                               "Option '--%s' requires a non-empty value.", name);
                result.status = APL_TUI_COMMAND_INVALID;
                return result;
            }
            apl_tui_command_option *const option =
                &result.request.options[result.request.option_count++];
            (void)snprintf(option->name, sizeof(option->name), "%s", name);
            (void)snprintf(option->value, sizeof(option->value), "%s", value);
            continue;
        }

        if (token[0] == '-' && token[1] == '-') {
            (void)snprintf(result.error, sizeof(result.error), "Invalid option '%.180s'.", token);
            result.status = APL_TUI_COMMAND_INVALID;
            return result;
        }
        if (result.request.positional_count >= spec->max_positionals) {
            (void)snprintf(result.error, sizeof(result.error),
                           "Too many values for '%s'. Quote names containing spaces.", spec->name);
            result.status = APL_TUI_COMMAND_INVALID;
            (void)snprintf(result.suggestion, sizeof(result.suggestion), "Usage: %s", spec->usage);
            return result;
        }
        char *const destination = result.request.positionals[result.request.positional_count++];
        (void)snprintf(destination, APL_TUI_COMMAND_WORD_MAX, "%s", token);
    }

    if (result.request.positional_count < spec->min_positionals) {
        result.status = APL_TUI_COMMAND_INCOMPLETE;
        (void)snprintf(result.error, sizeof(result.error), "Incomplete command.");
        (void)snprintf(result.suggestion, sizeof(result.suggestion), "Usage: %s", spec->usage);
        return result;
    }

    if (spec->choice_count > 0 && result.request.positional_count > 0 &&
        !choice_present(spec, result.request.positionals[0])) {
        char choices[APL_TUI_COMMAND_SUGGESTION_MAX];
        build_choice_list(spec, choices, sizeof(choices));
        (void)snprintf(result.error, sizeof(result.error), "Unknown value '%.180s'.",
                       result.request.positionals[0]);
        (void)snprintf(result.suggestion, sizeof(result.suggestion), "Valid values: %.220s",
                       choices);
        result.status = APL_TUI_COMMAND_INVALID;
        return result;
    }

    if (spec->id == APL_TUI_COMMAND_TEAM_CREATE) {
        const char *const option_name = apl_tui_command_option_value(&result.request, "name");
        if (result.request.positional_count > 0 && option_name != nullptr) {
            (void)snprintf(result.error, sizeof(result.error),
                           "Give the team name either as a value or with --name, not both.");
            result.status = APL_TUI_COMMAND_INVALID;
            return result;
        }
        if (result.request.positional_count == 0 && option_name == nullptr) {
            result.status = APL_TUI_COMMAND_INCOMPLETE;
            (void)snprintf(result.error, sizeof(result.error),
                           "A team name is required to create directly.");
            (void)snprintf(result.suggestion, sizeof(result.suggestion), "Usage: %s", spec->usage);
            return result;
        }
    }

    result.status = APL_TUI_COMMAND_OK;
    result.error[0] = '\0';
    result.suggestion[0] = '\0';
    return result;
}

void apl_tui_command_help(const apl_tui_command_spec *const spec, char *const out,
                          const size_t out_size)
{
    if (out == nullptr || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (spec == nullptr) {
        apl_tui_command_registry_help(out, out_size);
        return;
    }
    (void)append_output(out, out_size, "%s\n%s\n\nUsage: %s\n", spec->name, spec->summary,
                        spec->usage);
    if (spec->option_count > 0) {
        (void)append_output(out, out_size, "Options:");
        for (unsigned index = 0; index < spec->option_count; ++index) {
            (void)append_output(out, out_size, " --%s <value>", spec->option_names[index]);
        }
        (void)append_output(out, out_size, "\n");
    }
    if (spec->choice_count > 0) {
        char choices[APL_TUI_COMMAND_SUGGESTION_MAX];
        build_choice_list(spec, choices, sizeof(choices));
        (void)append_output(out, out_size, "Values: %s\n", choices);
    }
}

void apl_tui_command_registry_help(char *const out, const size_t out_size)
{
    if (out == nullptr || out_size == 0) {
        return;
    }
    out[0] = '\0';
    (void)append_output(out, out_size,
                        "Commands (open with :). Quoted names are single values.\n\n");
    for (size_t index = 0; index < COMMAND_COUNT; ++index) {
        (void)append_output(out, out_size, "%-46s %s\n", COMMANDS[index].usage,
                            COMMANDS[index].summary);
    }
}

void apl_tui_command_complete(const char *const prefix, char *const out, const size_t out_size)
{
    if (out == nullptr || out_size == 0) {
        return;
    }
    out[0] = '\0';
    const char *cursor = (prefix != nullptr) ? prefix : "";
    while (is_space(*cursor)) {
        ++cursor;
    }
    if (*cursor == ':') {
        ++cursor;
    }
    while (is_space(*cursor)) {
        ++cursor;
    }

    if (*cursor == '\0') {
        (void)snprintf(out, out_size,
                       "view | overview | team list | team switch | team create | help | about | "
                       "version | quit");
        return;
    }

    for (size_t index = 0; index < COMMAND_COUNT; ++index) {
        if (apl_tui_text_starts_with_ci(COMMANDS[index].name, cursor) ||
            apl_tui_text_starts_with_ci(COMMANDS[index].alias, cursor)) {
            (void)snprintf(out, out_size, "%s", COMMANDS[index].usage);
            return;
        }
    }
}
