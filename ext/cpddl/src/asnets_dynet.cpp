/***
 * Copyright (c)2022 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

#include "internal.h"
#include "sqlite3.h"
#include "toml.h"
#include "pddl/asnets.h"
#include "pddl/asnets_task.h"
#include "pddl/asnets_train_data.h"
#include "pddl/sha256.h"
#include "pddl/pddl_file.h"

#ifdef PDDL_DYNET
#include <dynet/dynet.h>
#include <dynet/expr.h>
#include <dynet/training.h>
#include <dynet/param-init.h>

static const float SMALL_CONST = 1E-6f;
static const float MIN_ACTIVATION_VALUE = -1.f;

void pddlASNetsConfigLog(const pddl_asnets_config_t *cfg, pddl_err_t *err)
{
    if (cfg->domain_pddl != NULL)
        LOG(err, "domain_pddl = %{domain_pddl}s", cfg->domain_pddl);
    LOG_CONFIG_INT(cfg, problem_pddl_size, err);
    for (int i = 0; i < cfg->problem_pddl_size; ++i)
        LOG(err, "problem_pddl[%d] = %{problem_pddl}s", i, cfg->problem_pddl[i]);
    LOG_CONFIG_INT(cfg, hidden_dimension, err);
    LOG_CONFIG_INT(cfg, num_layers, err);
    LOG_CONFIG_INT(cfg, random_seed, err);
    LOG_CONFIG_DBL(cfg, weight_decay, err);
    LOG_CONFIG_DBL(cfg, dropout_rate, err);
    LOG_CONFIG_INT(cfg, batch_size, err);
    LOG_CONFIG_INT(cfg, double_batch_size_every_epoch, err);
    LOG_CONFIG_INT(cfg, max_train_epochs, err);
    LOG_CONFIG_INT(cfg, train_steps, err);
    LOG_CONFIG_INT(cfg, policy_rollout_limit, err);
    LOG_CONFIG_DBL(cfg, teacher_timeout, err);
    LOG_CONFIG_DBL(cfg, early_termination_success_rate, err);
    LOG_CONFIG_INT(cfg, early_termination_epochs, err);
    switch (cfg->trainer){
        case PDDL_ASNETS_TRAINER_ASTAR_LMCUT:
            LOG(err, "trainer = astar-lmcut");
            break;
    }
    if (cfg->save_model_prefix != NULL)
        LOG_CONFIG_STR(cfg, save_model_prefix, err);
}

void pddlASNetsConfigInit(pddl_asnets_config_t *cfg)
{
    ZEROIZE(cfg);
    cfg->hidden_dimension = 16;
    cfg->num_layers = 2;
    cfg->random_seed = 6961;
    cfg->weight_decay = 2E-4;
    cfg->dropout_rate = 0.1;
    cfg->batch_size = 64;
    cfg->double_batch_size_every_epoch = 0;
    cfg->max_train_epochs = 300;
    cfg->train_steps = 700;
    cfg->policy_rollout_limit = 1000;
    cfg->teacher_timeout = 10.f;
    cfg->early_termination_success_rate = 0.999;
    cfg->early_termination_epochs = 20;
    cfg->trainer = PDDL_ASNETS_TRAINER_ASTAR_LMCUT;
    cfg->save_model_prefix = NULL;
}

void pddlASNetsConfigInitCopy(pddl_asnets_config_t *dst,
                              const pddl_asnets_config_t *src)
{
    *dst = *src;
    if (src->domain_pddl != NULL)
        dst->domain_pddl = STRDUP(src->domain_pddl);

    if (dst->problem_pddl_size > 0){
        dst->problem_pddl = ALLOC_ARR(char *, dst->problem_pddl_size);
        for (int i = 0; i < dst->problem_pddl_size; ++i)
            dst->problem_pddl[i] = STRDUP(src->problem_pddl[i]);
    }
}

#define TOML_INT(K) \
    do { \
        if (pddl_toml_key_exists(c, #K)){ \
            pddl_toml_datum_t d = pddl_toml_int_in(c, #K); \
            if (!d.ok){ \
                pddl_toml_free(top); \
                ERR_RET(err, -1, #K " must be int"); \
            } \
            cfg->K = d.u.i; \
        } \
    } while (0)

#define TOML_FLT(K) \
    do { \
        if (pddl_toml_key_exists(c, #K)){ \
            pddl_toml_datum_t d = pddl_toml_double_in(c, #K); \
            if (!d.ok){ \
                pddl_toml_free(top); \
                ERR_RET(err, -1, #K " must be float"); \
            } \
            cfg->K = d.u.d; \
        } \
    } while (0)

int pddlASNetsConfigInitFromFile(pddl_asnets_config_t *cfg,
                                 const char *filename,
                                 pddl_err_t *err)
{
    pddlASNetsConfigInit(cfg);

    FILE *fin = fopen(filename, "r");
    if (fin == NULL)
        ERR_RET(err, -1, "Could not open file %s", filename);

    pddl_toml_table_t *top = pddl_toml_parse_file(fin, err);
    fclose(fin);
    if (top == NULL){
        TRACE_RET(err, -1);
    }

    pddl_toml_table_t *c = pddl_toml_table_in(top, "asnets");
    if (c == NULL){
        pddl_toml_free(top);
        ERR_RET(err, -1, "No [asnets] section in the configuration file.");
    }

    char *root = NULL;
    if (pddl_toml_key_exists(c, "root")){
        pddl_toml_datum_t d = pddl_toml_string_in(c, "root");
        if (!d.ok){
            pddl_toml_free(top);
            ERR_RET(err, -1, "root must be string");
        }
        root = d.u.s;
        if (strcmp(root, "__PWD__") == 0){
            FREE(root);
            root = pddlDirname(filename);
        }
    }

    if (pddl_toml_key_exists(c, "domain")){
        pddl_toml_datum_t d = pddl_toml_string_in(c, "domain");
        if (!d.ok){
            pddl_toml_free(top);
            ERR_RET(err, -1, "domain must be string");
        }
        if (root != NULL){
            char *fn = ALLOC_ARR(char, strlen(root) + strlen(d.u.s) + 2);
            sprintf(fn, "%s/%s", root, d.u.s);
            pddlASNetsConfigSetDomain(cfg, fn);
            FREE(fn);
        }else{
            pddlASNetsConfigSetDomain(cfg, d.u.s);
        }
        FREE(d.u.s);
    }

    if (pddl_toml_key_exists(c, "problems")){
        const pddl_toml_array_t *arr = pddl_toml_array_in(c, "problems");
        if (arr == NULL){
            pddl_toml_free(top);
            ERR_RET(err, -1, "problems must be array");
        }
        int size = pddl_toml_array_nelem(arr);
        for (int i = 0; i < size; ++i){
            pddl_toml_datum_t d = pddl_toml_string_at(arr, i);
            if (!d.ok){
                pddl_toml_free(top);
                ERR_RET(err, -1, "Each element of problems must be string");
            }
            if (root != NULL){
                char *fn = ALLOC_ARR(char, strlen(root) + strlen(d.u.s) + 2);
                sprintf(fn, "%s/%s", root, d.u.s);
                if (pddlIsFile(fn)){
                    pddlASNetsConfigAddProblem(cfg, fn);
                }else{
                    int len;
                    char **files = pddlListDirPDDLFiles(fn, &len, err);
                    if (files == NULL){
                        FREE(fn);
                        TRACE_RET(err, -1);
                    }

                    for (int i = 0; i < len; ++i){
                        if (strstr(files[i], "domain") != NULL){
                            FREE(files[i]);
                            continue;
                        }
                        if (pddlIsFile(files[i]))
                            pddlASNetsConfigAddProblem(cfg, files[i]);
                        FREE(files[i]);
                    }
                    FREE(files);
                }
                FREE(fn);
            }else{
                pddlASNetsConfigAddProblem(cfg, d.u.s);
            }
            FREE(d.u.s);
        }
    }

    if (root != NULL)
        FREE(root);

    TOML_INT(hidden_dimension);
    TOML_INT(num_layers);
    TOML_INT(random_seed);
    TOML_FLT(weight_decay);
    TOML_FLT(dropout_rate);
    TOML_INT(batch_size);
    TOML_INT(double_batch_size_every_epoch);
    TOML_INT(max_train_epochs);
    TOML_INT(train_steps);
    TOML_INT(policy_rollout_limit);
    TOML_FLT(teacher_timeout);
    TOML_FLT(early_termination_success_rate);
    TOML_INT(early_termination_epochs);

    pddl_toml_free(top);
    return 0;
}

void pddlASNetsConfigFree(pddl_asnets_config_t *cfg)
{
    if (cfg->domain_pddl != NULL)
        FREE(cfg->domain_pddl);
    for (int i = 0; i < cfg->problem_pddl_size; ++i)
        FREE(cfg->problem_pddl[i]);
    if (cfg->problem_pddl != NULL)
        FREE(cfg->problem_pddl);
}

void pddlASNetsConfigSetDomain(pddl_asnets_config_t *cfg, const char *fn)
{
    if (cfg->domain_pddl != NULL)
        FREE(cfg->domain_pddl);
    cfg->domain_pddl = STRDUP(fn);
}

void pddlASNetsConfigAddProblem(pddl_asnets_config_t *cfg, const char *fn)
{
    cfg->problem_pddl = REALLOC_ARR(cfg->problem_pddl, char *,
                                    cfg->problem_pddl_size + 1);
    cfg->problem_pddl[cfg->problem_pddl_size++] = STRDUP(fn);
}

void pddlASNetsConfigWrite(const pddl_asnets_config_t *cfg, FILE *fout)
{
    fprintf(fout, "[asnets]\n");
    if (cfg->domain_pddl == NULL){
        fprintf(fout, "#\n");
        fprintf(fout, "# The following defines the input planning tasks:\n");
        fprintf(fout, "#\n");
        fprintf(fout, "# root = \"__PWD__\"\n");
        fprintf(fout, "# domain = \"domain.pddl\"\n");
        fprintf(fout, "# problems = [\"prob1.pddl\", \"prob2.pddl\"]\n");
    }else{
        fprintf(fout, "domain = \"%s\"\n", cfg->domain_pddl);
        fprintf(fout, "problems = [\n");
        for (int i = 0; i < cfg->problem_pddl_size; ++i)
            fprintf(fout, "    \"%s\",\n", cfg->problem_pddl[i]);
        fprintf(fout, "]\n");
    }
    fprintf(fout, "hidden_dimension = %d\n", cfg->hidden_dimension);
    fprintf(fout, "num_layers = %d\n", cfg->num_layers);
    fprintf(fout, "random_seed = %d\n", cfg->random_seed);
    fprintf(fout, "weight_decay = %f\n", cfg->weight_decay);
    fprintf(fout, "dropout_rate = %f\n", cfg->dropout_rate);
    fprintf(fout, "batch_size = %d\n", cfg->batch_size);
    fprintf(fout, "double_batch_size_every_epoch = %d\n",
            cfg->double_batch_size_every_epoch);
    fprintf(fout, "max_train_epochs = %d\n", cfg->max_train_epochs);
    fprintf(fout, "train_steps = %d\n", cfg->train_steps);
    fprintf(fout, "policy_rollout_limit = %d\n", cfg->policy_rollout_limit);
    fprintf(fout, "teacher_timeout = %f\n", cfg->teacher_timeout);
    fprintf(fout, "early_termination_success_rate = %f\n",
            cfg->early_termination_success_rate);
    fprintf(fout, "early_termination_epochs = %d\n",
            cfg->early_termination_epochs);
}

static dynet::Expression poolMax(const std::vector<dynet::Expression> &in)
{
    if (in.size() == 1)
        return in[0];

    dynet::Expression mat = dynet::concatenate(in, 1);
    return dynet::max_dim(mat, 1);
}

static dynet::Expression maskedSoftmax(dynet::ComputationGraph &cg,
                                       const dynet::Expression &in,
                                       const dynet::Expression &mask)
{
    // Subtract maximum for numerical stability
    dynet::Expression sm = in - dynet::max_dim(in);

    // Compute exponentials
    sm = dynet::exp(sm);

    // Multiply by the mask
    sm = dynet::cmult(sm, mask);

    // Compute sum and clip it so that we don't divide by zero
    dynet::Dim min_sum_dim({1}, sm.dim().batch_elems());
    dynet::Expression min_sum = dynet::constant(cg, min_sum_dim, SMALL_CONST);
    dynet::Expression sum = dynet::max(dynet::sum_rows(sm), min_sum);

    // Normalize each element
    sm = dynet::cdiv(sm, sum);

    return sm;
}

static dynet::Expression crossEntropyLoss(dynet::ComputationGraph &cg,
                                          dynet::Expression output,
                                          dynet::Expression labels)
{
    dynet::Expression o1 = 1 - output;
    dynet::Expression o2 = output;

    // Avoid log(0)
    dynet::Expression small_const = dynet::constant(cg, o1.dim(), SMALL_CONST);
    o1 = dynet::max(o1, small_const);
    o2 = dynet::max(o2, small_const);

    // (1 - y) * log (1 - \pi)
    dynet::Expression e = dynet::cmult(1 - labels, dynet::log(o1));
    // y * log(\pi)
    e = e + dynet::cmult(labels, dynet::log(o2));

    e = dynet::sum_elems(e);
    e = dynet::sum_batches(e);
    e = -e;
    return e;
}


struct ActionModule {
    int hidden_dim;
    int related_props;
    int layer;
    bool is_output;
    int input_vec_size;
    int output_dim;
    dynet::Parameter W;
    dynet::Parameter bias;

    ActionModule(const ActionModule&) = delete;

    // TODO: landmarks/...
    ActionModule(int hidden_dimension,
                 int num_related_propositions,
                 int layer,
                 bool is_output,
                 dynet::ParameterCollection &model)
        : hidden_dim(hidden_dimension),
          related_props(num_related_propositions),
          layer(layer),
          is_output(is_output)
    {
        if (layer == 0){
            // input state
            input_vec_size = related_props;
            // goal specification
            input_vec_size += related_props;
            // applicability of the action
            input_vec_size += 1;

        }else{
            // Related propositions
            input_vec_size = related_props * hidden_dim;
            // Skip connection
            input_vec_size += hidden_dim;
        }

        if (is_output){
            output_dim = 1;
        }else{
            output_dim = hidden_dim;
        }

        std::vector<long> dim_W(2);
        dim_W[0] = output_dim;
        dim_W[1] = input_vec_size;
        W = model.add_parameters(dynet::Dim(dim_W), dynet::ParameterInitNormal());

        std::vector<long> dim_bias(1);
        dim_bias[0] = output_dim;
        bias = model.add_parameters(dynet::Dim(dim_bias), dynet::ParameterInitNormal());
    }

    dynet::Expression expr(dynet::ComputationGraph &cg,
                           const std::vector<dynet::Expression> &input) const
    {
        dynet::Expression w = dynet::parameter(cg, W);
        dynet::Expression b = dynet::parameter(cg, bias);
        dynet::Expression u = dynet::concatenate(input);
        dynet::Expression e = (w * u) + b;
        if (is_output)
            return e;
        return dynet::elu(e);
    }

    dynet::Expression exprInput(dynet::ComputationGraph &cg,
                                const std::vector<dynet::Expression> &input_state,
                                const std::vector<dynet::Expression> &input_goal,
                                const dynet::Expression &input_applicable) const
    {
        ASSERT_RUNTIME(layer == 0);
        std::vector<dynet::Expression> input;
        input.insert(input.end(), input_state.begin(), input_state.end());
        input.insert(input.end(), input_goal.begin(), input_goal.end());
        input.push_back(input_applicable);
        return expr(cg, input);
    }
};

struct PropositionModule {
    int hidden_dim;
    int related_acts;
    int layer;
    int input_vec_size;
    dynet::Parameter W;
    dynet::Parameter bias;

    PropositionModule(const PropositionModule&) = delete;

    PropositionModule(int hidden_dimension,
                      int num_related_actions,
                      int layer,
                      dynet::ParameterCollection &model)
        : hidden_dim(hidden_dimension),
          related_acts(num_related_actions),
          layer(layer)
    {
        // Related actions
        input_vec_size = related_acts * hidden_dim;
        if (layer > 0){
            // Skip connection
            input_vec_size += hidden_dim;
        }

        std::vector<long> dim_W(2);
        dim_W[0] = hidden_dim;
        dim_W[1] = input_vec_size;
        W = model.add_parameters(dynet::Dim(dim_W), dynet::ParameterInitNormal());

        std::vector<long> dim_bias(1);
        dim_bias[0] = hidden_dim;
        bias = model.add_parameters(dynet::Dim(dim_bias), dynet::ParameterInitNormal());
    }

    dynet::Expression expr(dynet::ComputationGraph &cg,
                           const std::vector<std::vector<dynet::Expression>> &input) const 
    {
        std::vector<dynet::Expression> pooled_input(input.size());
        for (size_t i = 0; i < input.size(); ++i)
            pooled_input[i] = poolMax(input[i]);

        dynet::Expression w = dynet::parameter(cg, W);
        dynet::Expression b = dynet::parameter(cg, bias);
        dynet::Expression u = dynet::concatenate(pooled_input);
        return dynet::elu((w * u) + b);
    }
};

struct ModelParameters {
    int num_layers;
    int hidden_dim;
    std::vector<std::vector<ActionModule *>> action;
    std::vector<std::vector<PropositionModule *>> prop;
    dynet::ParameterCollection model;

    ModelParameters(const ModelParameters &) = delete;

    ModelParameters(int hidden_dimension,
                    int num_layers,
                    const pddl_asnets_lifted_task_t *task)
        : num_layers(num_layers),
          hidden_dim(hidden_dimension)
    {
        action.resize(num_layers + 1);
        prop.resize(num_layers);

        for (int layer = 0; layer < num_layers; ++layer){
            for (int aid = 0; aid < task->action_size; ++aid){
                ActionModule *am;
                am = new ActionModule(hidden_dimension,
                                      task->action[aid].related_atom_size,
                                      layer, false, model);
                action[layer].push_back(am);
            }

            for (int pid = 0; pid < task->pred_size; ++pid){
                ASSERT_RUNTIME(pid != task->pddl.pred.eq_pred
                                || task->pred[pid].related_action_size == 0);
                PropositionModule *pm;
                pm = new PropositionModule(hidden_dimension,
                                           task->pred[pid].related_action_size,
                                           layer, model);
                prop[layer].push_back(pm);
            }
        }

        for (int aid = 0; aid < task->action_size; ++aid){
            ActionModule *am;
            am = new ActionModule(hidden_dimension,
                                  task->action[aid].related_atom_size,
                                  num_layers, true, model);
            action[num_layers].push_back(am);
        }

        ASSERT_RUNTIME(num_layers == (int)action.size() - 1);
        ASSERT_RUNTIME(num_layers == (int)prop.size());
    }

    ~ModelParameters()
    {
        for (size_t i = 0; i < action.size(); ++i){
            for (size_t j = 0; j < action[i].size(); ++j)
                delete action[i][j];
        }
        for (size_t i = 0; i < prop.size(); ++i){
            for (size_t j = 0; j < prop[i].size(); ++j)
                delete prop[i][j];
        }
    }

    void dumpDebug() const
    {
        for (size_t layer = 0; layer < action.size(); ++layer){
            for (size_t ai = 0; ai < action[layer].size(); ++ai){
                ActionModule *m = action[layer][ai];
                {
                    dynet::Tensor *t = m->W.values();
                    std::vector<float> v = dynet::as_vector(*t);
                    std::cerr << "Action.W " << layer << " " << ai << std::endl;
                    for (float x : v)
                        std::cerr << " " << x;
                    std::cerr << std::endl;
                }

                {
                    dynet::Tensor *t = m->bias.values();
                    std::vector<float> v = dynet::as_vector(*t);
                    std::cerr << "Action.bias " << layer << " " << ai << std::endl;
                    for (float x : v)
                        std::cerr << " " << x;
                    std::cerr << std::endl;
                }
            }
        }

        for (size_t layer = 0; layer < prop.size(); ++layer){
            for (size_t pi = 0; pi < prop[layer].size(); ++pi){
                PropositionModule *m = prop[layer][pi];
                {
                    dynet::Tensor *t = m->W.values();
                    std::vector<float> v = dynet::as_vector(*t);
                    std::cerr << "Proposition.W " << layer << " " << pi << std::endl;
                    for (float x : v)
                        std::cerr << " " << x;
                    std::cerr << std::endl;
                }

                {
                    dynet::Tensor *t = m->bias.values();
                    std::vector<float> v = dynet::as_vector(*t);
                    std::cerr << "Action.bias " << layer << " " << pi << std::endl;
                    for (float x : v)
                        std::cerr << " " << x;
                    std::cerr << std::endl;
                }
            }
        }
    }
};


static void _actionLayer(const pddl_asnets_ground_task_t *g,
                         const ModelParameters &model,
                         dynet::ComputationGraph &cg,
                         int layer,
                         const std::vector<dynet::Expression> &prop_layer,
                         const std::vector<dynet::Expression> &prev_action_layer,
                         std::vector<dynet::Expression> &action_layer,
                         float dropout_rate)
{
    for (int op_id = 0; op_id < g->op_size; ++op_id){
        std::vector<dynet::Expression> in;
        for (int i = 0; i < g->op[op_id].related_fact_size; ++i){
            int fact_id = g->op[op_id].related_fact[i];
            in.push_back(prop_layer[fact_id]);
        }
        in.push_back(prev_action_layer[op_id]);
        int action_id = g->op[op_id].action->action_id;
        ActionModule *am = model.action[layer][action_id];
        dynet::Expression e = am->expr(cg, in);
        if (dropout_rate > 0.f && layer != model.num_layers){
            e = dynet::dropout(e, dropout_rate);
        }
        action_layer.push_back(e);
    }
}

static void _propLayer(const pddl_asnets_ground_task_t *g,
                       const ModelParameters &model,
                       dynet::ComputationGraph &cg,
                       int layer,
                       const std::vector<dynet::Expression> &action_layer,
                       const std::vector<dynet::Expression> *prev_prop_layer,
                       std::vector<dynet::Expression> &prop_layer,
                       float dropout_rate)
{
    dynet::Expression const_min;
    bool have_const_min = false;

    for (int fact_id = 0; fact_id < g->fact_size; ++fact_id){
        int pred_id = g->fact[fact_id].pred->pred_id;
        PropositionModule *pm = model.prop[layer][pred_id];

        std::vector<std::vector<dynet::Expression>> input;
        int input_size = g->fact[fact_id].related_op_size;
        if (prev_prop_layer != NULL)
            input_size += 1;
        input.resize(input_size);
        for (int ri = 0; ri < g->fact[fact_id].related_op_size; ++ri){
            int op_id;
            PDDL_IARR_FOR_EACH(g->fact[fact_id].related_op + ri, op_id){
                input[ri].push_back(action_layer[op_id]);
            }

            if (input[ri].size() == 0){
                // This means there is no operator having this fact in its
                // precondition or effect at position ri.
                // So, we set the input to the minimum value of the
                // activation function.
                if (!have_const_min){
                    std::vector<long> d(1, model.hidden_dim);
                    dynet::Dim const_min_dim(d);
                    const_min = dynet::constant(cg, const_min_dim, MIN_ACTIVATION_VALUE);
                    have_const_min = true;
                }
                input[ri].push_back(const_min);
            }
        }
        if (prev_prop_layer != NULL)
            input[input_size - 1].push_back((*prev_prop_layer)[fact_id]);

        dynet::Expression e = pm->expr(cg, input);
        if (dropout_rate > 0.f){
            e = dynet::dropout(e, dropout_rate);
        }
        prop_layer.push_back(e);
    }
}

static dynet::Expression asnetsExpr(const pddl_asnets_ground_task_t *g,
                                    const ModelParameters &model,
                                    dynet::ComputationGraph &cg,
                                    dynet::Expression input_state,
                                    dynet::Expression input_goal_condition,
                                    dynet::Expression input_applicable_ops,
                                    float dropout_rate)
{
    std::vector<std::vector<dynet::Expression>> action_layer;
    action_layer.resize(model.num_layers + 1);
    std::vector<std::vector<dynet::Expression>> prop_layer;
    prop_layer.resize(model.num_layers);

    int layer = 0;
    // First action layer needs to be connected to inputs
    for (int op_id = 0; op_id < g->op_size; ++op_id){
        std::vector<dynet::Expression> in_state;
        std::vector<dynet::Expression> in_goal;
        dynet::Expression in_applicable;
        for (int i = 0; i < g->op[op_id].related_fact_size; ++i){
            int fact_id = g->op[op_id].related_fact[i];
            in_state.push_back(dynet::pick(input_state, fact_id));
            in_goal.push_back(dynet::pick(input_goal_condition, fact_id));
            in_applicable = dynet::pick(input_applicable_ops, op_id);
        }
        int action_id = g->op[op_id].action->action_id;
        ActionModule *am = model.action[layer][action_id];
        dynet::Expression e = am->exprInput(cg, in_state, in_goal, in_applicable);
        action_layer[layer].push_back(e);
    }

    for (; layer < model.num_layers; ++layer){
        const std::vector<dynet::Expression> *prev_prop_layer = NULL;
        if (layer > 0)
            prev_prop_layer = &prop_layer[layer - 1];
        _propLayer(g, model, cg, layer, action_layer[layer],
                   prev_prop_layer, prop_layer[layer], dropout_rate);

        _actionLayer(g, model, cg, layer + 1, prop_layer[layer],
                     action_layer[layer], action_layer[layer + 1],
                     dropout_rate);
    }

    dynet::Expression out = dynet::concatenate(action_layer[layer]);

    return maskedSoftmax(cg, out, input_applicable_ops);
}

static void setApplicableOpsVector(const pddl_asnets_ground_task_t *task,
                                   const int *state,
                                   std::vector<float> &applicable_ops)
{
    applicable_ops.resize(task->strips.op.op_size);
    for (size_t i = 0; i < applicable_ops.size(); ++i)
        applicable_ops[i] = 0;

    PDDL_ISET(ops);
    pddlASNetsGroundTaskFDRApplicableOps(task, state, &ops);
    int op_id;
    PDDL_ISET_FOR_EACH(&ops, op_id)
        applicable_ops[op_id] = 1;
    pddlISetFree(&ops);
}

static void setStateVector(const pddl_asnets_ground_task_t *task,
                           const int *s,
                           std::vector<float> &state,
                           std::vector<float> &applicable_ops)
{
    state.resize(task->strips.fact.fact_size);
    for (size_t i = 0; i < state.size(); ++i)
        state[i] = 0;

    PDDL_ISET(strips_state);
    pddlASNetsGroundTaskFDRStateToStrips(task, s, &strips_state);
    int fact_id;
    PDDL_ISET_FOR_EACH(&strips_state, fact_id)
        state[fact_id] = 1;
    pddlISetFree(&strips_state);

    setApplicableOpsVector(task, s, applicable_ops);
}

static void setGoalVector(const pddl_asnets_ground_task_t *task,
                          std::vector<float> &goal)
{
    goal.resize(task->strips.fact.fact_size);
    for (size_t i = 0; i < goal.size(); ++i)
        goal[i] = 0;

    PDDL_ISET(strips_g);
    pddlASNetsGroundTaskFDRGoal(task, &strips_g);
    int fact_id;
    PDDL_ISET_FOR_EACH(&strips_g, fact_id)
        goal[fact_id] = 1;
    pddlISetFree(&strips_g);
}


static int runPolicy(const pddl_asnets_ground_task_t *task,
                     const ModelParameters &params,
                     dynet::ComputationGraph &cg,
                     const int *in_state,
                     int *out_state)
{
    std::vector<float> state;
    std::vector<float> goal;
    std::vector<float> applicable_ops;

    setGoalVector(task, goal);
    setStateVector(task, in_state, state, applicable_ops);

    cg.clear();

    std::vector<long> dim(1);
    dim[0] = state.size();
    dynet::Expression e_state = dynet::input(cg, dynet::Dim(dim), state);
    dynet::Expression e_goal = dynet::input(cg, dynet::Dim(dim), goal);

    dim[0] = applicable_ops.size();
    dynet::Expression e_applicable_ops = dynet::input(cg, dynet::Dim(dim), applicable_ops);
    dynet::Expression e_output = asnetsExpr(task, params, cg, e_state, e_goal,
                                            e_applicable_ops, -1);

    std::vector<float> out = dynet::as_vector(cg.forward(e_output));
    ASSERT_RUNTIME((int)out.size() == task->strips.op.op_size);

    int best_op_id = -1;
    float best_value = -1;
    for (size_t op_id = 0; op_id < out.size(); ++op_id){
        ASSERT(out[op_id] >= 0.f);
        // Skip operators that are not applicable
        if (applicable_ops[op_id] < .5)
            continue;
        if (out[op_id] > best_value){
            best_op_id = op_id;
            best_value = out[op_id];
        }
    }

    if (out_state != NULL && best_op_id >= 0)
        pddlASNetsGroundTaskFDRApplyOp(task, in_state, best_op_id, out_state);

    return best_op_id;
}

struct pddl_asnets_train_stats {
    int max_epochs;
    int epoch;
    int max_train_steps;
    int train_step;
    float overall_loss;
    float success_rate;
    int num_samples;
    int consecutive_successful_epochs;
};
typedef struct pddl_asnets_train_stats pddl_asnets_train_stats_t;

struct pddl_asnets {
    pddl_asnets_config_t cfg;
    pddl_asnets_lifted_task_t lifted_task;
    dynet::ComputationGraph *cg;
    dynet::Trainer *trainer;
    ModelParameters *params;
    pddl_asnets_ground_task_t *ground_task;
    int ground_task_size;

    pddl_asnets_train_stats_t train_stats;
};

struct ASNetsTrainMiniBatchTask {
    int task_id;
    int size;
    int fact_size;
    int op_size;
    std::vector<float> state;
    std::vector<float> goal;
    std::vector<float> applicable_ops;
    std::vector<unsigned int> selected_op;
    dynet::Expression e_state;
    dynet::Expression e_goal;
    dynet::Expression e_applicable_ops;
    dynet::Expression e_output;

    ASNetsTrainMiniBatchTask()
        : task_id(-1), size(0), fact_size(0), op_size(0)
    {}

    void add(std::vector<float> &in_state,
             std::vector<float> &in_applicable_ops,
             int in_selected_op)
    {
        state.insert(state.end(), in_state.begin(), in_state.end());
        applicable_ops.insert(applicable_ops.end(),
                              in_applicable_ops.begin(),
                              in_applicable_ops.end());

        ASSERT(in_selected_op >= 0 && in_selected_op < op_size);
        selected_op.push_back(in_selected_op);
        ++size;
    }

    void createInputs(dynet::ComputationGraph &cg)
    {
        if (size == 0)
            return;
        ASSERT_RUNTIME((int)state.size() == size * fact_size);
        ASSERT_RUNTIME((int)applicable_ops.size() == size * op_size);
        ASSERT_RUNTIME((int)selected_op.size() == size);
        ASSERT_RUNTIME((int)goal.size() == fact_size);

        std::vector<long> dim(1);
        dim[0] = fact_size;
        e_state = dynet::input(cg, dynet::Dim(dim, size), state);

        std::vector<float> g;
        for (int i = 0; i < size; ++i)
            g.insert(g.end(), goal.begin(), goal.end());
        e_goal = dynet::input(cg, dynet::Dim(dim, size), g);

        dim[0] = op_size;
        e_applicable_ops = dynet::input(cg, dynet::Dim(dim, size),
                                        applicable_ops);
        e_output = dynet::one_hot(cg, op_size, selected_op);
    }
};

struct ASNetsTrainMiniBatch {
    std::vector<ASNetsTrainMiniBatchTask> batch;

    ASNetsTrainMiniBatch(const pddl_asnets_t *a,
                         const pddl_asnets_train_data_t *data,
                         int minibatch_size)
    {
        if (minibatch_size < 0)
            minibatch_size = data->sample_size;
        minibatch_size = PDDL_MIN(minibatch_size, data->sample_size);
        batch.resize(a->ground_task_size);
        for (int i = 0; i < a->ground_task_size; ++i){
            batch[i].task_id = i;
            batch[i].fact_size = a->ground_task[i].strips.fact.fact_size;
            batch[i].op_size = a->ground_task[i].strips.op.op_size;
            setGoalVector(a->ground_task + i, batch[i].goal);
        }

        for (int sample = 0; sample < minibatch_size; ++sample){
            int task_id, selected_op;
            const int *fdr_state;
            pddlASNetsTrainDataGetSample(data, sample, &task_id,
                                         &selected_op, NULL, &fdr_state);
            std::vector<float> state, applicable_ops;
            setStateVector(a->ground_task + task_id, fdr_state,
                           state, applicable_ops);
            batch[task_id].add(state, applicable_ops, selected_op);
        }
    }

    void createInputs(dynet::ComputationGraph &cg)
    {
        for (size_t task_id = 0; task_id < batch.size(); ++task_id){
            if (batch[task_id].size == 0)
                continue;
            batch[task_id].createInputs(cg);
        }
    }
};


static int policyRollout(pddl_asnets_t *a,
                         const pddl_asnets_ground_task_t *task,
                         pddl_fdr_state_pool_t *states,
                         pddl_iarr_t *trace,
                         pddl_err_t *err)
{
    int ret = 0;
    int *state = ALLOC_ARR(int, task->fdr.var.var_size);
    int *state2 = ALLOC_ARR(int, task->fdr.var.var_size);

    // Start in the initial state
    pddl_state_id_t state_id = pddlFDRStatePoolInsert(states, task->fdr.init);
    for (int step = 0; step < a->cfg.policy_rollout_limit; ++step){
        // get the last reached state
        pddlFDRStatePoolGet(states, state_id, state);
        if (pddlFDRPartStateIsConsistentWithState(&task->fdr.goal, state)){
            ret = 1;
            break;
        }

        // Apply policy. If we get -1, it means the state is dead-end,
        // because there are no applicable operators
        int op_id = runPolicy(task, *a->params, *a->cg, state, state2);
        if (op_id < 0){
            break;
        }
        if (trace != NULL)
            pddlIArrAdd(trace, op_id);

        // Insert current state
        pddl_state_id_t prev_state_id = state_id;
        state_id = pddlFDRStatePoolInsert(states, state2);
        // If the new state was already in the pool, then we got a cycle
        if (state_id <= prev_state_id){
            break;
        }
    }

    FREE(state);
    FREE(state2);
    return ret;
}



pddl_asnets_t *pddlASNetsNew(const pddl_asnets_config_t *cfg, pddl_err_t *err)
{
    if (cfg->problem_pddl_size <= 0)
        ERR_RET(err, NULL, "ASNets: At least one problem file is required.");

    CTX(err, "asnets", "ASNets");
    pddl_asnets_t *a = ZALLOC(pddl_asnets_t);
    pddlASNetsConfigInitCopy(&a->cfg, cfg);
    CTX_NO_TIME(err, "cfg", "Cfg");
    pddlASNetsConfigLog(&a->cfg, err);
    CTXEND(err);

    int st;
    st = pddlASNetsLiftedTaskInit(&a->lifted_task, cfg->domain_pddl, err);
    if (st < 0){
        CTXEND(err);
        TRACE_RET(err, NULL);
    }

    a->ground_task_size = cfg->problem_pddl_size;
    a->ground_task = ALLOC_ARR(pddl_asnets_ground_task, a->ground_task_size);
    for (int probi = 0; probi < cfg->problem_pddl_size; ++probi){
        st = pddlASNetsGroundTaskInit(&a->ground_task[probi],
                                      &a->lifted_task,
                                      cfg->domain_pddl,
                                      cfg->problem_pddl[probi],
                                      err);
        if (st < 0){
            pddlASNetsLiftedTaskFree(&a->lifted_task);
            for (int i = 0; i < probi; ++i)
                pddlASNetsGroundTaskFree(&a->ground_task[i]);
            FREE(a->ground_task);
            CTXEND(err);
            TRACE_RET(err, NULL);
        }
    }

    dynet::DynetParams dynet_params;
    dynet_params.autobatch = false;
    //dynet_params.mem_descriptor = "4096";
    //dynet_params.profiling = 10;
    dynet_params.random_seed = a->cfg.random_seed;
    //dynet_params.shared_parameters = true;
    dynet_params.weight_decay = a->cfg.weight_decay;
    dynet::initialize(dynet_params);

    a->params = new ModelParameters(cfg->hidden_dimension,
                                    cfg->num_layers,
                                    &a->lifted_task);

    // TODO: Parametrize
    a->trainer = new dynet::AdamTrainer(a->params->model);
    a->cg = new dynet::ComputationGraph();
#ifdef PDDL_DEBUG
    a->cg->set_check_validity(true);
    a->cg->set_immediate_compute(true);
#endif /* PDDL_DEBUG */

    ZEROIZE(&a->train_stats);
    a->train_stats.max_epochs = a->cfg.max_train_epochs;
    a->train_stats.max_train_steps = a->cfg.train_steps;
    a->train_stats.success_rate = -1.f;
    a->train_stats.overall_loss = -1.f;

    CTXEND(err);
    return a;
}

