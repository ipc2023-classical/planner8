/***
 * cpddl
 * -------
 * Copyright (c)2021 Daniel Fiser <danfis@danfis.cz>,
 * Saarland University, and
 * Czech Technical University in Prague.
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

#include "pddl/sql_grounder.h"
#include "pddl/ground_atom.h"
#include "pddl/strips_maker.h"
#include "internal.h"
#include "sqlite3.h"

#define QUERY_SIZE 4096
#define QUERY_SELECT_SIZE (5 * QUERY_SIZE)

struct sql_pred {
    int pred;
    int arity;
    int is_static;
    char *table_name;
    pddl_sqlite3_stmt *stmt_atom;
    pddl_sqlite3_stmt *stmt_insert;
    pddl_sqlite3_stmt *stmt_clear;
};
typedef struct sql_pred sql_pred_t;

struct sql_action {
    int param_size;
    pddl_sqlite3_stmt *stmt;
    int applied0;
};
typedef struct sql_action sql_action_t;

struct pddl_sql_grounder {
    const pddl_t *pddl;
    pddl_prep_actions_t prep_action;
    pddl_sqlite3 *db;
    sql_pred_t *pred;
    sql_action_t *action;

    int it_action_id;
};

#define CHECK_SQL_ERR(db, code) \
    do { \
    if ((code) != SQLITE_OK){ \
        PANIC("Sqlite Error: %s: %s\n", \
                  pddl_sqlite3_errstr(code), pddl_sqlite3_errmsg(db)); \
    } \
    } while (0)


static void createTypeTable(pddl_sqlite3 *db, const pddl_t *pddl, int type)
{
    char query[QUERY_SIZE];
    sprintf(query, "CREATE TABLE type_%d (t int);", type);
    int ret = pddl_sqlite3_exec(db, query, NULL, NULL, NULL);
    CHECK_SQL_ERR(db, ret);

    int obj_size;
    const pddl_obj_id_t *objs;
    objs = pddlTypesObjsByType(&pddl->type, type, &obj_size);
    for (int i = 0; i < obj_size; ++i){
        sprintf(query, "INSERT INTO type_%d values(%d);", type, objs[i]);
        int ret = pddl_sqlite3_exec(db, query, NULL, NULL, NULL);
        CHECK_SQL_ERR(db, ret);
    }
}

static void createTypeTables(pddl_sqlite3 *db, const pddl_t *pddl)
{
    for (int type = 0; type < pddl->type.type_size; ++type)
        createTypeTable(db, pddl, type);
}

static void createPredTable(pddl_sqlite3 *db,
                            const char *table_name,
                            int param_size,
                            pddl_err_t *err)
{
    char query[QUERY_SIZE];
    int shift = sprintf(query, "CREATE TABLE %s (", table_name);
    for (int i = 0; i < param_size; ++i){
        if (i != 0)
            shift += sprintf(query + shift, ",");
        shift += sprintf(query + shift, "x%d int", i);
    }
    /* TODO: Disabled indexes
    shift += sprintf(query + shift, ", UNIQUE(");
    for (int i = 0; i < param_size; ++i){
        if (i != 0)
            shift += sprintf(query + shift, ",");
        shift += sprintf(query + shift, "x%d", i);
    }
    shift += sprintf(query + shift, ")");
    */
    shift += sprintf(query + shift, ");");
    ASSERT_RUNTIME(shift < QUERY_SIZE);

    //PDDL_INFO(err, "Predicate table: %s", query);
    int ret = pddl_sqlite3_exec(db, query, NULL, NULL, NULL);
    CHECK_SQL_ERR(db, ret);

    /* TODO: Disabled indexes
    for (int i = 0; i < param_size; ++i){
        sprintf(query, "CREATE INDEX index_%s_%d ON %s (x%d);",
                table_name, i, table_name, i);
        int ret = pddl_sqlite3_exec(db, query, NULL, NULL, NULL);
        CHECK_SQL_ERR(db, ret);
    }
    */
}

