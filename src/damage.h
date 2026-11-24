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
    float age;
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
                 unsigned slot, int damage);
void display_frame(Display *display, float seconds, const float view_projection[16],
                   float width, float height, DrawNumber draw);
#endif
