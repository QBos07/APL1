#include "app/version.h"

#include <stdio.h>

#ifndef APL1_VERSION_STRING
#define APL1_VERSION_STRING "0.1.0"
#endif

#ifndef APL1_BUILD_TYPE
#define APL1_BUILD_TYPE "unspecified"
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
    return (APL1_BUILD_TYPE[0] != '\0') ? APL1_BUILD_TYPE : "unspecified";
}

void apl1_version_info(char *const out, const size_t out_size)
{
    if (out == nullptr || out_size == 0) {
        return;
    }
    (void)snprintf(out, out_size, "%s %s (%s)", apl1_application_name(), apl1_version_string(),
                   apl1_build_type());
}