static void sqlPredInit(sql_pred_t *qpred,
                        pddl_sqlite3 *db,
                        const pddl_preds_t *preds,
                        int pred_id,
                        pddl_err_t *err)
{
    const pddl_pred_t *pred = preds->pred + pred_id;
    ZEROIZE(qpred);
    qpred->pred = pred->id;
    qpred->arity = pred->param_size;
    qpred->is_static = pddlPredIsStatic(&preds->pred[pred_id]);

    if (pred_id == preds->eq_pred){
        qpred->is_static = 1;
        return;
    }

    qpred->table_name = ALLOC_ARR(char, 2 + strlen(pred->name) + 1);
    sprintf(qpred->table_name, "t_%s", pred->name);
    int len = strlen(qpred->table_name);
    for (int i = 0; i < len; ++i){
        if (qpred->table_name[i] == '-'
                || qpred->table_name[i] == '='){
            qpred->table_name[i] = '_';
        }
    }

    if (pred->param_size > 0){
        createPredTable(db, qpred->table_name, pred->param_size, err);
    }else{
        createPredTable(db, qpred->table_name, 1, err);
    }

    char query[QUERY_SELECT_SIZE];
    int shift = 0;
    shift += sprintf(query + shift, "SELECT ");
    for (int i = 0; i < qpred->arity; ++i){
        if (i != 0)
            shift += sprintf(query + shift, ",");
        shift += sprintf(query + shift, " x%d", i);
    }
    if (qpred->arity == 0)
        shift += sprintf(query + shift, " x0");
    shift += sprintf(query + shift, " FROM %s", qpred->table_name);
    shift += sprintf(query + shift, " WHERE");
    for (int i = 0; i < qpred->arity; ++i){
        if (i != 0)
            shift += sprintf(query + shift, " AND");
        shift += sprintf(query + shift, " x%d = ?", i);
    }
    if (qpred->arity == 0)
        shift += sprintf(query + shift, " x0 = 1");
    shift += sprintf(query + shift, ";");

    int ret = pddl_sqlite3_prepare_v2(db, query, -1, &qpred->stmt_atom, NULL);
    CHECK_SQL_ERR(db, ret);

    shift = sprintf(query, "INSERT INTO %s values(", qpred->table_name);
    for (int i = 0; i < qpred->arity; ++i){
        if (i != 0)
            shift += sprintf(query + shift, ",");
        shift += sprintf(query + shift, "?");
    }
    if (qpred->arity == 0)
        shift += sprintf(query + shift, "1");
    sprintf(query + shift, ");");
    ASSERT_RUNTIME(shift < QUERY_SIZE);

    //PDDL_INFO(err, "Insert atom query: %s", query);
    ret = pddl_sqlite3_prepare_v2(db, query, -1, &qpred->stmt_insert, NULL);
    CHECK_SQL_ERR(db, ret);

    shift = sprintf(query, "DELETE FROM %s;", qpred->table_name);
    ASSERT_RUNTIME(shift < QUERY_SIZE);
    ret = pddl_sqlite3_prepare_v2(db, query, -1, &qpred->stmt_clear, NULL);
    CHECK_SQL_ERR(db, ret);
}

static void sqlPredFree(sql_pred_t *qpred, pddl_sqlite3 *db)
{
    if (qpred->table_name != NULL)
        FREE(qpred->table_name);
    if (qpred->stmt_atom != NULL)
        pddl_sqlite3_finalize(qpred->stmt_atom);
    if (qpred->stmt_insert != NULL)
        pddl_sqlite3_finalize(qpred->stmt_insert);
    if (qpred->stmt_clear != NULL)
        pddl_sqlite3_finalize(qpred->stmt_clear);
}

static int sqlPredHasAtomArg(sql_pred_t *qpred,
                             pddl_sqlite3 *db,
                             const pddl_obj_id_t *arg)
{
    ASSERT(qpred->stmt_atom != NULL);
    pddl_sqlite3_reset(qpred->stmt_atom);
    for (int i = 0; i < qpred->arity; ++i){
        int ret = pddl_sqlite3_bind_int(qpred->stmt_atom, i + 1, arg[i]);
        CHECK_SQL_ERR(db, ret);
    }
    int ret;
    int found = (ret = pddl_sqlite3_step(qpred->stmt_atom)) == SQLITE_ROW;
    if (ret != SQLITE_ROW && ret != SQLITE_DONE)
        CHECK_SQL_ERR(db, ret);
    return found;
}

