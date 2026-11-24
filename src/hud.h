#ifndef MHGU_HUD_H
#define MHGU_HUD_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int x, y, z, width, height, depth;
} HudRegion;

typedef void (*HudClearTexture)(void *commands, void *texture, const void *view,
                                const HudRegion *region, const float rgba[4],
                                int channel_mask);

typedef struct {
    void *commands;
    void *texture;
    HudClearTexture clear;
    int width, height;
} HudFrame;

// Call on the game's graphics thread before it ends recording this buffer.
// All pointers in the frame expire when the caller submits the buffer.
bool hud_begin(uintptr_t main_base, void *commands, HudFrame *out);
void hud_number(const HudFrame *frame, float x, float y, unsigned value,
                float opacity, bool highlight);

// Snapshot on the game thread while its active camera is live.
bool hud_view_projection(uintptr_t main_base, float out[16]);
uint64_t hud_tick(uintptr_t main_base);
uint32_t hud_ticks_per_second(uintptr_t main_base);

#endif
