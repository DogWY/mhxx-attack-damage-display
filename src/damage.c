#include "damage.h"

void display_reset(Display *display) {
    for (unsigned i = 0; i < NUMBER_CAPACITY; ++i)
        display->numbers[i].active = false;
    display->next = 0;
}

void display_hit(Display *display, const Contact *contact, uintptr_t target,
                 unsigned slot, int damage) {
    if (!contact || !contact->local || !target || target != contact->target ||
        slot != contact->slot || slot >= 8 || damage <= 0)
        return;

    Number *entry = &display->numbers[display->next];
    *entry = (Number){.world = contact->world, .damage = (unsigned)damage,
                      .lane = display->next % 3, .active = true, .highlight = contact->highlight};
    display->next = (display->next + 1) % NUMBER_CAPACITY;
}

void display_frame(Display *display, float seconds, const float m[16],
                   float width, float height, DrawNumber draw) {
    for (unsigned i = 0; i < NUMBER_CAPACITY; ++i) {
        Number *entry = &display->numbers[i];
        if (!entry->active) continue;
        if (seconds > 0) entry->age += seconds;
        if (entry->age >= 1.0f) {
            entry->active = false;
            continue;
        }
        if (!m || !draw || width <= 0 || height <= 0) continue;
        Point p = entry->world;
        float w = p.x*m[3] + p.y*m[7] + p.z*m[11] + m[15];
        if (!(w > 0.0001f)) continue;
        float x = (p.x*m[0] + p.y*m[4] + p.z*m[8]  + m[12]) / w;
        float y = (p.x*m[1] + p.y*m[5] + p.z*m[9]  + m[13]) / w;
        if (!(x >= -1 && x <= 1 && y >= -1 && y <= 1)) continue;
        float scale = height / 720.0f;
        float spread = ((int)entry->lane - 1) * 20.0f * entry->age;
        draw((1+x)*width*0.5f + spread*scale,
             (1-y)*height*0.5f - entry->age*36.0f*scale, entry->damage,
             entry->age < 0.6f ? 1.0f : (1.0f-entry->age)*2.5f, entry->highlight);
    }
}
