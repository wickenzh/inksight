#include "ui.h"

#include <stddef.h>
#include <string.h>

#include "board_config.h"

static const uint8_t GLYPH_SPACE[] = {0, 0, 0, 0, 0};
static const uint8_t GLYPH_A[] = {0x7E, 0x11, 0x11, 0x11, 0x7E};
static const uint8_t GLYPH_B[] = {0x7F, 0x49, 0x49, 0x49, 0x36};
static const uint8_t GLYPH_C[] = {0x3E, 0x41, 0x41, 0x41, 0x22};
static const uint8_t GLYPH_D[] = {0x7F, 0x41, 0x41, 0x22, 0x1C};
static const uint8_t GLYPH_E[] = {0x7F, 0x49, 0x49, 0x49, 0x41};
static const uint8_t GLYPH_F[] = {0x7F, 0x09, 0x09, 0x09, 0x01};
static const uint8_t GLYPH_G[] = {0x3E, 0x41, 0x49, 0x49, 0x3A};
static const uint8_t GLYPH_H[] = {0x7F, 0x08, 0x08, 0x08, 0x7F};
static const uint8_t GLYPH_I[] = {0x00, 0x41, 0x7F, 0x41, 0x00};
static const uint8_t GLYPH_J[] = {0x20, 0x40, 0x41, 0x3F, 0x01};
static const uint8_t GLYPH_K[] = {0x7F, 0x08, 0x14, 0x22, 0x41};
static const uint8_t GLYPH_L[] = {0x7F, 0x40, 0x40, 0x40, 0x40};
static const uint8_t GLYPH_M[] = {0x7F, 0x02, 0x0C, 0x02, 0x7F};
static const uint8_t GLYPH_N[] = {0x7F, 0x04, 0x08, 0x10, 0x7F};
static const uint8_t GLYPH_O[] = {0x3E, 0x41, 0x41, 0x41, 0x3E};
static const uint8_t GLYPH_P[] = {0x7F, 0x09, 0x09, 0x09, 0x06};
static const uint8_t GLYPH_Q[] = {0x3E, 0x41, 0x51, 0x21, 0x5E};
static const uint8_t GLYPH_R[] = {0x7F, 0x09, 0x19, 0x29, 0x46};
static const uint8_t GLYPH_S[] = {0x26, 0x49, 0x49, 0x49, 0x32};
static const uint8_t GLYPH_T[] = {0x01, 0x01, 0x7F, 0x01, 0x01};
static const uint8_t GLYPH_U[] = {0x3F, 0x40, 0x40, 0x40, 0x3F};
static const uint8_t GLYPH_V[] = {0x1F, 0x20, 0x40, 0x20, 0x1F};
static const uint8_t GLYPH_W[] = {0x3F, 0x40, 0x38, 0x40, 0x3F};
static const uint8_t GLYPH_X[] = {0x63, 0x14, 0x08, 0x14, 0x63};
static const uint8_t GLYPH_Y[] = {0x07, 0x08, 0x70, 0x08, 0x07};
static const uint8_t GLYPH_Z[] = {0x61, 0x51, 0x49, 0x45, 0x43};
static const uint8_t GLYPH_0[] = {0x3E, 0x51, 0x49, 0x45, 0x3E};
static const uint8_t GLYPH_1[] = {0x00, 0x42, 0x7F, 0x40, 0x00};
static const uint8_t GLYPH_2[] = {0x42, 0x61, 0x51, 0x49, 0x46};
static const uint8_t GLYPH_3[] = {0x21, 0x41, 0x45, 0x4B, 0x31};
static const uint8_t GLYPH_4[] = {0x18, 0x14, 0x12, 0x7F, 0x10};
static const uint8_t GLYPH_5[] = {0x27, 0x45, 0x45, 0x45, 0x39};
static const uint8_t GLYPH_6[] = {0x3C, 0x4A, 0x49, 0x49, 0x30};
static const uint8_t GLYPH_7[] = {0x01, 0x71, 0x09, 0x05, 0x03};
static const uint8_t GLYPH_8[] = {0x36, 0x49, 0x49, 0x49, 0x36};
static const uint8_t GLYPH_9[] = {0x06, 0x49, 0x49, 0x29, 0x1E};
static const uint8_t GLYPH_DASH[] = {0x08, 0x08, 0x08, 0x08, 0x08};
static const uint8_t GLYPH_DOT[] = {0x00, 0x60, 0x60, 0x00, 0x00};
static const uint8_t GLYPH_COLON[] = {0x00, 0x00, 0x36, 0x36, 0x00};
static const uint8_t GLYPH_SLASH[] = {0x20, 0x10, 0x08, 0x04, 0x02};

