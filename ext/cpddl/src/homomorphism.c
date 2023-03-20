/***
 * cpddl
 * -------
 * Copyright (c)2021 Daniel Fiser <danfis@danfis.cz>,
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

#include "pddl/err.h"
#include "pddl/rand.h"
#include "pddl/homomorphism.h"
#include "pddl/endomorphism.h"
#include "pddl/strips_ground_sql.h"
#include "internal.h"

#define METHOD_TYPE 1
#define METHOD_RANDOM_PAIR 2
#define METHOD_GAIFMAN 3
#define METHOD_RPG 4
#define METHOD_ENDOMORPHISM 5

void pddlHomomorphismConfigLog(const pddl_homomorphism_config_t *cfg,
                               pddl_err_t *err)
{
    if (cfg->type == PDDL_HOMOMORPHISM_TYPES){
        LOG(err, "type = types");
    }else if (cfg->type == PDDL_HOMOMORPHISM_RAND_OBJS){
        LOG(err, "type = rand-objs");
    }else if (cfg->type == PDDL_HOMOMORPHISM_RAND_TYPE_OBJS){
        LOG(err, "type = rand-type-objs");
    }else if (cfg->type == PDDL_HOMOMORPHISM_GAIFMAN){
        LOG(err, "type = gaifman");
    }else if (cfg->type == PDDL_HOMOMORPHISM_RPG){
        LOG(err, "type = rpg");
    }else{
        LOG(err, "type = unknown");
    }
    LOG_CONFIG_BOOL(cfg, use_endomorphism, err);
    LOG_CONFIG_DBL(cfg, rm_ratio, err);
    LOG_CONFIG_INT(cfg, random_seed, err);
    LOG_CONFIG_BOOL(cfg, keep_goal_objs, err);
    LOG_CONFIG_INT(cfg, rpg_max_depth, err);
}

struct fix_action {
    pddl_t *pddl;
    pddl_action_t *action;
    const int *affected_types;
};

static int _removeAffectedNegativeAtoms(pddl_fm_t **c, void *_data)
{
    if ((*c)->type == PDDL_FM_ATOM){
        const struct fix_action *data = _data;
        const pddl_param_t *params = data->action->param.param;
        pddl_fm_atom_t *atom = PDDL_FM_CAST(*c, atom);
        if (atom->neg){
            for (int pi = 0; pi < atom->arg_size; ++pi){
                int param = atom->arg[pi].param;
                if (param >= 0 && data->affected_types[params[param].type]){
                    pddlFmDel(*c);
                    *c = NULL;
                    break;
                }
            }
        }

    }else if ((*c)->type == PDDL_FM_WHEN){
        pddl_fm_when_t *w = PDDL_FM_CAST(*c, when);
        if (w->pre == NULL)
            w->pre = &pddlFmNewBool(1)->fm;
        if (w->eff == NULL){
            pddlFmDel(*c);
            *c = NULL;
        }
    }
    return 0;
}

static void fixAction(pddl_t *pddl,
                      pddl_action_t *action,
                      const int *affected_types,
                      pddl_err_t *err)
{
    struct fix_action data;
    data.pddl = pddl;
    data.action = action;
    data.affected_types = affected_types;
    pddlFmRebuild(&action->pre, NULL, _removeAffectedNegativeAtoms, &data);
    pddlFmRebuild(&action->eff, NULL, _removeAffectedNegativeAtoms, &data);
    if (action->pre == NULL)
        action->pre = &pddlFmNewBool(1)->fm;
}

static void fixActions(pddl_t *pddl,
                       int repr,
                       const int *collapse_map,
                       pddl_err_t *err)
{
    ASSERT(repr >= 0);
    int *affected_types = CALLOC_ARR(int, pddl->type.type_size);
    for (int ti = 0; ti < pddl->type.type_size; ++ti){
        if (pddlTypesObjHasType(&pddl->type, ti, repr))
            affected_types[ti] = 1;
    }
    for (int ai = 0; ai < pddl->action.action_size; ++ai)
        fixAction(pddl, pddl->action.action + ai, affected_types, err);
    FREE(affected_types);
}

static int _collectGoalObjs(pddl_fm_t *c, void *_goal_objs)
{
    pddl_iset_t *goal_objs = _goal_objs;
    if (c->type == PDDL_FM_ATOM){
        const pddl_fm_atom_t *atom = PDDL_FM_CAST(c, atom);
        for (int i = 0; i < atom->arg_size; ++i){
            if (atom->arg[i].obj >= 0)
                pddlISetAdd(goal_objs, atom->arg[i].obj);
        }
    }
    return 0;
}

static void collectGoalObjs(const pddl_t *pddl, pddl_iset_t *goal_objs)
{
    pddlFmTraverse(pddl->goal, NULL, _collectGoalObjs, goal_objs);
}

static int collapseObjs(pddl_t *pddl,
                        int *collapse_map,
                        pddl_obj_id_t *obj_map,
                        int obj_size,
                        pddl_err_t *err)
{
    int repr = -1;
    for (int i = 0; i < pddl->obj.obj_size; ++i){
        if (collapse_map[i]){
            repr = i;
            break;
        }
    }

    pddl_obj_id_t *remap = CALLOC_ARR(pddl_obj_id_t, pddl->obj.obj_size);
    for (int i = 0, idx = 0; i < pddl->obj.obj_size; ++i){
        if (collapse_map[i] && i != repr){
            remap[i] = repr;
        }else{
            remap[i] = idx++;
        }
    }

    if (obj_map != NULL){
        for (int i = 0; i < obj_size; ++i)
            obj_map[i] = remap[obj_map[i]];
    }

    pddlFmRemapObjs(&pddl->init->fm, remap);
    pddlFmRemapObjs(pddl->goal, remap);
    fixActions(pddl, repr, collapse_map, err);
    pddlActionsRemapObjs(&pddl->action, remap);

    for (int i = 0; i < pddl->obj.obj_size; ++i){
        if (collapse_map[i] && i != repr)
            remap[i] = -1;
    }
    pddlTypesRemapObjs(&pddl->type, remap);
    pddlObjsRemap(&pddl->obj, remap);
    // TODO
    //pddlNormalize(pddl);

    FREE(remap);
    return 0;
}


static int _collapseObjs(pddl_homomorphic_task_t *h,
                         int *collapse_map,
                         pddl_err_t *err)
{
    int repr = -1;
    for (int i = 0; i < h->task.obj.obj_size; ++i){
        if (collapse_map[i]){
            repr = i;
            break;
        }
    }

    pddl_obj_id_t *remap = CALLOC_ARR(pddl_obj_id_t, h->task.obj.obj_size);
    for (int i = 0, idx = 0; i < h->task.obj.obj_size; ++i){
        if (collapse_map[i] && i != repr){
            remap[i] = repr;
        }else{
            remap[i] = idx++;
        }
    }

    if (h->obj_map != NULL){
        for (int i = 0; i < h->input_obj_size; ++i)
            h->obj_map[i] = remap[h->obj_map[i]];
    }

    pddlFmRemapObjs(&h->task.init->fm, remap);
    pddlFmRemapObjs(h->task.goal, remap);
    fixActions(&h->task, repr, collapse_map, err);
    pddlActionsRemapObjs(&h->task.action, remap);

    for (int i = 0; i < h->task.obj.obj_size; ++i){
        if (collapse_map[i] && i != repr)
            remap[i] = -1;
    }
    pddlTypesRemapObjs(&h->task.type, remap);
    pddlObjsRemap(&h->task.obj, remap);
    // TODO
    //pddlNormalize(pddl);

    FREE(remap);
    return 0;
}

static int collapsePair(pddl_homomorphic_task_t *h,
                        pddl_obj_id_t o1,
                        pddl_obj_id_t o2,
                        pddl_err_t *err)
{
    int *collapse_map = CALLOC_ARR(int, h->task.obj.obj_size);
    collapse_map[o1] = collapse_map[o2] = 1;
    int ret = _collapseObjs(h, collapse_map, err);
    FREE(collapse_map);
    return ret;
}

static int collapseEndomorphism(pddl_t *pddl,
                                const pddl_homomorphism_config_t *cfg,
                                pddl_rand_t *rnd,
                                pddl_obj_id_t *obj_map,
                                int obj_size,
                                pddl_err_t *err)
{
    PDDL_INFO(err, "Collapse with endomorphisms");
    // TODO
    pddl_endomorphism_config_t ecfg = cfg->endomorphism_cfg;
    PDDL_ISET(redundant);
    pddl_obj_id_t *map = ALLOC_ARR(pddl_obj_id_t, pddl->obj.obj_size);
    int ret = pddlEndomorphismRelaxedLifted(pddl, &ecfg, &redundant, map, err);
    if (ret == 0 && pddlISetSize(&redundant) > 0){
        pddl_obj_id_t *remap = CALLOC_ARR(pddl_obj_id_t, pddl->obj.obj_size);
        int oid;
        PDDL_ISET_FOR_EACH(&redundant, oid)
            remap[oid] = PDDL_OBJ_ID_UNDEF;
        for (int i = 0, idx = 0; i < pddl->obj.obj_size; ++i){
            if (remap[i] != PDDL_OBJ_ID_UNDEF)
                remap[i] = idx++;
        }
        PDDL_ISET_FOR_EACH(&redundant, oid){
            remap[oid] = remap[map[oid]];
            ASSERT_RUNTIME(remap[oid] >= 0);
        }
        pddlRemapObjs(pddl, remap);
        pddlNormalize(pddl);

        if (obj_map != NULL){
            for (int i = 0; i < obj_size; ++i)
                obj_map[i] = remap[obj_map[i]];
        }

        if (remap != NULL)
            FREE(remap);
    }
    pddlISetFree(&redundant);
    if (map != NULL)
        FREE(map);
    PDDL_INFO(err, "Collapse with endomorphisms. DONE. ret: %d", ret);
    return ret;
}

static int collapseRandomPairTypeObj(pddl_t *pddl,
                                     const pddl_homomorphism_config_t *cfg,
                                     pddl_rand_t *rnd,
                                     pddl_obj_id_t *obj_map,
                                     int obj_size,
                                     pddl_err_t *err)
{
    int choose_types[pddl->type.type_size];
    int type_size = 0;
    for (int type = 0; type < pddl->type.type_size; ++type){
        if (pddlTypesIsMinimal(&pddl->type, type)
                && pddlTypeNumObjs(&pddl->type, type) > 1){
            choose_types[type_size++] = type;
        }
    }

    if (type_size == 0)
        return -1;

    int choice = pddlRand(rnd, 0, type_size);
    int type = choose_types[choice];
    int num_objs;
    const pddl_obj_id_t *objs;
    objs = pddlTypesObjsByType(&pddl->type, type, &num_objs);
    int obj1 = objs[(int)pddlRand(rnd, 0, num_objs)];
    int obj2 = obj1;
    while (obj1 == obj2)
        obj2 = objs[(int)pddlRand(rnd, 0, num_objs)];

    int *collapse_map = CALLOC_ARR(int, pddl->obj.obj_size);
    collapse_map[obj1] = collapse_map[obj2] = 1;
    int ret = collapseObjs(pddl, collapse_map, obj_map, obj_size, err);
    FREE(collapse_map);
    return ret;
}

static int collapseRandomPairObj(pddl_t *pddl,
                                 const pddl_homomorphism_config_t *cfg,
                                 pddl_rand_t *rnd,
                                 pddl_obj_id_t *obj_map,
                                 int obj_size,
                                 pddl_err_t *err)
{
    PDDL_ISET(goal_objs);
    if (cfg->keep_goal_objs){
        collectGoalObjs(pddl, &goal_objs);
        //PDDL_INFO(err, "Collected %d goal objects", pddlISetSize(&goal_objs));
    }

    int *choose_types = CALLOC_ARR(int, pddl->obj.obj_size);
    int *choose_objs = CALLOC_ARR(int, pddl->obj.obj_size);
    int objs_size = 0;
    for (int type = 0; type < pddl->type.type_size; ++type){
        if (pddlTypesIsMinimal(&pddl->type, type)
                && pddlTypeNumObjs(&pddl->type, type) > 1){
            int num_objs;
            const pddl_obj_id_t *objs;
            objs = pddlTypesObjsByType(&pddl->type, type, &num_objs);
            for (int i = 0; i < num_objs; ++i){
                if (!pddlISetIn(objs[i], &goal_objs)){
                    choose_objs[objs_size] = objs[i];
                    choose_types[objs_size++] = type;
                }
            }
        }
    }

    if (objs_size == 0){
        FREE(choose_types);
        FREE(choose_objs);
        pddlISetFree(&goal_objs);
        return -1;
    }

    int choice = pddlRand(rnd, 0, objs_size);
    int obj1 = choose_objs[choice];
    int type = choose_types[choice];
    int num_objs;
    const pddl_obj_id_t *objs;
    objs = pddlTypesObjsByType(&pddl->type, type, &num_objs);
    int obj2 = obj1;
    while (obj1 == obj2)
        obj2 = objs[(int)pddlRand(rnd, 0, num_objs)];

    int *collapse_map = CALLOC_ARR(int, pddl->obj.obj_size);
    collapse_map[obj1] = collapse_map[obj2] = 1;
    int ret = collapseObjs(pddl, collapse_map, obj_map, obj_size, err);
    FREE(collapse_map);
    FREE(choose_types);
    FREE(choose_objs);
    pddlISetFree(&goal_objs);
    return ret;
}

static int collapseType(pddl_t *pddl,
                        int type,
                        pddl_obj_id_t *obj_map,
                        int obj_size,
                        pddl_err_t *err)
{
    pddl_types_t *types = &pddl->type;
    if (!pddlTypesIsMinimal(types, type)){
        PDDL_ERR_RET(err, -1, "Type %d (%s) is not minimal!",
                     type, types->type[type].name);
    }
    if (pddlTypeNumObjs(types, type) <= 1){
        PDDL_INFO(err, "Type %d (%s) has no more than one object:"
                  " nothing to collapse",
                  type, types->type[type].name);
        return 0;
    }

    int init_num_objs = pddl->obj.obj_size;
    int *collapse_map = CALLOC_ARR(int, pddl->obj.obj_size);
    int objs_size;
    const pddl_obj_id_t *objs = pddlTypesObjsByType(types, type, &objs_size);
    for (int i = 0; i < objs_size; ++i)
        collapse_map[objs[i]] = 1;
    int ret = collapseObjs(pddl, collapse_map, obj_map, obj_size, err);
    FREE(collapse_map);

    if (ret == 0){
        PDDL_INFO(err, "Type %d (%s) collapsed. Num objs: %d -> %d",
                  type, types->type[type].name,
                  init_num_objs, pddl->obj.obj_size);
        return 0;
    }else{
        PDDL_TRACE_RET(err, -1);
    }
}

struct gaifman {
    int obj_size;
    int *obj_is_static;
    int num_static_objs;
    int num_static_nongoal_objs;
    pddl_iset_t *obj_relate_to;
    pddl_iset_t goal_objs;
};
typedef struct gaifman gaifman_t;

static void gaifmanInit(gaifman_t *g, const pddl_t *pddl, int preserve_goals)
{
    ZEROIZE(g);
    g->obj_size = pddl->obj.obj_size;
    g->obj_is_static = CALLOC_ARR(int, pddl->obj.obj_size);
    g->obj_relate_to = CALLOC_ARR(pddl_iset_t, pddl->obj.obj_size);
    if (preserve_goals)
        collectGoalObjs(pddl, &g->goal_objs);

    pddl_fm_const_it_atom_t it;
    const pddl_fm_atom_t *atom;
    PDDL_FM_FOR_EACH_ATOM(&pddl->init->fm, &it, atom){
        if (pddlPredIsStatic(pddl->pred.pred + atom->pred)
                && pddl->pred.pred[atom->pred].param_size > 1){
            for (int i = 0; i < atom->arg_size; ++i){
                ASSERT(atom->arg[i].obj >= 0);
                g->obj_is_static[atom->arg[i].obj] = 1;
            }
            for (int i = 0; i < atom->arg_size; ++i){
                int o1 = atom->arg[i].obj;
                for (int j = i + 1; j < atom->arg_size; ++j){
                    int o2 = atom->arg[j].obj;
                    if (o1 != o2){
                        pddlISetAdd(g->obj_relate_to + o1, o2);
                        pddlISetAdd(g->obj_relate_to + o2, o1);
                        /*
                        fprintf(stderr, "%d(%s) -- %d(%s)\n",
                                o1, pddl->obj.obj[o1].name,
                                o2, pddl->obj.obj[o2].name);
                        */
                    }
                }
            }
        }
    }

    for (int i = 0; i < pddl->obj.obj_size; ++i){
        g->num_static_objs += g->obj_is_static[i];
        if (!pddlISetIn(i, &g->goal_objs))
            g->num_static_nongoal_objs += g->obj_is_static[i];
    }
}

