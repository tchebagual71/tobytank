#pragma once

void tobytank_boot_diagnostics_print(void);
void tobytank_boot_diagnostics_log_health(void);

/*
 * Reports the identity block reserved for this boot and a genome self-check.
 * The self-check generates from a fixed sample identity rather than a real one,
 * so watching the log never consumes a visitor.
 */
void tobytank_boot_diagnostics_log_identity(void);