static int sqlPredHasAtom(sql_pred_t *qpred,
                          pddl_sqlite3 *db,
                          const pddl_fm_atom_t *atom)
{
    pddl_obj_id_t arg[qpred->arity];
    for (int i = 0; i < qpred->arity; ++i){
        ASSERT(atom->arg[i].obj >= 0);
        arg[i] = atom->arg[i].obj;
    }
    return sqlPredHasAtomArg(qpred, db, arg);
}

static int sqlPredInsertAtomArg(sql_pred_t *qpred,
                                pddl_sqlite3 *db,
                                const pddl_obj_id_t *arg,
                                pddl_err_t *err)
{
    pddl_sqlite3_reset(qpred->stmt_insert);
    for (int i = 0; i < qpred->arity; ++i){
        ASSERT(arg[i] >= 0);
        int ret = pddl_sqlite3_bind_int(qpred->stmt_insert, i + 1, arg[i]);
        CHECK_SQL_ERR(db, ret);
    }
    int ret = pddl_sqlite3_step(qpred->stmt_insert);
    if (ret != SQLITE_DONE && ret != SQLITE_CONSTRAINT)
        CHECK_SQL_ERR(db, ret);
    return ret == SQLITE_DONE;
}

static int sqlPredClear(sql_pred_t *qpred, pddl_sqlite3 *db, pddl_err_t *err)
{
    pddl_sqlite3_reset(qpred->stmt_clear);
    int ret = pddl_sqlite3_step(qpred->stmt_clear);
    if (ret != SQLITE_DONE && ret != SQLITE_CONSTRAINT)
        CHECK_SQL_ERR(db, ret);
    return ret == SQLITE_DONE;
}



static void sqlActionConstructColumns(char *query,
                                      const pddl_prep_action_t *prep_action,
                                      pddl_iset_t *type_tables)
{
    query[0] = 0x0;
    int shift = 0;
    for (int pi = 0; pi < prep_action->param_size; ++pi){
        if (pi != 0)
            shift += sprintf(query + shift, ", ");

        int found = 0;
        for (int ci = 0; ci < prep_action->pre.size; ++ci){
            const pddl_fm_t *c = prep_action->pre.fm[ci];
            const pddl_fm_atom_t *atom = PDDL_FM_CAST(c, atom);
            for (int ai = 0; ai < atom->arg_size; ++ai){
                if (atom->arg[ai].param >= 0 && atom->arg[ai].param == pi){
                    shift += sprintf(query + shift, "tb%d.x%d as arg%d",
                                     ci, ai, pi);
                    found = 1;
                    break;
                }
            }
            if (found)
                break;
        }
        if (!found){
            int type = prep_action->param_type[pi];
            if (pddlTypeNumObjs(prep_action->type, type) > 0){
                shift += sprintf(query + shift, "tb_type%d.t as arg%d", pi, pi);
                pddlISetAdd(type_tables, pi);
            }else{
                shift += sprintf(query + shift, "-1");
            }
        }
    }
    ASSERT_RUNTIME(shift < QUERY_SIZE);
}

static void sqlActionConstructTables(char *query,
                                     const sql_pred_t *preds,
                                     const pddl_prep_action_t *prep_action,
                                     const pddl_iset_t *type_tables)
{
    query[0] = 0x0;
    int shift = 0;
    for (int ci = 0; ci < prep_action->pre.size; ++ci){
        const pddl_fm_t *c = prep_action->pre.fm[ci];
        const pddl_fm_atom_t *atom = PDDL_FM_CAST(c, atom);
        if (ci != 0)
            shift += sprintf(query + shift, ", ");
        shift += sprintf(query + shift, "%s as tb%d",
                         preds[atom->pred].table_name, ci);
    }
    int idx;
    PDDL_ISET_FOR_EACH(type_tables, idx){
        if (shift != 0)
            shift += sprintf(query + shift, ", ");
        int type = prep_action->param_type[idx];
        shift += sprintf(query + shift, "type_%d as tb_type%d", type, idx);
    }
    ASSERT_RUNTIME(shift < QUERY_SIZE);
}

