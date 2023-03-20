/***
 * Copyright (c)2011 Daniel Fiser <danfis@danfis.cz>
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

#include "pddl/err.h"
#include "pddl/sort.h"
#include "internal.h"

#define DEFAULT_SORT pddlQSort

/**** INSERT SORT LIST ****/
void pddlListInsertSort(pddl_list_t *list, pddl_sort_list_cmp cmp, void *data)
{
    pddl_list_t out;
    pddl_list_t *cur, *item;

    // empty list - no need to sort
    if (pddlListEmpty(list))
        return;

    // list with one item - no need to sort
    if (pddlListNext(list) == pddlListPrev(list))
        return;

    pddlListInit(&out);
    while (!pddlListEmpty(list)){
        // pick up next item from list
        cur = pddlListNext(list);
        pddlListDel(cur);

        // find the place where to put it
        item = pddlListPrev(&out);
        while (item != &out && cmp(cur, item, data) < 0){
            item = pddlListPrev(item);
        }

        // put it after the item
        pddlListPrepend(item, cur);
    }

    // and finally store sorted
    *list = out;
    list->next->prev = list;
    list->prev->next = list;
}

/**** INSERT SORT LIST END ****/

void pddlCountSort(void *base, size_t nmemb, size_t size, int from, int to,
                   pddl_sort_key get_key, void *arg)
{
    int range = to - from + 1;
    int cnt[range];
    unsigned char *cur, *end, *tmp;
    int i, key;

    ZEROIZE_ARR(cnt, range);
    for (cur = base, end = cur + (nmemb * size); cur != end; cur += size){
        key = get_key(cur, arg) - from;
        ++cnt[key];
    }

    for (i = 1; i < range; ++i)
        cnt[i] += cnt[i - 1];

    tmp = ALLOC_ARR(unsigned char, nmemb * size);
    for (cur = base, end = cur + (nmemb * size); cur != end; cur += size){
        key = get_key(cur, arg) - from;
        --cnt[key];
        memcpy(tmp + (cnt[key] * size), cur, size);
    }
    memcpy(base, tmp, nmemb * size);
    FREE(tmp);
}

_pddl_inline void sort2(void *e1, void *e2, size_t size,
                        pddl_sort_cmp cmp, void *arg)
{
    if (cmp(e2, e1, arg) < 0){
        unsigned char tmp[size];
        memcpy(tmp, e1, size);
        memcpy(e1, e2, size);
        memcpy(e2, tmp, size);
    }
}

_pddl_inline void sort3(void *e1, void *e2, void *e3, size_t size,
                        pddl_sort_cmp cmp, void *arg)
{
    sort2(e1, e2, size, cmp, arg);
    sort2(e2, e3, size, cmp, arg);
    sort2(e1, e2, size, cmp, arg);
}

void pddlInsertSort(void *base, size_t nmemb, size_t size,
                    pddl_sort_cmp cmp, void *arg)
{
    unsigned char *begin = base, *cur, *end;
    unsigned char *prev, *ins;
    unsigned char tmp[size];

    if (nmemb <= 1)
        return;
    if (nmemb == 2){
        sort2(begin, begin + size, size, cmp, arg);
        return;
    }
    if (nmemb == 3){
        sort3(begin, begin + size, begin + size + size, size, cmp, arg);
        return;
    }

    end = begin + (nmemb * size);
    for (cur = begin + size; cur != end; cur += size){
        prev = cur - size;

        // Test whether current value is misplaced
        if (cmp(cur, prev, arg) < 0){
            // Remember the current value
            memcpy(tmp, cur, size);

            // Find its position backwards
            for (ins = prev, prev -= size;
                    prev >= begin && cmp(tmp, prev, arg) < 0;
                    prev -= size, ins -= size);

            // Move all values from ins to the right
            memmove(ins + size, ins, cur - ins);

            // Finally insert remembered value
            memcpy(ins, tmp, size);
        }
    }
}

void pddlInsertSortInt(int *arr, size_t nmemb)
{
    int tmp, i, j;

    if (nmemb <= 1)
        return;

    for (i = 1; i < nmemb; ++i){
        if (arr[i] < arr[i - 1]){
            tmp = arr[i];

            for (j = i - 1; j >= 0 && arr[j] > tmp; --j)
                arr[j + 1] = arr[j];
            arr[j + 1] = tmp;
        }
    }
}

int pddlSort(void *base, size_t nmemb, size_t size,
             pddl_sort_cmp cmp, void *carg)
{
    if (nmemb <= 1)
        return 0;
    if (nmemb == 2){
        char *begin = base;
        sort2(begin, begin + size, size, cmp, carg);
        return 0;
    }
    if (nmemb == 3){
        char *begin = base;
        sort3(begin, begin + size, begin + size + size, size, cmp, carg);
        return 0;
    }

    int ret = DEFAULT_SORT(base, nmemb, size, cmp, carg);
#ifdef PDDL_DEBUG
    for (int i = 1; i < nmemb; ++i){
        char *ca = base;
        ASSERT(cmp(ca + (i - 1) * size, ca + i * size, carg) <= 0);
    }
#endif /* PDDL_DEBUG */

    return ret;
}

int pddlStableSort(void *base, size_t nmemb, size_t size,
                   pddl_sort_cmp cmp, void *carg)
{
    if (nmemb <= 1)
        return 0;
    if (nmemb == 2){
        char *begin = base;
        sort2(begin, begin + size, size, cmp, carg);
        return 0;
    }
    if (nmemb == 3){
        char *begin = base;
        sort3(begin, begin + size, begin + size + size, size, cmp, carg);
        return 0;
    }

    int ret = pddlTimSort(base, nmemb, size, cmp, carg);
#ifdef PDDL_DEBUG
    for (int i = 1; i < nmemb; ++i){
        char *ca = base;
        ASSERT(cmp(ca + (i - 1) * size, ca + i * size, carg) <= 0);
    }
#endif /* PDDL_DEBUG */

    return ret;
}

static int cmpIntKey(const void *a, const void *b, void *d)
{
    long offset = (long)d;
    int i1 = *(int *)(((char *)a) + offset);
    int i2 = *(int *)(((char *)b) + offset);

    if (i1 < i2)
        return -1;
    if (i1 > i2)
        return 1;
    return 0;
}

static int cmpLongKey(const void *a, const void *b, void *d)
{
    long offset = (long)d;
    long i1 = *(long *)(((char *)a) + offset);
    long i2 = *(long *)(((char *)b) + offset);

    if (i1 < i2)
        return -1;
    if (i1 > i2)
        return 1;
    return 0;
}

int pddlSortByIntKey(void *base, size_t nmemb, size_t size, size_t offset)
{
    return pddlSort(base, nmemb, size, cmpIntKey, (void *)offset);
}

int pddlSortByLongKey(void *base, size_t nmemb, size_t size, size_t offset)
{
    return pddlSort(base, nmemb, size, cmpLongKey, (void *)offset);
}
