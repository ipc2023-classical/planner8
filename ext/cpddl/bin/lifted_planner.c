#include "pddl/pddl.h"
#include "options.h"
#include "print_to_file.h"

static int lifted_search_started = 0;
static int lifted_terminate = 0;

typedef pddl_homomorphism_heur_t *(*heur_homo_fn)(
                    const pddl_t *pddl,
                    const pddl_homomorphism_config_t *cfg,
                    pddl_err_t *err);


static void liftedPlannerSigHandlerTerminate(int signal)
{
    if (lifted_search_started && lifted_terminate){
        fprintf(stderr, "Received second %s signal\n", strsignal(signal));
        fprintf(stderr, "Forced Exit\n");
        fflush(stderr);
        exit(-1);
    }

    fprintf(stderr, "Received %s signal\n", strsignal(signal));
    fflush(stderr);
    if (lifted_search_started){
        lifted_terminate = 1;
    }else{
        exit(-1);
    }
}

static pddl_homomorphism_heur_t *
    _liftedPlannerHeurCollapseAllExceptOneType(const pddl_t *pddl,
                                               heur_homo_fn heur_fn,
                                               int except,
                                               pddl_err_t *err)
{
    pddl_homomorphism_config_t homo_cfg = opt.lifted_planner.homomorph_cfg;
    for (int type = 0; type < pddl->type.type_size; ++type){
        if (type == except)
            continue;
        if (pddlTypesIsMinimal(&pddl->type, type))
            pddlISetAdd(&homo_cfg.collapse_types, type);
    }
    pddl_homomorphism_heur_t *heur;
    if ((heur = heur_fn(pddl, &homo_cfg, err)) == NULL){
        fprintf(stderr, "Error: ");
        pddlErrPrint(err, 1, stderr);
        return NULL;
    }
    return heur;
}

static pddl_homomorphism_heur_t *
    liftedPlannerHeurCollapseAllExceptOneType(const pddl_t *pddl,
                                              heur_homo_fn heur_fn,
                                              pddl_err_t *err)
{
    pddl_homomorphism_heur_t *heur = NULL;
    int best_hval = -1;
    for (int type = 0; type < pddl->type.type_size; ++type){
        if (pddlTypesIsMinimal(&pddl->type, type)
                && pddlTypeNumObjs(&pddl->type, type) > 1){
            pddl_homomorphism_heur_t *h;
            h = _liftedPlannerHeurCollapseAllExceptOneType(pddl, heur_fn, type, err);
            if (h == NULL)
                continue;

            int hval = pddlHomomorphismHeurEvalGroundInit(h);
            PDDL_INFO(err, "Homomorph heur: Heuristic value for the init: %d", hval);
            if (hval > best_hval && hval != PDDL_COST_DEAD_END){
                if (heur != NULL)
                    pddlHomomorphismHeurDel(heur);
                heur = h;
                best_hval = hval;
            }else{
                pddlHomomorphismHeurDel(h);
            }
        }
    }
    return heur;
}

static pddl_homomorphism_heur_t *
    _liftedPlannerHeurCollapseRandom(const pddl_t *pddl,
                                     heur_homo_fn heur_fn,
                                     int seed,
                                     pddl_err_t *err)
{
    pddl_homomorphism_config_t homo_cfg = opt.lifted_planner.homomorph_cfg;
    homo_cfg.random_seed = seed;
    pddl_homomorphism_heur_t *heur;
    if ((heur = heur_fn(pddl, &homo_cfg, err)) == NULL){
        fprintf(stderr, "Error: ");
        pddlErrPrint(err, 1, stderr);
        return NULL;
    }
    return heur;
}