static void gaifmanFree(gaifman_t *g)
{
    if (g->obj_is_static != NULL)
        FREE(g->obj_is_static);
    for (int i = 0; i < g->obj_size; ++i)
        pddlISetFree(&g->obj_relate_to[i]);
    if (g->obj_relate_to != NULL)
        FREE(g->obj_relate_to);
    pddlISetFree(&g->goal_objs);
}

static int gaifmanFindPairDepth(gaifman_t *g,
                                const pddl_t *pddl,
                                int depth,
                                pddl_obj_id_t *o1,
                                pddl_obj_id_t *o2)
{
    int found = 0;
    int found_candidate = 0;
    int degree = INT_MAX;
    PDDL_ISET(neigh);
    PDDL_IARR(queue);
    int *visited = CALLOC_ARR(int, pddl->obj.obj_size);

    for (int x = 0; x < g->obj_size; ++x){
        if (!g->obj_is_static[x] || pddlISetSize(&g->obj_relate_to[x]) == 0)
            continue;
        if (pddlISetIn(x, &g->goal_objs))
            continue;
        int xtype = pddl->obj.obj[x].type;
        ZEROIZE_ARR(visited, pddl->obj.obj_size);
        visited[x] = 1;
        pddlIArrEmpty(&queue);
        pddlIArrAdd(&queue, x);
        for (int i = 0; i < pddlIArrSize(&queue); ++i){
            int o = pddlIArrGet(&queue, i);

            int y;
            PDDL_ISET_FOR_EACH(&g->obj_relate_to[o], y){
                if (visited[y])
                    continue;

                if (visited[o] == depth){
                    found_candidate = 1;
                    if (xtype == pddl->obj.obj[y].type
                            && !pddlISetIn(y, &g->goal_objs)){
                        pddlISetUnion2(&neigh, &g->obj_relate_to[x],
                                              &g->obj_relate_to[y]);
                        if (!found || pddlISetSize(&neigh) < degree){
                            found = 1;
                            *o1 = x;
                            *o2 = y;
                            degree = pddlISetSize(&neigh);
                        }
                    }
                }else{
                    pddlIArrAdd(&queue, y);
                }
                visited[y] = visited[o] + 1;
            }
        }
    }

    if (visited != NULL)
        FREE(visited);
    pddlIArrFree(&queue);
    pddlISetFree(&neigh);

    if (!found_candidate)
        return -1;
    return found;
}

