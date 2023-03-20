/***
 * cpddl
 * -------
 * Copyright (c)2017 Daniel Fiser <danfis@danfis.cz>,
 * AI Center, Department of Computer Science,
 * Faculty of Electrical Engineering, Czech Technical University in Prague.
 * All rights reserved.
 *
 * This file is part of cpddl.
 *
 * Distributed under the OSI-approved BSD License (the "License");
 * see accompanying file LICENSE for details or see
 * <http://www.opensource.org/licenses/bsd-license.php>.
 *
 * This software is distributed WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the License for more information.
 */

#include "internal.h"
#include "profile.h"
#include "pddl/timer.h"

struct pddl_profile_slot {
    pddl_timer_t timer;
    int counter;
    double elapsed;
};
typedef struct pddl_profile_slot pddl_profile_slot_t;

struct pddl_profile {
    pddl_profile_slot_t *slot;
    int slot_size;
    int slot_alloc;
};
typedef struct pddl_profile pddl_profile_t;

static pddl_profile_t profile = { NULL, 0, 0 };

void pddlProfileStart(int slot)
{
    if (slot >= profile.slot_alloc){
        if (profile.slot_alloc == 0)
            profile.slot_alloc = 2;
        while (slot >= profile.slot_alloc)
            profile.slot_alloc *= 2;
        profile.slot = REALLOC_ARR(profile.slot, pddl_profile_slot_t,
                                   profile.slot_alloc);
        for (int i = profile.slot_size; i < profile.slot_alloc; ++i){
            profile.slot[i].counter = 0;
            profile.slot[i].elapsed = 0.;
        }
    }
    profile.slot_size = PDDL_MAX(profile.slot_size, slot + 1);
    pddlTimerStart(&profile.slot[slot].timer);
}

void pddlProfileStop(int slot)
{
    pddl_profile_slot_t *s = profile.slot + slot;
    pddlTimerStop(&s->timer);
    ++s->counter;
    s->elapsed += pddlTimerElapsedInSF(&s->timer);
}

void pddlProfilePrint(void)
{
    for (int i = 0; i < profile.slot_size; ++i){
        fprintf(stderr, "Profile[%d]: %.8f / %d = %.8f\n",
                i, profile.slot[i].elapsed, profile.slot[i].counter,
                profile.slot[i].elapsed / profile.slot[i].counter);
    }
}
