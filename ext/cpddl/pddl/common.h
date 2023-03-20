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

#ifndef __PDDL_COMMON_H__
#define __PDDL_COMMON_H__

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif /* _DEFAULT_SOURCE */

#ifndef _BSD_SOURCE
#define _BSD_SOURCE 1
#endif /* _BSD_SOURCE */

#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <poll.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <pddl/config.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

enum pddl_status {
    PDDL_OK = 0,
    PDDL_FAIL = 1,
    PDDL_ERR = 2,
};
typedef enum pddl_status pddl_status_t;

/** Compiler-specific pragmas */
#if defined(__clang__) && __clang_major__ < 10
# pragma clang diagnostic ignored "-Wmissing-braces"
#endif

#ifdef __ICC
/* disable unused parameter warning */
# pragma warning(disable:869)
/* disable annoying "operands are evaluated in unspecified order" warning */
# pragma warning(disable:981)
#endif /* __ICC */



/**
 * Returns offset of member in given type (struct).
 */
#if defined(__clang__) && __clang_major__ < 10
# define pddl_offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#else
# define pddl_offsetof(TYPE, MEMBER) offsetof(TYPE, MEMBER)
/* #define pddl_offsetof(TYPE, MEMBER) __builtin_offsetof(TYPE, MEMBER) */
#endif

/**
 * Returns container of given member
 */
#define pddl_container_of(ptr, type, member) \
    ((type *)( (char *)ptr - pddl_offsetof(type, member)))

/**
 * Marks inline function.
 */
#if defined(__GNUC__) || defined(__clang__)
#  ifdef PDDL_DEBUG
#    define _pddl_inline static __attribute__((unused))
#  else /* PDDL_DEBUG */
#    ifdef __NO_INLINE__
#      define _pddl_inline static __attribute__((unused))
#    else /* __NO_INLINE */
#      define _pddl_inline static inline __attribute__((always_inline,unused))
#    endif /* __NO_INLINE */
#  endif /* PDDL_DEBUG */
#else /* defined(__GNUC__) || defined(__clang__) */
# define _pddl_inline static inline
#endif /* defined(__GNUC__) || defined(__clang__) */

/**
 * pddl_packed - mark struct as "packed", i.e., no alignment of members
 * _pddl_prefetch(x) - prefetches the cacheline at "x" for read
 * _pddl_prefetchw(x) - prefetches the cacheline at "x" for write
 * pddl_likely/pddl_unlikely - mark likely/unlikely branch
 * PDDL_UNUSED - mark function as possibly unused
 */
#if defined(__GNUC__) || defined(__clang__)
# define pddl_packed __attribute__ ((packed))
# define _pddl_prefetch(x) __builtin_prefetch(x)
# define _pddl_prefetchw(x) __builtin_prefetch(x,1)
# define pddl_likely(x) __builtin_expect(!!(x), 1)
# define pddl_unlikely(x) __builtin_expect(!!(x), 0)
# define PDDL_UNUSED(f) f __attribute__((unused))
#else /* defined(__GNUC__) || defined(__clang__) */
# define pddl_packed
# define _pddl_prefetch(x)
# define _pddl_prefetchw(x)
# define pddl_likely(x) !!(x)
# define pddl_unlikely(x) !!(x)
# define PDDL_UNUSED(f)
#endif /* defined(__GNUC__) || defined(__clang__) */


#define PDDL_MIN(x, y) ((x) < (y) ? (x) : (y)) /*!< minimum */
#define PDDL_MAX(x, y) ((x) > (y) ? (x) : (y)) /*!< maximum */

/**
 * Swaps {a} and {b} using given temporary variable {tmp}.
 */
#define PDDL_SWAP(a, b, tmp) \
    do { \
        (tmp) = (a); \
        (a) = (b); \
        (b) = (tmp); \
    } while (0)


typedef struct pddl pddl_t;
typedef struct pddl_strips pddl_strips_t;

/** Type for holding number of objects */
typedef uint16_t pddl_obj_size_t;
/** Type for holding number of action parameters */
typedef uint16_t pddl_action_param_size_t;

typedef int pddl_obj_id_t;

/** Constant for undefined object ID.
 *  It should be always defined as something negative so we can test object
 *  ID with >= 0 and < 0. */
#define PDDL_OBJ_ID_UNDEF ((pddl_obj_id_t)-1)

/** Dead-end (infinity) cost */
#define PDDL_COST_DEAD_END (INT_MAX / 2)
/** Maximum cost that can be assigned */
#define PDDL_COST_MAX ((INT_MAX / 2) - 1)
/** Minimum cost */
#define PDDL_COST_MIN ((INT_MIN / 2) + 1)
/** Zeroize given struct */
#define PDDL_ZEROIZE(SPTR) memset((SPTR), 0, sizeof(*(SPTR)))
#define PDDL_ZEROIZE_ARR(SPTR, SZ) memset((SPTR), 0, sizeof(*(SPTR)) * (SZ))
#define PDDL_ZEROIZE_RAW(SPTR, SZ) memset((SPTR), 0, (SZ))


/**
 * Type for storing state ID.
 */
typedef unsigned int pddl_state_id_t;

#define PDDL_NO_STATE_ID ((pddl_state_id_t)-1)


/**
 * Type of one word in buffer of packed variable values.
 * Bear in mind that the word's size must be big enough to store the whole
 * range of the biggest variable.
 */
typedef uint32_t pddl_fdr_packer_word_t;

/**
 * Number of bits in packed word
 */
#define PDDL_FDR_PACKER_WORD_BITS (8u * sizeof(pddl_fdr_packer_word_t))

/**
 * Word with only highest bit set (i.e., 0x80000...)
 */
#define PDDL_FDR_PACKER_WORD_SET_HI_BIT \
    (((pddl_fdr_packer_word_t)1u) << (PDDL_FDR_PACKER_WORD_BITS - 1u))

/**
 * Word with all bits set (i.e., 0xffff...)
 */
#define PDDL_FDR_PACKER_WORD_SET_ALL_BITS ((pddl_fdr_packer_word_t)-1)


extern const char *pddl_version;
extern const char *pddl_build_commit;

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_COMMON_H__ */
