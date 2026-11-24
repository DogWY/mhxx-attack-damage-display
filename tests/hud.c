#include "../src/hud.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <string.h>

static unsigned calls;
static bool ink, outline, gold;

static void check_region(void *commands, void *texture, const void *view,
                         const HudRegion *r, const float color[4], int mask) {
    (void)commands;
    (void)texture;
    assert(!view && mask == 15 && r->z == 0 && r->depth == 1);
    assert(color[3] == 1);
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

// Zero represents an untouched scene pixel. Record the final color after
// overlapping ink and outline clears, just as the target texture would.
typedef unsigned char Tile[64][64];
static void paint(void *commands, void *texture, const void *view,
                  const HudRegion *r, const float color[4], int mask) {
    check_region(commands, texture, view, r, color, mask);
    Tile *tile = texture;
    assert(r->x + r->width <= 64 && r->y + r->height <= 64);
    for (int y = r->y; y < r->y + r->height; ++y)
        for (int x = r->x; x < r->x + r->width; ++x)
            (*tile)[y][x] = color[0] == 1 ? 1 : 2;
}

static void check_fade(int height) {
    Tile current, previous;
    HudFrame frame = {.clear = paint, .texture = &current, .width = 1280, .height = height};
    unsigned previous_count = 0;
    for (int level = 16; level >= 0; --level) {
        memset(current, 0, sizeof current);
        calls = 0;
        hud_number(&frame, 32, 32, 8, level / 16.0f, gold);
        assert(calls <= 100);
        unsigned painted = 0;
        for (int y = 0; y < 64; ++y) {
            for (int x = 0; x < 64; ++x) {
                if (!current[y][x]) continue;
                if (level < 16) assert(current[y][x] == previous[y][x]);
                ++painted;
            }
        }
        if (level > 0) assert(painted > 0);
        else assert(calls == 0);
        if (level < 16) assert(painted < previous_count);
        previous_count = painted;
        memcpy(previous, current, sizeof current);
    }
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
    check_fade(720);
    gold = !gold;
    check_fade(480);

    const float partial[][2] = {{-1, 360}, {1281, 360}, {640, -1}, {640, 721}};
    for (unsigned i = 0; i < sizeof partial / sizeof partial[0]; ++i) {
        calls = 0;
        hud_number(&frame, partial[i][0], partial[i][1], 88, 1, gold);
        assert(calls > 0);
    }
    // At 720p an 88 is 47 px wide, 30 px high, with a 2 px outline.
    const float outside[][2] = {{-26, 360}, {1305, 360}, {640, -17}, {640, 737},
                                {FLT_MAX, 360}, {640, -FLT_MAX}, {NAN, 360}, {640, INFINITY}};
    calls = 0;
    for (unsigned i = 0; i < sizeof outside / sizeof outside[0]; ++i)
        hud_number(&frame, outside[i][0], outside[i][1], 88, 1, gold);
    assert(calls == 0);
}
