#include "fish/identity.h"

#include <stddef.h>

void tobytank_identity_reserve_block(tobytank_identity_allocator_t *allocator,
                                     uint64_t persisted_counter)
{
    if (allocator == NULL) {
        return;
    }

    if (persisted_counter > UINT64_MAX - TOBYTANK_IDENTITY_BLOCK_SIZE) {
        allocator->next_identity = TOBYTANK_IDENTITY_INVALID;
        allocator->reserved_counter = UINT64_MAX;
        allocator->block_end = TOBYTANK_IDENTITY_INVALID;
        return;
    }

    /* Identity 0 is reserved as "no fish", so the counter is one behind the
       first identity of the block. */
    allocator->next_identity = persisted_counter + 1u;
    allocator->reserved_counter = persisted_counter + TOBYTANK_IDENTITY_BLOCK_SIZE;
    allocator->block_end = allocator->reserved_counter + 1u;
}

int tobytank_identity_block_exhausted(const tobytank_identity_allocator_t *allocator)
{
    if (allocator == NULL) {
        return 1;
    }
    return allocator->next_identity >= allocator->block_end;
}

uint64_t tobytank_identity_block_remaining(const tobytank_identity_allocator_t *allocator)
{
    if (allocator == NULL || tobytank_identity_block_exhausted(allocator)) {
        return 0;
    }
    return allocator->block_end - allocator->next_identity;
}

int tobytank_identity_next(tobytank_identity_allocator_t *allocator,
                           tobytank_identity_t *out_identity)
{
    if (allocator == NULL || out_identity == NULL ||
        tobytank_identity_block_exhausted(allocator)) {
        return 0;
    }

    *out_identity = allocator->next_identity;
    allocator->next_identity += 1u;
    return 1;
}