static const uint8_t *glyph_for(char character) {
    if (character >= 'a' && character <= 'z') {
        character = (char)(character - 'a' + 'A');
    }
    switch (character) {
        case 'A': return GLYPH_A;
        case 'B': return GLYPH_B;
        case 'C': return GLYPH_C;
        case 'D': return GLYPH_D;
        case 'E': return GLYPH_E;
        case 'F': return GLYPH_F;
        case 'G': return GLYPH_G;
        case 'H': return GLYPH_H;
        case 'I': return GLYPH_I;
        case 'J': return GLYPH_J;
        case 'K': return GLYPH_K;
        case 'L': return GLYPH_L;
        case 'M': return GLYPH_M;
        case 'N': return GLYPH_N;
        case 'O': return GLYPH_O;
        case 'P': return GLYPH_P;
        case 'Q': return GLYPH_Q;
        case 'R': return GLYPH_R;
        case 'S': return GLYPH_S;
        case 'T': return GLYPH_T;
        case 'U': return GLYPH_U;
        case 'V': return GLYPH_V;
        case 'W': return GLYPH_W;
        case 'X': return GLYPH_X;
        case 'Y': return GLYPH_Y;
        case 'Z': return GLYPH_Z;
        case '0': return GLYPH_0;
        case '1': return GLYPH_1;
        case '2': return GLYPH_2;
        case '3': return GLYPH_3;
        case '4': return GLYPH_4;
        case '5': return GLYPH_5;
        case '6': return GLYPH_6;
        case '7': return GLYPH_7;
        case '8': return GLYPH_8;
        case '9': return GLYPH_9;
        case '-': return GLYPH_DASH;
        case '.': return GLYPH_DOT;
        case ':': return GLYPH_COLON;
        case '/': return GLYPH_SLASH;
        default: return GLYPH_SPACE;
    }
}

static void set_black(uint8_t *frame, int x, int y) {
    if (x < 0 || x >= INKSIGHT_WIDTH || y < 0 || y >= INKSIGHT_HEIGHT) {
        return;
    }
    frame[y * INKSIGHT_ROW_BYTES + x / 8] &=
        (uint8_t)~(0x80U >> (x % 8));
}

static void fill_rect(uint8_t *frame, int x, int y, int width, int height) {
    for (int pixel_y = y; pixel_y < y + height; pixel_y++) {
        for (int pixel_x = x; pixel_x < x + width; pixel_x++) {
            set_black(frame, pixel_x, pixel_y);
        }
    }
}

static int text_width(const char *text, int scale) {
    if (text == NULL || text[0] == '\0') {
        return 0;
    }
    return (int)strlen(text) * 6 * scale - scale;
}

static void draw_text(
    uint8_t *frame,
    const char *text,
    int x,
    int y,
    int scale
) {
    if (frame == NULL || text == NULL) {
        return;
    }

    for (size_t character_index = 0;
         text[character_index] != '\0';
         character_index++) {
        const uint8_t *glyph = glyph_for(text[character_index]);
        int character_x = x + (int)character_index * 6 * scale;
        for (int column = 0; column < 5; column++) {
            for (int row = 0; row < 7; row++) {
                if ((glyph[column] & (1U << row)) == 0) {
                    continue;
                }
                for (int dy = 0; dy < scale; dy++) {
                    for (int dx = 0; dx < scale; dx++) {
                        set_black(
                            frame,
                            character_x + column * scale + dx,
                            y + row * scale + dy
                        );
                    }
                }
            }
        }
    }
}

static void draw_centered(
    uint8_t *frame,
    const char *text,
    int y,
    int scale
) {
    int x = (INKSIGHT_WIDTH - text_width(text, scale)) / 2;
    if (x < 0) {
        x = 0;
    }
    draw_text(frame, text, x, y, scale);
}

void ui_clear(uint8_t *frame) {
    if (frame != NULL) {
        memset(frame, 0xFF, INKSIGHT_FRAME_BYTES);
    }
}

void ui_draw_setup(uint8_t *frame, const char *ap_name) {
    ui_clear(frame);
    fill_rect(frame, 0, 0, INKSIGHT_WIDTH, 32);
    fill_rect(frame, 32, 96, INKSIGHT_WIDTH - 64, 3);
    fill_rect(frame, 32, 222, INKSIGHT_WIDTH - 64, 3);

    draw_centered(frame, "INKSIGHT SETUP", 48, 3);
    draw_centered(frame, "CONNECT TO", 116, 2);
    draw_centered(frame, ap_name, 148, 3);
    draw_centered(frame, "OPEN 192.168.4.1", 244, 2);
}

void ui_draw_status(
    uint8_t *frame,
    const char *title,
    const char *line_1,
    const char *line_2,
    const char *line_3
) {
    ui_clear(frame);
    fill_rect(frame, 0, 0, INKSIGHT_WIDTH, 32);
    draw_centered(frame, title != NULL ? title : "INKSIGHT", 52, 3);

    if (line_1 != NULL) {
        draw_centered(frame, line_1, 126, 2);
    }
    if (line_2 != NULL) {
        draw_centered(frame, line_2, 166, 2);
    }
    if (line_3 != NULL) {
        draw_centered(frame, line_3, 206, 2);
    }
}