static pddl_homomorphism_heur_t *
    liftedPlannerHeurCollapseRandom(const pddl_t *pddl,
                                    heur_homo_fn heur_fn,
                                    pddl_err_t *err)
{
    int seed = opt.lifted_planner.homomorph_cfg.random_seed;
    pddl_homomorphism_heur_t *heur = NULL;
    int best_hval = -1;
    for (int i = 0; i < opt.lifted_planner.homomorph_samples; ++i){
        pddl_homomorphism_heur_t *h;
        h = _liftedPlannerHeurCollapseRandom(pddl, heur_fn, seed, err);
        int hval = pddlHomomorphismHeurEvalGroundInit(h);
        PDDL_INFO(err, "Homomorph heur: Heuristic value for the init: %d", hval);
        if (hval > best_hval && hval != PDDL_COST_DEAD_END){
            if (heur != NULL)
                pddlHomomorphismHeurDel(heur);
            heur = h;
            best_hval = hval;
        }else{
            pddlHomomorphismHeurDel(h);
        }
        ++seed;
    }
    return heur;
}

static pddl_homomorphism_heur_t *liftedHomomorphHeur(const pddl_t *pddl,
                                                     pddl_err_t *err)
{
    pddl_homomorphism_heur_t *heur = NULL;

    PDDL_CTX_NO_TIME(err, "cfg.heur", "Cfg Heur");
    heur_homo_fn heur_fn = pddlHomomorphismHeurLMCut;
    switch (opt.lifted_planner.heur){
        case LIFTED_PLAN_HEUR_HOMO_LMC:
            PDDL_LOG(err, "heur = homo-lmc");
            heur_fn = pddlHomomorphismHeurLMCut;
            break;
        case LIFTED_PLAN_HEUR_HOMO_FF:
            PDDL_LOG(err, "heur = homo-ff");
            heur_fn = pddlHomomorphismHeurHFF;
            break;
    }
    PDDL_CTX_NO_TIME(err, "homomorph", "Homomorph");
    pddlHomomorphismConfigLog(&opt.lifted_planner.homomorph_cfg, err);
    PDDL_LOG(err, "samples = %{samples}d", opt.lifted_planner.homomorph_samples);
    PDDL_CTXEND(err);
    PDDL_CTXEND(err);

    if ((opt.lifted_planner.homomorph_cfg.type & 0xfu)
            == PDDL_HOMOMORPHISM_TYPES){
        heur = liftedPlannerHeurCollapseAllExceptOneType(pddl, heur_fn, err);
    }else{
        heur = liftedPlannerHeurCollapseRandom(pddl, heur_fn, err);
    }

    return heur;
}