void pddlASNetsDel(pddl_asnets_t *a)
{
    pddlASNetsLiftedTaskFree(&a->lifted_task);
    for (int i = 0; i < a->ground_task_size; ++i)
        pddlASNetsGroundTaskFree(&a->ground_task[i]);
    FREE(a->ground_task);

    delete a->params;
    if (a->trainer != NULL)
        delete a->trainer;
    if (a->cg != NULL)
        delete a->cg;
    pddlASNetsConfigFree(&a->cfg);
    dynet::cleanup();
}

#define SIG_ACTION_W 0
#define SIG_ACTION_B 1
#define SIG_PROP_W 2
#define SIG_PROP_B 3

static const char sql_create_info[]
    = "DROP TABLE IF EXISTS asnets_info;"
      "CREATE TABLE asnets_info ("
            "parameter TEXT,"
            "int_value INT DEFAULT -1,"
            "flt_value REAL DEFAULT -1.,"
            "str_value TEXT DEFAULT NULL"
      ");";
static const char sql_query_info[]
    = "SELECT int_value, flt_value, str_value"
      " FROM asnets_info WHERE parameter = ?;";

static const char sql_create_weights[]
    = "DROP TABLE IF EXISTS asnets_weights;"
      "CREATE TABLE asnets_weights ("
            "id INT PRIMARY KEY,"
            "layer INT,"
            "sig INT,"
            "name TEXT,"
            "idx INT,"
            "weights BLOB"
      ");";