static void sqlActionConstructJoinCond(char *query,
                                       const sql_pred_t *preds,
                                       const pddl_prep_action_t *prep_action)
{
    query[0] = 0x0;
    int ins = 0;
    int shift = 0;
    for (int ci1 = 0; ci1 < prep_action->pre.size; ++ci1){
        const pddl_fm_t *c1 = prep_action->pre.fm[ci1];
        const pddl_fm_atom_t *atom1 = PDDL_FM_CAST(c1, atom);
        for (int ci2 = ci1 + 1; ci2 < prep_action->pre.size; ++ci2){
            const pddl_fm_t *c2 = prep_action->pre.fm[ci2];
            const pddl_fm_atom_t *atom2 = PDDL_FM_CAST(c2, atom);

            for (int a1 = 0; a1 < atom1->arg_size; ++a1){
                if (atom1->arg[a1].param < 0)
                    continue;
                for (int a2 = 0; a2 < atom2->arg_size; ++a2){
                    if (atom2->arg[a2].param == atom1->arg[a1].param){
                        if (ins != 0){
                            shift += sprintf(query + shift, " AND ");
                        }else{
                            shift += sprintf(query + shift, "ON(");
                        }
                        shift += sprintf(query + shift, "tb%d.x%d = tb%d.x%d",
                                         ci1, a1, ci2, a2);
                        ++ins;
                    }
                }
            }
        }
    }
    if (query[0] != 0x0)
        shift += sprintf(query + shift, ")");
    ASSERT_RUNTIME(shift < QUERY_SIZE);
}

static int objsConsecutive(const pddl_obj_id_t *objs, int obj_size)
{
    for (int i = 1; i < obj_size; ++i){
        if (objs[i - 1] + 1 != objs[i])
            return 0;
    }
    return 1;
}

static int addEqCond(char *query,
                     int shift,
                     const pddl_fm_atom_t *atom,
                     const char *cmp,
                     const char *prefix)
{
    if (atom->arg[0].param >= 0 && atom->arg[1].param >= 0){
        shift += sprintf(query + shift, "%s arg%d %s arg%d",
                         prefix, atom->arg[0].param, cmp, atom->arg[1].param);

    }else if (atom->arg[0].param >= 0){
        shift += sprintf(query + shift, "%s arg%d %s %d",
                         prefix, atom->arg[0].param, cmp, atom->arg[1].obj);

    }else if (atom->arg[1].param >= 0){
        shift += sprintf(query + shift, "%s arg%d %s %d",
                         prefix, atom->arg[1].param, cmp, atom->arg[0].obj);

    }else{
        shift += sprintf(query + shift, "%s %d %s %d",
                         prefix, atom->arg[1].obj, cmp, atom->arg[0].obj);
    }
    return shift;
}

