/** Adapted from https://github.com/swenson/sort/ licensed under MIT license */
/* Copyright (c) 2010-2019 Christopher Swenson. */
/* Copyright (c) 2012 Vojtech Fried. */
/* Copyright (c) 2012 Google Inc. All Rights Reserved. */
/* Copyright (c)2022 Daniel Fiser <danfis@danfis.cz> */

#include "internal.h"
#include "pddl/sort.h"

#ifndef TIM_SORT_STACK_SIZE
#define TIM_SORT_STACK_SIZE 128
#endif

struct run {
    size_t start;
    size_t length;
};
typedef struct run run_t;

struct tmp_storage {
    size_t alloc;
    char *storage;
};
typedef struct tmp_storage tmp_storage_t;

/* Function used to do a binary search for binary insertion sort */
_pddl_inline size_t binaryInsertionFind(const char *a,
                                        const char *x,
                                        const size_t nmemb,
                                        const size_t size,
                                        pddl_sort_cmp cmp,
                                        void *carg)
{
    size_t l, c, r;
    const char *cx;
    l = 0;
    r = nmemb - 1;
    c = r >> 1;

    /* check for out of bounds at the beginning. */
    if (cmp(x, a, carg) < 0){
        return 0;

    }else if (cmp(x, a + (r * size), carg) > 0){
        return r;
    }

    cx = a + (c * size);
    while (1){
        if (cmp(x, cx, carg) < 0){
            if (c - l <= 1)
                return c;
            r = c;

        }else{ /* allow = for stability. The binary search favors the right. */
            if (r - c <= 1)
                return c + 1;
            l = c;
        }

        c = l + ((r - l) >> 1);
        cx = a + (c * size);
    }
}

/* Binary insertion sort, but knowing that the first "start" entries are sorted.  Used in timsort. */
static void binaryInsertionStart(char *a,
                                 const size_t start,
                                 const size_t nmemb,
                                 const size_t size,
                                 pddl_sort_cmp cmp,
                                 void *carg)
{
    char tmp[size];
    char *ca = a;
    ca += start * size;
    for (size_t i = start; i < nmemb; i++, ca += size){
        /* If this entry is already correct, just move along */
        if (cmp(ca - size, ca, carg) <= 0)
            continue;

        /* Else we need to find the right place, shift everything over, and squeeze in */
        memcpy(tmp, ca, size);
        size_t location = binaryInsertionFind(a, tmp, i, size, cmp, carg);

        char *mv = ca;
        for (size_t j = i; j > location; j--, mv -= size)
            memcpy(mv, mv - size, size);
        memcpy(mv, tmp, size);
    }
}

/* Binary insertion sort */
static void binaryInsertion(char *a,
                            const size_t nmemb,
                            const size_t size,
                            pddl_sort_cmp cmp,
                            void *carg)
{
    if (nmemb <= 1)
        return;

    binaryInsertionStart(a, 1, nmemb, size, cmp, carg);
}

#ifndef CLZ
/* clang-only */
#ifndef __has_builtin
#define __has_builtin(x) 0
#endif
#if __has_builtin(__builtin_clzll) || (defined(__GNUC__) && ((__GNUC__ == 3 && __GNUC_MINOR__ >= 4) || (__GNUC__ > 3)))
#define CLZ __builtin_clzll
#else

static int clzll(uint64_t);

/* adapted from Hacker's Delight */
static int clzll(uint64_t x){
    int n;

    if (x == 0){
        return 64;
    }

    n = 0;

    if (x <= 0x00000000FFFFFFFFL){
        n = n + 32;
        x = x << 32;
    }

    if (x <= 0x0000FFFFFFFFFFFFL){
        n = n + 16;
        x = x << 16;
    }

    if (x <= 0x00FFFFFFFFFFFFFFL){
        n = n + 8;
        x = x << 8;
    }

    if (x <= 0x0FFFFFFFFFFFFFFFL){
        n = n + 4;
        x = x << 4;
    }

    if (x <= 0x3FFFFFFFFFFFFFFFL){
        n = n + 2;
        x = x << 2;
    }

    if (x <= 0x7FFFFFFFFFFFFFFFL){
        n = n + 1;
    }

    return n;
}

#define CLZ clzll
#endif
#endif

_pddl_inline int compute_minrun(const uint64_t size)
{
    const int top_bit = 64 - CLZ(size);
    const int shift = PDDL_MAX(top_bit, 6) - 6;
    const int minrun = (int)(size >> shift);
    const uint64_t mask = (1ULL << shift) - 1;

    if (mask & size){
        return minrun + 1;
    }

    return minrun;
}

_pddl_inline void swap(char *a, char *b, const size_t size)
{
    for (size_t i = 0; i < size; ++i){
        char tmp;
        PDDL_SWAP(a[i], b[i], tmp);
    }
}

