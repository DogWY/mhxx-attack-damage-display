#include "hud.h"

#include "contact.h"
#include <stdatomic.h>

// For the two unmodified retail NSOs main and subsdk0, the Switch loader
// places subsdk1 immediately after their combined mapped extents. The packer
// verifies both build IDs and extents before emitting the companion IPS.
extern const char _start[] __attribute__((visibility("hidden")));
static uintptr_t game_base(void) {
    return (uintptr_t)_start - MHGU_MODULE_OFFSET;
}

static Display display;
static atomic_flag display_lock = ATOMIC_FLAG_INIT;
static uint64_t last_tick;
static uintptr_t current_actor;
static uint8_t current_area;
static float camera[16];
static bool camera_valid;
static atomic_bool has_hits;

// Collect while holding the state lock. NVN commands are recorded only after
// the lock has been released, so gameplay never waits for graphics commands.
typedef struct { float x, y, alpha; unsigned damage; bool highlight; } Glyph;
static Glyph frame_glyphs[NUMBER_CAPACITY];
static unsigned frame_count;

static void lock_display(void) {
    while (atomic_flag_test_and_set_explicit(&display_lock, memory_order_acquire))
        __asm__ volatile("yield");
}

bool mhgu_target_alive(uintptr_t target) {
    // Switch GU 1.4.0: the HP setter at main+0x9a764 uses these fields.
    uintptr_t stats = *(const uintptr_t *)(target + 0x1428);
    return stats && *(const int32_t *)(stats + 0x370) > 0;
}

const uint8_t *mhgu_hitzone_values(uintptr_t target, unsigned index) {
    typedef const uint8_t *(*Lookup)(uintptr_t, unsigned);
    return ((Lookup)(game_base() + 0xbab60))(target, index);
}

uintptr_t mhgu_current_player(void) {
    uintptr_t main = game_base();
    uintptr_t manager = *(uintptr_t *)(main + 0x1885b58);
    typedef uintptr_t (*LookupPlayer)(uintptr_t);
    return manager ? ((LookupPlayer)(main + 0x276278))(manager) : 0;
}

unsigned mhgu_original_dispatch(uintptr_t manager, const uint8_t *attack,
                               uintptr_t target, const Point *impact,
                               uintptr_t axis_start, uintptr_t axis_end,
                               const uint8_t *zone, uintptr_t flags) {
    MhguDispatch *dispatch = (MhguDispatch *)(game_base() + 0x170968);
    return dispatch(manager, attack, target, impact, axis_start, axis_end, zone, flags);
}

int mhgu_original_commit(uintptr_t target, unsigned part, int raw, int mode) {
    MhguCommit *commit = (MhguCommit *)(game_base() + 0x9d6f8);
    return commit(target, part, raw, mode);
}

void mhgu_publish_hit(const Contact *contact, int damage) {
    uintptr_t actor = mhgu_current_player();
    if (!actor) return;
    lock_display();
    uint8_t area = *(const uint8_t *)(actor + 0x1054);
    if (actor != current_actor || area != current_area) {
        display_reset(&display);
        camera_valid = false;
        current_actor = actor;
        current_area = area;
    }
    has_hits = true;
    display_hit(&display, contact, contact->target, contact->slot, damage);
    atomic_flag_clear_explicit(&display_lock, memory_order_release);
}

int mhgu_input_update(void *pad) {
    uintptr_t main = game_base();
    int result = ((int (*)(void *))(main + 0x3de064))(pad);
    lock_display();
    if (has_hits) {
        if (current_actor == mhgu_current_player() &&
            *(const uint8_t *)(current_actor + 0x1054) == current_area &&
            hud_view_projection(main, camera)) {
            camera_valid = true;
        } else {
            display_reset(&display);
            has_hits = camera_valid = false;
        }
    }
    atomic_flag_clear_explicit(&display_lock, memory_order_release);
    return result;
}

static void collect_number(float x, float y, unsigned damage, float opacity, bool highlight) {
    frame_glyphs[frame_count++] = (Glyph){x, y, opacity, damage, highlight};
}

uint64_t mhgu_end_frame(void *window) {
    uintptr_t main = game_base();
    // Retail wrapper main+0x86a3a0 only tail-calls the loaded NVN proc:
    // r0 = window->commands (+376); r1 = *GOT[main+0x1841784];
    // bx r1. Reuse that exact dispatch table entry and its 64-bit return.
    typedef uint64_t (*EndRecording)(void *);
    EndRecording end = *(EndRecording *)(main + 0x191fc4c);
    void *commands = *(void **)((char *)window + 376);
    if (!atomic_load_explicit(&has_hits, memory_order_acquire)) {
        last_tick = 0;
        return end(commands);
    }
    HudFrame frame;
    if (hud_begin(main, commands, &frame)) {
        // The present callbacks are the mutually exclusive single/dual-thread
        // render paths. The scratch glyph list belongs only to that presenter.
        float seconds = hud_frame_seconds(main, &last_tick);
        lock_display();
        frame_count = 0;
        if (has_hits) {
            display_frame(&display, seconds, camera_valid ? camera : 0, frame.width,
                          frame.height, collect_number);
            bool remaining = false;
            for (unsigned i = 0; i < NUMBER_CAPACITY; ++i)
                remaining |= display.numbers[i].active;
            has_hits = remaining;
        }
        atomic_flag_clear_explicit(&display_lock, memory_order_release);
        for (unsigned i = 0; i < frame_count; ++i) {
            Glyph glyph = frame_glyphs[i];
            hud_number(&frame, glyph.x, glyph.y, glyph.damage, glyph.alpha, glyph.highlight);
        }
    }
    return end(commands);
}