static void sqlActionConstructWhereCond(char *query,
                                        const sql_pred_t *preds,
                                        const pddl_prep_action_t *prep_action)
{
    static const char *prefix[2] = {" WHERE", " AND"};
    static const char *cmp[2] = {"=", "!="};
    query[0] = 0x0;
    int ins = 0;
    int shift = 0;
    for (int ci = 0; ci < prep_action->pre_eq.size; ++ci){
        const pddl_fm_t *c = prep_action->pre_eq.fm[ci];
        const pddl_fm_atom_t *atom = PDDL_FM_CAST(c, atom);
        shift = addEqCond(query, shift, atom,
                          cmp[(atom->neg ? 1 : 0)],
                          prefix[(ins == 0 ? 0 : 1)]);
        ++ins;
    }

    int used_param[prep_action->param_size];
    ZEROIZE_ARR(used_param, prep_action->param_size);
    for (int ci = 0; ci < prep_action->pre.size; ++ci){
        const pddl_fm_t *c = prep_action->pre.fm[ci];
        const pddl_fm_atom_t *atom = PDDL_FM_CAST(c, atom);
        for (int ai = 0; ai < atom->arg_size; ++ai){
            if (atom->arg[ai].param >= 0)
                used_param[atom->arg[ai].param] = 1;
        }

        if (atom->arg_size == 0){
            if (ins != 0){
                shift += sprintf(query + shift, " AND ");
            }else{
                shift += sprintf(query + shift, "WHERE ");
            }
            shift += sprintf(query + shift, "tb%d.x0 = 1", ci);
            ++ins;
        }else{
            for (int ai = 0; ai < atom->arg_size; ++ai){
                if (atom->arg[ai].obj >= 0){
                    if (ins != 0){
                        shift += sprintf(query + shift, " AND ");
                    }else{
                        shift += sprintf(query + shift, "WHERE ");
                    }
                    shift += sprintf(query + shift, "tb%d.x%d = %d",
                                     ci, ai, atom->arg[ai].obj);
                    ++ins;
                }
            }

        }
    }

    for (int pi = 0; pi < prep_action->param_size; ++pi){
        if (!used_param[pi])
            continue;

        int type = prep_action->param_type[pi];
        int obj_size;
        const pddl_obj_id_t *objs;
        objs = pddlTypesObjsByType(prep_action->type, type, &obj_size);
        if (ins != 0){
            shift += sprintf(query + shift, " AND ");
        }else{
            shift += sprintf(query + shift, "WHERE ");
        }
        if (obj_size == 0){
            // This action is not groundable, so add some dummy condition
            //shift += sprintf(query + shift, "arg%d = -10000", pi);
            shift += sprintf(query + shift, "1 = 2");
        }else if (obj_size == 1){
            shift += sprintf(query + shift, "arg%d = %d", pi, objs[0]);
        }else if (objsConsecutive(objs, obj_size)){
            shift += sprintf(query + shift, "arg%d >= %d AND arg%d <= %d",
                             pi, objs[0], pi, objs[obj_size - 1]);
        }else{
            shift += sprintf(query + shift, "arg%d IN (", pi);
            for (int oi = 0; oi < obj_size; ++oi){
                if (oi != 0)
                    shift += sprintf(query + shift, ",");
                shift += sprintf(query + shift, "%d", objs[oi]);
            }
            shift += sprintf(query + shift, ")");
        }
        ++ins;
    }
    ASSERT_RUNTIME(shift < QUERY_SIZE);
}

static void sqlActionInit(sql_action_t *action,
                          pddl_sqlite3 *db,
                          const sql_pred_t *preds,
                          const pddl_prep_action_t *prep_action,
                          pddl_err_t *err)
{
    ZEROIZE(action);
    action->param_size = prep_action->param_size;

    if (action->param_size == 0)
        return;

    PDDL_ISET(type_tables);
    char qcols[QUERY_SIZE];
    sqlActionConstructColumns(qcols, prep_action, &type_tables);
    char qtables[QUERY_SIZE];
    sqlActionConstructTables(qtables, preds, prep_action, &type_tables);
    char qjoincond[QUERY_SIZE];
    sqlActionConstructJoinCond(qjoincond, preds, prep_action);
    char qwhere[QUERY_SIZE];
    sqlActionConstructWhereCond(qwhere, preds, prep_action);
    pddlISetFree(&type_tables);

    char query[QUERY_SELECT_SIZE];
    int used = sprintf(query, "SELECT %s FROM %s %s %s;",
                       qcols, qtables, qjoincond, qwhere);
    ASSERT_RUNTIME(used < QUERY_SELECT_SIZE);

    //PDDL_INFO(err, "Action query %s: %s", prep_action->action->name, query);
    int ret = pddl_sqlite3_prepare_v2(db, query, -1, &action->stmt, NULL);
    CHECK_SQL_ERR(db, ret);
}