static int gaifmanFindPair(gaifman_t *g,
                           const pddl_t *pddl,
                           pddl_obj_id_t *o1,
                           pddl_obj_id_t *o2)
{
    for (int depth = 1; 1; ++depth){
        int ret;
        if ((ret = gaifmanFindPairDepth(g, pddl, depth, o1, o2)) > 0)
            return 1;
        if (ret < 0)
            return 0;
    }
    return 0;
}

static int collapseGaifman(pddl_t *pddl,
                           const pddl_homomorphism_config_t *cfg,
                           pddl_rand_t *rnd,
                           pddl_obj_id_t *obj_map,
                           int obj_size,
                           pddl_err_t *err)
{
    int ret = 0;
    gaifman_t gaif;
    gaifmanInit(&gaif, pddl, cfg->keep_goal_objs);
    PDDL_INFO(err, "Static objects: %d/%d",
              gaif.num_static_objs, pddl->obj.obj_size);
    PDDL_INFO(err, "Non-goal static objects: %d/%d",
              gaif.num_static_nongoal_objs, pddl->obj.obj_size);
    pddl_obj_id_t o1 = 0, o2 = 0;
    if (gaifmanFindPair(&gaif, pddl, &o1, &o2)){
        PDDL_INFO(err, "Collapsing %d:(%s) and %d:(%s)",
                  o1, pddl->obj.obj[o1].name,
                  o2, pddl->obj.obj[o2].name);
        int *collapse_map = CALLOC_ARR(int, pddl->obj.obj_size);
        collapse_map[o1] = collapse_map[o2] = 1;
        ret = collapseObjs(pddl, collapse_map, obj_map, obj_size, err);
        if (collapse_map != NULL)
            FREE(collapse_map);
    }else{
        PDDL_INFO(err, "Nothing to collapse.");
        ret = 1;
    }

    gaifmanFree(&gaif);
    return ret;
}

