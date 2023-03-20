/***
 * cpddl
 * -------
 * Copyright (c)2019 Daniel Fiser <danfis@danfis.cz>,
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

#include "pddl/strips_fact_cross_ref.h"
#include "internal.h"

void pddlStripsFactCrossRefInit(pddl_strips_fact_cross_ref_t *cref,
                                const pddl_strips_t *strips,
                                int init,
                                int goal,
                                int op_pre,
                                int op_add,
                                int op_del)
{
    if (strips->has_cond_eff){
        PANIC("pddlStripsFactCrossRefInit() does not support"
                    " conditional effects!");
    }

    int fact;

    ZEROIZE(cref);

    cref->fact_size = strips->fact.fact_size;
    cref->fact = CALLOC_ARR(pddl_strips_fact_cross_ref_fact_t,
                            cref->fact_size);

    if (init){
        PDDL_ISET_FOR_EACH(&strips->init, fact)
            cref->fact[fact].is_init = 1;
    }

    if (goal){
        PDDL_ISET_FOR_EACH(&strips->goal, fact)
            cref->fact[fact].is_goal = 1;
    }

    if (op_pre || op_add || op_del){
        for (int op_id = 0; op_id < strips->op.op_size; ++op_id){
            const pddl_strips_op_t *op = strips->op.op[op_id];

            if (op_pre){
                PDDL_ISET_FOR_EACH(&op->pre, fact)
                    pddlISetAdd(&cref->fact[fact].op_pre, op_id);
            }
            if (op_add){
                PDDL_ISET_FOR_EACH(&op->add_eff, fact)
                    pddlISetAdd(&cref->fact[fact].op_add, op_id);
            }
            if (op_del){
                PDDL_ISET_FOR_EACH(&op->del_eff, fact)
                    pddlISetAdd(&cref->fact[fact].op_del, op_id);
            }
        }
    }
}

void pddlStripsFactCrossRefFree(pddl_strips_fact_cross_ref_t *cref)
{
    for (int i = 0; i < cref->fact_size; ++i){
        pddlISetFree(&cref->fact[i].op_pre);
        pddlISetFree(&cref->fact[i].op_add);
        pddlISetFree(&cref->fact[i].op_del);
    }
    if (cref->fact != NULL)
        FREE(cref->fact);
}
