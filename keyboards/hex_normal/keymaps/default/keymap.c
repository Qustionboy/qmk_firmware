
#include QMK_KEYBOARD_H

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = LAYOUT_ortho_6x5(
        KC_A, KC_PGUP, KC_END,  KC_PGDN, KC_DEL,
        KC_B, KC_F1,   KC_HOME, KC_F2,   KC_INS,
        KC_C, KC_7,    KC_8,    KC_9,    KC_BSPC,
        KC_D, KC_4,    KC_5,    KC_6,    KC_ENT,
        KC_E, KC_1,    KC_2,    KC_3,    KC_MINS,
        KC_F, KC_0,    KC_0,    KC_DOT,  KC_PLUS
    )
};