static int atomHasObj(const pddl_fm_atom_t *atom, pddl_obj_id_t o)
{
    for (int i = 0; i < atom->arg_size; ++i){
        if (atom->arg[i].obj == o)
            return 1;
    }
    return 0;
}


static int rpgTestPair(const pddl_t *pddl,
                       const pddl_ground_atoms_t *ga,
                       pddl_obj_id_t o1,
                       pddl_obj_id_t o2,
                       pddl_err_t *err)
{
    pddl_fm_const_it_atom_t it;
    const pddl_fm_atom_t *atom;
    PDDL_FM_FOR_EACH_ATOM(&pddl->init->fm, &it, atom){
        if (atomHasObj(atom, o2)){
            pddl_obj_id_t args[atom->arg_size];
            for (int i = 0; i < atom->arg_size; ++i){
                if (atom->arg[i].obj == o2){
                    args[i] = o1;
                }else{
                    args[i] = atom->arg[i].obj;
                }
            }
            if (pddlGroundAtomsFindPred(ga, atom->pred, args, atom->arg_size) != NULL){
                return 0;
            }
        }
    }
    return 1;
}

static int rpgFindPair(const pddl_t *pddl,
                       int max_depth,
                       const pddl_iset_t *goal_objs,
                       pddl_obj_id_t *o1,
                       pddl_obj_id_t *o2,
                       pddl_err_t *err)
{
    pddl_ground_config_t ground_cfg = PDDL_GROUND_CONFIG_INIT;
    pddl_ground_atoms_t ga;
    pddlGroundAtomsInit(&ga);
    if (pddlStripsGroundSqlLayered(pddl, &ground_cfg, max_depth,
                                   INT_MAX, NULL, &ga, err) != 0){
        pddlGroundAtomsFree(&ga);
        PDDL_TRACE_RET(err, -1);
    }

    for (int type_id = 0; type_id < pddl->type.type_size; ++type_id){
        const pddl_type_t *type = pddl->type.type + type_id;
        if (pddlISetSize(&type->child) == 0){
            int p1, p2;
            PDDL_OBJSET_FOR_EACH(&type->obj, p1){
                PDDL_OBJSET_FOR_EACH(&type->obj, p2){
                    if (p1 == p2 || pddlISetIn(p2, goal_objs))
                        continue;
                    if (rpgTestPair(pddl, &ga, p1, p2, err)){
                        *o1 = p1;
                        *o2 = p2;
                        pddlGroundAtomsFree(&ga);
                        return 1;
                    }
                }
            }
        }
    }

    pddlGroundAtomsFree(&ga);
    return 0;
}

