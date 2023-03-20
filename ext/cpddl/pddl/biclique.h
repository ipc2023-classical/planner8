/***
 * cpddl
 * -------
 * Copyright (c)2020 Daniel Fiser <danfis@danfis.cz>,
 * AI Center, Department of Computer Science,
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

#ifndef __PDDL_BICLIQUE_H__
#define __PDDL_BICLIQUE_H__

#include <pddl/graph.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Inference of maximal bicliques (i.e., maximal non-induced complete
 * bipartite subgraphs).
 * Implementation of MICA algorithm from
 * Alexe et al. (2001). Consensus algorithms for the generation of all
 * maximal bicliques. https://doi.org/10.1016/j.dam.2003.09.004
 */
void pddlBicliqueFindMaximal(const pddl_graph_simple_t *g,
                             void (*cb)(const pddl_iset_t *left,
                                        const pddl_iset_t *right, void *ud),
                             void *ud);


#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_BICLIQUE_H__ */
