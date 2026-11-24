#include "damage.h"

void display_reset(Display *display) {
    for (unsigned i = 0; i < NUMBER_CAPACITY; ++i)
        display->numbers[i].active = false;
    display->next = 0;
}

void display_hit(Display *display, const Contact *contact, uintptr_t target,
                 unsigned slot, int damage, uint64_t published_tick) {
    if (!contact || !contact->local || !target || target != contact->target ||
        slot != contact->slot || slot >= 8 || damage <= 0)
        return;

    Number *entry = &display->numbers[display->next];
    *entry = (Number){.world = contact->world, .damage = (unsigned)damage,
                      .published_tick = published_tick,
                      .lane = display->next % 3, .active = true, .highlight = contact->highlight};
    display->next = (display->next + 1) % NUMBER_CAPACITY;
}

void display_frame(Display *display, uint64_t now, uint32_t ticks_per_second, const float m[16],
                   float width, float height, DrawNumber draw) {
    for (unsigned i = 0; i < NUMBER_CAPACITY; ++i) {
        Number *entry = &display->numbers[i];
        if (!entry->active) continue;
        uint64_t elapsed = now >= entry->published_tick ? now - entry->published_tick : 0;
        if (ticks_per_second && elapsed >= ticks_per_second) {
            entry->active = false;
            continue;
        }
        if (!m || !draw || width <= 0 || height <= 0) continue;
        Point p = entry->world;
        float w = p.x*m[3] + p.y*m[7] + p.z*m[11] + m[15];
        float z = p.x*m[2] + p.y*m[6] + p.z*m[10] + m[14];
        if (!(w > 0.0001f && z >= 0 && z <= w)) continue;
        float x = (p.x*m[0] + p.y*m[4] + p.z*m[8]  + m[12]) / w;
        float y = (p.x*m[1] + p.y*m[5] + p.z*m[9]  + m[13]) / w;
        // The HUD clips the glyph bounds after movement and scaling.
        float scale = height / 720.0f;
        float age = ticks_per_second ? (float)(uint32_t)elapsed / (float)ticks_per_second : 0;
        float spread = ((int)entry->lane - 1) * 20.0f * age;
        draw((1+x)*width*0.5f + spread*scale,
             (1-y)*height*0.5f - age*36.0f*scale, entry->damage,
             age < 0.6f ? 1.0f : (1.0f-age)*2.5f, entry->highlight);
    }
}