static int collapseRPG(pddl_t *pddl,
                       const pddl_homomorphism_config_t *cfg,
                       pddl_rand_t *rnd,
                       pddl_obj_id_t *obj_map,
                       int obj_size,
                       pddl_err_t *err)
{
    int ret = 1;
    PDDL_ISET(goal_objs);
    if (cfg->keep_goal_objs)
        collectGoalObjs(pddl, &goal_objs);

    int max_depth = cfg->rpg_max_depth;
    for (int depth = 1; depth <= max_depth; ++depth){
        pddl_obj_id_t o1, o2;
        int found = rpgFindPair(pddl, depth, &goal_objs, &o1, &o2, err);
        if (found < 0){
            pddlISetFree(&goal_objs);
            PDDL_TRACE_RET(err, -1);
        }else if (found > 0){
            PDDL_INFO(err, "Found pair %d:(%s) %d:(%s) in depth %d",
                      o1, pddl->obj.obj[o1].name,
                      o2, pddl->obj.obj[o2].name, depth);
            int *collapse_map = CALLOC_ARR(int, pddl->obj.obj_size);
            collapse_map[o1] = collapse_map[o2] = 1;
            ret = collapseObjs(pddl, collapse_map, obj_map, obj_size, err);
            if (collapse_map != NULL)
                FREE(collapse_map);
            break;
        }
        if (ret == 1)
            PDDL_INFO(err, "No pair found in depth %d", depth);
    }


    pddlISetFree(&goal_objs);
    return ret;
}


static void _deduplicateCostsPart(pddl_fm_junc_t *p)
{
    pddl_list_t *item = pddlListNext(&p->part);
    while (item != &p->part){
        pddl_fm_t *c1 = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
        if (c1->type != PDDL_FM_ASSIGN){
            item = pddlListNext(item);
            continue;
        }
        pddl_fm_func_op_t *ass1 = PDDL_FM_CAST(c1, func_op);
        ASSERT_RUNTIME(ass1->lvalue != NULL);
        ASSERT_RUNTIME(ass1->fvalue == NULL);
        int min_value = ass1->value;

        pddl_list_t *item2 = pddlListNext(item);
        while (item2 != &p->part){
            pddl_fm_t *c2 = PDDL_LIST_ENTRY(item2, pddl_fm_t, conn);
            if (c2->type == PDDL_FM_ASSIGN){
                pddl_fm_func_op_t *ass2 = PDDL_FM_CAST(c2, func_op);
                ASSERT_RUNTIME(ass2->lvalue != NULL);
                ASSERT_RUNTIME(ass2->fvalue == NULL);
                if (pddlFmAtomCmp(ass1->lvalue, ass2->lvalue) == 0){
                    min_value = PDDL_MIN(min_value, ass2->value);
                    pddl_list_t *item_del = item2;
                    item2 = pddlListNext(item2);
                    pddlListDel(item_del);
                    pddlFmDel(c2);

                }else{
                    item2 = pddlListNext(item2);
                }

            }else{
                item2 = pddlListNext(item2);
            }
        }

        ass1->value = min_value;
        item = pddlListNext(item);
    }
}

static int _deduplicateCosts(pddl_fm_t **c, void *data)
{
    if ((*c)->type == PDDL_FM_AND || (*c)->type == PDDL_FM_OR)
        _deduplicateCostsPart(pddlFmToJunc(*c));
    return 0;
}

static pddl_fm_t *deduplicateCosts(pddl_fm_t *c)
{
    pddlFmRebuild(&c, NULL, _deduplicateCosts, NULL);
    return c;
}

static void deduplicate(pddl_t *pddl)
{
    pddl_fm_t *init = pddlFmDeduplicateAtoms(&pddl->init->fm, pddl);
    init = deduplicateCosts(init);
    pddl->init = pddlFmToAnd(init);
    pddl->goal = pddlFmDeduplicateAtoms(pddl->goal, pddl);
}

