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

#ifndef __PDDL_ALLOC_H__
#define __PDDL_ALLOC_H__

#include <pddl/common.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Alloc - Memory Allocation
 * ==========================
 *
 * Functions and macros for memory allocation.
 */

/* Memory allocation: - internal macro */
#ifdef PDDL_MEMCHECK
# define _PDDL_ALLOC_MEMORY(type, ptr_old, size) \
    (type *)pddlRealloc((void *)ptr_old, (size), __FILE__, __LINE__, __func__)

# define _PDDL_ALLOC_ALIGN_MEMORY(type, size, align) \
    (type *)pddlAllocAlign((size), (align), __FILE__, __LINE__, __func__)

# define _PDDL_CALLOC_MEMORY(type, num_elements, size_of_el) \
    (type *)pddlCalloc((num_elements), (size_of_el), __FILE__, __LINE__, __func__)

# define _PDDL_STRDUP(str) pddlStrdup((str), __FILE__, __LINE__, __func__)

# define PDDL_FREE(ptr) pddlFreeCheck((ptr), __FILE__, __LINE__, __func__)

#else /* PDDL_MEMCHECK */
# define _PDDL_ALLOC_MEMORY(type, ptr_old, size) \
    (type *)pddlRealloc((void *)ptr_old, (size))

# define _PDDL_ALLOC_ALIGN_MEMORY(type, size, align) \
    (type *)pddlAllocAlign((size), (align))

# define _PDDL_CALLOC_MEMORY(type, num_elements, size_of_el) \
    (type *)pddlCalloc((num_elements), (size_of_el))

# define _PDDL_STRDUP(str) pddlStrdup((str))

# define PDDL_FREE(ptr) free(ptr) /*!< Deallocates memory */
#endif /* PDDL_MEMCHECK */



/**
 * Allocate memory for one element of given type.
 */
#define PDDL_ALLOC(type) \
    _PDDL_ALLOC_MEMORY(type, NULL, sizeof(type))

/**
 * Allocates aligned memory
 */
#define PDDL_ALLOC_ALIGN(type, align) \
    _PDDL_ALLOC_ALIGN_MEMORY(type, sizeof(type), align)

/**
 * Allocates aligned array
 */
#define PDDL_ALLOC_ALIGN_ARR(type, num_els, align) \
    _PDDL_ALLOC_ALIGN_MEMORY(type, sizeof(type) * (num_els), align)

/**
 * Allocate array of elements of given type.
 */
#define PDDL_ALLOC_ARR(type, num_elements) \
    _PDDL_ALLOC_MEMORY(type, NULL, sizeof(type) * (num_elements))

/**
 * Reallocates array.
 */
#define PDDL_REALLOC_ARR(ptr, type, num_elements) \
    _PDDL_ALLOC_MEMORY(type, ptr, sizeof(type) * (num_elements))

/**
 * Allocate array of elements of given type initialized to zero.
 */
#define PDDL_CALLOC_ARR(type, num_elements) \
    _PDDL_CALLOC_MEMORY(type, (num_elements), sizeof(type))

/**
 * Allocated zeroized struct
 */
#define PDDL_ZALLOC(type) PDDL_CALLOC_ARR(type, 1)

/**
 * Raw memory allocation.
 */
#define PDDL_MALLOC(size) \
    _PDDL_ALLOC_MEMORY(void, NULL, (size))

/**
 * Raw memory allocation.
 */
#define PDDL_ZMALLOC(size) _PDDL_CALLOC_MEMORY(void, 1, (size))

/**
 * Raw realloc
 */
#define PDDL_REALLOC(ptr, size) \
    _PDDL_ALLOC_MEMORY(void, (ptr), (size))

/**
 * Wrapped strdup() for consistency in memory allocation.
 */
#define PDDL_STRDUP(str) \
    _PDDL_STRDUP(str)


#ifdef PDDL_MEMCHECK
void *pddlRealloc(void *ptr, size_t size,
                  const char *file, int line, const char *func);
void *pddlAllocAlign(size_t size, size_t alignment,
                     const char *file, int line, const char *func);
void *pddlCalloc(size_t nmemb, size_t size,
                 const char *file, int line, const char *func);
char *pddlStrdup(const char *str, const char *file, int line, const char *func);
void pddlFreeCheck(void *ptr, const char *file, int line, const char *func);

#else /* PDDL_MEMCHECK */
void *pddlRealloc(void *ptr, size_t size);
void *pddlAllocAlign(size_t size, size_t alignment);
void *pddlCalloc(size_t nmemb, size_t size);
char *pddlStrdup(const char *str);
#endif /* PDDL_MEMCHECK */

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_ALLOC_H__ */

