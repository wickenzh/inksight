#pragma once

#include <stdint.h>

void ui_clear(uint8_t *frame);
void ui_draw_setup(uint8_t *frame, const char *ap_name);
void ui_draw_status(
    uint8_t *frame,
    const char *title,
    const char *line_1,
    const char *line_2,
    const char *line_3
);