int pddlHomomorphism(pddl_t *pddl,
                     const pddl_t *src,
                     const pddl_homomorphism_config_t *cfg,
                     pddl_obj_id_t *obj_map,
                     pddl_err_t *err)
{
    ASSERT_RUNTIME_M(cfg->type != 0u, "Invalid configuration");
    if (cfg->type == PDDL_HOMOMORPHISM_TYPES
            && pddlISetSize(&cfg->collapse_types) == 0){
        PDDL_ERR_RET(err, -1, "Nothing to do!");
    }

    CTX(err, "homo", "Homomorphism");
    CTX_NO_TIME(err, "cfg", "Cfg");
    pddlHomomorphismConfigLog(cfg, err);
    CTXEND(err);
    PDDL_INFO(err, "Computing homomorphism (objs: %d).", src->obj.obj_size);
    if (obj_map != NULL){
        for (int i = 0; i < src->obj.obj_size; ++i)
            obj_map[i] = i;
    }

    pddlInitCopy(pddl, src);
    if (cfg->type == PDDL_HOMOMORPHISM_TYPES){
        int type;
        PDDL_ISET_FOR_EACH(&cfg->collapse_types, type){
            if (collapseType(pddl, type, obj_map, src->obj.obj_size, err) != 0)
                PDDL_TRACE_RET(err, -1);
        }

    }else if (cfg->type == PDDL_HOMOMORPHISM_RAND_OBJS
                || cfg->type == PDDL_HOMOMORPHISM_RAND_TYPE_OBJS
                || cfg->type == PDDL_HOMOMORPHISM_GAIFMAN
                || cfg->type == PDDL_HOMOMORPHISM_RPG){
        int (*fn[2])(pddl_t *pddl,
                     const pddl_homomorphism_config_t *cfg,
                     pddl_rand_t *rnd,
                     pddl_obj_id_t *obj_map,
                     int obj_size,
                     pddl_err_t *err) = { NULL, NULL };
        if (cfg->type == PDDL_HOMOMORPHISM_RAND_OBJS)
            fn[0] = fn[1] = collapseRandomPairObj;
        if (cfg->type == PDDL_HOMOMORPHISM_RAND_TYPE_OBJS)
            fn[0] = fn[1] = collapseRandomPairTypeObj;
        if (cfg->type == PDDL_HOMOMORPHISM_GAIFMAN)
            fn[0] = fn[1] = collapseGaifman;
        if (cfg->type == PDDL_HOMOMORPHISM_RPG)
            fn[0] = fn[1] = collapseRPG;
        ASSERT_RUNTIME(fn[0] != NULL && fn[1] != NULL);
        int obj_size = src->obj.obj_size;
        pddl_rand_t rnd;
        pddlRandInit(&rnd, cfg->random_seed);
        int target = pddl->obj.obj_size * (1.f - cfg->rm_ratio);
        PDDL_INFO(err, "Target number of objects: %d", target);

        if (cfg->use_endomorphism)
            fn[0] = collapseEndomorphism;

        int fni = 0;
        while (pddl->obj.obj_size >= 1
                && pddl->obj.obj_size > target){
            if (fn[fni](pddl, cfg, &rnd, obj_map, obj_size, err) != 0){
                if (fn[fni] == collapseEndomorphism){
                    PDDL_INFO(err, "Endomorphism failed -- disabling...");
                    fn[fni] = fn[(fni + 1) % 2];
                }else{
                    break;
                }
            }
            fni = (fni + 1) % 2;
        }

    }else{
        PANIC("Homomorphism: Unkown type %d", cfg->type);
    }

    deduplicate(pddl);
    pddlNormalize(pddl);
    PDDL_INFO(err, "Homomorphism computed (objs: %d, from objs: %d).",
              pddl->obj.obj_size,
              src->obj.obj_size);
    CTXEND(err);
    return 0;
}



void pddlHomomorphicTaskInit(pddl_homomorphic_task_t *h, const pddl_t *in)
{
    ZEROIZE(h);
    h->input_obj_size = in->obj.obj_size;
    pddlInitCopy(&h->task, in);
    if (h->input_obj_size > 0){
        h->obj_map = CALLOC_ARR(pddl_obj_id_t, h->input_obj_size);
        for (int i = 0; i < h->input_obj_size; ++i)
            h->obj_map[i] = i;
    }
    pddlRandInitAuto(&h->rnd);
}

void pddlHomomorphicTaskFree(pddl_homomorphic_task_t *h)
{
    pddlFree(&h->task);
    if (h->obj_map != NULL)
        FREE(h->obj_map);
}

void pddlHomomorphicTaskSeed(pddl_homomorphic_task_t *h, uint32_t seed)
{
    pddlRandReseed(&h->rnd, seed);
}

int pddlHomomorphicTaskCollapseType(pddl_homomorphic_task_t *h,
                                    int type,
                                    pddl_err_t *err)
{
    pddl_types_t *types = &h->task.type;
    if (!pddlTypesIsMinimal(types, type)){
        PDDL_ERR_RET(err, -1, "Type %d (%s) is not minimal!",
                     type, types->type[type].name);
    }
    if (pddlTypeNumObjs(types, type) <= 1){
        PDDL_INFO(err, "Type %d (%s) has no more than one object:"
                  " nothing to collapse",
                  type, types->type[type].name);
        return 0;
    }

    int init_num_objs = h->task.obj.obj_size;
    int *collapse_map = CALLOC_ARR(int, h->task.obj.obj_size);
    int objs_size;
    const pddl_obj_id_t *objs = pddlTypesObjsByType(types, type, &objs_size);
    for (int i = 0; i < objs_size; ++i)
        collapse_map[objs[i]] = 1;
    int ret = _collapseObjs(h, collapse_map, err);
    FREE(collapse_map);

    if (ret == 0){
        PDDL_INFO(err, "Type %d (%s) collapsed. Num objs: %d -> %d",
                  type, types->type[type].name,
                  init_num_objs, h->task.obj.obj_size);
        return 0;
    }else{
        PDDL_TRACE_RET(err, -1);
    }
}

