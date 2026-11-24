#ifndef MHGU_DAMAGE_H
#define MHGU_DAMAGE_H
#include <stdint.h>
#include <stdbool.h>

typedef struct { float x, y, z; } Point;
typedef struct {
    uintptr_t target;
    unsigned slot;
    bool local;
    Point world;
    bool highlight;
} Contact;

typedef struct {
    Point world;
    unsigned damage;
    uint64_t published_tick;
    unsigned lane;
    bool active;
    bool highlight;
} Number;

enum { NUMBER_CAPACITY = 64 };
typedef struct {
    Number numbers[NUMBER_CAPACITY];
    unsigned next;
} Display;

typedef void (*DrawNumber)(float x, float y, unsigned damage, float alpha, bool highlight);
void display_reset(Display *display);
void display_hit(Display *display, const Contact *contact, uintptr_t target,
                 unsigned slot, int damage, uint64_t published_tick);
void display_frame(Display *display, uint64_t now, uint32_t ticks_per_second,
                   const float view_projection[16],
                   float width, float height, DrawNumber draw);
#endif
