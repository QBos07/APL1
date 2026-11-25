#ifndef APL_TUI_RECORD_ROUTE_H
#define APL_TUI_RECORD_ROUTE_H

#include "domain/record.h"
#include "tui/navigation.h"

/*
 * Record-kind routing registry (ARCH-05).
 *
 * History rows carry a stable kind and ID. This small registry centralizes the
 * mapping from that kind to the tab that can open the record, so Phase 8 can
 * add explicit open actions without scattering kind/switch blocks.
 */

/** Tab that owns records of `kind`, or SETTINGS for unknown kinds. */
apl_tui_section apl_tui_record_section(apl_record_kind kind);

#endif /* APL_TUI_RECORD_ROUTE_H */
