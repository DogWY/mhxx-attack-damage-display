#include "hud.h"
#include "hud_digits.h"

// MHGU Switch 1.4.0, FB08F1D20FD1204F. Offsets are relative to main.
// sub_60869ECC builds three window textures from gameTexture[15].
// sub_6086A28C stores the acquired gameTexture in windowContext[4659].
// sub_60BBE66C and the render worker pass renderer+213248 to EndRecording.
bool hud_begin(uintptr_t base, void *commands, HudFrame *out) {
    if (!base || !commands || !out) return false;
    uintptr_t renderer = *(uintptr_t *)(base + 0x2126998);
    if (!renderer) return false;
    uintptr_t window = renderer + 213248;
    if (*(void **)(window + 376) != commands) return false;
    unsigned acquired = *(unsigned *)(window + 4660 * 4);
    if (acquired >= 3) return false;
    uintptr_t game_texture = *(uintptr_t *)(window + 4659 * 4);
    if (!game_texture || game_texture != *(uintptr_t *)(window + (4656 + acquired) * 4))
        return false;
    void *texture = *(void **)(game_texture + 60);
    HudClearTexture clear = *(HudClearTexture *)(base + 0x191fd90);
    if (!clear || !texture) return false;
    // The allocated swap texture may exceed the game viewport. GU presents
    // (0,0,w,h) using nvnWindowSetCrop at main+0x86a24c (see main+0xbc0874).
    int w = *(int *)(renderer + 176);
    int h = *(int *)(renderer + 180);
    if (w < 1 || h < 1 || w > 8192 || h > 8192) return false;
    *out = (HudFrame){commands, texture, clear, w, h};
    return true;
}

// Coordinates refer to the native display texture. The clear operates only on
// its subregion, leaving the game's shaders, viewport and scissors intact.
static void rectangle(const HudFrame *frame, int x, int y, int w, int h,
                      const float color[4]) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > frame->width) w = frame->width - x;
    if (y + h > frame->height) h = frame->height - y;
    if (w <= 0 || h <= 0) return;
    HudRegion rect = {x, y, 0, w, h, 1};
    frame->clear(frame->commands, frame->texture, 0, &rect, color, 15);
}

// Round shared design-grid edges consistently so connected strokes meet at
// every game resolution. Curved terminals and variable stroke widths are
// carried by the glyph art rather than by a segmented display primitive.
static int scale_edge(int units, int height) {
    return (units * height + 9) / 19;
}

void hud_number(const HudFrame *frame, float x, float y, unsigned value,
                float opacity, bool highlight) {
    if (!(opacity > 0) || !(x >= 0 && x <= frame->width) ||
        !(y >= 0 && y <= frame->height)) return;
    if (opacity > 1) opacity = 1;
    enum { tracking = 2 };
    const Digit *sequence[10];
    int count = 0, width = 0;
    do {
        const Digit *digit = &digits[value % 10];
        sequence[count++] = digit;
        width += digit->width + tracking;
        value /= 10;
    } while (value);

    int height = (frame->height * 30 + 360) / 720;
    if (height < 1) height = 1;
    int stroke = (height + 8) / 16;
    if (stroke < 1) stroke = 1;
    int left = (int)x - scale_edge(width - tracking, height) / 2;
    int top = (int)y - height / 2;
    const float outline[4] = {0.0392f, 0.0392f, 0.0392f, 1};
    const float fill[4] = {
        opacity,
        opacity * (highlight ? 220.0f/255.0f : 1.0f),
        opacity * (highlight ? 80.0f/255.0f : 1.0f), 1
    };
    for (int pass = 0; pass < 2; ++pass) {
        int border = pass == 0 ? stroke : 0;
        int pen = 0;
        for (int i = count - 1; i >= 0; --i) {
            const Digit *digit = sequence[i];
            for (int k = 0; k < digit->count; ++k) {
                InkRect r = digit_ink[digit->first + k];
                int x0 = scale_edge(pen + r.x, height);
                int x1 = scale_edge(pen + r.x + r.w, height);
                int y0 = scale_edge(r.y, height);
                int y1 = scale_edge(r.y + r.h, height);
                if (x0 == x1 || y0 == y1) continue;
                rectangle(frame, left+x0-border, top+y0-border,
                          x1-x0+2*border, y1-y0+2*border, pass ? fill : outline);
            }
            pen += digit->width + tracking;
        }
    }
}

// Uses the same active camera and the two matrix paths as the game's world
// projection (main+0x18a48). Its slot and manager are also used by v0.5.
// Matrix getters use different argument order for the virtual and direct paths.
bool hud_view_projection(uintptr_t base, float out[16]) {
    typedef void (*DirectMatrix)(void *, float *);
    typedef void (*VirtualMatrix)(float *, void *);
    if (!base || !out) return false;
    uintptr_t manager = *(uintptr_t *)(base + 0x184d750);
    uintptr_t gui = *(uintptr_t *)(base + 0x212668c);
    if (!manager || !gui) return false;
    unsigned slot = *(uint8_t *)(gui + 236);
    if (slot >= 8) return false;
    uintptr_t camera = *(uintptr_t *)(manager + 432*slot + 84);
    if (!camera) return false;
    _Alignas(16) float view[16], projection[16];
    if (*(uint8_t *)(camera + 8) & 1) {
        ((DirectMatrix)(base + 0x1c380))((void *)camera, view);
        ((DirectMatrix)(base + 0x1c3a0))((void *)camera, projection);
    } else {
        uintptr_t vtable = *(uintptr_t *)camera;
        VirtualMatrix get_projection = *(VirtualMatrix *)(vtable + 80);
        VirtualMatrix get_view = *(VirtualMatrix *)(vtable + 76);
        get_projection(projection, (void *)camera);
        get_view(view, (void *)camera);
    }
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            float sum = 0;
            for (int k = 0; k < 4; ++k)
                sum += view[4*r+k] * projection[4*k+c];
            out[4*r+c] = sum;
        }
    }
    return true;
}

float hud_frame_seconds(uintptr_t base, uint64_t *previous) {
    typedef uint64_t (*GetTick)(void);
    uint64_t now = ((GetTick)(base + 0x13f1af8))();
    uint64_t last = *previous;
    *previous = now;
    if (!last || now <= last) return 0;
    uint32_t frequency = (uint32_t)((GetTick)(base + 0x13f1b04))();
    if (!frequency) return 0;
    uint64_t elapsed = now - last;
    // One second already expires every number in the display.
    if (elapsed >= frequency) return 1;
    return (float)(uint32_t)elapsed / (float)frequency;
}
