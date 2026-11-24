#include "app/version.h"

#include <stdio.h>

#ifndef APL1_VERSION_STRING
#error "APL1_VERSION_STRING must come from the CMake project version"
#endif

#ifndef APL1_BUILD_TYPE
#error "APL1_BUILD_TYPE must come from the build configuration"
#endif

const char *apl1_application_name(void)
{
    return "apl1";
}

const char *apl1_version_string(void)
{
    return APL1_VERSION_STRING;
}

const char *apl1_build_type(void)
{
    return APL1_BUILD_TYPE;
}

void apl1_version_info(char *const out, const size_t out_size)
{
    if (out == nullptr || out_size == 0) {
        return;
    }
    (void)snprintf(out, out_size, "%s %s (%s)", apl1_application_name(), apl1_version_string(),
                   apl1_build_type());
}
