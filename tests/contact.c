#include "../src/contact.h"
#include <assert.h>
#include <stdatomic.h>
#include <pthread.h>

static const uintptr_t local_hunter = 0x12345678;
static const uintptr_t monster = 0x4321;
static _Atomic unsigned count;
static _Atomic unsigned threads_ready;
static uint8_t resist[3];
static bool expected_highlight = true;
static bool missing_zone;
static unsigned lookup_count;
static int monster_hp = 100;

bool mhgu_target_alive(uintptr_t target) {
    assert(target == monster);
    return monster_hp > 0;
}

const uint8_t *mhgu_hitzone_values(uintptr_t target, unsigned index) {
    assert(target == monster && index == 7);
    ++lookup_count;
    return missing_zone ? 0 : resist;
}

uintptr_t mhgu_current_player(void) {
    return local_hunter;
}

int mhgu_original_commit(uintptr_t target, unsigned part, int raw, int mode) {
    (void)target;
    (void)part;
    return mode == 2 ? 10000 : raw * 2;
}

void mhgu_publish_hit(const Contact *contact, int damage) {
    assert(contact->target == monster && contact->slot == 2);
    assert(contact->world.x == 3 && contact->world.y == 4 && contact->world.z == 5);
    assert(damage == 34 && contact->highlight == expected_highlight);
    ++count;
}

enum { BASIC, NESTED, SIMULTANEOUS, KILLING_HIT };
unsigned mhgu_original_dispatch(uintptr_t manager, const uint8_t *attack,
                                uintptr_t target, const Point *impact,
                                uintptr_t axis_start, uintptr_t axis_end,
                                const uint8_t *zone, uintptr_t flags) {
    (void)attack;
    assert(manager == 11 && axis_start == 17 && axis_end == 19);
    if (flags == KILLING_HIT) {
        monster_hp = 0;
        // A follow-up contact during the lethal hit must mask the outer hit.
        assert(mhgu_contact_dispatch(manager, attack, target, impact,
                                     axis_start, axis_end, zone, BASIC) == 421);
    } else if (flags == NESTED) {
        uint8_t palico[0x74] = {0};
        assert(mhgu_contact_dispatch(manager, palico, target, impact,
                                     axis_start, axis_end, zone, BASIC) == 421);
    } else if (flags == SIMULTANEOUS) {
        atomic_fetch_add(&threads_ready, 1);
        while (atomic_load(&threads_ready) < 2) {}
    }
    assert(mhgu_damage_commit(target, 2, 17, 0) == 34);
    assert(mhgu_damage_commit(target+1, 2, 21, 0) == 42);
    assert(mhgu_damage_commit(target, 1, 21, 0) == 42);
    assert(mhgu_damage_commit(target, 2, 21, 2) == 10000);
    return 421;
}

static void dispatch(const uint8_t *record, unsigned flags) {
    const Point point = {3, 4, 5};
    uint8_t zone[9] = {[6] = 7, [8] = 2};
    assert(mhgu_contact_dispatch(11, record, monster, &point, 17, 19, zone, flags) == 421);
}

static void *dispatch_thread(void *record) {
    dispatch(record, SIMULTANEOUS);
    return 0;
}

static void test_highlight(void) {
    uint8_t attack[0x74] = {0};
    uint8_t hunter[0x2b72] = {0};
    // Fixed hits bypass the hitzone lookup entirely.
    assert(mhgu_hit_highlight(attack, hunter, monster, 7));
    assert(!lookup_count);

    attack[0x30] = 1;
    resist[2] = 44;
    assert(!mhgu_hit_highlight(attack, hunter, monster, 7));
    resist[2] = 45;
    assert(mhgu_hit_highlight(attack, hunter, monster, 7));

    // 0x8000 chooses between cut and impact with equal weights.
    attack[0x31] = 0x80;
    resist[0] = 44;
    resist[1] = 45;
    assert(mhgu_hit_highlight(attack, hunter, monster, 7));
    // 0x10000 applies the selector at attack+0x40.
    attack[0x32] = 1;
    attack[0x40] = 2;
    assert(!mhgu_hit_highlight(attack, hunter, monster, 7));
    attack[0x40] = 0;
    assert(mhgu_hit_highlight(attack, hunter, monster, 7));

    // 0x20 takes precedence over 0x10000. A weighted tie selects cut;
    // both products are large but the raw cut value still falls below 45.
    attack[0x30] = 0x21;
    resist[0] = 30;
    resist[1] = 60;
    hunter[0x2b70] = 100;
    hunter[0x2b71] = 50;
    assert(!mhgu_hit_highlight(attack, hunter, monster, 7));
    hunter[0x2b71] = 51;
    assert(mhgu_hit_highlight(attack, hunter, monster, 7));
    hunter[0x13cc] = 4;
    attack[0x40] = 2;
    assert(!mhgu_hit_highlight(attack, hunter, monster, 7));
    missing_zone = true;
    assert(!mhgu_hit_highlight(attack, hunter, monster, 7));
    missing_zone = false;
}

int main(void) {
    uint8_t direct[0x74] = {
        [0x6d] = 1, [0xc] = 0x78, [0xd] = 0x56, [0xe] = 0x34, [0xf] = 0x12
    };
    uint8_t indirect[0x74] = {
        [0x6d] = 1, [0x10] = 1,
        [0x70] = 0x78, [0x71] = 0x56, [0x72] = 0x34, [0x73] = 0x12
    };
    dispatch(direct, NESTED);
    dispatch(indirect, BASIC);
    assert(count == 2);
    indirect[0x6e] = 1;
    dispatch(indirect, BASIC);
    mhgu_damage_commit(monster, 2, 17, 0);
    assert(count == 2);
    pthread_t a, b;
    assert(!pthread_create(&a, 0, dispatch_thread, direct));
    assert(!pthread_create(&b, 0, dispatch_thread, indirect));
    assert(!pthread_join(a, 0) && !pthread_join(b, 0));
    assert(count == 3);
    test_highlight();
    direct[0x30] = 1;
    resist[2] = 44;
    expected_highlight = false;
    dispatch(direct, BASIC);
    assert(count == 4);
    dispatch(direct, KILLING_HIT);
    assert(count == 5);
    dispatch(direct, BASIC);
    assert(count == 5);
    monster_hp = 100;
    dispatch(direct, BASIC);
    assert(count == 6);
}
