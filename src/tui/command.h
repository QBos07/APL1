#ifndef APL_TUI_COMMAND_H
#define APL_TUI_COMMAND_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Ex-style command registry and parser (work item 2.26).
 *
 * The parser turns one command line into a typed request. It never executes a
 * shell, SQL, or Notcurses operation: command text is only tokenized and matched
 * against the registered Phase 2 vocabulary. Incomplete and invalid commands
 * are distinguishable so the shell can open a form or show usage help.
 */

#define APL_TUI_COMMAND_MAX_POSITIONALS 4
#define APL_TUI_COMMAND_MAX_OPTIONS 8
#define APL_TUI_COMMAND_WORD_MAX 256
#define APL_TUI_COMMAND_NAME_MAX 64
#define APL_TUI_COMMAND_OPTION_MAX 32
#define APL_TUI_COMMAND_ERROR_MAX 256
#define APL_TUI_COMMAND_SUGGESTION_MAX 256
#define APL_TUI_COMMAND_HELP_MAX 2048

typedef enum apl_tui_command_id {
    APL_TUI_COMMAND_NONE = 0,
    APL_TUI_COMMAND_VIEW,
    APL_TUI_COMMAND_OVERVIEW,
    APL_TUI_COMMAND_TEAM_LIST,
    APL_TUI_COMMAND_TEAM_SWITCH,
    APL_TUI_COMMAND_TEAM_CREATE,
    APL_TUI_COMMAND_HELP,
    APL_TUI_COMMAND_ABOUT,
    APL_TUI_COMMAND_VERSION,
    APL_TUI_COMMAND_QUIT,
    APL_TUI_COMMAND_ID_COUNT,
} apl_tui_command_id;

typedef enum apl_tui_command_status {
    APL_TUI_COMMAND_OK = 0,
    APL_TUI_COMMAND_EMPTY,
    APL_TUI_COMMAND_INCOMPLETE,
    APL_TUI_COMMAND_INVALID,
    APL_TUI_COMMAND_UNKNOWN,
} apl_tui_command_status;

typedef struct apl_tui_command_spec {
    const char *name;
    const char *alias;
    const char *usage;
    const char *summary;
    apl_tui_command_id id;
    unsigned min_positionals;
    unsigned max_positionals;
    const char *const *choices;
    unsigned choice_count;
    const char *const *option_names;
    unsigned option_count;
} apl_tui_command_spec;

typedef struct apl_tui_command_option {
    char name[APL_TUI_COMMAND_OPTION_MAX];
    char value[APL_TUI_COMMAND_WORD_MAX];
} apl_tui_command_option;

typedef struct apl_tui_command_request {
    apl_tui_command_id id;
    char command[APL_TUI_COMMAND_NAME_MAX];
    unsigned positional_count;
    char positionals[APL_TUI_COMMAND_MAX_POSITIONALS][APL_TUI_COMMAND_WORD_MAX];
    unsigned option_count;
    apl_tui_command_option options[APL_TUI_COMMAND_MAX_OPTIONS];
} apl_tui_command_request;

typedef struct apl_tui_command_parse_result {
    apl_tui_command_status status;
    apl_tui_command_request request;
    const apl_tui_command_spec *spec;
    char error[APL_TUI_COMMAND_ERROR_MAX];
    char suggestion[APL_TUI_COMMAND_SUGGESTION_MAX];
} apl_tui_command_parse_result;

/** Number of registered commands. */
size_t apl_tui_command_count(void);

/** Registered command at `index`, or nullptr. */
const apl_tui_command_spec *apl_tui_command_at(size_t index);

/** Registered command with a stable id, or nullptr. */
const apl_tui_command_spec *apl_tui_command_by_id(apl_tui_command_id id);

/** Find a command whose canonical name or alias exactly matches `words`
 *  (whitespace-separated, case-insensitive). */
const apl_tui_command_spec *apl_tui_command_lookup(const char *words);

/** Parse one command line (optional leading colon and whitespace accepted). */
apl_tui_command_parse_result apl_tui_command_parse(const char *line);

/** Stable status name for tests/logs. */
const char *apl_tui_command_status_name(apl_tui_command_status status);

/** Find an option value in a parsed request (nullptr when absent). */
const char *apl_tui_command_option_value(const apl_tui_command_request *request, const char *name);

/** Write command-specific help or registry-wide help into `out`. */
void apl_tui_command_help(const apl_tui_command_spec *spec, char *out, size_t out_size);
void apl_tui_command_registry_help(char *out, size_t out_size);

/** Write a short usage/completion suggestion for `prefix`, or an empty string. */
void apl_tui_command_complete(const char *prefix, char *out, size_t out_size);

#endif /* APL_TUI_COMMAND_H */
