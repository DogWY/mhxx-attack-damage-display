#include "../src/damage.h"
#include <assert.h>
static unsigned count, total;
static float last_alpha;
static void collect(float x, float y, unsigned value, float alpha, bool highlight) {
    assert(highlight == (value == 13));
    assert(x > 0 && y > 0 && alpha > 0 && alpha <= 1);
    ++count;
    total += value;
    last_alpha = alpha;
}

static void test_depth_clipping(void) {
    // Right-handed perspective with near = 1 and far = 5, looking down -Z.
    const float projection[16] = {[0]=1, [5]=1, [10]=-1.25f, [11]=-1, [14]=-1.25f};
    const struct { float z; bool visible; } cases[] = {
        {-0.5f, false}, {-1, true}, {-2, true}, {-5, true}, {-6, false}
    };
    Display display;
    Contact hit = {.target = 11, .slot = 1, .local = true};
    for (unsigned i = 0; i < sizeof(cases) / sizeof(*cases); ++i) {
        display_reset(&display);
        hit.world.z = cases[i].z;
        display_hit(&display, &hit, 11, 1, 12, 0);
        unsigned before = count;
        display_frame(&display, 0, 1000, projection, 1280, 720, collect);
        assert(count == before + cases[i].visible);
    }
}

static void collect_edge(float x, float y, unsigned value, float alpha, bool highlight) {
    assert(x > -1 && x < 0 && y == 360);
    assert(value == 12 && alpha == 1 && !highlight);
    ++count;
}

static void test_partially_visible_number(void) {
    Display display;
    display_reset(&display);
    Contact hit = {.target = 11, .slot = 1, .local = true, .world = {-1.001f, 0, 0}};
    const float identity[16] = {[0]=1, [5]=1, [10]=1, [15]=1};
    display_hit(&display, &hit, 11, 1, 12, 0);
    unsigned before = count;
    display_frame(&display, 0, 1000, identity, 1280, 720, collect_edge);
    assert(count == before + 1);
}

int main(void) {
    Display display;
    display_reset(&display);
    Contact hit = {.target = 11, .slot = 1, .local = true};
    const uint64_t start = UINT64_C(1) << 48;
    display_hit(&display, &hit, 11, 1, 12, start);
    hit.highlight = true;
    display_hit(&display, &hit, 11, 1, 13, start);
    display_hit(&display, &hit, 22, 1, 4, start);
    display_hit(&display, &hit, 11, 2, 5, start);
    hit.local = false;
    display_hit(&display, &hit, 11, 1, 7, start);
    float m[16] = {[0]=1, [5]=1, [10]=1, [15]=-1};
    display_frame(&display, start + 100, 1000, m, 1280, 720, collect);
    assert(!count);
    m[15] = 1;
    display_frame(&display, start + 200, 1000, m, 1280, 720, collect);
    assert(count == 2 && total == 25);

    // A hit at the end of a long render gap still gets its own full lifetime.
    hit.local = true;
    display_hit(&display, &hit, 11, 1, 13, start + 1990);
    display_frame(&display, start + 2000, 1000, m, 1280, 720, collect);
    assert(count == 3 && total == 38 && last_alpha == 1.0f);
    display_frame(&display, start + 2300, 1000, 0, 1280, 720, collect);
    assert(count == 3);
    display_frame(&display, start + 2790, 1000, m, 1280, 720, collect);
    assert(count == 4 && total == 51 && last_alpha > 0.49f && last_alpha < 0.51f);
    display_frame(&display, start + 2990, 1000, m, 1280, 720, collect);
    assert(count == 4);
    test_depth_clipping();
    test_partially_visible_number();
}
