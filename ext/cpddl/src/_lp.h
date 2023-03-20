/***
 * cpddl
 * --------
 * Copyright (c)2017 Daniel Fiser <danfis@danfis.cz>
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

#ifndef __PDDL__LP_H__
#define __PDDL__LP_H__

#include "pddl/config.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_lp_cls {
    pddl_lp_solver_t solver_id;
    const char *solver_name;
    const char *solver_version;
    pddl_lp_t *(*new)(const pddl_lp_config_t *cfg, pddl_err_t *err);
    void (*del)(pddl_lp_t *);
    void (*set_obj)(pddl_lp_t *lp, int i, double coef);
    void (*set_var_range)(pddl_lp_t *lp, int i, double lb, double ub);
    void (*set_var_free)(pddl_lp_t *lp, int i);
    void (*set_var_int)(pddl_lp_t *lp, int i);
    void (*set_var_binary)(pddl_lp_t *lp, int i);
    void (*set_coef)(pddl_lp_t *lp, int row, int col, double coef);
    void (*set_rhs)(pddl_lp_t *lp, int row, double rhs, char sense);
    void (*add_rows)(pddl_lp_t *lp, int cnt, const double *rhs, const char *sense);
    void (*del_rows)(pddl_lp_t *lp, int begin, int end);
    int (*num_rows)(const pddl_lp_t *lp);
    void (*add_cols)(pddl_lp_t *lp, int cnt);
    void (*del_cols)(pddl_lp_t *lp, int begin, int end);
    int (*num_cols)(const pddl_lp_t *lp);
    int (*solve)(pddl_lp_t *lp, double *val, double *obj);
    void (*write)(pddl_lp_t *lp, const char *fn);
};
typedef struct pddl_lp_cls pddl_lp_cls_t;

struct pddl_lp {
    pddl_lp_cls_t *cls;
    pddl_err_t *err;
    pddl_lp_config_t cfg;
};

extern pddl_lp_cls_t *pddl_lp_default;
extern pddl_lp_cls_t pddl_lp_not_available;
extern pddl_lp_cls_t pddl_lp_cplex;
extern pddl_lp_cls_t pddl_lp_gurobi;
extern pddl_lp_cls_t pddl_lp_glpk;
extern pddl_lp_cls_t pddl_lp_highs;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PDDL__LP_H__ */
