/***
 * Boruvka
 * --------
 * Copyright (c)2014 Daniel Fiser <danfis@danfis.cz>
 *
 *  This file is part of Boruvka.
 *
 *  Distributed under the OSI-approved BSD License (the "License");
 *  see accompanying file BDS-LICENSE for details or see
 *  <http://www.opensource.org/licenses/bsd-license.splaytreep>.
 *
 *  This software is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the License for more information.
 */

#ifndef __PDDL_SPLAYTREE_DEF_H__
#define __PDDL_SPLAYTREE_DEF_H__

#include "pddl/list.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Splay Tree Definition
 * ======================
 *
 * This header should never be part of public API of any project that uses
 * pddluvka library. The purpose of this is to provided macro-driven
 * definition of splay-tree routines that can be modified to your own goal.
 *
 * How to use this file, see splaytree and splaytree_int modules.
 *
 * The code was adopted from the FreeBSD's sys/tree.h:
 *
 * Copyright 2002 Niels Provos <provos@citi.umich.edu>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PDDL_SPLAY_KEY_EQ
# define PDDL_SPLAY_KEY_EQ(head, key1, key2) \
(PDDL_SPLAY_KEY_CMP((head), (key1), (key2)) == 0)
#endif /* PDDL_SPLAY_KEY_EQ */

#define PDDL_SPLAY_LEFT(elm)   (elm)->spe_left
#define PDDL_SPLAY_RIGHT(elm)  (elm)->spe_right
#define PDDL_SPLAY_ROOT(head)  (head)->root
#define PDDL_SPLAY_EMPTY(head) (PDDL_SPLAY_ROOT(head) == NULL)

/* PDDL_SPLAY_ROTATE_{LEFT,RIGHT} expect that tmp hold PDDL_SPLAY_{RIGHT,LEFT} */
#define PDDL_SPLAY_ROTATE_RIGHT(head, tmp) do { \
    PDDL_SPLAY_LEFT((head)->root) = PDDL_SPLAY_RIGHT(tmp); \
    PDDL_SPLAY_RIGHT(tmp) = (head)->root; \
    (head)->root = tmp; \
} while (/*CONSTCOND*/ 0)

#define PDDL_SPLAY_ROTATE_LEFT(head, tmp) do { \
    PDDL_SPLAY_RIGHT((head)->root) = PDDL_SPLAY_LEFT(tmp); \
    PDDL_SPLAY_LEFT(tmp) = (head)->root; \
    (head)->root = tmp; \
} while (/*CONSTCOND*/ 0)

#define PDDL_SPLAY_LINKLEFT(head, tmp) do { \
    PDDL_SPLAY_LEFT(tmp) = (head)->root; \
    tmp = (head)->root; \
    (head)->root = PDDL_SPLAY_LEFT((head)->root); \
} while (/*CONSTCOND*/ 0)

#define PDDL_SPLAY_LINKRIGHT(head, tmp) do { \
    PDDL_SPLAY_RIGHT(tmp) = (head)->root; \
    tmp = (head)->root; \
    (head)->root = PDDL_SPLAY_RIGHT((head)->root); \
} while (/*CONSTCOND*/ 0)

#define PDDL_SPLAY_ASSEMBLE(head, node, left, right) do { \
    PDDL_SPLAY_RIGHT(left) = PDDL_SPLAY_LEFT((head)->root); \
    PDDL_SPLAY_LEFT(right) = PDDL_SPLAY_RIGHT((head)->root); \
    PDDL_SPLAY_LEFT((head)->root) = PDDL_SPLAY_RIGHT(node); \
    PDDL_SPLAY_RIGHT((head)->root) = PDDL_SPLAY_LEFT(node); \
} while (/*CONSTCOND*/ 0)



_pddl_inline void pddlSplayInit(PDDL_SPLAY_TREE_T *head)
{
    head->root = NULL;
}

_pddl_inline void pddlSplayFree(PDDL_SPLAY_TREE_T *head)
{
    head->root = NULL;
}

