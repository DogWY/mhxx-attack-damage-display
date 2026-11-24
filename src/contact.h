#ifndef MHGU_CONTACT_H
#define MHGU_CONTACT_H

#include "damage.h"

// ARM AAPCS, Switch GU 1.4.0. All uintptr_t arguments are one guest word.
typedef unsigned MhguDispatch(uintptr_t manager, const uint8_t *attack,
                              uintptr_t target, const Point *impact,
                              uintptr_t axis_start, uintptr_t axis_end,
                              const uint8_t *zone, uintptr_t flags);
typedef int MhguCommit(uintptr_t target, unsigned part, int raw, int mode);

extern MhguDispatch mhgu_original_dispatch;
extern MhguCommit mhgu_original_commit;
uintptr_t mhgu_current_player(void);
bool mhgu_target_alive(uintptr_t target);
const uint8_t *mhgu_hitzone_values(uintptr_t target, unsigned index);
bool mhgu_hit_highlight(const uint8_t *attack, const uint8_t *player,
                        uintptr_t target, unsigned zone_index);

// Synchronous sink supplied by the display integration.
void mhgu_publish_hit(const Contact *contact, int damage);
MhguDispatch mhgu_contact_dispatch;
MhguCommit mhgu_damage_commit;
#endif
