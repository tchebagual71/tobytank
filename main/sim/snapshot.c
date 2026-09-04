#include "sim/snapshot.h"

const char *tobytank_visitor_state_name(tobytank_visitor_state_t state)
{
    switch (state) {
    case TOBYTANK_VISITOR_EMPTY: return "empty";
    case TOBYTANK_VISITOR_ENTERING: return "entering";
    case TOBYTANK_VISITOR_EXPLORING: return "exploring";
    case TOBYTANK_VISITOR_EXITING: return "exiting";
    default: return "unknown";
    }
}

