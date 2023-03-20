#include "pddl/segmarr.h"
#include "internal.h"

pddl_segmarr_t *pddlSegmArrNew(size_t el_size, size_t segm_size)
{
    pddl_segmarr_t *arr;

    if (el_size > segm_size)
        return NULL;

    arr = ALLOC(pddl_segmarr_t);
    arr->el_size      = el_size;
    arr->segm_size    = segm_size;
    arr->els_per_segm = segm_size / el_size;
    arr->segm         = NULL;
    arr->num_segm     = 0;
    arr->alloc_segm   = 0;

    return arr;
}

void pddlSegmArrDel(pddl_segmarr_t *arr)
{
    size_t i;

    if (arr->segm){
        for (i = 0; i < arr->num_segm; ++i)
            FREE(arr->segm[i]);
        FREE(arr->segm);
    }
    FREE(arr);
}

void pddlSegmArrExpandSegments(pddl_segmarr_t *arr, size_t num_segs)
{
    size_t i;

    if (num_segs <= arr->num_segm)
        return;

    if (arr->alloc_segm < num_segs){
        arr->alloc_segm *= 2;
        if (arr->alloc_segm < num_segs)
            arr->alloc_segm = num_segs;

        arr->segm = REALLOC_ARR(arr->segm, char *, arr->alloc_segm);

        // zeroize yet not needed segment slots
        for (i = num_segs; i < arr->alloc_segm; ++i){
            arr->segm[i] = NULL;
        }
    }

    // allocate all needed segments
    for (; arr->num_segm < num_segs; ++arr->num_segm){
        arr->segm[arr->num_segm] = ALLOC_ARR(char, arr->segm_size);
    }
}
