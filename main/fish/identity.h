#pragma once

#include <stdint.h>

/*
 * Visitor identity allocation.
 *
 * Every fish gets a 64-bit identity that is never handed out twice, including
 * across ordinary resets and power loss. The mechanism is block reservation:
 * storage holds a single counter of identities already reserved, a boot claims
 * a whole block up front and persists that immediately, and identities are then
 * handed out from RAM. If power is lost mid-block the unused remainder is
 * skipped forever, which trades a bounded number of wasted identities for a
 * guarantee that survives any reset.
 *
 * This file is the pure allocator, with no storage dependency, so the reboot
 * behaviour can be tested on the host. `main/memory/identity_store.*` supplies
 * the NVS-backed counter.
 */

#define TOBYTANK_IDENTITY_INVALID 0ULL

/* Identities reserved per boot. Wasted identities per power cycle are bounded
   by this, and 64 bits of identity space makes the waste irrelevant. */
#define TOBYTANK_IDENTITY_BLOCK_SIZE 64u

typedef uint64_t tobytank_identity_t;

typedef struct {
    tobytank_identity_t next_identity; /* next identity to hand out */
    tobytank_identity_t block_end;     /* one past the last identity in the block */
    uint64_t reserved_counter;         /* value that storage must now hold */
} tobytank_identity_allocator_t;

static inline int tobytank_identity_is_valid(tobytank_identity_t identity)
{
    return identity != TOBYTANK_IDENTITY_INVALID;
}

/*
 * Claims a block on top of the counter read from storage. `persisted_counter`
 * is the number of identities reserved by all previous boots, so it is 0 the
 * first time. After this call `reserved_counter` must be written to storage
 * before any identity is used.
 */
void tobytank_identity_reserve_block(tobytank_identity_allocator_t *allocator,
                                     uint64_t persisted_counter);

/* True when the block is used up and a new one must be reserved. */
int tobytank_identity_block_exhausted(const tobytank_identity_allocator_t *allocator);

/* Number of identities still available in the current block. */
uint64_t tobytank_identity_block_remaining(const tobytank_identity_allocator_t *allocator);

/*
 * Hands out the next identity. Returns 1 on success, or 0 when the block is
 * exhausted, in which case the caller must reserve another block first.
 */
int tobytank_identity_next(tobytank_identity_allocator_t *allocator,
                           tobytank_identity_t *out_identity);
