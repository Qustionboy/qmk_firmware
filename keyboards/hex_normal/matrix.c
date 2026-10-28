
#include QMK_KEYBOARD_H

#include "matrix.h"
#include "gpio.h"
#include "timer.h"
#include "wait.h"
#include "print.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// ============================================
// HEX Normal RP2040
// Full Custom Matrix Scanner
//
// Matrix: 6 rows x 5 columns
//
// COL0 = GP0
// COL1 = GP1
// COL2 = GP2
// COL3 = GP3
// COL4 = GP4
//
// ROW0 = GP5
// ROW1 = GP6
// ROW2 = GP7
// ROW3 = GP8
// ROW4 = GP9
// ROW5 = GP10
//
// Diode cathode connected to ROW
//
// Scanning:
//   Selected ROW = LOW
//   Other ROWs   = High-Z
//   Columns      = Input Pull-up
//
// Pressed = Column reads LOW
// ============================================

#if MATRIX_ROWS != 6 || MATRIX_COLS != 5
#    error "HEX Normal requires 6x5 matrix"
#endif

// ============================================
// Adjustable parameters
// ============================================

// Row selection settling time
#define HEX_ROW_SETTLE_US 50

// Delay after releasing a row
#define HEX_ROW_RELEASE_US 10

// Extra delay between complete scans
#define HEX_SCAN_PAUSE_US 100

// Independent debounce for each key
#define HEX_DEBOUNCE_MS 15

// ============================================
// GPIO mapping
// ============================================

static const pin_t row_pins[MATRIX_ROWS] = {
    GP5,
    GP6,
    GP7,
    GP8,
    GP9,
    GP10
};

static const pin_t col_pins[MATRIX_COLS] = {
    GP0,
    GP1,
    GP2,
    GP3,
    GP4
};

// ============================================
// Matrix state
// ============================================

// Raw sampled state
static matrix_row_t raw_matrix[MATRIX_ROWS];

// Stable state reported to QMK
static matrix_row_t stable_matrix[MATRIX_ROWS];

// Last raw transition time, per key
static uint32_t debounce_time
    [MATRIX_ROWS][MATRIX_COLS];

// ============================================
// QMK callbacks
// ============================================

__attribute__((weak))
void matrix_init_user(void) {
}

__attribute__((weak))
void matrix_scan_user(void) {
}

__attribute__((weak))
void matrix_init_kb(void) {
    matrix_init_user();
}

__attribute__((weak))
void matrix_scan_kb(void) {
    matrix_scan_user();
}

// ============================================
// GPIO operations
// ============================================

// Release ROW: High impedance
static void release_row(uint8_t row) {
    gpio_set_pin_input(row_pins[row]);
}

// Activate ROW: Drive LOW
static void select_row(uint8_t row) {
    gpio_write_pin_low(row_pins[row]);
    gpio_set_pin_output(row_pins[row]);
}

// Release all rows
static void release_all_rows(void) {
    for (uint8_t r = 0; r < MATRIX_ROWS; r++) {
        release_row(r);
    }
}

// ============================================
// Matrix information functions
// ============================================

uint8_t matrix_rows(void) {
    return MATRIX_ROWS;
}

uint8_t matrix_cols(void) {
    return MATRIX_COLS;
}

matrix_row_t matrix_get_row(uint8_t row) {
    if (row >= MATRIX_ROWS) {
        return 0;
    }

    return stable_matrix[row];
}

bool matrix_is_on(uint8_t row, uint8_t col) {

    if (row >= MATRIX_ROWS ||
        col >= MATRIX_COLS) {
        return false;
    }

    matrix_row_t bit = (matrix_row_t)1U << col;

    return (stable_matrix[row] & bit) != 0;
}

// ============================================
// Debug print
// ============================================

void matrix_print(void) {

    print("\nHEX MATRIX\n");

    for (uint8_t r = 0; r < MATRIX_ROWS; r++) {

        print("ROW");
        print_hex8(r);
        print(": ");

        for (uint8_t c = 0; c < MATRIX_COLS; c++) {

            if (matrix_is_on(r, c)) {
                print("1");
            } else {
                print("0");
            }
        }

        print("\n");
    }
}

