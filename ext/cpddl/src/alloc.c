/***
 * Copyright (c)2022 Daniel Fiser <danfis@danfis.cz>,
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

#include "pddl/alloc.h"

#ifdef PDDL_MEMCHECK
#include <malloc.h>

#ifndef PDDL_MEMCHECK_REPORT_THRESHOLD
# define PDDL_MEMCHECK_REPORT_THRESHOLD (1024 * 1024) // 1MB
#endif /* PDDL_MEMCHECK_REPORT_THRESHOLD */

# define _PDDL_MEMCHECK_INC(ptr) do { \
        size_t size = malloc_usable_size((ptr)); \
        cur_alloc += size; \
        if (cur_alloc > peak_alloc) \
            peak_alloc = cur_alloc; \
        \
        if (!reg_at_exit){ \
            atexit(stats); \
            reg_at_exit = 1; \
        } \
        if (size >= reportThreshold()){ \
            fprintf(stderr, "%s:%d[%s] Allocated %d bytes\n", \
                    file, line, func, (int)size); \
            stats(); \
        } \
    } while (0)

# define _PDDL_MEMCHECK_DEC(ptr) do { \
        if ((ptr) != NULL){ \
            size_t size = malloc_usable_size((ptr)); \
            cur_alloc -= size; \
            if (size >= reportThreshold()){ \
                fprintf(stderr, "%s:%d[%s] Freed %d bytes\n", \
                        file, line, func, (int)size); \
                stats(); \
            } \
        } \
    } while (0)

# define _PDDL_MEMCHECK_ARGS , const char *file, int line, const char *func

/** currently allocated memory */
static unsigned long cur_alloc = 0L;
/* maximal amount of allocated mem during algorithm */
static unsigned long peak_alloc = 0L;
static int reg_at_exit = 0;
static size_t report_threshold = (size_t)-1;

struct _info_t {
    size_t size;
};
typedef struct _info_t info_t;

static void stats(void)
{
    struct mallinfo info;

    info = mallinfo();

    fprintf(stderr, "MemCheck stats:\n");
    fprintf(stderr, "    peak mem:       %10lu bytes\n", peak_alloc);
    fprintf(stderr, "    unfreed:        %10lu bytes\n", cur_alloc);
    fprintf(stderr, "    arena size:     %10d bytes\n", info.arena);
    fprintf(stderr, "    mmaped size:    %10d bytes\n", info.hblkhd);
    fprintf(stderr, "    highwater mark: %10d\n", info.usmblks);
    fprintf(stderr, "    in-use:         %10d bytes\n", info.uordblks);
    fprintf(stderr, "    free blocks:    %10d bytes\n", info.fordblks);
    fprintf(stderr, "    top-head free:  %10d bytes\n", info.keepcost);
    fprintf(stderr, "\n");
}

_pddl_inline size_t reportThreshold(void)
{
    if (report_threshold == (size_t)-1){
        const char *env = getenv("PDDL_MEMCHECK_REPORT_THRESHOLD");
        if (env != NULL){
            report_threshold = atoi(env);
        }else{
            report_threshold = PDDL_MEMCHECK_REPORT_THRESHOLD;
        }
    }

    return report_threshold;
}

void pddlFreeCheck(void *ptr _PDDL_MEMCHECK_ARGS)
{
    _PDDL_MEMCHECK_DEC(ptr);
    free(ptr);
}

#else /* PDDL_MEMCHECK */

# define _PDDL_MEMCHECK_INC(ptr)
# define _PDDL_MEMCHECK_DEC(ptr)
# define _PDDL_MEMCHECK_ARGS
#endif /* PDDL_MEMCHECK */


void *pddlRealloc(void *ptr, size_t size _PDDL_MEMCHECK_ARGS)
{
    void *ret;
   
    _PDDL_MEMCHECK_DEC(ptr);

    ret = realloc(ptr, size);
    if (ret == NULL && size != 0){
        fprintf(stderr, "Fatal error: Allocation of memory failed!\n");
        fflush(stderr);
        exit(-1);
    }

    _PDDL_MEMCHECK_INC(ret);

    return ret;
}

void *pddlAllocAlign(size_t size, size_t alignment _PDDL_MEMCHECK_ARGS)
{
    void *mem;

    if (posix_memalign(&mem, alignment, size) != 0){
        fprintf(stderr, "Fatal error: Allocation of memory failed!\n");
        fflush(stderr);
        exit(-1);
    }

    _PDDL_MEMCHECK_INC(mem);

    return mem;
}

void *pddlCalloc(size_t nmemb, size_t size _PDDL_MEMCHECK_ARGS)
{
    void *ret;

    ret = calloc(nmemb, size);
    if (ret == NULL && size != 0){
        fprintf(stderr, "Fatal error: Allocation of memory failed!\n");
        fflush(stderr);
        exit(-1);
    }

    _PDDL_MEMCHECK_INC(ret);

    return ret;
}

char *pddlStrdup(const char *str _PDDL_MEMCHECK_ARGS)
{
    char *ret = strdup(str);
    if (ret == NULL){
        fprintf(stderr, "Fatal error: Allocation of memory failed!\n");
        fflush(stderr);
        exit(-1);
    }

    _PDDL_MEMCHECK_INC(ret);

    return ret;
}