_pddl_inline void reverseElements(char *a,
                                  const size_t size,
                                  size_t start,
                                  size_t end)
{
    start *= size;
    end *= size;
    while (1){
        if (start >= end)
            return;

        swap(a + start, a + end, size);
        start += size;
        end -= size;
    }
}

static size_t countRun(char *a,
                       size_t nmemb,
                       size_t size,
                       size_t start,
                       pddl_sort_cmp cmp,
                       void *carg)
{
    size_t curr;

    if (nmemb - start == 1)
        return 1;

    if (start >= nmemb - 2){
        if (cmp(a + (nmemb - 2) * size, a + (nmemb - 1) * size, carg) > 0)
            swap(a + (nmemb - 2) * size, a + (nmemb - 1) * size, size);

        return 2;
    }

    curr = start + 2;

    if (cmp(a + start * size, a + (start + 1) * size, carg) <= 0){
        /* increasing run */
        while (1){
            if (curr == nmemb - 1)
                break;
            if (cmp(a + (curr - 1) * size, a + curr * size, carg) > 0)
                break;

            curr++;
        }

        return curr - start;

    }else{
        /* decreasing run */
        while (1){
            if (curr == nmemb - 1){
                break;
            }

            if (cmp(a + (curr - 1) * size, a + curr * size, carg) <= 0){
                break;
            }

            curr++;
        }

        /* reverse in-place */
        reverseElements(a, size, start, curr - 1);
        return curr - start;
    }
}

static int checkInvariant(run_t *stack, const int stack_curr)
{
    size_t A, B, C;

    if (stack_curr < 2){
        return 1;
    }

    if (stack_curr == 2){
        const size_t A1 = stack[stack_curr - 2].length;
        const size_t B1 = stack[stack_curr - 1].length;

        if (A1 <= B1){
            return 0;
        }

        return 1;
    }

    A = stack[stack_curr - 3].length;
    B = stack[stack_curr - 2].length;
    C = stack[stack_curr - 1].length;

    if ((A <= B + C) || (B <= C)){
        return 0;
    }

    return 1;
}

static void timSortResize(tmp_storage_t *store,
                          const size_t elsize,
                          const size_t new_size)
{
    if ((store->storage == NULL) || (store->alloc < new_size)){
        char *tempstore = REALLOC_ARR(store->storage, char, elsize * new_size);

        if (tempstore == NULL){
            fprintf(stderr, "Error allocating temporary storage for tim sort: need %lu bytes",
                    (unsigned long)(elsize * new_size));
            exit(1);
        }

        store->storage = tempstore;
        store->alloc = new_size;
    }
}

static void timSortMerge(char *a,
                         const size_t size,
                         const run_t *stack,
                         const int stack_curr,
                         tmp_storage_t *store,
                         pddl_sort_cmp cmp,
                         void *carg)
{
    const size_t A = stack[stack_curr - 2].length;
    const size_t B = stack[stack_curr - 1].length;
    const size_t curr = stack[stack_curr - 2].start;
    char *storage;
    size_t i, j, k;
    timSortResize(store, size, PDDL_MIN(A, B));
    storage = store->storage;

    /* left merge */
    if (A < B){
        memcpy(storage, a + (curr * size), A * size);
        i = 0;
        j = curr + A;

        for (k = curr; k < curr + A + B; k++){
            if ((i < A) && (j < curr + A + B)){
                if (cmp(storage + i * size, a + j * size, carg) <= 0){
                    memcpy(a + k * size, storage + i * size, size);
                    ++i;
                }else{
                    memcpy(a + k * size, a + j * size, size);
                    ++j;
                }
            }else if (i < A){
                memcpy(a + k * size, storage + i * size, size);
                ++i;
            }else{
                break;
            }
        }
    }else{
        /* right merge */
        memcpy(storage, a + (curr + A) * size, B * size);
        i = B;
        j = curr + A;
        k = curr + A + B;

        while (k-- > curr){
            if ((i > 0) && (j > curr)){
                if (cmp(a + (j - 1) * size, storage + (i - 1) * size, carg) > 0){
                    memcpy(a + k * size, a + (j - 1) * size, size);
                    --j;
                }else{
                    memcpy(a + k * size, storage + (i - 1) * size, size);
                    --i;
                }
            }else if (i > 0){
                memcpy(a + k * size, storage + (i - 1) * size, size);
                --i;
            }else{
                break;
            }
        }
    }
}