static void sqlActionFree(sql_action_t *action, pddl_sqlite3 *db)
{
    if (action->stmt != NULL)
        pddl_sqlite3_finalize(action->stmt);
}


static int actionCheckNegPreStatic(pddl_sql_grounder_t *g,
                                   const pddl_prep_action_t *paction,
                                   const pddl_obj_id_t *row)
{
    for (int i = 0; i < paction->pre_neg_static.size; ++i){
        const pddl_fm_atom_t *atom;
        atom = PDDL_FM_CAST(paction->pre_neg_static.fm[i], atom);
        if (atom->arg_size == 0){
            if (sqlPredHasAtom(g->pred + atom->pred, g->db, atom))
                return 0;
        }else{
            pddl_obj_id_t arg[atom->arg_size];
            for (int ai = 0; ai < atom->arg_size; ++ai){
                if (atom->arg[ai].obj >= 0){
                    arg[ai] = atom->arg[ai].obj;
                }else{
                    arg[ai] = row[atom->arg[ai].param];
                }
            }
            if (sqlPredHasAtomArg(g->pred + atom->pred, g->db, arg))
                return 0;
        }
    }

    return 1;
}


static int actionCheckGroundPre(pddl_sql_grounder_t *g,
                                const pddl_prep_action_t *paction)
{
    for (int i = 0; i < paction->pre_eq.size; ++i){
        const pddl_fm_atom_t *atom;
        atom = PDDL_FM_CAST(paction->pre_eq.fm[i], atom);
        if (atom->neg){
            if (atom->arg[0].obj == atom->arg[1].obj)
                return 0;
        }else{
            if (atom->arg[0].obj != atom->arg[1].obj)
                return 0;
        }
    }

    for (int i = 0; i < paction->pre.size; ++i){
        const pddl_fm_atom_t *atom;
        atom = PDDL_FM_CAST(paction->pre.fm[i], atom);
        if (!sqlPredHasAtom(g->pred + atom->pred, g->db, atom))
            return 0;
    }

    for (int i = 0; i < paction->pre_neg_static.size; ++i){
        const pddl_fm_atom_t *atom;
        atom = PDDL_FM_CAST(paction->pre_neg_static.fm[i], atom);
        if (sqlPredHasAtom(g->pred + atom->pred, g->db, atom))
            return 0;
    }

    return 1;
}


pddl_sql_grounder_t *pddlSqlGrounderNew(const pddl_t *pddl, pddl_err_t *err)
{
    CTX(err, "sql_grounder", "SQL Grounder");
    pddl_sql_grounder_t *g = ZALLOC(pddl_sql_grounder_t);

    g->pddl = pddl;
    if (pddlPrepActionsInit(g->pddl, &g->prep_action, err) != 0){
        FREE(g);
        CTXEND(err);
        PDDL_TRACE_RET(err, NULL);
    }

    // Create a database
    int flags = SQLITE_OPEN_READWRITE
                    | SQLITE_OPEN_CREATE
                    | SQLITE_OPEN_MEMORY
                    | SQLITE_OPEN_PRIVATECACHE;
    int ret = pddl_sqlite3_open_v2("db.sql", &g->db, flags, NULL);
    CHECK_SQL_ERR(g->db, ret);
    PDDL_INFO(err, "Sqlite database created");
    ASSERT_RUNTIME(pddl_sqlite3_get_autocommit(g->db));

    // Create type tables
    createTypeTables(g->db, g->pddl);

    // Create sql predicates
    g->pred = CALLOC_ARR(sql_pred_t, pddl->pred.pred_size);
    for (int pi = 0; pi < pddl->pred.pred_size; ++pi)
        sqlPredInit(g->pred + pi, g->db, &g->pddl->pred, pi, err);
    PDDL_INFO(err, "%d predicate tables created.", pddl->pred.pred_size);

    // Create sql actions
    g->action = CALLOC_ARR(sql_action_t, g->prep_action.action_size);
    for (int ai = 0; ai < g->prep_action.action_size; ++ai){
        sqlActionInit(g->action + ai, g->db, g->pred,
                      g->prep_action.action + ai, err);
    }
    PDDL_INFO(err, "%d action sql queries prepared.",
             g->prep_action.action_size);

    CTXEND(err);
    return g;
}

