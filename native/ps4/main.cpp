#include <stdint.h>
#include <stdio.h>
#include <sstream>
#include "graphics.h"

std::stringstream debugLogStream;

static const uint8_t FONT[][5] = {
    {0x7e,0x11,0x11,0x11,0x7e}, /* A */
    {0x7f,0x49,0x49,0x49,0x36}, /* B */
    {0x3e,0x41,0x41,0x41,0x22}, /* C */
    {0x7f,0x41,0x41,0x22,0x1c}, /* D */
    {0x7f,0x49,0x49,0x49,0x41}, /* E */
    {0x7f,0x09,0x09,0x09,0x01}, /* F */
    {0x3e,0x41,0x49,0x49,0x7a}, /* G */
    {0x7f,0x08,0x08,0x08,0x7f}, /* H */
    {0x00,0x41,0x7f,0x41,0x00}, /* I */
    {0x20,0x40,0x41,0x3f,0x01}, /* J */
    {0x7f,0x08,0x14,0x22,0x41}, /* K */
    {0x7f,0x40,0x40,0x40,0x40}, /* L */
    {0x7f,0x02,0x0c,0x02,0x7f}, /* M */
    {0x7f,0x04,0x08,0x10,0x7f}, /* N */
    {0x3e,0x41,0x41,0x41,0x3e}, /* O */
    {0x7f,0x09,0x09,0x09,0x06}, /* P */
    {0x3e,0x41,0x51,0x21,0x5e}, /* Q */
    {0x7f,0x09,0x19,0x29,0x46}, /* R */
    {0x46,0x49,0x49,0x49,0x31}, /* S */
    {0x01,0x01,0x7f,0x01,0x01}, /* T */
    {0x3f,0x40,0x40,0x40,0x3f}, /* U */
    {0x1f,0x20,0x40,0x20,0x1f}, /* V */
    {0x3f,0x40,0x38,0x40,0x3f}, /* W */
    {0x63,0x14,0x08,0x14,0x63}, /* X */
    {0x07,0x08,0x70,0x08,0x07}, /* Y */
    {0x61,0x51,0x49,0x45,0x43}, /* Z */
    {0x3e,0x51,0x49,0x45,0x3e}, /* 0 */
    {0x00,0x42,0x7f,0x40,0x00}, /* 1 */
    {0x42,0x61,0x51,0x49,0x46}, /* 2 */
    {0x21,0x41,0x45,0x4b,0x31}, /* 3 */
    {0x18,0x14,0x12,0x7f,0x10}, /* 4 */
    {0x27,0x45,0x45,0x45,0x39}, /* 5 */
    {0x3c,0x4a,0x49,0x49,0x30}, /* 6 */
    {0x01,0x71,0x09,0x05,0x03}, /* 7 */
    {0x36,0x49,0x49,0x49,0x36}, /* 8 */
    {0x06,0x49,0x49,0x29,0x1e}  /* 9 */
};

static Color rgb(uint8_t r, uint8_t g, uint8_t b) {
    Color c = {r, g, b};
    return c;
}

static void rectangle(Scene2D *s, int x, int y, int w, int h,
                      uint8_t r, uint8_t g, uint8_t b) {
    s->DrawRectangle(x, y, w, h, rgb(r, g, b));
}

static void draw_text(Scene2D *s, const char *text, int x, int y,
                      int scale, Color color) {
    for (int n = 0; text[n] != '\0'; ++n) {
        char ch = text[n];
        if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
        const uint8_t *glyph = 0;
        if (ch >= 'A' && ch <= 'Z') glyph = FONT[ch - 'A'];
        else if (ch >= '0' && ch <= '9') glyph = FONT[26 + ch - '0'];
        if (glyph) {
            for (int col = 0; col < 5; ++col)
                for (int row = 0; row < 7; ++row)
                    if ((glyph[col] >> row) & 1)
                        s->DrawRectangle(x + col * scale, y + row * scale,
                                         scale, scale, color);
        } else if (ch == ':') {
            s->DrawRectangle(x, y + scale, scale, scale, color);
            s->DrawRectangle(x, y + 4 * scale, scale, scale, color);
        } else if (ch == '-') {
            s->DrawRectangle(x, y + 3 * scale, 4 * scale, scale, color);
        }
        x += 6 * scale;
    }
}

int main() {
    setvbuf(stdout, NULL, _IONBF, 0);
    Scene2D scene(1920, 1080, 4);
    if (!scene.Init(0xC000000, 2)) {
        for (;;) sceKernelUsleep(1000000);
    }
    scene.SetActiveFrameBuffer(0);

    const Color white = rgb(235, 242, 255);
    const Color muted = rgb(135, 157, 183);
    const Color cyan = rgb(50, 220, 220);
    const Color panel = rgb(20, 33, 52);
    const Color dark = rgb(8, 14, 25);

    int frame = 0;
    for (;;) {
        scene.FrameBufferClear();
        rectangle(&scene, 0, 0, 1920, 1080, 8, 14, 25);
        rectangle(&scene, 0, 0, 1920, 12, 50, 220, 220);
        rectangle(&scene, 120, 110, 1680, 220, 20, 33, 52);
        rectangle(&scene, 120, 365, 810, 270, 20, 33, 52);
        rectangle(&scene, 970, 365, 830, 270, 20, 33, 52);
        rectangle(&scene, 120, 670, 1680, 230, 20, 33, 52);
        rectangle(&scene, 120, 110, 12, 220, 50, 220, 220);

        draw_text(&scene, "TIM", 170, 150, 10, cyan);
        draw_text(&scene, "PS4 DOWNLOAD MANAGER", 430, 175, 4, white);
        draw_text(&scene, "NATIVE UI TEST BUILD", 430, 230, 3, muted);

        draw_text(&scene, "QUEUE", 165, 405, 5, cyan);
        draw_text(&scene, "READY", 165, 485, 5, white);
        draw_text(&scene, "0 ACTIVE TASKS", 165, 555, 3, muted);

        draw_text(&scene, "TORRENT ENGINE", 1015, 405, 4, cyan);
        draw_text(&scene, "NOT CONNECTED", 1015, 485, 4, white);
        draw_text(&scene, "REAL BACKEND REQUIRED", 1015, 555, 3, muted);

        draw_text(&scene, "NEXT: CONNECT LIBRARY AND CATALOG", 165, 725, 4, white);
        draw_text(&scene, "THIS BUILD DOES NOT DOWNLOAD FILES", 165, 800, 3, muted);

        scene.SubmitFlip(frame);
        scene.FrameWait(frame);
        scene.FrameBufferSwap();
        ++frame;
    }
    return 0;
}
