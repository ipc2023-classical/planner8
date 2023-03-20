/***
 * Copyright (c)2022 Daniel Fiser <danfis@danfis.cz>
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

#include "internal.h"
#include "pddl/lp.h"
#include "_lp.h"

#ifdef PDDL_GLPK
# include <glpk.h>

struct _lp_t {
    pddl_lp_t cls;
    glp_prob *lp;
    int mip;
    int *ri;
    int *ci;
    double *coef;
    int mat_size;
    int mat_alloc;
};
typedef struct _lp_t lp_t;

#define LP(l) pddl_container_of((l), lp_t, cls)

static pddl_lp_t *new(const pddl_lp_config_t *cfg, pddl_err_t *err)
{
    lp_t *lp;

    lp = ZALLOC(lp_t);
    lp->cls.cls = &pddl_lp_glpk;
    lp->cls.err = err;
    lp->cls.cfg = *cfg;
    lp->mip = 0;
    lp->lp = glp_create_prob();
    if (cfg->maximize){
        glp_set_obj_dir(lp->lp, GLP_MAX);
    }else{
        glp_set_obj_dir(lp->lp, GLP_MIN);
    }

    if (cfg->cols > 0)
        glp_add_cols(lp->lp, cfg->cols);
    if (cfg->rows > 0)
        glp_add_rows(lp->lp, cfg->rows);

    if (cfg->num_threads > 1){
        LOG(err, "cfg.num_threads = %d is not supported by GLPK",
            cfg->num_threads);
    }

    return &lp->cls;
}

static void del(pddl_lp_t *_lp)
{
    lp_t *lp = LP(_lp);
    glp_delete_prob(lp->lp);
    FREE(lp);
}

static void setObj(pddl_lp_t *_lp, int i, double coef)
{
    lp_t *lp = LP(_lp);
    glp_set_obj_coef(lp->lp, i + 1, coef);
}

static void setVarRange(pddl_lp_t *_lp, int i, double lb, double ub)
{
    lp_t *lp = LP(_lp);
    if (lb <= -1E20 && ub >= 1E20){
        glp_set_col_bnds(lp->lp, i + 1, GLP_FR, 0, 0);
    }else if (lb <= -1E20){
        glp_set_col_bnds(lp->lp, i + 1, GLP_UP, 0, ub);
    }else if (ub >= 1E20){
        glp_set_col_bnds(lp->lp, i + 1, GLP_LO, lb, 0);
    }else{
        glp_set_col_bnds(lp->lp, i + 1, GLP_DB, lb, ub);
    }
}

static void setVarFree(pddl_lp_t *_lp, int i)
{
    lp_t *lp = LP(_lp);
    glp_set_col_bnds(lp->lp, i + 1, GLP_FR, 0, 0);
}

static void setVarInt(pddl_lp_t *_lp, int i)
{
    lp_t *lp = LP(_lp);
    glp_set_col_kind(lp->lp, i + 1, GLP_IV);
    lp->mip = 1;
}

static void setVarBinary(pddl_lp_t *_lp, int i)
{
    lp_t *lp = LP(_lp);
    glp_set_col_kind(lp->lp, i + 1, GLP_BV);
    lp->mip = 1;
}

static void setCoef(pddl_lp_t *_lp, int row, int col, double coef)
{
    lp_t *lp = LP(_lp);
    if (lp->mat_size == lp->mat_alloc){
        if (lp->mat_alloc == 0)
            lp->mat_alloc = 8;
        lp->mat_alloc *= 2;
        lp->ri = REALLOC_ARR(lp->ri, int, lp->mat_alloc + 1);
        lp->ci = REALLOC_ARR(lp->ci, int, lp->mat_alloc + 1);
        lp->coef = REALLOC_ARR(lp->coef, double, lp->mat_alloc + 1);
        if (lp->mat_size == 0){
            lp->ri[0] = lp->ci[0] = 0;
            lp->coef[0] = 0.;
        }
    }

    int id = ++lp->mat_size;
    lp->ri[id] = row + 1;
    lp->ci[id] = col + 1;
    lp->coef[id] = coef;
}

static void setRHS(pddl_lp_t *_lp, int row, double rhs, char sense)
{
    lp_t *lp = LP(_lp);
    if (sense == 'L'){
        glp_set_row_bnds(lp->lp, row + 1, GLP_UP, 0., rhs);
    }else if (sense == 'G'){
        glp_set_row_bnds(lp->lp, row + 1, GLP_LO, rhs, 0.);
    }else if (sense == 'E'){
        glp_set_row_bnds(lp->lp, row + 1, GLP_FX, rhs, rhs);
    }
}

static void addRows(pddl_lp_t *_lp, int cnt, const double *rhs, const char *sense)
{
    lp_t *lp = LP(_lp);
    int rowid = glp_add_rows(lp->lp, cnt);
    for (int i = 0; i < cnt; ++i){
        setRHS(_lp, rowid - 1, rhs[i], sense[i]);
        ++rowid;
    }
}

static void delRows(pddl_lp_t *_lp, int begin, int end)
{
    lp_t *lp = LP(_lp);
    int size = end - begin + 1;
    int ids[size + 1];
    ids[0] = 0;
    for (int i = begin, ins = 1; i <= end; ++i, ++ins)
        ids[ins] = i;
    glp_del_rows(lp->lp, size, ids);
}

static int numRows(const pddl_lp_t *_lp)
{
    lp_t *lp = LP(_lp);
    return glp_get_num_rows(lp->lp);
}

static void addCols(pddl_lp_t *_lp, int cnt)
{
    lp_t *lp = LP(_lp);
    glp_add_cols(lp->lp, cnt);
}

static void delCols(pddl_lp_t *_lp, int begin, int end)
{
    lp_t *lp = LP(_lp);
    int size = end - begin + 1;
    int ids[size + 1];
    ids[0] = 0;
    for (int i = begin, ins = 1; i <= end; ++i, ++ins)
        ids[ins] = i;
    glp_del_cols(lp->lp, size, ids);
}

static int numCols(const pddl_lp_t *_lp)
{
    lp_t *lp = LP(_lp);
    return glp_get_num_cols(lp->lp);
}

static int lpSolve(pddl_lp_t *_lp, double *val, double *obj)
{
    lp_t *lp = LP(_lp);


    glp_load_matrix(lp->lp, lp->mat_size, lp->ri, lp->ci, lp->coef);

    glp_smcp cfg;
    glp_init_smcp(&cfg);
    cfg.msg_lev = GLP_MSG_ERR;
    if (lp->cls.cfg.time_limit > 0.f)
        cfg.tm_lim = 1000 * lp->cls.cfg.time_limit;

    int ret = glp_simplex(lp->lp, &cfg);
    if (ret != 0)
        return -1;

    if (lp->mip){
        glp_iocp cfg;
        glp_init_iocp(&cfg);
        cfg.msg_lev = GLP_MSG_ERR;
        if (lp->cls.cfg.time_limit > 0.f)
            cfg.tm_lim = 1000 * lp->cls.cfg.time_limit;
        ret = glp_intopt(lp->lp, &cfg);
        if (ret == 0){
            if (val != NULL)
                *val = glp_mip_obj_val(lp->lp);
            if (obj != NULL){
                int size = numCols(_lp);
                for (int i = 0; i < size; ++i)
                    obj[i] = glp_mip_col_val(lp->lp, i + 1);
            }
        }

    }else{
        if (val != NULL)
            *val = glp_get_obj_val(lp->lp);
        if (obj != NULL){
            int size = numCols(_lp);
            for (int i = 0; i < size; ++i)
                obj[i] = glp_get_col_prim(lp->lp, i + 1);
        }
    }

    if (ret != 0)
        return -1;
    return 0;
}

static void lpWrite(pddl_lp_t *_lp, const char *fn)
{
    lp_t *lp = LP(_lp);
    glp_write_lp(lp->lp, NULL, fn);
}


#define TOSTR1(x) #x
#define TOSTR(x) TOSTR1(x)
pddl_lp_cls_t pddl_lp_glpk = {
    PDDL_LP_GLPK,
    "glpk",
    TOSTR(GLP_MAJOR_VERSION.GLP_MINOR_VERSION),
    new,
    del,
    setObj,
    setVarRange,
    setVarFree,
    setVarInt,
    setVarBinary,
    setCoef,
    setRHS,
    addRows,
    delRows,
    numRows,
    addCols,
    delCols,
    numCols,
    lpSolve,
    lpWrite,
};
#else /* PDDL_GLPK */
pddl_lp_cls_t pddl_lp_glpk = { 0 };
#endif /* PDDL_GLPK */