static const char sql_insert_weights[]
    = "INSERT INTO asnets_weights VALUES(?,?,?,?,?,?);";
static const char sql_query_weights[]
    = "SELECT layer, sig, name, idx, weights"
      " FROM asnets_weights WHERE id = ?;";



struct Info {
    char cpddl_version[128];
    char domain_name[128];
    char domain_pddl[4096];
    char domain_hash[PDDL_SHA256_HASH_STR_SIZE];
    pddl_asnets_config_t cfg;
    pddl_asnets_train_stats_t train_stats;
    // TODO: store the whole domain pddl file?

    Info()
    {
        cpddl_version[0] = '\x0';
        domain_name[0] = '\x0';
        domain_hash[0] = '\x0';
        ZEROIZE(&cfg);
        ZEROIZE(&train_stats);
    }

    Info(const pddl_asnets_t *a)
    {
        strncpy(cpddl_version, pddl_version, sizeof(cpddl_version) - 1);
        strncpy(domain_name, a->lifted_task.pddl.domain_name, sizeof(domain_name) - 1);
        strncpy(domain_pddl, a->lifted_task.pddl.domain_lisp->filename, sizeof(domain_pddl) - 1);
        pddlASNetsLiftedTaskToSHA256(&a->lifted_task, domain_hash);
        cfg = a->cfg;
        train_stats = a->train_stats;
    }