// ============================================
// Matrix initialization
// ============================================

void matrix_init(void) {

    // Release all ROWs first
    release_all_rows();

    // Configure all columns as pull-up inputs
    for (uint8_t c = 0; c < MATRIX_COLS; c++) {
        gpio_set_pin_input_high(col_pins[c]);
    }

    // Reset matrix state
    memset(raw_matrix, 0, sizeof(raw_matrix));
    memset(stable_matrix, 0, sizeof(stable_matrix));

    uint32_t now = timer_read32();

    // Initialize debounce timestamps
    for (uint8_t r = 0; r < MATRIX_ROWS; r++) {

        for (uint8_t c = 0; c < MATRIX_COLS; c++) {

            debounce_time[r][c] = now;
        }
    }

    // Allow GPIO levels to settle
    wait_ms(10);

    matrix_init_kb();
}

// ============================================
// Physical matrix scan
// ============================================

static void scan_gpio_matrix(matrix_row_t sample[]) {

    // Clear current sample
    for (uint8_t r = 0; r < MATRIX_ROWS; r++) {
        sample[r] = 0;
    }

    // Make sure no ROW is selected
    release_all_rows();

    // Scan ROW0 to ROW5
    for (uint8_t r = 0; r < MATRIX_ROWS; r++) {

        // Select the current ROW
        select_row(r);

        // Wait for signals to settle
        wait_us(HEX_ROW_SETTLE_US);

        // Read all five columns
        for (uint8_t c = 0; c < MATRIX_COLS; c++) {

            bool level = gpio_read_pin(col_pins[c]);

            // LOW indicates a pressed key
            if (!level) {

                sample[r] |=
                    ((matrix_row_t)1U << c);
            }
        }

        // Release selected ROW
        release_row(r);

        // Wait for signal recovery
        wait_us(HEX_ROW_RELEASE_US);
    }

    // Additional delay between full scans
    wait_us(HEX_SCAN_PAUSE_US);
}

// ============================================
// Independent per-key debounce
// ============================================

static bool update_debounce(
    const matrix_row_t sample[]
) {

    bool changed = false;

    uint32_t now = timer_read32();

    // Check every physical key independently
    for (uint8_t r = 0; r < MATRIX_ROWS; r++) {

        for (uint8_t c = 0; c < MATRIX_COLS; c++) {

            matrix_row_t bit =
                ((matrix_row_t)1U << c);

            bool new_raw =
                (sample[r] & bit) != 0;

            bool old_raw =
                (raw_matrix[r] & bit) != 0;

            bool old_stable =
                (stable_matrix[r] & bit) != 0;

            // Detect raw signal transition
            if (new_raw != old_raw) {

                if (new_raw) {
                    raw_matrix[r] |= bit;
                } else {
                    raw_matrix[r] &=
                        (matrix_row_t)~bit;
                }

                debounce_time[r][c] = now;
            }

            // Accept after stable interval
            if (new_raw != old_stable) {

                uint32_t elapsed =
                    now - debounce_time[r][c];

                if (elapsed >= HEX_DEBOUNCE_MS) {

                    if (new_raw) {
                        stable_matrix[r] |= bit;
                    } else {
                        stable_matrix[r] &=
                            (matrix_row_t)~bit;
                    }

                    changed = true;
                }
            }
        }
    }

    return changed;
}

// ============================================
// Main QMK matrix scan entry
// ============================================

uint8_t matrix_scan(void) {

    matrix_row_t sample[MATRIX_ROWS] = {0};

    // 1. Scan physical GPIO
    scan_gpio_matrix(sample);

    // 2. Apply independent debounce
    bool changed = update_debounce(sample);

    // 3. Run QMK keyboard callback
    matrix_scan_kb();

    // 4. Tell QMK if stable matrix changed
    return changed ? 1 : 0;
}
