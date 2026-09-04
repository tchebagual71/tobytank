#pragma once

#include "fish/genome.h"
#include "fish/identity.h"

typedef enum {
    TOBYTANK_VISITOR_EMPTY = 0,
    TOBYTANK_VISITOR_ENTERING,
    TOBYTANK_VISITOR_EXPLORING,
    TOBYTANK_VISITOR_EXITING,
} tobytank_visitor_state_t;

typedef struct {
    int has_fish;
    tobytank_visitor_state_t state;
    tobytank_identity_t identity;
    tobytank_genome_t genome;
    float x;
    float y;
    float facing;
    float tail_phase;
    float fin_phase;
    float state_time_remaining;
    float state_age;
} tobytank_fish_snapshot_t;

const char *tobytank_visitor_state_name(tobytank_visitor_state_t state);

