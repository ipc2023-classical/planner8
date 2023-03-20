/***
 * Boruvka
 * --------
 * Copyright (c)2017 Daniel Fiser <danfis@danfis.cz>
 *
 *  This file is part of Boruvka.
 *
 *  Distributed under the OSI-approved BSD License (the "License");
 *  see accompanying file BDS-LICENSE for details or see
 *  <http://www.opensource.org/licenses/bsd-license.php>.
 *
 *  This software is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the License for more information.
 */

#include "pddl/set.h"
#include "internal.h"

void pddlSetInit(pddl_set_t *s)
{
    ZEROIZE(s);
}

void pddlSetFree(pddl_set_t *s)
{
    if (s->s)
        FREE(s->s);
}

int pddlSetHas(const pddl_set_t *s, TYPE v)
{
    // TODO: binary search
    for (int i = 0; i < s->size; ++i){
        if (s->s[i] == v)
            return 1;
    }
    return 0;
}

int pddlSetIsSubset(const pddl_set_t *s1, const pddl_set_t *s2)
{
    int i, j, size;

    if (s1->size > s2->size)
        return 0;

    size = s1->size;
    for (i = j = 0; i < size && j < s2->size;){
        if (s1->s[i] == s2->s[j]){
            ++i;
            ++j;
        }else if (s1->s[i] < s2->s[j]){
            return 0;
        }else{
            ++j;
        }
    }
    return i == size;
}

int pddlSetIntersectionSize(const pddl_set_t *s1, const pddl_set_t *s2)
{
    int i, j, size, setsize;

    setsize = 0;
    size = s1->size;
    for (i = j = 0; i < size && j < s2->size;){
        if (s1->s[i] == s2->s[j]){
            ++setsize;
            ++i;
            ++j;
        }else if (s1->s[i] < s2->s[j]){
            ++i;
        }else{
            ++j;
        }
    }
    return setsize;
}

int pddlSetIntersectionSizeAtLeast(const pddl_set_t *s1, const pddl_set_t *s2,
                                   int limit)
{
    int i, j, size, setsize;

    if (limit == 0)
        return 1;

    setsize = 0;
    size = s1->size;
    for (i = j = 0; i < size && j < s2->size;){
        if (s1->s[i] == s2->s[j]){
            if (++setsize == limit)
                return 1;
            ++i;
            ++j;
        }else if (s1->s[i] < s2->s[j]){
            ++i;
        }else{
            ++j;
        }
    }
    return 0;
}

int pddlSetIntersectionSizeAtLeast3(const pddl_set_t *s1,
                                    const pddl_set_t *s2,
                                    const pddl_set_t *s3,
                                    int limit)
{
    int i, j, k, setsize;

    if (limit == 0)
        return 1;

    setsize = 0;
    for (i = j = k = 0; i < s1->size && j < s2->size && k < s3->size;){
        if (s1->s[i] == s2->s[j] && s1->s[i] == s3->s[k]){
            if (++setsize == limit)
                return 1;
            ++i;
            ++j;
            ++k;
        }else if (s1->s[i] < s2->s[j] || s1->s[i] < s3->s[k]){
            ++i;
        }else if (s2->s[j] < s1->s[i] || s2->s[j] < s3->s[k]){
            ++j;
        }else if (s3->s[k] < s1->s[i] || s3->s[k] < s2->s[j]){
            ++k;
        }
    }
    return 0;
}

void pddlSetSet(pddl_set_t *d, const pddl_set_t *s)
{
    if (d->alloc < s->size){
        if (d->alloc == 0)
            d->alloc = 1;
        while (d->alloc < s->size)
            d->alloc *= 2;
        d->s = REALLOC_ARR(d->s, TYPE, d->alloc);
    }
    memcpy(d->s, s->s, sizeof(TYPE) * s->size);
    d->size = s->size;
}