int pddlHomomorphicTaskCollapseRandomPair(pddl_homomorphic_task_t *h,
                                          int preserve_goals,
                                          pddl_err_t *err)
{
    PDDL_ISET(goal_objs);
    if (preserve_goals){
        collectGoalObjs(&h->task, &goal_objs);
        //PDDL_INFO(err, "Collected %d goal objects", pddlISetSize(&goal_objs));
    }

    int *choose_types = CALLOC_ARR(int, h->task.obj.obj_size);
    int *choose_objs = CALLOC_ARR(int, h->task.obj.obj_size);
    int objs_size = 0;
    for (int type = 0; type < h->task.type.type_size; ++type){
        if (pddlTypesIsMinimal(&h->task.type, type)
                && pddlTypeNumObjs(&h->task.type, type) > 1){
            int num_objs;
            const pddl_obj_id_t *objs;
            objs = pddlTypesObjsByType(&h->task.type, type, &num_objs);
            for (int i = 0; i < num_objs; ++i){
                if (!pddlISetIn(objs[i], &goal_objs)){
                    choose_objs[objs_size] = objs[i];
                    choose_types[objs_size++] = type;
                }
            }
        }
    }

    if (objs_size == 0){
        FREE(choose_types);
        FREE(choose_objs);
        pddlISetFree(&goal_objs);
        return -1;
    }

    int choice = pddlRand(&h->rnd, 0, objs_size);
    int obj1 = choose_objs[choice];
    int type = choose_types[choice];
    int num_objs;
    const pddl_obj_id_t *objs;
    objs = pddlTypesObjsByType(&h->task.type, type, &num_objs);
    int obj2 = obj1;
    while (obj1 == obj2)
        obj2 = objs[(int)pddlRand(&h->rnd, 0, num_objs)];

    int ret = collapsePair(h, obj1, obj2, err);
    FREE(choose_types);
    FREE(choose_objs);
    pddlISetFree(&goal_objs);
    return ret;
}

int pddlHomomorphicTaskCollapseGaifman(pddl_homomorphic_task_t *h,
                                       int preserve_goals,
                                       pddl_err_t *err)
{
    int ret = 0;
    gaifman_t gaif;
    gaifmanInit(&gaif, &h->task, preserve_goals);
    PDDL_INFO(err, "Static objects: %d/%d",
              gaif.num_static_objs, h->task.obj.obj_size);
    PDDL_INFO(err, "Non-goal static objects: %d/%d",
              gaif.num_static_nongoal_objs, h->task.obj.obj_size);
    pddl_obj_id_t o1 = 0, o2 = 0;
    if (gaifmanFindPair(&gaif, &h->task, &o1, &o2)){
        PDDL_INFO(err, "Collapsing %d:(%s) and %d:(%s)",
                  o1, h->task.obj.obj[o1].name,
                  o2, h->task.obj.obj[o2].name);
        ret = collapsePair(h, o1, o2, err);
    }else{
        PDDL_INFO(err, "Nothing to collapse.");
        ret = 1;
    }

    gaifmanFree(&gaif);
    return ret;
}

int pddlHomomorphicTaskCollapseRPG(pddl_homomorphic_task_t *h,
                                   int preserve_goals,
                                   int max_depth,
                                   pddl_err_t *err)
{
    int ret = 1;
    PDDL_ISET(goal_objs);
    if (preserve_goals)
        collectGoalObjs(&h->task, &goal_objs);

    for (int depth = 1; depth <= max_depth; ++depth){
        pddl_obj_id_t o1, o2;
        int found = rpgFindPair(&h->task, depth, &goal_objs, &o1, &o2, err);
        if (found < 0){
            pddlPrintDebug(&h->task, stderr);
            pddlISetFree(&goal_objs);
            PDDL_TRACE_RET(err, -1);
        }else if (found > 0){
            PDDL_INFO(err, "Found pair %d:(%s) %d:(%s) in depth %d",
                      o1, h->task.obj.obj[o1].name,
                      o2, h->task.obj.obj[o2].name, depth);
            ret = collapsePair(h, o1, o2, err);
            break;
        }
        if (ret == 1)
            PDDL_INFO(err, "No pair found in depth %d", depth);
    }

    pddlISetFree(&goal_objs);
    return ret;

}

