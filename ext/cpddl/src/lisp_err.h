/***
 * cpddl
 * -------
 * Copyright (c)2018 Daniel Fiser <danfis@danfis.cz>,
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

#ifndef __PDDL_LISP_ERR_H__
#define __PDDL_LISP_ERR_H__

#include "pddl/err.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define ERR_LISP(E, N, format, ...) do { \
        PDDL_ERR((E), format " on line %d.", __VA_ARGS__, (N)->lineno); \
    } while (0)
#define ERR_LISP2(E, N, msg) do { \
        PDDL_ERR((E), msg " on line %d.", (N)->lineno); \
    } while (0)
#define ERR_LISP_RET(E, V, N, format, ...) do { \
        ERR_LISP((E), (N), format, __VA_ARGS__); \
        return (V); \
    } while (0)
#define ERR_LISP_RET2(E, V, N, msg) do { \
        ERR_LISP2((E), (N), msg); \
        return (V); \
    } while (0)


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PDDL_LISP_ERR_H__ */
