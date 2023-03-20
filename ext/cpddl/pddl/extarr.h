#ifndef __PDDL_DATAARR_H__
#define __PDDL_DATAARR_H__

#include <pddl/segmarr.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Extendable Array
 * =================
 *
 * Extendable array is a wrapper around segmented array (see pddl_segmarr_t)
 * which provides counting of the elements stored in the array and their
 * automatic initialization through template data or initialization
 * function. The array is extendable but never changes the place of once
 * stored data, so the pointers to the array are always valid.
 *
 * See pddl_extarr_t.
 */

/**
 * Default minimal number of elements per segment.
 */
#define PDDL_EXTARR_MIN_ELS_PER_SEGMENT 32

/**
 * Default size of a segment expressed as a multiple of a pagesize.
 */
#define PDDL_EXTARR_PAGESIZE_MULTIPLE 8

/**
 * Callback for element initialization.
 * The function gets pointer to the element stored in the array, index of
 * that element and optional user data.
 */
typedef void (*pddl_extarr_el_init_fn)(void *el, int idx, const void *userdata);

struct _pddl_extarr_t {
    pddl_segmarr_t *arr; /*!< Underlying segmented array */
    size_t size;        /*!< Number of elements stored in the array. */

    /*!< Initialization structures */
    pddl_extarr_el_init_fn init_fn;
    void *init_data;
};
typedef struct _pddl_extarr_t pddl_extarr_t;

/**
 * Creates a new extendable array.
 * The size of one segment is determined automatically as multiple of
 * pagesize.
 *
 * If init_fn function callback is non-NULL, it is used for initialization
 * of each element before the element is first returned. In this case
 * init_data is used as last argument of init_fn function.
 * If init_fn is non-NULL, the init_data should point to a memory of el_size
 * size and the data it points at are copied to the internal storage and used
 * for the initialization of each element before it is first returned.
 */
pddl_extarr_t *pddlExtArrNew(size_t el_size,
                             pddl_extarr_el_init_fn init_fn,
                             const void *init_data);

/**
 * Same as pddlExtArrNew() but initial multiple of pagesize can be provided
 * as well as minimal number of elements per one segment.
 * For example if we want to have one segment of the array to have at least
 * 128 times pagesize bytes, we set init_pagesize_multiple to 128.
 */
pddl_extarr_t *pddlExtArrNew2(size_t el_size,
                              size_t init_pagesize_multiple,
                              size_t min_els_per_segment,
                              pddl_extarr_el_init_fn init_fn,
                              const void *init_data);

/**
 * Deletes extendable array
 */
void pddlExtArrDel(pddl_extarr_t *arr);

/**
 * Creates an exact copy of the extendable array.
 */
pddl_extarr_t *pddlExtArrClone(const pddl_extarr_t *arr);

/**
 * Returns pointer to the i'th element of the array.
 * If i is greater than the current size of array, the array is
 * automatically extended.
 */
_pddl_inline void *pddlExtArrGet(pddl_extarr_t *arr, size_t i);

/**
 * Returns number of elements stored in the array.
 */
_pddl_inline size_t pddlExtArrSize(const pddl_extarr_t *arr);

/**
 * Ensures that the array has at least i elements.
 */
void pddlExtArrResize(pddl_extarr_t *arr, size_t i);

/**** INLINES ****/
_pddl_inline void *pddlExtArrGet(pddl_extarr_t *arr, size_t i)
{
    if (i >= arr->size){
        pddlExtArrResize(arr, i);
    }

    return pddlSegmArrGet(arr->arr, i);
}

_pddl_inline size_t pddlExtArrSize(const pddl_extarr_t *arr)
{
    return arr->size;
}

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_DATAARR_H__ */