    int checkLoadedInfo(const Info &o, pddl_err_t *err)
    {
        if (cfg.hidden_dimension != o.cfg.hidden_dimension){
            ERR_RET(err, 0, "Hidden dimensions don't match. asnets: %d, loaded: %d",
                    cfg.hidden_dimension, o.cfg.hidden_dimension);
        }

        if (cfg.num_layers != o.cfg.num_layers){
            ERR_RET(err, 0, "Number of layers don't match. asnets: %d, loaded: %d",
                    cfg.num_layers, o.cfg.num_layers);
        }

        if (strcmp(domain_name, o.domain_name) != 0){
            ERR_RET(err, 0, "Domain names differ. asnets: %s, loaded: %s",
                    domain_name, o.domain_name);
        }

        if (strcmp(domain_hash, o.domain_hash) != 0){
            ERR_RET(err, 0, "Domain hash differ. asnets: %s, loaded: %s",
                    domain_hash, o.domain_hash);
        }

        return 1;
    }

    int create(pddl_sqlite3 *db, pddl_err_t *err)
    {
        char *errmsg = NULL;
        int ret = pddl_sqlite3_exec(db, sql_create_info, NULL, NULL, &errmsg);
        if (ret != SQLITE_OK){
            ERR(err, "Sqlite Error: %s", errmsg);
            pddl_sqlite3_free(errmsg);
            return -1;
        }
        return 0;
    }

