#pragma once

// Matrix uses custom debounce logic in matrix.c.
// Timing is controlled by HEX_DEBOUNCE_MS.

// Disable Bootmagic during debugging
#define BOOTMAGIC_LITE_ROW 0
#define BOOTMAGIC_LITE_COLUMN 0
