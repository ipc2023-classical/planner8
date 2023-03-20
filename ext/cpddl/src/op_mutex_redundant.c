/***
 * Copyright (c)2018 Daniel Fiser <danfis@danfis.cz>,
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
#include "pddl/op_mutex_redundant.h"

int pddlOpMutexFindRedundant(const pddl_op_mutex_pairs_t *op_mutex,
                             const pddl_strips_sym_t *sym,
                             const pddl_op_mutex_redundant_config_t *cfg,
                             pddl_iset_t *red,
                             pddl_err_t *err)
{
    switch (cfg->method){
        case PDDL_OP_MUTEX_REDUNDANT_GREEDY:
            return pddlOpMutexFindRedundantGreedy(op_mutex, sym, cfg, red, err);
        case PDDL_OP_MUTEX_REDUNDANT_MAX:
            return pddlOpMutexFindRedundantMax(op_mutex, sym, cfg, red, err);
        default:
            PANIC("Unknown method %d", cfg->method);
            return -1;
    }
}