int pddlHomomorphicTaskApplyRelaxedEndomorphism(
            pddl_homomorphic_task_t *h,
            const pddl_endomorphism_config_t *cfg,
            pddl_err_t *err)
{
    PDDL_INFO(err, "Relaxed endomorphisms");
    PDDL_ISET(redundant);
    pddl_obj_id_t *map = ALLOC_ARR(pddl_obj_id_t, h->task.obj.obj_size);
    int ret = pddlEndomorphismRelaxedLifted(&h->task, cfg, &redundant, map, err);
    if (ret == 0 && pddlISetSize(&redundant) > 0){
        pddl_obj_id_t *remap = CALLOC_ARR(pddl_obj_id_t, h->task.obj.obj_size);
        int oid;
        PDDL_ISET_FOR_EACH(&redundant, oid)
            remap[oid] = PDDL_OBJ_ID_UNDEF;
        for (int i = 0, idx = 0; i < h->task.obj.obj_size; ++i){
            if (remap[i] != PDDL_OBJ_ID_UNDEF)
                remap[i] = idx++;
        }
        PDDL_ISET_FOR_EACH(&redundant, oid){
            remap[oid] = remap[map[oid]];
            ASSERT_RUNTIME(remap[oid] >= 0);
        }
        pddlRemapObjs(&h->task, remap);
        pddlNormalize(&h->task);

        for (int i = 0; i < h->input_obj_size; ++i)
            h->obj_map[i] = remap[h->obj_map[i]];

        if (remap != NULL)
            FREE(remap);
    }
    pddlISetFree(&redundant);
    if (map != NULL)
        FREE(map);
    PDDL_INFO(err, "Relaxed endomorphism. DONE. ret: %d", ret);
    return ret;
}

void pddlHomomorphicTaskReduceInit(pddl_homomorphic_task_reduce_t *r,
                                   int target_obj_size)
{
    ZEROIZE(r);
    r->target_obj_size = target_obj_size;
    pddlListInit(&r->method);
}

void pddlHomomorphicTaskReduceFree(pddl_homomorphic_task_reduce_t *r)
{
    pddl_list_t *item;
    while (!pddlListEmpty(&r->method)){
        item = pddlListNext(&r->method);
        pddlListDel(item);
        pddl_homomorphic_task_method_t *m;
        m = PDDL_LIST_ENTRY(item, pddl_homomorphic_task_method_t, conn);
        FREE(m);
    }
}

static pddl_homomorphic_task_method_t *methodNew(int method)
{
    pddl_homomorphic_task_method_t *m;
    m = ZALLOC(pddl_homomorphic_task_method_t);
    m->method = method;
    pddlListInit(&m->conn);
    return m;
}

static int methodRun(const pddl_homomorphic_task_method_t *m,
                     pddl_homomorphic_task_t *h,
                     pddl_err_t *err)
{
    switch (m->method){
        case METHOD_TYPE:
            return pddlHomomorphicTaskCollapseType(h, m->arg_type, err);
        case METHOD_RANDOM_PAIR:
            return pddlHomomorphicTaskCollapseRandomPair(h, m->arg_preserve_goals, err);
        case METHOD_GAIFMAN:
            return pddlHomomorphicTaskCollapseGaifman(h, m->arg_preserve_goals, err);
        case METHOD_RPG:
            return pddlHomomorphicTaskCollapseRPG(h, m->arg_preserve_goals,
                                                  m->arg_max_depth, err);
        case METHOD_ENDOMORPHISM:
            return pddlHomomorphicTaskApplyRelaxedEndomorphism(
                        h, &m->arg_endomorphism_cfg, err);
        default:
            PDDL_ERR_RET(err, -1, "Uknown method %d", m->method);
    }
    PDDL_ERR_RET(err, -1, "Uknown method %d", m->method);
}

void pddlHomomorphicTaskReduceAddType(pddl_homomorphic_task_reduce_t *r,
                                      int type)
{
    pddl_homomorphic_task_method_t *m = methodNew(METHOD_TYPE);
    m->arg_type = type;
    pddlListAppend(&r->method, &m->conn);
}

void pddlHomomorphicTaskReduceAddRandomPair(pddl_homomorphic_task_reduce_t *r,
                                            int preserve_goals)
{
    pddl_homomorphic_task_method_t *m = methodNew(METHOD_RANDOM_PAIR);
    m->arg_preserve_goals = preserve_goals;
    pddlListAppend(&r->method, &m->conn);
}

void pddlHomomorphicTaskReduceAddGaifman(pddl_homomorphic_task_reduce_t *r,
                                         int preserve_goals)
{
    pddl_homomorphic_task_method_t *m = methodNew(METHOD_GAIFMAN);
    m->arg_preserve_goals = preserve_goals;
    pddlListAppend(&r->method, &m->conn);
}

void pddlHomomorphicTaskReduceAddRPG(pddl_homomorphic_task_reduce_t *r,
                                     int preserve_goals,
                                     int max_depth)
{
    pddl_homomorphic_task_method_t *m = methodNew(METHOD_RPG);
    m->arg_preserve_goals = preserve_goals;
    m->arg_max_depth = max_depth;
    pddlListAppend(&r->method, &m->conn);
}

void pddlHomomorphicTaskReduceAddRelaxedEndomorphism(
            pddl_homomorphic_task_reduce_t *r,
            const pddl_endomorphism_config_t *cfg)
{
    pddl_homomorphic_task_method_t *m = methodNew(METHOD_ENDOMORPHISM);
    m->arg_endomorphism_cfg = *cfg;
    pddlListAppend(&r->method, &m->conn);
}

int pddlHomomorphicTaskReduce(pddl_homomorphic_task_reduce_t *r,
                              pddl_homomorphic_task_t *h,
                              pddl_err_t *err)
{
    while (h->task.obj.obj_size > r->target_obj_size){
        int init_obj_size = h->task.obj.obj_size;
        pddl_homomorphic_task_method_t *m;
        PDDL_LIST_FOR_EACH_ENTRY(&r->method, pddl_homomorphic_task_method_t, m, conn){
            if (h->task.obj.obj_size <= r->target_obj_size)
                return 0;
            int ret = methodRun(m, h, err);
            if (ret < 0)
                PDDL_TRACE_RET(err, ret);
        }

        if (h->task.obj.obj_size == init_obj_size)
            return 0;
    }
    return 0;
}