int liftedPlanner(const pddl_t *pddl, pddl_err_t *err)
{
    void (*old_sigint)(int);
    void (*old_sigterm)(int);
    old_sigint = signal(SIGINT, liftedPlannerSigHandlerTerminate);
    old_sigterm = signal(SIGTERM, liftedPlannerSigHandlerTerminate);

    PDDL_CTX(err, "lplan", "LPLAN");

    pddl_homomorphism_heur_t *heur_homo = NULL;
    pddl_lifted_heur_t *heur = NULL;
    switch (opt.lifted_planner.heur){
        case LIFTED_PLAN_HEUR_BLIND:
            PDDL_INFO(err, "cfg.heur = blind");
            heur = pddlLiftedHeurBlind();
            break;
        case LIFTED_PLAN_HEUR_HMAX:
            PDDL_INFO(err, "cfg.heur = hmax");
            heur = pddlLiftedHeurHMax(pddl, err);
            break;
        case LIFTED_PLAN_HEUR_HADD:
            PDDL_INFO(err, "cfg.heur = hadd");
            heur = pddlLiftedHeurHAdd(pddl, err);
            break;
        case LIFTED_PLAN_HEUR_HOMO_LMC:
        case LIFTED_PLAN_HEUR_HOMO_FF:
            heur_homo = liftedHomomorphHeur(pddl, err);
            heur = pddlLiftedHeurHomomorphism(heur_homo);
            break;
        default:
            PDDL_PANIC("Unknown lifted heuristic.");
            break;
    }

    pddl_lifted_search_config_t search_cfg = PDDL_LIFTED_SEARCH_CONFIG_INIT;
    search_cfg.pddl = pddl;
    search_cfg.heur = heur;

    switch (opt.lifted_planner.succ_gen){
        case LIFTED_PLAN_SUCC_GEN_DL:
            search_cfg.succ_gen = PDDL_LIFTED_APP_ACTION_DL;
            PDDL_INFO(err, "Search successor generator: datalog");
            break;
        case LIFTED_PLAN_SUCC_GEN_SQL:
            search_cfg.succ_gen = PDDL_LIFTED_APP_ACTION_SQL;
            PDDL_INFO(err, "Search successor generator: sql");
            break;
        default:
            search_cfg.succ_gen = PDDL_LIFTED_APP_ACTION_DL;
    }

    switch (opt.lifted_planner.search){
        case LIFTED_PLAN_ASTAR:
            search_cfg.alg = PDDL_LIFTED_SEARCH_ASTAR;
            PDDL_INFO(err, "Search: astar");
            break;
        case LIFTED_PLAN_GBFS:
            search_cfg.alg = PDDL_LIFTED_SEARCH_GBFS;
            PDDL_INFO(err, "Search: gbfs");
            break;
        case LIFTED_PLAN_LAZY:
            search_cfg.alg = PDDL_LIFTED_SEARCH_LAZY;
            PDDL_INFO(err, "Search: lazy");
            break;
        default:
            PDDL_PANIC("Unknown lifted planner %d", opt.lifted_planner.search);
    }

    pddl_lifted_search_t *search = pddlLiftedSearchNew(&search_cfg, err);
    pddl_lifted_search_status_t st = pddlLiftedSearchInitStep(search);
    lifted_search_started = 1;

    pddl_timer_t info_timer;
    pddlTimerStart(&info_timer);
    for (int step = 1; st == PDDL_LIFTED_SEARCH_CONT; ++step){
        if (lifted_terminate){
            st = PDDL_LIFTED_SEARCH_ABORT;
            break;
        }

        st = pddlLiftedSearchStep(search);
        if (step >= 100){
            pddlTimerStop(&info_timer);
            if (pddlTimerElapsedInSF(&info_timer) >= 1.){
                pddlLiftedSearchStatLog(search, err);
                pddlTimerStart(&info_timer);
            }
            step = 0;
        }
    }
    pddlLiftedSearchStatLog(search, err);

    PDDL_PROP_BOOL(err, "finished", 1);
    PDDL_PROP_BOOL(err, "unsolvable", st == PDDL_LIFTED_SEARCH_UNSOLVABLE);
    PDDL_PROP_BOOL(err, "found", st == PDDL_LIFTED_SEARCH_FOUND);
    PDDL_PROP_BOOL(err, "aborted", st == PDDL_LIFTED_SEARCH_ABORT);

    if (st == PDDL_LIFTED_SEARCH_UNSOLVABLE){
        PDDL_INFO(err, "Problem is unsolvable.");

    }else if (st == PDDL_LIFTED_SEARCH_FOUND){
        PDDL_INFO(err, "Plan found.");
        const pddl_lifted_plan_t *plan = pddlLiftedSearchPlan(search);
        PDDL_INFO(err, "Plan Cost: %d", plan->plan_cost);
        PDDL_PROP_INT(err, "plan_cost", plan->plan_cost);
        PDDL_INFO(err, "Plan Length: %d", plan->plan_len);
        PDDL_PROP_INT(err, "plan_length", plan->plan_len);
        PRINT_TO_FILE(err, opt.lifted_planner.plan_out, "plan",
                      pddlLiftedSearchPlanPrint(search, fout));

    }else if (st == PDDL_LIFTED_SEARCH_ABORT){
        PDDL_INFO(err, "Search aborted.");

    }else{
        PDDL_PANIC("Unkown return status: %d", (int)st);
    }

    pddlLiftedSearchDel(search);
    if (heur_homo != NULL)
        pddlHomomorphismHeurDel(heur_homo);
    if (heur != NULL)
        pddlLiftedHeurDel(heur);
    PDDL_CTXEND(err);
    signal(SIGINT, old_sigint);
    signal(SIGTERM, old_sigterm);
    return 1;
}