_pddl_inline void pddlSplay(PDDL_SPLAY_TREE_T *head,
                            PDDL_SPLAY_KEY_T key)
{
    PDDL_SPLAY_TREE_NODE_T __node, *__left, *__right, *__tmp;
    int __comp;

    PDDL_SPLAY_LEFT(&__node) = PDDL_SPLAY_RIGHT(&__node) = NULL;
    __left = __right = &__node;

    while ((__comp = PDDL_SPLAY_KEY_CMP(head, key, PDDL_SPLAY_NODE_KEY((head)->root))) != 0) {
        if (__comp < 0) {
            __tmp = PDDL_SPLAY_LEFT((head)->root);
            if (__tmp == NULL)
                break;
            if (PDDL_SPLAY_KEY_CMP(head, key, PDDL_SPLAY_NODE_KEY(__tmp)) < 0){
                PDDL_SPLAY_ROTATE_RIGHT(head, __tmp);
                if (PDDL_SPLAY_LEFT((head)->root) == NULL)
                    break;
            }
            PDDL_SPLAY_LINKLEFT(head, __right);
        } else if (__comp > 0) {
            __tmp = PDDL_SPLAY_RIGHT((head)->root);
            if (__tmp == NULL)
                break;
            if (PDDL_SPLAY_KEY_CMP(head, key, PDDL_SPLAY_NODE_KEY(__tmp)) > 0){
                PDDL_SPLAY_ROTATE_LEFT(head, __tmp);
                if (PDDL_SPLAY_RIGHT((head)->root) == NULL)
                    break;
            }
            PDDL_SPLAY_LINKRIGHT(head, __left);
        }
    }
    PDDL_SPLAY_ASSEMBLE(head, &__node, __left, __right);
}

_pddl_inline PDDL_SPLAY_TREE_NODE_T *pddlSplayInsert(PDDL_SPLAY_TREE_T *head,
                                                     PDDL_SPLAY_KEY_T key,
                                                     PDDL_SPLAY_TREE_NODE_T *elm)
{
    PDDL_SPLAY_NODE_SET_KEY(head, elm, key);

    if (PDDL_SPLAY_EMPTY(head)) {
        PDDL_SPLAY_LEFT(elm) = PDDL_SPLAY_RIGHT(elm) = NULL;
    } else {
        int __comp;
        pddlSplay(head, key);
        __comp = PDDL_SPLAY_KEY_CMP(head, key, PDDL_SPLAY_NODE_KEY(head->root));
        if(__comp < 0) {
            PDDL_SPLAY_LEFT(elm) = PDDL_SPLAY_LEFT((head)->root);
            PDDL_SPLAY_RIGHT(elm) = (head)->root;
            PDDL_SPLAY_LEFT((head)->root) = NULL;
        } else if (__comp > 0) {
            PDDL_SPLAY_RIGHT(elm) = PDDL_SPLAY_RIGHT((head)->root);
            PDDL_SPLAY_LEFT(elm) = (head)->root;
            PDDL_SPLAY_RIGHT((head)->root) = NULL;
        } else
            return ((head)->root);
    }
    (head)->root = (elm);
    return (NULL);
}

_pddl_inline PDDL_SPLAY_TREE_NODE_T *pddlSplayRemove(PDDL_SPLAY_TREE_T *head,
                                                     PDDL_SPLAY_TREE_NODE_T *elm)
{
    PDDL_SPLAY_TREE_NODE_T *__tmp;
    if (PDDL_SPLAY_EMPTY(head))
        return (NULL);
    pddlSplay(head, PDDL_SPLAY_NODE_KEY(elm));
    if (PDDL_SPLAY_KEY_EQ(head, PDDL_SPLAY_NODE_KEY(elm), PDDL_SPLAY_NODE_KEY((head)->root))) {
        if (PDDL_SPLAY_LEFT((head)->root) == NULL) {
            (head)->root = PDDL_SPLAY_RIGHT((head)->root);
        } else {
            __tmp = PDDL_SPLAY_RIGHT((head)->root);
            (head)->root = PDDL_SPLAY_LEFT((head)->root);
            pddlSplay(head, PDDL_SPLAY_NODE_KEY(elm));
            PDDL_SPLAY_RIGHT((head)->root) = __tmp;
        }
        return (elm);
    }
    return (NULL);
}

