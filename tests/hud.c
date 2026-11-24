#include "../src/hud.h"
#include <assert.h>

static unsigned calls;
static bool ink, outline, gold;

static void check_region(void *commands, void *texture, const void *view,
                         const HudRegion *r, const float color[4], int mask) {
    (void)commands;
    (void)texture;
    assert(!view && mask == 15 && r->z == 0 && r->depth == 1);
    assert(r->width > 0 && r->height > 0 && r->x >= 0 && r->y >= 0);
    assert(r->x + r->width <= 1280 && r->y + r->height <= 720);
    if (color[0] == 1.0f) {
        assert(color[1] == (gold ? 220.0f / 255 : 1.0f));
        assert(color[2] == (gold ? 80.0f / 255 : 1.0f));
        ink = true;
    } else {
        assert(color[0] < 0.05f && color[1] == color[0] && color[2] == color[0]);
        outline = true;
    }
    ++calls;
}

int main(void) {
    HudFrame frame = {.clear = check_region, .width = 1280, .height = 720};
    for (unsigned i = 0; i < 10; ++i) {
        calls = 0;
        ink = outline = false;
        gold = (i % 2 == 0);
        hud_number(&frame, 640, 360, i, 1, gold);
        assert(ink && outline && calls <= 32);
        hud_number(&frame, 0, 0, i, 1, gold);
        hud_number(&frame, 1279, 719, UINT32_MAX, 1, gold);
    }
    calls = 0;
    hud_number(&frame, 640, 360, 1, 0, gold);
    assert(calls == 0);
}
