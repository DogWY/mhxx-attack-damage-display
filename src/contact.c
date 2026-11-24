#include "contact.h"
#include <stdatomic.h>

typedef struct {
    Contact contact;
    const uint8_t *attack;
    const uint8_t *player;
    unsigned hitzone;
} ActiveHit;

typedef struct {
    atomic_uintptr_t owner;
    const ActiveHit *active;
} ContactThread;

static ContactThread scopes[16];

static uintptr_t thread_identity(void) {
#if defined(__arm__)
    uintptr_t thread;
    __asm__("mrc p15, 0, %0, c13, c0, 3" : "=r"(thread));
    return thread;
#else
    static _Thread_local char identity;
    return (uintptr_t)&identity;
#endif
}

static ContactThread *find_scope(uintptr_t thread) {
    for (unsigned i = 0; i < sizeof(scopes)/sizeof(*scopes); ++i)
        if (atomic_load_explicit(&scopes[i].owner, memory_order_acquire) == thread)
            return &scopes[i];
    return 0;
}

static ContactThread *claim_scope(uintptr_t thread) {
    ContactThread *scope = find_scope(thread);
    if (scope) return scope;
    for (unsigned i = 0; i < sizeof(scopes)/sizeof(*scopes); ++i) {
        uintptr_t empty = 0;
        if (atomic_compare_exchange_strong_explicit(&scopes[i].owner, &empty, thread,
                                                    memory_order_acquire,
                                                    memory_order_relaxed))
            return &scopes[i];
    }
    return 0;
}

static uint32_t read_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

bool mhgu_hit_highlight(const uint8_t *attack, const uint8_t *player,
                        uintptr_t target, unsigned zone_index) {
    // These masks and the weighted comparison follow v0.5's commit callback.
    // The comparison selects a column. The threshold applies to its raw value.
    uint32_t types = read_u32(attack + 0x30);
    const uint32_t melee_types = 0x20 | 0x8000 | 0x10000;
    if (!(types & (melee_types | 1))) return true;  // Fixed damage.

    const uint8_t *zone = mhgu_hitzone_values(target, zone_index);
    if (!zone) return false;
    if (!(types & melee_types)) return zone[2] >= 45;

    unsigned cut_weight = 100, impact_weight = 100;
    bool restrict_type = (types & 0x10000) != 0;
    if (types & 0x20) {
        cut_weight = player[0x2b70];
        impact_weight = player[0x2b71];
        restrict_type = (player[0x13cc] & 4) != 0;
    }
    if (restrict_type)
        return zone[(attack[0x40] & 2) ? 0 : 1] >= 45;
    unsigned column = cut_weight * zone[0] < impact_weight * zone[1] ? 1 : 0;
    return zone[column] >= 45;
}

static uintptr_t attack_owner(const uint8_t *attack) {
    if (!attack || attack[0x6d] != 1) return 0;
    if (attack[0x10] && attack[0x6e]) return 0;
    // Both pointer fields are little-endian 32-bit addresses, including when
    // fixtures exercise this decoder on a 64-bit host.
    const uint8_t *p = attack + (attack[0x10] ? 0x70 : 0x0c);
    return read_u32(p);
}

unsigned mhgu_contact_dispatch(uintptr_t manager, const uint8_t *attack,
                               uintptr_t target, const Point *impact,
                               uintptr_t axis_start, uintptr_t axis_end,
                               const uint8_t *zone, uintptr_t flags) {
    ActiveHit hit = {0};
    uintptr_t owner = attack_owner(attack);
    if (owner && target && impact && zone && zone[8] < 8 &&
        owner == mhgu_current_player() && mhgu_target_alive(target)) {
        hit.contact.target = target;
        hit.contact.slot = zone[8];
        hit.contact.local = true;
        hit.contact.world = *impact;
        hit.attack = attack;
        hit.player = (const uint8_t *)owner;
        hit.hitzone = zone[6];
    }

    uintptr_t thread = thread_identity();
    ContactThread *scope = thread ? claim_scope(thread) : 0;
    const ActiveHit *previous = 0;
    if (scope) {
        previous = scope->active;
        scope->active = &hit; // Also masks an outer hit during nonlocal contacts.
    }
    unsigned result = mhgu_original_dispatch(manager, attack, target, impact,
                                             axis_start, axis_end, zone, flags);
    if (scope) {
        scope->active = previous;
        if (!previous)
            atomic_store_explicit(&scope->owner, 0, memory_order_release);
    }
    return result;
}

int mhgu_damage_commit(uintptr_t target, unsigned part, int raw, int mode) {
    int damage = mhgu_original_commit(target, part, raw, mode);
    if (raw <= 0 || damage <= 0 || mode == 2 || part >= 8) return damage;
    uintptr_t thread = thread_identity();
    ContactThread *scope = thread ? find_scope(thread) : 0;
    const ActiveHit *hit = scope ? scope->active : 0;
    if (hit && hit->contact.local && hit->contact.target == target && hit->contact.slot == part) {
        Contact contact = hit->contact;
        contact.highlight = mhgu_hit_highlight(hit->attack, hit->player, target, hit->hitzone);
        mhgu_publish_hit(&contact, damage);
    }
    return damage;
}
