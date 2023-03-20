/***
 * cpddl
 * -------
 * Copyright (c)2018 Daniel Fiser <danfis@danfis.cz>,
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

#ifndef __PLAN_TIME_LIMIT_H__
#define __PLAN_TIME_LIMIT_H__

#include <pddl/timer.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_time_limit {
    pddl_timer_t timer;
    double limit;
};
typedef struct pddl_time_limit pddl_time_limit_t;

/**
 * Initializes time limit as "unlimited".
 */
_pddl_inline void pddlTimeLimitInit(pddl_time_limit_t *tm)
{
    pddlTimerStart(&tm->timer);
    tm->limit = 1E100;
}

/**
 * Sets the time limit in seconds and starts counting.
 */
_pddl_inline void pddlTimeLimitSet(pddl_time_limit_t *tm, double limit)
{
    pddlTimerStart(&tm->timer);
    if (limit > 0.){
        tm->limit = limit;
    }else{
        tm->limit = 1E100;
    }
}

/**
 * Checks the time limit.
 * Returns 0 if we are still withing time limit, -1 otherwise.
 */
_pddl_inline int pddlTimeLimitCheck(pddl_time_limit_t *tm)
{
    if (tm->limit >= 1E100)
        return 0;

    pddlTimerStop(&tm->timer);
    if (pddlTimerElapsedInSF(&tm->timer) > tm->limit)
        return -1;
    return 0;
}

/**
 * Returns the remaining time from the time limit.
 */
_pddl_inline double pddlTimeLimitRemain(pddl_time_limit_t *tm)
{
    if (tm->limit >= 1E100)
        return 1E100;

    pddlTimerStop(&tm->timer);
    double elapsed = pddlTimerElapsedInSF(&tm->timer);
    if (elapsed > tm->limit)
        return 0.;
    return tm->limit - elapsed;
}

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PLAN_TIME_LIMIT_H__ */