void pddlSqlGrounderDel(pddl_sql_grounder_t *g)
{
    for (int pi = 0; pi < g->pddl->pred.pred_size; ++pi)
        sqlPredFree(g->pred + pi, g->db);
    if (g->pred != NULL)
        FREE(g->pred);
    for (int ai = 0; ai < g->prep_action.action_size; ++ai)
        sqlActionFree(g->action + ai, g->db);
    if (g->action != NULL)
        FREE(g->action);

    pddlPrepActionsFree(&g->prep_action);
    int ret = pddl_sqlite3_close_v2(g->db);
    CHECK_SQL_ERR(g->db, ret);
    FREE(g);
}

int pddlSqlGrounderPrepActionSize(const pddl_sql_grounder_t *g)
{
    return g->prep_action.action_size;
}

const pddl_prep_action_t *pddlSqlGrounderPrepAction(
                const pddl_sql_grounder_t *g, int action_id)
{
    return g->prep_action.action + action_id;
}

int pddlSqlGrounderInsertAtomArgs(pddl_sql_grounder_t *g,
                                  int pred_id,
                                  const pddl_obj_id_t *args,
                                  pddl_err_t *err)
{
    return sqlPredInsertAtomArg(g->pred + pred_id, g->db, args, err);
}

int pddlSqlGrounderInsertGroundAtom(pddl_sql_grounder_t *g,
                                    const pddl_ground_atom_t *ga,
                                    pddl_err_t *err)
{
    return pddlSqlGrounderInsertAtomArgs(g, ga->pred, ga->arg, err);
}

int pddlSqlGrounderInsertAtom(pddl_sql_grounder_t *g,
                              const pddl_fm_atom_t *a,
                              pddl_err_t *err)
{
    pddl_obj_id_t args[a->arg_size];
    for (int i = 0; i < a->arg_size; ++i){
        if (a->arg[i].param >= 0)
            PDDL_ERR_RET(err, -1, "SQL Grounder: Atom is not grounded!");
        args[i] = a->arg[i].obj;
    }
    return pddlSqlGrounderInsertAtomArgs(g, a->pred, args, err);
}

int pddlSqlGrounderClearNonStatic(pddl_sql_grounder_t *g, pddl_err_t *err)
{
    for (int pi = 0; pi < g->pddl->pred.pred_size; ++pi){
        if (!g->pred[pi].is_static)
            sqlPredClear(g->pred + pi, g->db, err);
    }
    return 0;
}

int pddlSqlGrounderActionStart(pddl_sql_grounder_t *g,
                               int action_id,
                               pddl_err_t *err)
{
    sql_action_t *action = g->action + action_id;
    g->it_action_id = action_id;
    if (action->stmt != NULL)
        pddl_sqlite3_reset(action->stmt);
    return 0;
}

int pddlSqlGrounderActionNext(pddl_sql_grounder_t *g,
                              pddl_obj_id_t *args,
                              pddl_err_t *err)
{
    if (g->it_action_id < 0)
        return 0;

    sql_action_t *action = g->action + g->it_action_id;
    if (action->param_size == 0){
        if (actionCheckGroundPre(g, g->prep_action.action + g->it_action_id)){
            g->it_action_id = -1;
            return 1;
        }
        return 0;
    }

    if (action->stmt == NULL)
        return 0;

    while (pddl_sqlite3_step(action->stmt) == SQLITE_ROW){
        int invalid = 0;
        for (int i = 0; i < action->param_size; ++i){
            args[i] = pddl_sqlite3_column_int(action->stmt, i);
            if (args[i] < 0){
                invalid = 1;
                break;
            }
        }
        if (invalid){
            PDDL_INFO(err, "Invalid row");
            continue;
        }
        const pddl_prep_action_t *paction;
        paction = g->prep_action.action + g->it_action_id;
        if (!actionCheckNegPreStatic(g, paction, args))
            continue;

        return 1;
    }

    return 0;
}
