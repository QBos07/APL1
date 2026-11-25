#include "domain/record.h"

#include <string.h>

const char *apl_record_kind_code(const apl_record_kind kind)
{
    switch (kind) {
    case APL_RECORD_TEAM:
        return "team";
    case APL_RECORD_SEASON:
        return "season";
    case APL_RECORD_DRIVER:
        return "driver";
    case APL_RECORD_MEMBERSHIP:
        return "membership";
    case APL_RECORD_CAR:
        return "car";
    case APL_RECORD_ACTIVITY:
        return "activity";
    case APL_RECORD_LEDGER_ENTRY:
        return "ledger_entry";
    case APL_RECORD_BUDGET_PLAN:
        return "budget_plan";
    case APL_RECORD_NONE:
        return "";
    }
    return "";
}

const char *apl_record_kind_label(const apl_record_kind kind)
{
    switch (kind) {
    case APL_RECORD_TEAM:
        return "Team";
    case APL_RECORD_SEASON:
        return "Season";
    case APL_RECORD_DRIVER:
        return "Driver";
    case APL_RECORD_MEMBERSHIP:
        return "Membership";
    case APL_RECORD_CAR:
        return "Car";
    case APL_RECORD_ACTIVITY:
        return "Activity";
    case APL_RECORD_LEDGER_ENTRY:
        return "Ledger entry";
    case APL_RECORD_BUDGET_PLAN:
        return "Budget plan";
    case APL_RECORD_NONE:
        return "Unknown";
    }
    return "Unknown";
}

apl_record_kind apl_record_kind_from_code(const char *const code)
{
    if (code == nullptr) {
        return APL_RECORD_NONE;
    }
    for (int kind = APL_RECORD_TEAM; kind <= APL_RECORD_BUDGET_PLAN; ++kind) {
        if (strcmp(code, apl_record_kind_code((apl_record_kind)kind)) == 0) {
            return (apl_record_kind)kind;
        }
    }
    return APL_RECORD_NONE;
}

apl_record_ref apl_record_ref_make(const apl_record_kind kind, const int64_t id)
{
    return (apl_record_ref){.kind = kind, .id = id};
}

bool apl_record_ref_valid(const apl_record_ref *const ref)
{
    return ref != nullptr && ref->kind != APL_RECORD_NONE && ref->id > 0;
}

bool apl_record_ref_equal(const apl_record_ref *const left, const apl_record_ref *const right)
{
    return left != nullptr && right != nullptr && left->kind == right->kind &&
           left->id == right->id;
}