    int _sqlInsertInfo(pddl_sqlite3 *db,
                       const char *param,
                       int int_val,
                       float float_val,
                       const char *str_val,
                       pddl_err_t *err)
    {
        char *query = ALLOC_ARR(char, 1024 * 1024);
        int query_size = 0;
        query_size = sprintf(query, "INSERT INTO asnets_info (parameter");
        if (str_val != NULL){
            query_size += sprintf(query + query_size, ",str_value)");
            query_size += sprintf(query + query_size, " VALUES('%s'", param);
            query_size += sprintf(query + query_size, ",'%s');", str_val);

        }else if (float_val > -FLT_MAX){
            query_size += sprintf(query + query_size, ",flt_value)");
            query_size += sprintf(query + query_size, " VALUES('%s'", param);
            query_size += sprintf(query + query_size, ",%f);", float_val);

        }else{
            query_size += sprintf(query + query_size, ",int_value)");
            query_size += sprintf(query + query_size, " VALUES('%s'", param);
            query_size += sprintf(query + query_size, ",%d);", int_val);
        }

        char *errmsg = NULL;
        int ret = pddl_sqlite3_exec(db, query, NULL, NULL, &errmsg);
        FREE(query);
        if (ret != SQLITE_OK){
            ERR(err, "Sqlite Error: %s", errmsg);
            pddl_sqlite3_free(errmsg);
            return -1;
        }
        return 0;
    }

#define SQL_INS_INFO_STR(P, V) \
    _sqlInsertInfo(db, P, INT_MIN, -FLT_MAX, V, err)
#define SQL_INS_INFO_INT(P, V) \
    _sqlInsertInfo(db, P, V, -FLT_MAX, NULL, err)
#define SQL_INS_INFO_FLT(P, V) \
    _sqlInsertInfo(db, P, INT_MIN, V, NULL, err)

    int save(pddl_sqlite3 *db, pddl_err_t *err)
    {
        char *errmsg = NULL;
        if (SQL_INS_INFO_STR("cpddl_version", pddl_version) != 0
                || SQL_INS_INFO_STR("domain_name", domain_name) != 0
                || SQL_INS_INFO_STR("domain_pddl", domain_pddl) != 0
                || SQL_INS_INFO_STR("domain_hash", domain_hash) != 0

                || SQL_INS_INFO_INT("epoch", train_stats.epoch) != 0
                || SQL_INS_INFO_INT("num_samples", train_stats.num_samples) != 0
                || SQL_INS_INFO_FLT("overall_loss", train_stats.overall_loss) != 0
                || SQL_INS_INFO_FLT("success_rate", train_stats.success_rate) != 0

                || SQL_INS_INFO_INT("cfg_hidden_dimension", cfg.hidden_dimension) != 0
                || SQL_INS_INFO_INT("cfg_num_layers", cfg.num_layers) != 0
                || SQL_INS_INFO_INT("cfg_random_seed", cfg.random_seed) != 0
                || SQL_INS_INFO_FLT("cfg_weight_decay", cfg.weight_decay) != 0
                || SQL_INS_INFO_FLT("cfg_dropout_rate", cfg.dropout_rate) != 0
                || SQL_INS_INFO_INT("cfg_batch_size", cfg.batch_size) != 0
                || SQL_INS_INFO_INT("cfg_double_batch_size_every_epoch",
                                    cfg.double_batch_size_every_epoch) != 0
                || SQL_INS_INFO_INT("cfg_max_train_epochs", cfg.max_train_epochs) != 0
                || SQL_INS_INFO_INT("cfg_train_steps", cfg.train_steps) != 0
                || SQL_INS_INFO_INT("cfg_policy_rollout_limit", cfg.policy_rollout_limit) != 0
                || SQL_INS_INFO_FLT("cfg_teacher_timeout", cfg.teacher_timeout) != 0
                || SQL_INS_INFO_FLT("cfg_early_termination_success_rate",
                                    cfg.early_termination_success_rate) != 0
                || SQL_INS_INFO_INT("cfg_early_termination_epochs",
                                    cfg.early_termination_epochs) != 0){
            pddl_sqlite3_free(errmsg);
            TRACE_RET(err, -1);
        }
        return 0;
    }


    int _sqlSelectInfo(pddl_sqlite3 *db,
                       pddl_sqlite3_stmt *stmt,
                       const char *param,
                       int *int_val,
                       float *flt_val,
                       char *str_val,
                       pddl_err_t *err)
    {
        pddl_sqlite3_reset(stmt);
        int ret = pddl_sqlite3_bind_text(stmt, 1, param, -1, SQLITE_STATIC);
        if (ret != SQLITE_OK){
            ERR_RET(err, -1, "Sqlite Error: %s: %s",
                    pddl_sqlite3_errstr(ret), pddl_sqlite3_errmsg(db));
        }

        int found = (ret = pddl_sqlite3_step(stmt)) == SQLITE_ROW;
        if (ret != SQLITE_ROW && ret != SQLITE_DONE){
            ERR_RET(err, -1, "Sqlite Error: %s: %s",
                    pddl_sqlite3_errstr(ret), pddl_sqlite3_errmsg(db));
        }
        if (!found)
            ERR_RET(err, -1, "Parameter %s not found.", param);

        if (int_val != NULL)
            *int_val = pddl_sqlite3_column_int(stmt, 0);
        if (flt_val != NULL)
            *flt_val = pddl_sqlite3_column_double(stmt, 1);
        if (str_val != NULL){
            const unsigned char *v = pddl_sqlite3_column_text(stmt, 2);
            strcpy(str_val, (const char *)v);
        }
        return 0;
    }

    int _sqlSelectInfoInt(pddl_sqlite3 *db,
                          pddl_sqlite3_stmt *stmt,
                          const char *param,
                          pddl_err_t *err)
    {
        int val;
        int ret = _sqlSelectInfo(db, stmt, param, &val, NULL, NULL, err);
        if (ret < 0)
            TRACE_RET(err, INT_MIN);
        return val;

    }

    float _sqlSelectInfoFlt(pddl_sqlite3 *db,
                            pddl_sqlite3_stmt *stmt,
                            const char *param,
                            pddl_err_t *err)
    {
        float val;
        int ret = _sqlSelectInfo(db, stmt, param, NULL, &val, NULL, err);
        if (ret < 0)
            TRACE_RET(err, -FLT_MAX);
        return val;

    }

