#include "pddl/extarr.h"
#include "internal.h"

pddl_extarr_t *_pddlExtArrNew(size_t el_size, size_t segment_size,
                              pddl_extarr_el_init_fn init_fn,
                              const void *init_data)
{
    pddl_extarr_t *arr;

    arr = ALLOC(pddl_extarr_t);
    arr->arr = pddlSegmArrNew(el_size, segment_size);
    if (arr->arr == NULL){
        fprintf(stderr, "Error: Cannot create a segmented array with elemen"
                " size of %d bytes and segment size of %d bytes.\n",
                (int)el_size, (int)segment_size);
        FREE(arr);
        exit(-1);
    }

    arr->size = 0;
    arr->init_fn = NULL;
    arr->init_data = NULL;

    if (init_fn){
        arr->init_fn   = init_fn;
        arr->init_data = (void *)init_data;
    }else if (init_data){
        arr->init_data = ALLOC_ARR(char, el_size);
        memcpy(arr->init_data, init_data, el_size);
    }

    return arr;
}

pddl_extarr_t *pddlExtArrNew2(size_t el_size,
                              size_t init_pagesize_multiple,
                              size_t min_els_per_segment,
                              pddl_extarr_el_init_fn init_fn,
                              const void *init_data)
{
    size_t segment_size;

    // compute best segment size
    segment_size = sysconf(_SC_PAGESIZE);
    segment_size *= init_pagesize_multiple;
    while (segment_size < min_els_per_segment * el_size)
        segment_size *= 2;

    return _pddlExtArrNew(el_size, segment_size, init_fn, init_data);
}

pddl_extarr_t *pddlExtArrNew(size_t el_size,
                             pddl_extarr_el_init_fn init_fn,
                             const void *init_data)
{
    return pddlExtArrNew2(el_size, PDDL_EXTARR_PAGESIZE_MULTIPLE,
                          PDDL_EXTARR_MIN_ELS_PER_SEGMENT,
                          init_fn, init_data);
}

void pddlExtArrDel(pddl_extarr_t *arr)
{
    if (arr->arr)
        pddlSegmArrDel(arr->arr);
    if (!arr->init_fn && arr->init_data)
        FREE(arr->init_data);
    FREE(arr);
}

pddl_extarr_t *pddlExtArrClone(const pddl_extarr_t *src)
{
    pddl_extarr_t *arr;
    size_t elsize, segmsize;
    void *data;
    size_t i;

    elsize = src->arr->el_size;
    segmsize = src->arr->segm_size;

    arr = _pddlExtArrNew(elsize, segmsize, src->init_fn, src->init_data);
    arr->size = src->size;
    for (i = 0; i < src->size; ++i){
        data = pddlSegmArrGet(arr->arr, i);
        memcpy(data, pddlSegmArrGet(src->arr, i), elsize);
    }

    return arr;
}

void pddlExtArrResize(pddl_extarr_t *arr, size_t eli)
{
    size_t i;
    void *data;

    if (!arr->init_fn && !arr->init_data){
        arr->size = eli + 1;
        return;
    }

    for (i = arr->size; i < eli + 1; ++i){
        data = pddlSegmArrGet(arr->arr, i);
        if (arr->init_fn){
            arr->init_fn(data, i, arr->init_data);
        }else{
            memcpy(data, arr->init_data, arr->arr->el_size);
        }
    }

    arr->size = eli + 1;
}
