/***
 * cpddl
 * -------
 * Copyright (c)2019 Daniel Fiser <danfis@danfis.cz>,
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

#include "pddl/set.h"

void pddlISetPrintCompressed(const pddl_iset_t *set, FILE *fout)
{
    for (int i = 0; i < pddlISetSize(set); ++i){
        if (i != 0)
            fprintf(fout, " ");
        if (i == pddlISetSize(set) - 1){
            fprintf(fout, "%d", pddlISetGet(set, i));
        }else{
            int j;
            for (j = i + 1; j < pddlISetSize(set)
                    && pddlISetGet(set, j) == pddlISetGet(set, j - 1) + 1;
                    ++j);
            if (j - 1 == i){
                fprintf(fout, "%d", pddlISetGet(set, i));
            }else{
                fprintf(fout, "%d-%d",
                        pddlISetGet(set, i), pddlISetGet(set, j - 1));
            }
            i = j - 1;
        }
    }
}

void pddlISetPrint(const pddl_iset_t *set, FILE *fout)
{
    int not_first = 0;
    int v;
    PDDL_ISET_FOR_EACH(set, v){
        if (not_first)
            fprintf(fout, " ");
        fprintf(fout, "%d", v);
        not_first = 1;
    }
}

void pddlISetPrintln(const pddl_iset_t *set, FILE *fout)
{
    pddlISetPrint(set, fout);
    fprintf(fout, "\n");
}