    int _sqlSelectInfoStr(pddl_sqlite3 *db,
                          pddl_sqlite3_stmt *stmt,
                          const char *param,
                          char *val,
                          pddl_err_t *err)
    {
        int ret = _sqlSelectInfo(db, stmt, param, NULL, NULL, val, err);
        if (ret < 0)
            TRACE_RET(err, -1);
        return 0;
    }

#define SQL_INFO_CFG_INT(N) \
    do { \
        cfg.N = _sqlSelectInfoInt(db, stmt, "cfg_" #N, err); \
        if (cfg.N == INT_MIN){ \
            TRACE_RET(err, -1); \
        }else{ \
            LOG(err, "cfg." #N " = %d", cfg.N); \
        } \
    } while (0)

#define SQL_INFO_CFG_FLT(N) \
    do { \
        cfg.N = _sqlSelectInfoFlt(db, stmt, "cfg_" #N, err); \
        if (cfg.N == -FLT_MAX){ \
            TRACE_RET(err, -1); \
        }else{ \
            LOG(err, "cfg." #N " = %f", cfg.N); \
        } \
    } while (0)

    int load(pddl_sqlite3 *db, pddl_err_t *err)
    {
        pddl_sqlite3_stmt *stmt;
        int ret = pddl_sqlite3_prepare_v2(db, sql_query_info, -1, &stmt, NULL);
        if (ret != SQLITE_OK){
            ERR_RET(err, -1, "Sqlite Error: %s: %s",
                    pddl_sqlite3_errstr(ret), pddl_sqlite3_errmsg(db));
        }

        if (_sqlSelectInfoStr(db, stmt, "cpddl_version", cpddl_version, err) != 0)
            TRACE_RET(err, -1);
        LOG(err, "cpddl version = %s", cpddl_version);
        if (_sqlSelectInfoStr(db, stmt, "domain_name", domain_name, err) != 0)
            TRACE_RET(err, -1);
        LOG(err, "domain name = %s", domain_name);
        if (_sqlSelectInfoStr(db, stmt, "domain_pddl", domain_pddl, err) != 0)
            TRACE_RET(err, -1);
        LOG(err, "domain pddl = %s", domain_pddl);
        if (_sqlSelectInfoStr(db, stmt, "domain_hash", domain_hash, err) != 0)
            TRACE_RET(err, -1);
        LOG(err, "domain hash = %s", domain_hash);

        train_stats.epoch = _sqlSelectInfoInt(db, stmt, "epoch", err);
        if (train_stats.epoch == INT_MIN)
            TRACE_RET(err, -1);
        LOG(err, "train epoch = %d", train_stats.epoch);
        train_stats.num_samples = _sqlSelectInfoInt(db, stmt, "num_samples", err);
        if (train_stats.num_samples == INT_MIN)
            TRACE_RET(err, -1);
        LOG(err, "num samples = %d", train_stats.num_samples);
        train_stats.success_rate = _sqlSelectInfoFlt(db, stmt, "success_rate", err);
        if (train_stats.success_rate == -FLT_MAX)
            TRACE_RET(err, -1);
        LOG(err, "success rate = %f", train_stats.success_rate);
        train_stats.overall_loss = _sqlSelectInfoFlt(db, stmt, "overall_loss", err);
        if (train_stats.overall_loss == -FLT_MAX)
            TRACE_RET(err, -1);
        LOG(err, "overall loss = %f", train_stats.overall_loss);

        SQL_INFO_CFG_INT(hidden_dimension);
        SQL_INFO_CFG_INT(num_layers);
        SQL_INFO_CFG_FLT(weight_decay);
        SQL_INFO_CFG_FLT(dropout_rate);
        SQL_INFO_CFG_INT(random_seed);
        SQL_INFO_CFG_INT(batch_size);
        SQL_INFO_CFG_INT(double_batch_size_every_epoch);
        SQL_INFO_CFG_INT(max_train_epochs);
        SQL_INFO_CFG_INT(train_steps);
        SQL_INFO_CFG_INT(policy_rollout_limit);
        SQL_INFO_CFG_FLT(teacher_timeout);
        SQL_INFO_CFG_FLT(early_termination_success_rate);
        SQL_INFO_CFG_INT(early_termination_epochs);
        return 0;
    }
};



static int sqlInsertWeights(pddl_sqlite3 *db,
                            pddl_sqlite3_stmt *stmt,
                            int id,
                            int layer,
                            int sig,
                            const char *name,
                            int idx,
                            const dynet::Parameter &param,
                            pddl_err_t *err)
{
    pddl_sqlite3_reset(stmt);
    int ret = pddl_sqlite3_bind_int(stmt, 1, id);
    if (ret != SQLITE_OK){
        ERR_RET(err, -1, "Sqlite Error: %s: %s",
                pddl_sqlite3_errstr(ret), pddl_sqlite3_errmsg(db));
    }

    ret = pddl_sqlite3_bind_int(stmt, 2, layer);
    if (ret != SQLITE_OK){
        ERR_RET(err, -1, "Sqlite Error: %s: %s",
                pddl_sqlite3_errstr(ret), pddl_sqlite3_errmsg(db));
    }

    ret = pddl_sqlite3_bind_int(stmt, 3, sig);
    if (ret != SQLITE_OK){
        ERR_RET(err, -1, "Sqlite Error: %s: %s",
                pddl_sqlite3_errstr(ret), pddl_sqlite3_errmsg(db));
    }

    ret = pddl_sqlite3_bind_text(stmt, 4, name, -1, SQLITE_STATIC);
    if (ret != SQLITE_OK){
        ERR_RET(err, -1, "Sqlite Error: %s: %s",
                pddl_sqlite3_errstr(ret), pddl_sqlite3_errmsg(db));
    }

    ret = pddl_sqlite3_bind_int(stmt, 5, idx);
    if (ret != SQLITE_OK){
        ERR_RET(err, -1, "Sqlite Error: %s: %s",
                pddl_sqlite3_errstr(ret), pddl_sqlite3_errmsg(db));
    }

    const dynet::Tensor *val = ((dynet::Parameter &)param).values();
    size_t size = sizeof(float) * val->d.size();
    ret = pddl_sqlite3_bind_blob(stmt, 6, val->v, size, SQLITE_STATIC);
    if (ret != SQLITE_OK){
        ERR_RET(err, -1, "Sqlite Error: %s: %s",
                pddl_sqlite3_errstr(ret), pddl_sqlite3_errmsg(db));
    }

    ret = pddl_sqlite3_step(stmt);
    if (ret != SQLITE_DONE && ret != SQLITE_CONSTRAINT){
        ERR_RET(err, -1, "Sqlite Error: %s: %s",
                pddl_sqlite3_errstr(ret), pddl_sqlite3_errmsg(db));
    }
    return 0;
}


static int sqlSelectWeights(pddl_sqlite3 *db,
                            pddl_sqlite3_stmt *stmt,
                            int id,
                            int param_layer,
                            int param_sig,
                            const char *param_name,
                            int param_idx,
                            dynet::Parameter &param,
                            pddl_err_t *err)
{
    pddl_sqlite3_reset(stmt);
    int ret = pddl_sqlite3_bind_int(stmt, 1, id);
    if (ret != SQLITE_OK){
        ERR_RET(err, -1, "Sqlite Error: %s: %s",
                pddl_sqlite3_errstr(ret), pddl_sqlite3_errmsg(db));
    }

    int found = (ret = pddl_sqlite3_step(stmt)) == SQLITE_ROW;
    if (ret != SQLITE_ROW && ret != SQLITE_DONE){
        ERR_RET(err, -1, "Sqlite Error: %s: %s",
                pddl_sqlite3_errstr(ret), pddl_sqlite3_errmsg(db));
    }
    if (!found)
        ERR_RET(err, -1, "Weight %d not found.", id);

    int layer = pddl_sqlite3_column_int(stmt, 0);
    if (layer != param_layer){
        ERR_RET(err, -1, "Layers do not match (stored layer: %d, requested: %d)",
                layer, param_layer);
    }

    int sig = pddl_sqlite3_column_int(stmt, 1);
    if (sig != param_sig)
        ERR_RET(err, -1, "Stored weights don't match");

    const unsigned char *name = pddl_sqlite3_column_text(stmt, 2);
    if (strcmp((const char *)name, param_name) != 0){
        ERR_RET(err, -1, "Stored weights don't match"
                " (stored name: %s, requested: %s)", name, param_name);
    }

    int idx = pddl_sqlite3_column_int(stmt, 3);
    if (idx != param_idx){
        ERR_RET(err, -1, "Stored weights don't match"
                " (stored index: %d, requested: %d)", idx, param_idx);
    }

    int w_size = pddl_sqlite3_column_bytes(stmt, 4) / sizeof(float);
    if (w_size != (int)param.dim().size()){
        ERR_RET(err, -1, "Size of weights don't match"
                " (stored size: %d, requested: %d)",
                w_size, (int)param.dim().size());
    }

    const float *w = (const float *)pddl_sqlite3_column_blob(stmt, 4);
    std::vector<float> warr(w, w + w_size);
    dynet::TensorTools::set_elements(*param.values(), warr);

    return 0;
}

int pddlASNetsSave(const pddl_asnets_t *a, const char *fn, pddl_err_t *err)
{
    pddl_sqlite3 *db;
    int flags = SQLITE_OPEN_READWRITE
                    | SQLITE_OPEN_CREATE;
    int ret = pddl_sqlite3_open_v2(fn, &db, flags, NULL);
    if (ret != SQLITE_OK){
        ERR_RET(err, -1, "Sqlite Error: %s: %s",
                pddl_sqlite3_errstr(ret), pddl_sqlite3_errmsg(db));
    }

    Info info(a);
    if (info.create(db, err) != 0 || info.save(db, err) != 0){
        pddl_sqlite3_close_v2(db);
        TRACE_RET(err, -1);
    }

    char *errmsg = NULL;
    ret = pddl_sqlite3_exec(db, sql_create_weights, NULL, NULL, &errmsg);
    if (ret != SQLITE_OK){
        pddl_sqlite3_close_v2(db);
        ERR(err, "Sqlite Error: %s", errmsg);
        pddl_sqlite3_free(errmsg);
        return -1;
    }

    pddl_sqlite3_stmt *stmt;
    ret = pddl_sqlite3_prepare_v2(db, sql_insert_weights, -1, &stmt, NULL);
    if (ret != SQLITE_OK){
        pddl_sqlite3_close_v2(db);
        ERR_RET(err, -1, "Sqlite Error: %s: %s",
                pddl_sqlite3_errstr(ret), pddl_sqlite3_errmsg(db));
    }

    int id = 0;
    for (int layer = 0; layer <= a->params->num_layers; ++layer){
        const std::vector<ActionModule *> &acts = a->params->action[layer];
        for (size_t i = 0; i < acts.size(); ++i){
            ret = sqlInsertWeights(db, stmt, id, layer, SIG_ACTION_W,
                                   a->lifted_task.pddl.action.action[i].name,
                                   i, acts[i]->W, err);
            if (ret != 0){
                pddl_sqlite3_close_v2(db);
                TRACE_RET(err, -1);
            }
            ++id;

            ret = sqlInsertWeights(db, stmt, id, layer, SIG_ACTION_B,
                                   a->lifted_task.pddl.action.action[i].name,
                                   i, acts[i]->bias, err);
            if (ret != 0){
                pddl_sqlite3_close_v2(db);
                TRACE_RET(err, -1);
            }
            ++id;
        }
        if (layer == a->params->num_layers)
            break;

        const std::vector<PropositionModule *> &props = a->params->prop[layer];
        for (size_t i = 0; i < props.size(); ++i){
            ret = sqlInsertWeights(db, stmt, id, layer, SIG_PROP_W,
                                   a->lifted_task.pddl.pred.pred[i].name,
                                   i, props[i]->W, err);
            if (ret != 0){
                pddl_sqlite3_close_v2(db);
                TRACE_RET(err, -1);
            }
            ++id;

            ret = sqlInsertWeights(db, stmt, id, layer, SIG_PROP_B,
                                   a->lifted_task.pddl.pred.pred[i].name,
                                   i, props[i]->bias, err);
            if (ret != 0){
                pddl_sqlite3_close_v2(db);
                TRACE_RET(err, -1);
            }
            ++id;
        }
    }
    pddl_sqlite3_finalize(stmt);

    ret = pddl_sqlite3_close_v2(db);
    if (ret != SQLITE_OK){
        ERR_RET(err, -1, "Sqlite Error: %s: %s",
                pddl_sqlite3_errstr(ret), pddl_sqlite3_errmsg(db));
    }
    return 0;
}

int pddlASNetsLoad(pddl_asnets_t *a, const char *fn, pddl_err_t *err)
{
    CTX(err, "asnets_load", "ASNets-Load");
    LOG(err, "Loading model from %s", fn);
    pddl_sqlite3 *db;
    int flags = SQLITE_OPEN_READONLY;
    int ret = pddl_sqlite3_open_v2(fn, &db, flags, NULL);
    if (ret != SQLITE_OK){
        CTXEND(err);
        ERR_RET(err, -1, "Sqlite Error: %s: %s",
                pddl_sqlite3_errstr(ret), pddl_sqlite3_errmsg(db));
    }

    Info info;
    if (info.load(db, err) != 0){
        pddl_sqlite3_close_v2(db);
        CTXEND(err);
        TRACE_RET(err, -1);
    }

    Info info_cur(a);
    if (!info_cur.checkLoadedInfo(info, err)){
        pddl_sqlite3_close_v2(db);
        CTXEND(err);
        TRACE_RET(err, -1);
    }


    pddl_sqlite3_stmt *w_stmt;
    ret = pddl_sqlite3_prepare_v2(db, sql_query_weights, -1, &w_stmt, NULL);
    if (ret != SQLITE_OK){
        pddl_sqlite3_close_v2(db);
        CTXEND(err);
        ERR_RET(err, -1, "Sqlite Error: %s: %s",
                pddl_sqlite3_errstr(ret), pddl_sqlite3_errmsg(db));
    }

    int id = 0;
    for (int layer = 0; layer <= a->params->num_layers; ++layer){
        std::vector<ActionModule *> &acts = a->params->action[layer];
        for (size_t i = 0; i < acts.size(); ++i){
            ret = sqlSelectWeights(db, w_stmt, id, layer, SIG_ACTION_W,
                                   a->lifted_task.pddl.action.action[i].name,
                                   i, acts[i]->W, err);
            if (ret != 0){
                pddl_sqlite3_close_v2(db);
                CTXEND(err);
                TRACE_RET(err, -1);
            }
            ++id;

            ret = sqlSelectWeights(db, w_stmt, id, layer, SIG_ACTION_B,
                                   a->lifted_task.pddl.action.action[i].name,
                                   i, acts[i]->bias, err);
            if (ret != 0){
                pddl_sqlite3_close_v2(db);
                CTXEND(err);
                TRACE_RET(err, -1);
            }
            ++id;
        }
        if (layer == a->params->num_layers)
            break;

        std::vector<PropositionModule *> &props = a->params->prop[layer];
        for (size_t i = 0; i < props.size(); ++i){
            ret = sqlSelectWeights(db, w_stmt, id, layer, SIG_PROP_W,
                                   a->lifted_task.pddl.pred.pred[i].name,
                                   i, props[i]->W, err);
            if (ret != 0){
                pddl_sqlite3_close_v2(db);
                CTXEND(err);
                TRACE_RET(err, -1);
            }
            ++id;

            ret = sqlSelectWeights(db, w_stmt, id, layer, SIG_PROP_B,
                                   a->lifted_task.pddl.pred.pred[i].name,
                                   i, props[i]->bias, err);
            if (ret != 0){
                pddl_sqlite3_close_v2(db);
                CTXEND(err);
                TRACE_RET(err, -1);
            }
            ++id;
        }
    }


    pddl_sqlite3_finalize(w_stmt);

    ret = pddl_sqlite3_close_v2(db);
    if (ret != SQLITE_OK){
        CTXEND(err);
        ERR_RET(err, -1, "Sqlite Error: %s: %s",
                pddl_sqlite3_errstr(ret), pddl_sqlite3_errmsg(db));
    }
    CTXEND(err);
    return 0;
}

int pddlASNetsPrintModelInfo(const char *fn, pddl_err_t *err)
{
    CTX(err, "asnets_model_info", "ASNets-Info");
    LOG(err, "Loading model from %s", fn);
    pddl_sqlite3 *db;
    int flags = SQLITE_OPEN_READONLY;
    int ret = pddl_sqlite3_open_v2(fn, &db, flags, NULL);
    if (ret != SQLITE_OK){
        CTXEND(err);
        ERR_RET(err, -1, "Sqlite Error: %s: %s",
                pddl_sqlite3_errstr(ret), pddl_sqlite3_errmsg(db));
    }

    Info info;
    if (info.load(db, err) != 0){
        pddl_sqlite3_close_v2(db);
        CTXEND(err);
        TRACE_RET(err, -1);
    }

    CTXEND(err);
    return 0;
}

int pddlASNetsNumGroundTasks(const pddl_asnets_t *a)
{
    return a->ground_task_size;
}

const pddl_asnets_ground_task_t *
pddlASNetsGetGroundTask(const pddl_asnets_t *a, int id)
{
    if (id < 0 || id >= a->ground_task_size)
        return NULL;
    return a->ground_task + id;
}

int pddlASNetsRunPolicy(pddl_asnets_t *a,
                        const pddl_asnets_ground_task_t *task,
                        const int *in_state,
                        int *out_state)
{
    return runPolicy(task, *a->params, *a->cg, in_state, out_state);
}

int pddlASNetsSolveTask(pddl_asnets_t *a,
                        const pddl_asnets_ground_task_t *task,
                        pddl_iarr_t *trace,
                        pddl_err_t *err)
{
    pddl_fdr_state_pool_t states;
    pddlFDRStatePoolInit(&states, &task->fdr.var, NULL);
    int ret = policyRollout(a, task, &states, trace, NULL);
    pddlFDRStatePoolFree(&states);
    return ret;
}

static dynet::Expression asnetsTrainExpr(pddl_asnets_t *a,
                                         pddl_asnets_train_data_t *data,
                                         int minibatch_size,
                                         dynet::ComputationGraph &cg)
{
    cg.clear();

    // Sample a minibatch
    ASNetsTrainMiniBatch batch(a, data, minibatch_size);
    batch.createInputs(cg);

    // Construct network for all relevant ground tasks at once
    std::vector<dynet::Expression> nets;
    int batch_size = 0;
    for (int task_id = 0; task_id < a->ground_task_size; ++task_id){
        if (batch.batch[task_id].size == 0)
            continue;
        const ASNetsTrainMiniBatchTask &b = batch.batch[task_id];
        //LOG(err, "Batch: task: %d, size: %d", task_id, b.size);
        dynet::Expression e = asnetsExpr(a->ground_task + task_id,
                                         *a->params,
                                         cg,
                                         b.e_state,
                                         b.e_goal,
                                         b.e_applicable_ops,
                                         a->cfg.dropout_rate);
        dynet::Expression e_loss = crossEntropyLoss(cg, e, b.e_output);
        nets.push_back(e_loss);
        batch_size += b.size;
    }

    ASSERT_RUNTIME(nets.size() > 0);
    // Compute mean over all losses
    dynet::Expression e_loss = dynet::sum(nets) / batch_size;
    return e_loss;
}


static int trainStep(pddl_asnets_t *a,
                     int epoch,
                     int train_step,
                     pddl_asnets_train_data_t *data,
                     pddl_err_t *err)
{
    a->train_stats.train_step = train_step + 1;

    // Sample a minibatch
    pddlASNetsTrainDataShuffle(data);

    // Construct network with the right input data
    dynet::Expression e_loss = asnetsTrainExpr(a, data, a->cfg.batch_size, *a->cg);
    // TODO: L2 regularization -- is it done automatically by dynet?

    // Learn parameters
    float loss_val = dynet::as_scalar(a->cg->forward(e_loss));
    a->cg->backward(e_loss);
    a->trainer->update();

    LOG(err, "epoch %d/%d, step: %d/%d, loss: %.3f, succ: %.2f, samples: %d,"
        " succ epochs: %d"
        " | minibatch loss: %{batch_loss}f, size: %{batch_size}d",
        a->train_stats.epoch, a->train_stats.max_epochs,
        a->train_stats.train_step, a->train_stats.max_train_steps,
        a->train_stats.overall_loss, a->train_stats.success_rate,
        a->train_stats.num_samples,
        a->train_stats.consecutive_successful_epochs,
        loss_val, a->cfg.batch_size);

    return 0;
}

static int trainExploration(pddl_asnets_t *a,
                            int epoch,
                            int ground_task_id,
                            pddl_asnets_train_data_t *data,
                            pddl_err_t *err)
{
    const pddl_asnets_ground_task_t *task = a->ground_task + ground_task_id;
    CTX(err, "exploration", "Exploration Phase");

    pddl_fdr_state_pool_t states;
    pddlFDRStatePoolInit(&states, &task->fdr.var, err);

    // Collect states from the policy rollout
    int reached_goal = policyRollout(a, task, &states, NULL, err);
    LOG(err, "Policy rollout: %{policy_rollout_states}d states,"
        " reached goal: %{reached_goal}d",
        states.num_states, reached_goal);

    // TODO: Here we can add also states from random walks.
    //       Maybe for the for the first epoch?

    // Extend training data with teacher rollouts
    int *state = ALLOC_ARR(int, task->fdr.var.var_size);
    for (pddl_state_id_t state_id = 0; state_id < states.num_states; ++state_id){
        pddlFDRStatePoolGet(&states, state_id, state);
        int ret;

        switch (a->cfg.trainer){
            case PDDL_ASNETS_TRAINER_ASTAR_LMCUT:
                ret = pddlASNetsTrainDataRolloutAStarLMCut(data, ground_task_id,
                                                           state, &task->fdr,
                                                           a->cfg.teacher_timeout,
                                                           err);
                break;
        }

        if (ret < 0){
            FREE(state);
            pddlFDRStatePoolFree(&states);
            CTXEND(err);
            TRACE_RET(err, -1);
        }
    }
    FREE(state);

    pddlFDRStatePoolFree(&states);
    CTXEND(err);
    return 0;
}

static float overallLoss(pddl_asnets_t *a,
                         pddl_asnets_train_data_t *data)
{
    dynet::Expression e_loss = asnetsTrainExpr(a, data, -1, *a->cg);
    float loss = dynet::as_scalar(a->cg->forward(e_loss));
    return loss;
}

static float successRate(pddl_asnets_t *a)
{
    int num_solved = 0;
    for (int task_id = 0; task_id < a->ground_task_size; ++task_id){
        const pddl_asnets_ground_task_t *task = a->ground_task + task_id;
        pddl_fdr_state_pool_t states;
        pddlFDRStatePoolInit(&states, &task->fdr.var, NULL);
        if (policyRollout(a, task, &states, NULL, NULL))
            num_solved += 1;
        pddlFDRStatePoolFree(&states);
    }

    return num_solved / (float)a->ground_task_size;
}

static int trainEpoch(pddl_asnets_t *a,
                      int epoch,
                      pddl_asnets_train_data_t *data,
                      pddl_err_t *err)
{
    LOG(err, "epoch: %{epoch}d/%d", epoch, a->cfg.max_train_epochs);
    a->train_stats.epoch = epoch + 1;

    // Exploration phase
    for (int ground_task = 0; ground_task < a->ground_task_size; ++ground_task){
        int ret;
        if ((ret = trainExploration(a, epoch, ground_task, data, err)) != 0){
            if (ret < 0)
                TRACE_RET(err, ret);
            return ret;
        }
    }
    a->train_stats.num_samples = data->sample_size;

    // Training phase
    int num_steps = a->cfg.train_steps;
    //num_steps = PDDL_MIN(num_steps, data->sample_size / a->cfg.batch_size);
    //num_steps = PDDL_MAX(num_steps, 1);
    LOG(err, "num training steps: %{training_steps}d", num_steps);
    for (int train_step = 0; train_step < num_steps; ++train_step){
        int ret;
        if ((ret = trainStep(a, epoch, train_step, data, err)) != 0){
            if (ret < 0)
                TRACE_RET(err, ret);
            return ret;
        }
    }

    CTX(err, "success_rate", "Success Rate");
    a->train_stats.success_rate = successRate(a);
    LOG(err, "Success rate: %{success_rate}f", a->train_stats.success_rate);
    CTXEND(err);
    CTX(err, "overall_loss", "Overall Loss");
    a->train_stats.overall_loss = overallLoss(a, data);
    LOG(err, "Overall loss: %{overall_loss}f", a->train_stats.overall_loss);
    CTXEND(err);
    LOG(err, "Train samples: %{train_samples}d", a->train_stats.num_samples);
    LOG(err, "epoch %d/%d, step: %d/%d, loss: %.3f, succ: %.2f, samples: %d,"
        " succ epochs: %d",
        a->train_stats.epoch, a->train_stats.max_epochs,
        a->train_stats.train_step, a->train_stats.max_train_steps,
        a->train_stats.overall_loss, a->train_stats.success_rate,
        a->train_stats.num_samples,
        a->train_stats.consecutive_successful_epochs);
    return 0;
}

int pddlASNetsTrain(pddl_asnets_t *a, pddl_err_t *err)
{
    CTX(err, "asnets_train", "ASNets-Train");
    pddl_asnets_train_data_t data;
    pddlASNetsTrainDataInit(&data);

    float best_success_rate = 0.f;
    float best_success_rate_loss = 1E10f;
    a->train_stats.success_rate = successRate(a);

    for (int epoch = 0; epoch < a->cfg.max_train_epochs; ++epoch){
        if (a->cfg.double_batch_size_every_epoch > 0
                && epoch > 0
                && epoch % a->cfg.double_batch_size_every_epoch == 0){
            a->cfg.batch_size *= 2;
        }

        int ret;
        if ((ret = trainEpoch(a, epoch, &data, err)) != 0){
            pddlASNetsTrainDataFree(&data);
            CTXEND(err);
            if (ret < 0)
                TRACE_RET(err, ret);
            return ret;
        }

        if (a->train_stats.success_rate > best_success_rate
                || (a->train_stats.success_rate == best_success_rate
                        && a->train_stats.overall_loss < best_success_rate_loss)){
            best_success_rate = a->train_stats.success_rate;
            best_success_rate_loss = a->train_stats.overall_loss;
            if (a->cfg.save_model_prefix != NULL){
                char fn[4096];
                sprintf(fn, "%s-%.2f-%.03f.policy",
                        a->cfg.save_model_prefix,
                        best_success_rate,
                        best_success_rate_loss);
                LOG(err, "Saving model to %s (success rate: %.2f, loss: %.3f)",
                    fn, best_success_rate, best_success_rate_loss);
                pddlASNetsSave(a, fn, err);
            }
        }

        if (a->train_stats.success_rate >= a->cfg.early_termination_success_rate){
            a->train_stats.consecutive_successful_epochs += 1;
        }else{
            a->train_stats.consecutive_successful_epochs = 0;
        }

        LOG(err, "Consecutive successful epochs: %d",
            a->train_stats.consecutive_successful_epochs);
        if (a->train_stats.consecutive_successful_epochs
                >= a->cfg.early_termination_epochs){
            LOG(err, "Reached %d/%d consecutive successful epochs.",
                a->train_stats.consecutive_successful_epochs,
                a->cfg.early_termination_epochs);
            LOG(err, "Terminating training.");
            break;
        }
    }
    LOG(err, "epoch %d/%d, step: %d/%d, loss: %.3f, succ: %.2f, samples: %d,"
        " succ epochs: %d",
        a->train_stats.epoch, a->train_stats.max_epochs,
        a->train_stats.train_step, a->train_stats.max_train_steps,
        a->train_stats.overall_loss, a->train_stats.success_rate,
        a->train_stats.num_samples,
        a->train_stats.consecutive_successful_epochs);

    pddlASNetsTrainDataFree(&data);
    CTXEND(err);
    return 0;
}

#else /* PDDL_DYNET */

pddl_asnets_t *pddlASNetsNew(const char *domain_fn,
                             const char **problem_fn,
                             int problem_fn_size,
                             const pddl_asnets_config_t *cfg,
                             pddl_err_t *err)
{
    PANIC("This module requires dynet library.");
    return NULL;
}

void pddlASNetsDel(pddl_asnets_t *a)
{
    PANIC("This module requires dynet library.");
}

int pddlASNetsSave(const pddl_asnets_t *a, const char *fn, pddl_err_t *err)
{
    PANIC("This module requires dynet library.");
    return -1;
}

int pddlASNetsLoad(pddl_asnets_t *a, const char *fn, pddl_err_t *err)
{
    PANIC("This module requires dynet library.");
    return -1;
}

int pddlASNetsNumGroundTasks(const pddl_asnets_t *a)
{
    PANIC("This module requires dynet library.");
    return -1;
}

const pddl_asnets_ground_task_t *
pddlASNetsGetGroundTask(const pddl_asnets_t *a, int id)
{
    PANIC("This module requires dynet library.");
    return NULL;
}

int pddlASNetsRunPolicy(pddl_asnets_t *a,
                        const pddl_asnets_ground_task_t *task,
                        const int *in_state,
                        int *out_state)
{
    PANIC("This module requires dynet library.");
    return -1;
}

int pddlASNetsSolveTask(pddl_asnets_t *a,
                        const pddl_asnets_ground_task_t *task,
                        pddl_iarr_t *trace,
                        pddl_err_t *err)
{
    PANIC("This module requires dynet library.");
    return -1;
}

int pddlASNetsTrain(pddl_asnets_t *a, pddl_err_t *err)
{
    PANIC("This module requires dynet library.");
    return -1;
}

#endif /* PDDL_DYNET */
