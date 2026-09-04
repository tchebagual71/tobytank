#pragma once

#include "esp_err.h"
#include "fish/identity.h"

/*
 * NVS-backed persistence for the identity allocator.
 *
 * Storage holds one counter: how many identities have ever been reserved. Each
 * boot claims a block, writes the new counter before handing anything out, and
 * serves identities from RAM. A reset therefore skips whatever remained of the
 * previous block, which is the price of never reusing an identity after an
 * unexpected power loss.
 *
 * The guarantee covers ordinary resets and power loss. It cannot survive NVS
 * being erased or the partition being replaced; that case is logged explicitly
 * rather than hidden.
 */

esp_err_t tobytank_identity_store_init(void);

/* Hands out the next identity, reserving and persisting a new block if the
   current one is exhausted. */
esp_err_t tobytank_identity_store_next(tobytank_identity_t *out_identity);

/* Identity the next call would return, without consuming it. */
tobytank_identity_t tobytank_identity_store_peek(void);

/* Identities left in the reserved block. */
uint64_t tobytank_identity_store_remaining(void);

/* Counter currently persisted in NVS. */
uint64_t tobytank_identity_store_counter(void);

/* True when the last init had to recover from unusable NVS, meaning the
   identity history was lost and uniqueness restarts from zero. */
int tobytank_identity_store_history_lost(void);
