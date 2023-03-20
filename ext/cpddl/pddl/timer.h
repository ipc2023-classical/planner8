/***
 * Copyright (c)2018 Daniel Fiser <danfis@danfis.cz>,
 * All rights reserved.
 *
 * This file is part of cpddl.
 *
 * Distributed under the OSI-approved BSD License (the "License");
 * see accompanying file BDS-LICENSE for details or see
 * <http://www.opensource.org/licenses/bsd-license.php>.
 *
 * This software is distributed WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the License for more information.
 */

#ifndef __PDDL_TIMER_H__
#define __PDDL_TIMER_H__

#include <pddl/common.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Timer
 * ======
 */
struct pddl_timer {
    struct timespec t_start;   /*!< Time when timer was started */
    struct timespec t_elapsed; /*!< Elapsed time from start of timer in
                                    moment when pddlTimerStop() was called */
};
typedef struct pddl_timer pddl_timer_t;

/**
 * Functions
 * ----------
 */

/**
 * Starts timer.
 *
 * In fact, this only fills .t_start member of pddl_timer_t struct.
 */
_pddl_inline void pddlTimerStart(pddl_timer_t *t);

/**
 * Stops timer and computes elapsed time from last pddlTimerStart() call.
 */
_pddl_inline void pddlTimerStop(pddl_timer_t *t);

/**
 * Returns elapsed time.
 */
_pddl_inline const struct timespec *pddlTimerElapsed(const pddl_timer_t *t);

/**
 * Returns nanosecond part of elapsed time.
 */
_pddl_inline long pddlTimerElapsedNs(const pddl_timer_t *t);

/**
 * Returns microsecond part of elapsed time.
 */
_pddl_inline long pddlTimerElapsedUs(const pddl_timer_t *t);

/**
 * Returns milisecond part of elapsed time.
 */
_pddl_inline long pddlTimerElapsedMs(const pddl_timer_t *t);

/**
 * Returns second part of elapsed time.
 */
_pddl_inline long pddlTimerElapsedS(const pddl_timer_t *t);

/**
 * Returns minute part of elapsed time.
 */
_pddl_inline long pddlTimerElapsedM(const pddl_timer_t *t);

/**
 * Returns hour part of elapsed time.
 */
_pddl_inline long pddlTimerElapsedH(const pddl_timer_t *t);


/**
 * Returns elapsed time in ns.
 */
_pddl_inline unsigned long pddlTimerElapsedInNs(const pddl_timer_t *t);

/**
 * Returns elapsed time in us.
 */
_pddl_inline unsigned long pddlTimerElapsedInUs(const pddl_timer_t *t);

/**
 * Returns elapsed time in ms.
 */
_pddl_inline unsigned long pddlTimerElapsedInMs(const pddl_timer_t *t);

/**
 * Returns elapsed time in s.
 */
_pddl_inline unsigned long pddlTimerElapsedInS(const pddl_timer_t *t);

/**
 * Returns elapsed time in m.
 */
_pddl_inline unsigned long pddlTimerElapsedInM(const pddl_timer_t *t);

/**
 * Returns elapsed time in h.
 */
_pddl_inline unsigned long pddlTimerElapsedInH(const pddl_timer_t *t);


/**
 * Returns elapsed time in s as float number
 */
_pddl_inline double pddlTimerElapsedInSF(const pddl_timer_t *t);

/**** INLINES ****/
_pddl_inline void pddlTimerStart(pddl_timer_t *t)
{
    clock_gettime(CLOCK_MONOTONIC, &t->t_start);
}

_pddl_inline void pddlTimerStop(pddl_timer_t *t)
{
    struct timespec cur;
    clock_gettime(CLOCK_MONOTONIC, &cur);

    /* compute diff */
    if (cur.tv_nsec > t->t_start.tv_nsec){
        t->t_elapsed.tv_nsec = cur.tv_nsec - t->t_start.tv_nsec;
        t->t_elapsed.tv_sec = cur.tv_sec - t->t_start.tv_sec;
    }else{
        t->t_elapsed.tv_nsec = cur.tv_nsec + 1000000000L - t->t_start.tv_nsec;
        t->t_elapsed.tv_sec = cur.tv_sec - 1 - t->t_start.tv_sec;
    }
}

_pddl_inline const struct timespec *pddlTimerElapsed(const pddl_timer_t *t)
{
    return &t->t_elapsed;
}

_pddl_inline long pddlTimerElapsedNs(const pddl_timer_t *t)
{
    return t->t_elapsed.tv_nsec % 1000L;
}

_pddl_inline long pddlTimerElapsedUs(const pddl_timer_t *t)
{
    return (t->t_elapsed.tv_nsec / 1000L) % 1000L;
}

_pddl_inline long pddlTimerElapsedMs(const pddl_timer_t *t)
{
    return t->t_elapsed.tv_nsec / 1000000L;
}

_pddl_inline long pddlTimerElapsedS(const pddl_timer_t *t)
{
    return t->t_elapsed.tv_sec % 60L;
}

_pddl_inline long pddlTimerElapsedM(const pddl_timer_t *t)
{
    return (t->t_elapsed.tv_sec / 60L) % 60L;
}

_pddl_inline long pddlTimerElapsedH(const pddl_timer_t *t)
{
    return t->t_elapsed.tv_sec / 3600L;
}


_pddl_inline unsigned long pddlTimerElapsedInNs(const pddl_timer_t *t)
{
    return t->t_elapsed.tv_nsec + t->t_elapsed.tv_sec * 1000000000L;
}

_pddl_inline unsigned long pddlTimerElapsedInUs(const pddl_timer_t *t)
{
    return t->t_elapsed.tv_nsec / 1000L + t->t_elapsed.tv_sec * 1000000L;
}

_pddl_inline unsigned long pddlTimerElapsedInMs(const pddl_timer_t *t)
{
    return t->t_elapsed.tv_nsec / 1000000L + t->t_elapsed.tv_sec * 1000L;
}

_pddl_inline unsigned long pddlTimerElapsedInS(const pddl_timer_t *t)
{
    return t->t_elapsed.tv_sec;
}

_pddl_inline unsigned long pddlTimerElapsedInM(const pddl_timer_t *t)
{
    return t->t_elapsed.tv_sec / 60L;
}

_pddl_inline unsigned long pddlTimerElapsedInH(const pddl_timer_t *t)
{
    return t->t_elapsed.tv_sec / 3600L;
}

_pddl_inline double pddlTimerElapsedInSF(const pddl_timer_t *t)
{
    double time;
    time  = t->t_elapsed.tv_nsec / 1000000000.;
    time += t->t_elapsed.tv_sec;
    return time;
}

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_TIMER_H__ */
