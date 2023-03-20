/***
 * cpddl
 * -------
 * Copyright (c)2019 Daniel Fiser <danfis@danfis.cz>,
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

#include "pddl/critical_path.h"

int pddlHm(int m,
           const pddl_strips_t *strips,
           pddl_mutex_pairs_t *mutex,
           pddl_iset_t *unreachable_facts,
           pddl_iset_t *unreachable_ops,
           float time_limit,
           size_t excess_memory,
           pddl_err_t *err)
{
    if (m == 1){
        if (time_limit > 0 || excess_memory > 0 || mutex != NULL)
            PDDL_INFO(err, "h^1 using pddlHm() ignores mutex pairs, time limit"
                       " and memory limit");
        return pddlH1(strips, unreachable_facts, unreachable_ops, err);

    }else if (m == 2){
        if (excess_memory > 0)
            PDDL_INFO(err, "h^2 using pddlHm() ignores the memory limit");
        return pddlH2(strips, mutex, unreachable_facts, unreachable_ops,
                      time_limit, err);

    }else if (m == 3){
        return pddlH3(strips, mutex, unreachable_facts, unreachable_ops,
                      time_limit, excess_memory, err);

    }else{
        PDDL_INFO(err, "h^%d not supported!", m);
        return -1;
    }
}