_pddl_inline PDDL_SPLAY_TREE_NODE_T *pddlSplayFind(PDDL_SPLAY_TREE_T *head,
                                                   PDDL_SPLAY_KEY_T key)
{
    if (PDDL_SPLAY_EMPTY(head))
        return(NULL);
    pddlSplay(head, key);
    if (PDDL_SPLAY_KEY_EQ(head, key, PDDL_SPLAY_NODE_KEY((head)->root)))
        return (head->root);
    return (NULL);
}

_pddl_inline PDDL_SPLAY_TREE_NODE_T *pddlSplayNext(PDDL_SPLAY_TREE_T *head,
                                                   PDDL_SPLAY_TREE_NODE_T *elm)
{
    pddlSplay(head, PDDL_SPLAY_NODE_KEY(elm));
    if (PDDL_SPLAY_RIGHT(elm) != NULL) {
        elm = PDDL_SPLAY_RIGHT(elm);
        while (PDDL_SPLAY_LEFT(elm) != NULL) {
            elm = PDDL_SPLAY_LEFT(elm);
        }
    } else
        elm = NULL;
    return (elm);
}

_pddl_inline PDDL_SPLAY_TREE_NODE_T *pddlSplayPrev(PDDL_SPLAY_TREE_T *head,
                                                   PDDL_SPLAY_TREE_NODE_T *elm)
{
    pddlSplay(head, PDDL_SPLAY_NODE_KEY(elm));
    if (PDDL_SPLAY_LEFT(elm) != NULL) {
        elm = PDDL_SPLAY_LEFT(elm);
        while (PDDL_SPLAY_RIGHT(elm) != NULL) {
            elm = PDDL_SPLAY_RIGHT(elm);
        }
    } else
        elm = NULL;
    return (elm);
}

_pddl_inline PDDL_SPLAY_TREE_NODE_T *pddlSplayMin(PDDL_SPLAY_TREE_T *head)
{
    PDDL_SPLAY_TREE_NODE_T __node, *__left, *__right, *__tmp;

    if (head->root == NULL)
        return NULL;

    PDDL_SPLAY_LEFT(&__node) = PDDL_SPLAY_RIGHT(&__node) = NULL;
    __left = __right = &__node;

    while (1) {
        __tmp = PDDL_SPLAY_LEFT((head)->root);
        if (__tmp == NULL)
            break;
        PDDL_SPLAY_ROTATE_RIGHT(head, __tmp);
        if (PDDL_SPLAY_LEFT((head)->root) == NULL)
            break;
        PDDL_SPLAY_LINKLEFT(head, __right);
    }
    PDDL_SPLAY_ASSEMBLE(head, &__node, __left, __right);
    return (PDDL_SPLAY_ROOT(head));
}

_pddl_inline PDDL_SPLAY_TREE_NODE_T *pddlSplayMax(PDDL_SPLAY_TREE_T *head)
{
    PDDL_SPLAY_TREE_NODE_T __node, *__left, *__right, *__tmp;

    if (head->root == NULL)
        return NULL;

    PDDL_SPLAY_LEFT(&__node) = PDDL_SPLAY_RIGHT(&__node) = NULL;
    __left = __right = &__node;

    while (1) {
        __tmp = PDDL_SPLAY_RIGHT((head)->root);
        if (__tmp == NULL)
            break;
        PDDL_SPLAY_ROTATE_LEFT(head, __tmp);
        if (PDDL_SPLAY_RIGHT((head)->root) == NULL)
            break;
        PDDL_SPLAY_LINKRIGHT(head, __left);
    }
    PDDL_SPLAY_ASSEMBLE(head, &__node, __left, __right);
    return (PDDL_SPLAY_ROOT(head));
}

#endif /* __PDDL_SPLAYTREE_DEF_H__ */
