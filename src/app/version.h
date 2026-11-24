#ifndef APL_APP_VERSION_H
#define APL_APP_VERSION_H

#include <stddef.h>

/*
 * Single source of truth for application identity (work item 2.17).
 *
 * The version and build type are injected by CMake from the project version
 * and the selected build configuration; no screen or command duplicates a
 * literal version string. The fallback values only apply to unusual build
 * setups that bypass the project definitions.
 */

/** Human-readable application name. */
const char *apl1_application_name(void);

/** Version string from the CMake project version (for example "0.1.0"). */
const char *apl1_version_string(void);

/** Build configuration (for example "Debug"), or "unspecified". */
const char *apl1_build_type(void);

/** Format "apl1 <version> (<build type>)" into `out`. */
void apl1_version_info(char *out, size_t out_size);

#endif /* APL_APP_VERSION_H */