void pddlSetAdd(pddl_set_t *s, TYPE v)
{
    if (s->size >= s->alloc){
        if (s->alloc == 0)
            s->alloc = 1;
        s->alloc *= 2;
        s->s = REALLOC_ARR(s->s, TYPE, s->alloc);
    }
    s->s[s->size++] = v;

    if (s->size > 1 && v <= s->s[s->size - 2]){
        TYPE *f = s->s + s->size - 1;
        for (; f > s->s && f[0] < f[-1]; --f){
            TYPE tmp = f[0];
            f[0] = f[-1];
            f[-1] = tmp;
        }
        if (f > s->s && f[0] == f[-1]){
            for (--s->size; f != s->s + s->size; ++f)
                *f = f[1];
        }
    }
}

int pddlSetRm(pddl_set_t *s, TYPE v)
{
    int i;

    for (i = 0; i < s->size && s->s[i] < v; ++i);
    if (i < s->size && s->s[i] == v){
        for (++i; i < s->size; ++i)
            s->s[i - 1] = s->s[i];
        --s->size;
        return 1;
    }
    return 0;
}

void pddlSetUnion(pddl_set_t *dst, const pddl_set_t *src)
{
    for (int i = 0; i < src->size; ++i)
        pddlSetAdd(dst, src->s[i]);
}

void pddlSetUnion2(pddl_set_t *dst, const pddl_set_t *s1, const pddl_set_t *s2)
{
    int i, j;

    pddlSetEmpty(dst);
    for (i = 0, j = 0; i < s1->size && j < s2->size;){
        if (s1->s[i] == s2->s[j]){
            pddlSetAdd(dst, s1->s[i]);
            ++i;
            ++j;
        }else if (s1->s[i] < s2->s[j]){
            pddlSetAdd(dst, s1->s[i]);
            ++i;
        }else{
            pddlSetAdd(dst, s2->s[j]);
            ++j;
        }
    }
    for (; i < s1->size; ++i)
        pddlSetAdd(dst, s1->s[i]);
    for (; j < s2->size; ++j)
        pddlSetAdd(dst, s2->s[j]);
}

void pddlSetIntersect(pddl_set_t *dst, const pddl_set_t *src)
{
    int w, i, j, size;

    size = dst->size;
    for (w = i = j = 0; i < size && j < src->size;){
        if (dst->s[i] == src->s[j]){
            dst->s[w++] = dst->s[i];
            ++i;
            ++j;
        }else if (dst->s[i] < src->s[j]){
            ++i;
        }else{
            ++j;
        }
    }
    dst->size = w;
}

void pddlSetIntersect2(pddl_set_t *dst, const pddl_set_t *s1, const pddl_set_t *s2)
{
    int i, j;

    pddlSetEmpty(dst);
    for (i = j = 0; i < s1->size && j < s2->size;){
        if (s1->s[i] == s2->s[j]){
            pddlSetAdd(dst, s1->s[i]);
            ++i;
            ++j;
        }else if (s1->s[i] < s2->s[j]){
            ++i;
        }else{
            ++j;
        }
    }
}

void pddlSetMinus(pddl_set_t *s1, const pddl_set_t *s2)
{
    int w, i, j;

    for (w = i = j = 0; i < s1->size && j < s2->size;){
        if (s1->s[i] == s2->s[j]){
            ++i;
            ++j;
        }else if (s1->s[i] < s2->s[j]){
            s1->s[w++] = s1->s[i++];
        }else{
            ++j;
        }
    }
    for (; i < s1->size; ++i, ++w)
        s1->s[w] = s1->s[i];
    s1->size = w;
}

void pddlSetMinus2(pddl_set_t *d, const pddl_set_t *s1, const pddl_set_t *s2)
{
    int i, j;

    pddlSetEmpty(d);
    for (i = j = 0; i < s1->size && j < s2->size;){
        if (s1->s[i] == s2->s[j]){
            ++i;
            ++j;
        }else if (s1->s[i] < s2->s[j]){
            pddlSetAdd(d, s1->s[i]);
            ++i;
        }else{
            ++j;
        }
    }
    for (; i < s1->size; ++i)
        pddlSetAdd(d, s1->s[i]);
}

void pddlSetRemap(pddl_set_t *dst, const TYPE *remap)
{
    PDDL_SET(tmp);
    pddlSetUnion(&tmp, dst);

    pddlSetEmpty(dst);
    int v;
    PDDL_SET_FOR_EACH(&tmp, v)
        pddlSetAdd(dst, remap[v]);
    pddlSetFree(&tmp);
}
