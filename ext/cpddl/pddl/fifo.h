/***
 * cpddl
 * --------
 * Copyright (c)2014 Daniel Fiser <danfis@danfis.cz>
 *
 *  This file is part of cpddl.
 *
 *  Distributed under the OSI-approved BSD License (the "License");
 *  see accompanying file BDS-LICENSE for details or see
 *  <http://www.opensource.org/licenses/bsd-license.php>.
 *
 *  This software is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the License for more information.
 */

#ifndef __PDDL_FIFO_H__
#define __PDDL_FIFO_H__

#include <pddl/list.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * FIFO -- First In First Out Queue
 * =================================
 *
 */

/**
 * Default size of the buffer used for one segment.
 */
#define PDDL_FIFO_BUF_SIZE 512


/** vvvv */
struct _pddl_fifo_segm_t {
    struct _pddl_fifo_segm_t *next; /*!< Next segment */
};
typedef struct _pddl_fifo_segm_t pddl_fifo_segm_t;

struct _pddl_fifo_t {
    size_t segm_size;       /*!< Size of a single segment */
    size_t el_size;         /*!< Element size */
    pddl_fifo_segm_t *front; /*!< First segment */
    char *front_el;
    char *front_end;
    pddl_fifo_segm_t *back;  /*!< The last segment */
    char *back_el;
    char *back_end;
};
typedef struct _pddl_fifo_t pddl_fifo_t;
/** ^^^^ */


/**
 * Functions
 * ----------
 */

/**
 * Creates a new FIFO structure with default size of the segment buffer.
 */
pddl_fifo_t *pddlFifoNew(size_t el_size);

/**
 * Creates a FIFO structe with specified buffer size.
 */
pddl_fifo_t *pddlFifoNewSize(size_t el_size, size_t buf_size);

/**
 * Deletes a fifo.
 */
void pddlFifoDel(pddl_fifo_t *fifo);

/**
 * In-place initialization of fifo structure
 */
void pddlFifoInit(pddl_fifo_t *fifo, size_t el_size);

/**
 * In-place initialization with specified buffer size.
 */
void pddlFifoInitSize(pddl_fifo_t *fifo, size_t el_size, size_t buf_size);

/**
 * Frees allocated resources.
 */
void pddlFifoFree(pddl_fifo_t *fifo);

/**
 * Returns true if the FIFO is empty.
 */
_pddl_inline int pddlFifoEmpty(const pddl_fifo_t *fifo);

/**
 * Clears the FIFO queue. After this the FIFO will be empty.
 */
void pddlFifoClear(pddl_fifo_t *fifo);

/**
 * Pushes a next element data at the back of the FIFO, the provided data
 * must have size as specified in *New*() function.
 */
void pddlFifoPush(pddl_fifo_t *fifo, void *el_data);

/**
 * Removes an element from the front of FIFO.
 */
void pddlFifoPop(pddl_fifo_t *fifo);

/**
 * Returns pointer to the front element in FIFO.
 */
_pddl_inline void *pddlFifoFront(pddl_fifo_t *fifo);


/**** INLINES ****/
_pddl_inline int pddlFifoEmpty(const pddl_fifo_t *fifo)
{
    return fifo->front == NULL;
}

_pddl_inline void *pddlFifoFront(pddl_fifo_t *fifo)
{
    return fifo->front_el;
}

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_FIFO_H__ */
