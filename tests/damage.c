#include "../src/damage.h"
#include <assert.h>
static unsigned count, total;
static void collect(float x, float y, unsigned value, float alpha, bool highlight) {
    assert(highlight == (count == 1));
    assert(x > 0 && y > 0 && alpha > 0 && alpha <= 1);
    ++count;
    total += value;
}

int main(void) {
    Display display;
    display_reset(&display);
    Contact hit = {.target = 11, .slot = 1, .local = true};
    display_hit(&display, &hit, 11, 1, 12);
    hit.highlight = true;
    display_hit(&display, &hit, 11, 1, 12);
    display_hit(&display, &hit, 22, 1, 4);
    display_hit(&display, &hit, 11, 2, 5);
    hit.local = false;
    display_hit(&display, &hit, 11, 1, 7);
    float m[16] = {[0]=1, [5]=1, [10]=1, [15]=-1};
    display_frame(&display, 0.1f, m, 1280, 720, collect);
    assert(!count);
    m[15] = 1;
    display_frame(&display, 0.2f, m, 1280, 720, collect);
    assert(count == 2 && total == 24);
    display_frame(&display, 1, m, 1280, 720, collect);
    assert(count == 2);
}