static int timSortCollapse(char *a,
                           const size_t nmemb,
                           const size_t size,
                           run_t *stack,
                           int stack_curr,
                           tmp_storage_t *store,
                           pddl_sort_cmp cmp,
                           void *carg)
{
    while (1){
        size_t A, B, C, D;
        int ABC, BCD, CD;

        /* if the stack only has one thing on it, we are done with the collapse */
        if (stack_curr <= 1){
            break;
        }

        /* if this is the last merge, just do it */
        if ((stack_curr == 2) && (stack[0].length + stack[1].length == nmemb)){
            timSortMerge(a, size, stack, stack_curr, store, cmp, carg);
            stack[0].length += stack[1].length;
            stack_curr--;
            break;
        }
        /* check if the invariant is off for a stack of 2 elements */
        else if ((stack_curr == 2) && (stack[0].length <= stack[1].length)){
            timSortMerge(a, size, stack, stack_curr, store, cmp, carg);
            stack[0].length += stack[1].length;
            stack_curr--;
            break;
        }else if (stack_curr == 2){
            break;
        }

        B = stack[stack_curr - 3].length;
        C = stack[stack_curr - 2].length;
        D = stack[stack_curr - 1].length;

        if (stack_curr >= 4){
            A = stack[stack_curr - 4].length;
            ABC = (A <= B + C);
        }else{
            ABC = 0;
        }

        BCD = (B <= C + D) || ABC;
        CD = (C <= D);

        /* Both invariants are good */
        if (!BCD && !CD){
            break;
        }

        /* left merge */
        if (BCD && !CD){
            timSortMerge(a, size, stack, stack_curr - 1, store, cmp, carg);
            stack[stack_curr - 3].length += stack[stack_curr - 2].length;
            stack[stack_curr - 2] = stack[stack_curr - 1];
            stack_curr--;
        }else{
            /* right merge */
            timSortMerge(a, size, stack, stack_curr, store, cmp, carg);
            stack[stack_curr - 2].length += stack[stack_curr - 1].length;
            stack_curr--;
        }
    }

    return stack_curr;
}

_pddl_inline int pushNext(char *a,
                          const size_t nmemb,
                          const size_t size,
                          tmp_storage_t *store,
                          const size_t minrun,
                          run_t *run_stack,
                          size_t *stack_curr,
                          size_t *curr,
                          pddl_sort_cmp cmp,
                          void *carg)
{
    size_t len = countRun(a, nmemb, size, *curr, cmp, carg);
    size_t run = minrun;

    if (run > nmemb - *curr){
        run = nmemb - *curr;
    }

    if (run > len){
        binaryInsertionStart(a + *curr * size, len, run, size, cmp, carg);
        len = run;
    }

    run_stack[*stack_curr].start = *curr;
    run_stack[*stack_curr].length = len;
    (*stack_curr)++;
    *curr += len;

    if (*curr == nmemb){
        /* finish up */
        while (*stack_curr > 1){
            timSortMerge(a, size, run_stack, (int)*stack_curr, store, cmp, carg);
            run_stack[*stack_curr - 2].length += run_stack[*stack_curr - 1].length;
            (*stack_curr)--;
        }

        if (store->storage != NULL){
            free(store->storage);
            store->storage = NULL;
        }

        return 0;
    }

    return 1;
}

int pddlTimSort(void *base,
                size_t nmemb,
                size_t size,
                pddl_sort_cmp cmp,
                void *carg)
{
    char *a = base;
    size_t minrun;
    tmp_storage_t _store, *store;
    run_t run_stack[TIM_SORT_STACK_SIZE];
    size_t stack_curr = 0;
    size_t curr = 0;

    /* don't bother sorting an array of nmemb 1 */
    if (nmemb <= 1)
        return 0;

    if (nmemb < 64){
        binaryInsertion(base, nmemb, size, cmp, carg);
        return 0;
    }

    /* compute the minimum run length */
    minrun = compute_minrun(nmemb);
    /* temporary storage for merges */
    store = &_store;
    store->alloc = 0;
    store->storage = NULL;

    if (!pushNext(a, nmemb, size, store, minrun, run_stack, &stack_curr,
                  &curr, cmp, carg)){
        return 0;
    }

    if (!pushNext(a, nmemb, size, store, minrun, run_stack, &stack_curr,
                  &curr, cmp, carg)){
        return 0;
    }

    if (!pushNext(a, nmemb, size, store, minrun, run_stack, &stack_curr,
                  &curr, cmp, carg)){
        return 0;
    }

    while (1){
        if (!checkInvariant(run_stack, (int)stack_curr)){
            stack_curr = timSortCollapse(a, nmemb, size, run_stack,
                                         (int)stack_curr, store,
                                         cmp, carg);
            continue;
        }

        if (!pushNext(a, nmemb, size, store, minrun, run_stack,
                      &stack_curr, &curr, cmp, carg)){
            return 0;
        }
    }

    return 0;
}
