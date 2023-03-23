"""
This is the "Scorpion Maidu" sequential portfolio, participating on the
agile track of the International Planning Competition of 2023.

"""


OPTIMAL = False
CONFIGS = [
    # lama-type-based-prefops
    (1, ['--evaluator', 'hlm=lmcount(lm_factory=lm_reasonable_orders_hps(lm_rhw()),transform=adapt_costs(one),pref=false)', '--evaluator', 'hff=ff(transform=adapt_costs(one))', '--search', 'lazy(alt([single(hff), single(hff, pref_only=true), single(hlm), single(hlm, pref_only=true), type_based([hff, g()])], boost=1000),preferred=[hff,hlm], cost_type=one,reopen_closed=false,bound=BOUND)']),
    # fdss-2018-18 1
    (1, ['--evaluator', 'hcg=cg(transform=adapt_costs(plusone))', '--search', 'lazy(alt([type_based([g()]),single(hcg),single(hcg,pref_only=true)],boost=0),preferred=[hcg],reopen_closed=true,cost_type=plusone,bound=BOUND)']),
    # lama-novelty-1-reset-clear-prefops
    (3, ['--evaluator', 'hlm=lmcount(lm_factory=lm_reasonable_orders_hps(lm_rhw()),transform=adapt_costs(one),pref=false)', '--evaluator', 'hff=ff(transform=adapt_costs(one))', '--search', 'lazy(alt([single(hff), single(hff, pref_only=true), single(hlm), single(hlm, pref_only=true), novelty_open_list(novelty(width=1, consider_only_novel_states=true, reset_after_progress=True), break_ties_randomly=False, handle_progress=clear)], boost=1000), preferred=[hff,hlm], cost_type=one,reopen_closed=false,bound=BOUND)']),
    # fdss-2018-21 1
    (1, ['--evaluator', 'hcea=cea(transform=adapt_costs(one))', '--search', 'eager_greedy([hcea],preferred=[hcea],cost_type=one,bound=BOUND)']),
    # fdss-2018-08 1
    (1, ['--evaluator', 'hadd=add(transform=adapt_costs(one))', '--evaluator', 'hlm=lmcount(lm_reasonable_orders_hps(lm_rhw()),transform=adapt_costs(one))', '--search', 'lazy(alt([type_based([g()]),single(hadd),single(hadd,pref_only=true),single(hlm),single(hlm,pref_only=true)]),preferred=[hadd,hlm],cost_type=one,bound=BOUND)']),
    # lama-type-based-prefops
    (8, ['--evaluator', 'hlm=lmcount(lm_factory=lm_reasonable_orders_hps(lm_rhw()),transform=adapt_costs(one),pref=false)', '--evaluator', 'hff=ff(transform=adapt_costs(one))', '--search', 'lazy(alt([single(hff), single(hff, pref_only=true), single(hlm), single(hlm, pref_only=true), type_based([hff, g()])], boost=1000),preferred=[hff,hlm], cost_type=one,reopen_closed=false,bound=BOUND)']),
    # fdss-2018-03 1
    (1, ['--evaluator', 'hlm=lmcount(lm_reasonable_orders_hps(lm_rhw()),transform=adapt_costs(one))', '--evaluator', 'hff=ff(transform=adapt_costs(one))', '--search', 'lazy(alt([single(hff),single(hff,pref_only=true),single(hlm),single(hlm,pref_only=true)],boost=1000),preferred=[hff,hlm],cost_type=one,reopen_closed=false,randomize_successors=false,preferred_successors_first=true,bound=BOUND)']),
    # fdss-2018-01 1
    (1, ['--evaluator', 'hlm=lmcount(lm_reasonable_orders_hps(lm_rhw()),transform=adapt_costs(one))', '--evaluator', 'hff=ff(transform=adapt_costs(one))', '--search', 'lazy(alt([single(hff),single(hff,pref_only=true),single(hlm),single(hlm,pref_only=true),type_based([hff,g()])],boost=1000),preferred=[hff,hlm],cost_type=one,reopen_closed=false,randomize_successors=true,preferred_successors_first=false,bound=BOUND)']),
    # fdss-2018-21 3
    (3, ['--evaluator', 'hcea=cea(transform=adapt_costs(one))', '--search', 'eager_greedy([hcea],preferred=[hcea],cost_type=one,bound=BOUND)']),
    # lama-novelty-1-reset-clear-prefops
    (20, ['--evaluator', 'hlm=lmcount(lm_factory=lm_reasonable_orders_hps(lm_rhw()),transform=adapt_costs(one),pref=false)', '--evaluator', 'hff=ff(transform=adapt_costs(one))', '--search', 'lazy(alt([single(hff), single(hff, pref_only=true), single(hlm), single(hlm, pref_only=true), novelty_open_list(novelty(width=1, consider_only_novel_states=true, reset_after_progress=True), break_ties_randomly=False, handle_progress=clear)], boost=1000), preferred=[hff,hlm], cost_type=one,reopen_closed=false,bound=BOUND)']),
    # fdss-2018-31 1
    (1, ['--evaluator', 'hff=ff(transform=adapt_costs(one))', '--search', 'lazy(alt([single(sum([weight(g(),2),weight(hff,3)])),single(sum([weight(g(),2),weight(hff,3)]),pref_only=true)],boost=5000),preferred=[hff],reopen_closed=true,cost_type=one,bound=BOUND)']),
    # fdss-2018-27 1
    (1, ['--landmarks', 'lmg=lm_reasonable_orders_hps(lm_rhw(only_causal_landmarks=true,disjunctive_landmarks=true,use_orders=true))', '--evaluator', 'hblind=blind()', '--evaluator', 'hadd=add()', '--evaluator', 'hlm=lmcount(lmg,admissible=false,pref=true,transform=adapt_costs(plusone))', '--evaluator', 'hff=ff()', '--search', 'lazy(alt([single(sum([weight(g(),2),weight(hblind,3)])),single(sum([weight(g(),2),weight(hblind,3)]),pref_only=true),single(sum([weight(g(),2),weight(hff,3)])),single(sum([weight(g(),2),weight(hff,3)]),pref_only=true),single(sum([weight(g(),2),weight(hlm,3)])),single(sum([weight(g(),2),weight(hlm,3)]),pref_only=true),single(sum([weight(g(),2),weight(hadd,3)])),single(sum([weight(g(),2),weight(hadd,3)]),pref_only=true)],boost=2474),preferred=[hadd],reopen_closed=false,cost_type=one,bound=BOUND)']),
    # fdss-2018-10 3
    (3, ['--evaluator', 'hcg=cg(transform=adapt_costs(one))', '--evaluator', 'hlm=lmcount(lm_reasonable_orders_hps(lm_rhw()),transform=adapt_costs(one))', '--search', 'eager(alt([type_based([g()]),single(hcg),single(hcg,pref_only=true),single(hlm),single(hlm,pref_only=true)]),preferred=[hcg,hlm],cost_type=one,bound=BOUND)']),
    # fdss-2018-18 12
    (12, ['--evaluator', 'hcg=cg(transform=adapt_costs(plusone))', '--search', 'lazy(alt([type_based([g()]),single(hcg),single(hcg,pref_only=true)],boost=0),preferred=[hcg],reopen_closed=true,cost_type=plusone,bound=BOUND)']),
    # fdss-2018-38 1
    (1, ['--landmarks', 'lmg=lm_merged([lm_rhw(only_causal_landmarks=false,disjunctive_landmarks=false,use_orders=true),lm_hm(m=1,conjunctive_landmarks=true,use_orders=true)])', '--evaluator', 'hff=ff()', '--evaluator', 'hlm=lmcount(lmg,admissible=true)', '--search', 'lazy(alt([single(sum([g(),weight(hff,10)])),single(sum([g(),weight(hff,10)]),pref_only=true),single(sum([g(),weight(hlm,10)])),single(sum([g(),weight(hlm,10)]),pref_only=true)],boost=500),preferred=[hff],reopen_closed=false,cost_type=plusone,bound=BOUND)']),
    # fdss-2018-29 3
    (3, ['--evaluator', 'hadd=add(transform=adapt_costs(plusone))', '--evaluator', 'hff=ff()', '--search', 'lazy(alt([tiebreaking([sum([weight(g(),4),weight(hff,5)]),hff]),tiebreaking([sum([weight(g(),4),weight(hff,5)]),hff],pref_only=true),tiebreaking([sum([weight(g(),4),weight(hadd,5)]),hadd]),tiebreaking([sum([weight(g(),4),weight(hadd,5)]),hadd],pref_only=true)],boost=2537),preferred=[hff,hadd],reopen_closed=true,bound=BOUND)']),
    # lama-novelty-2-reset-move-prefops
    (42, ['--evaluator', 'hlm=lmcount(lm_factory=lm_reasonable_orders_hps(lm_rhw()),transform=adapt_costs(one),pref=false)', '--evaluator', 'hff=ff(transform=adapt_costs(one))', '--search', 'lazy(alt([single(hff), single(hff, pref_only=true), single(hlm), single(hlm, pref_only=true), novelty_open_list(novelty(width=2, consider_only_novel_states=true, reset_after_progress=True), break_ties_randomly=False, handle_progress=move)], boost=1000), preferred=[hff,hlm], cost_type=one,reopen_closed=false,bound=BOUND)']),
    # fdss-2018-05 2
    (2, ['--evaluator', 'hlm=lmcount(lm_reasonable_orders_hps(lm_rhw()),transform=adapt_costs(one))', '--evaluator', 'hff=ff(transform=adapt_costs(one))', '--search', 'lazy(alt([single(hff),single(hff,pref_only=true),single(hlm),single(hlm,pref_only=true)],boost=1000),preferred=[hff,hlm],cost_type=one,reopen_closed=false,randomize_successors=true,preferred_successors_first=true,bound=BOUND)']),
    # fdss-2018-33 2
    (2, ['--landmarks', 'lmg=lm_hm(conjunctive_landmarks=false,use_orders=false,m=1)', '--evaluator', 'hlm=lmcount(lmg,admissible=true)', '--evaluator', 'hff=ff()', '--search', 'lazy(alt([type_based([g()]),single(hlm),single(hlm,pref_only=true),single(hff),single(hff,pref_only=true)],boost=1000),preferred=[hlm,hff],reopen_closed=false,cost_type=one,bound=BOUND)']),
    # fdss-2018-31 2
    (2, ['--evaluator', 'hff=ff(transform=adapt_costs(one))', '--search', 'lazy(alt([single(sum([weight(g(),2),weight(hff,3)])),single(sum([weight(g(),2),weight(hff,3)]),pref_only=true)],boost=5000),preferred=[hff],reopen_closed=true,cost_type=one,bound=BOUND)']),
    # fdss-2018-22 2
    (2, ['--evaluator', 'hff=ff(transform=adapt_costs(one))', '--search', 'eager(alt([single(sum([g(),weight(hff,3)])),single(sum([g(),weight(hff,3)]),pref_only=true)]),preferred=[hff],cost_type=one,bound=BOUND)']),
    # fdss-2018-15 4
    (4, ['--evaluator', 'hff=ff(transform=adapt_costs(one))', '--evaluator', 'hlm=lmcount(lm_reasonable_orders_hps(lm_rhw()),transform=adapt_costs(one))', '--search', 'eager(alt([type_based([g()]),single(sum([g(),weight(hff,3)])),single(sum([g(),weight(hff,3)]),pref_only=true),single(sum([g(),weight(hlm,3)])),single(sum([g(),weight(hlm,3)]),pref_only=true)]),preferred=[hff,hlm],cost_type=one,bound=BOUND)']),
    # fdss-2018-13 4
    (4, ['--evaluator', 'hcg=cg(transform=adapt_costs(one))', '--evaluator', 'hff=ff(transform=adapt_costs(one))', '--search', 'lazy(alt([single(sum([g(),weight(hff,10)])),single(sum([g(),weight(hff,10)]),pref_only=true),single(sum([g(),weight(hcg,10)])),single(sum([g(),weight(hcg,10)]),pref_only=true)],boost=100),preferred=[hcg],reopen_closed=false,cost_type=one,bound=BOUND)']),
    # fdss-2018-07 1
    (1, ['--evaluator', 'hcea=cea(transform=adapt_costs(one))', '--evaluator', 'hlm=lmcount(lm_reasonable_orders_hps(lm_rhw()),transform=adapt_costs(one))', '--search', 'lazy_greedy([hcea,hlm],preferred=[hcea,hlm],cost_type=one,bound=BOUND)']),
    # fdss-2018-27 2
    (2, ['--landmarks', 'lmg=lm_reasonable_orders_hps(lm_rhw(only_causal_landmarks=true,disjunctive_landmarks=true,use_orders=true))', '--evaluator', 'hblind=blind()', '--evaluator', 'hadd=add()', '--evaluator', 'hlm=lmcount(lmg,admissible=false,pref=true,transform=adapt_costs(plusone))', '--evaluator', 'hff=ff()', '--search', 'lazy(alt([single(sum([weight(g(),2),weight(hblind,3)])),single(sum([weight(g(),2),weight(hblind,3)]),pref_only=true),single(sum([weight(g(),2),weight(hff,3)])),single(sum([weight(g(),2),weight(hff,3)]),pref_only=true),single(sum([weight(g(),2),weight(hlm,3)])),single(sum([weight(g(),2),weight(hlm,3)]),pref_only=true),single(sum([weight(g(),2),weight(hadd,3)])),single(sum([weight(g(),2),weight(hadd,3)]),pref_only=true)],boost=2474),preferred=[hadd],reopen_closed=false,cost_type=one,bound=BOUND)']),
    # fdss-2018-29 9
    (9, ['--evaluator', 'hadd=add(transform=adapt_costs(plusone))', '--evaluator', 'hff=ff()', '--search', 'lazy(alt([tiebreaking([sum([weight(g(),4),weight(hff,5)]),hff]),tiebreaking([sum([weight(g(),4),weight(hff,5)]),hff],pref_only=true),tiebreaking([sum([weight(g(),4),weight(hadd,5)]),hadd]),tiebreaking([sum([weight(g(),4),weight(hadd,5)]),hadd],pref_only=true)],boost=2537),preferred=[hff,hadd],reopen_closed=true,bound=BOUND)']),
    # fdss-2018-07 8
    (8, ['--evaluator', 'hcea=cea(transform=adapt_costs(one))', '--evaluator', 'hlm=lmcount(lm_reasonable_orders_hps(lm_rhw()),transform=adapt_costs(one))', '--search', 'lazy_greedy([hcea,hlm],preferred=[hcea,hlm],cost_type=one,bound=BOUND)']),
    # fdss-2018-39 7
    (7, ['--landmarks', 'lmg=lm_exhaust(only_causal_landmarks=false)', '--evaluator', 'hgoalcount=goalcount(transform=adapt_costs(plusone))', '--evaluator', 'hlm=lmcount(lmg,admissible=false)', '--evaluator', 'hff=ff()', '--evaluator', 'hblind=blind()', '--search', 'eager(alt([tiebreaking([sum([weight(g(),8),weight(hblind,9)]),hblind]),tiebreaking([sum([weight(g(),8),weight(hlm,9)]),hlm]),tiebreaking([sum([weight(g(),8),weight(hff,9)]),hff]),tiebreaking([sum([weight(g(),8),weight(hgoalcount,9)]),hgoalcount])],boost=2005),preferred=[],reopen_closed=true,bound=BOUND)']),
    # fdss-2018-18 38
    (38, ['--evaluator', 'hcg=cg(transform=adapt_costs(plusone))', '--search', 'lazy(alt([type_based([g()]),single(hcg),single(hcg,pref_only=true)],boost=0),preferred=[hcg],reopen_closed=true,cost_type=plusone,bound=BOUND)']),
    # fdss-2018-01 3
    (3, ['--evaluator', 'hlm=lmcount(lm_reasonable_orders_hps(lm_rhw()),transform=adapt_costs(one))', '--evaluator', 'hff=ff(transform=adapt_costs(one))', '--search', 'lazy(alt([single(hff),single(hff,pref_only=true),single(hlm),single(hlm,pref_only=true),type_based([hff,g()])],boost=1000),preferred=[hff,hlm],cost_type=one,reopen_closed=false,randomize_successors=true,preferred_successors_first=false,bound=BOUND)']),
    # fdss-2018-35 11
    (11, ['--landmarks', 'lmg=lm_hm(conjunctive_landmarks=false,use_orders=false,m=1)', '--evaluator', 'hcg=cg(transform=adapt_costs(one))', '--evaluator', 'hlm=lmcount(lmg,admissible=true)', '--search', 'lazy(alt([single(hlm),single(hlm,pref_only=true),single(hcg),single(hcg,pref_only=true)],boost=0),preferred=[hcg],reopen_closed=false,cost_type=one,bound=BOUND)']),
    # fdss-2018-13 18
    (18, ['--evaluator', 'hcg=cg(transform=adapt_costs(one))', '--evaluator', 'hff=ff(transform=adapt_costs(one))', '--search', 'lazy(alt([single(sum([g(),weight(hff,10)])),single(sum([g(),weight(hff,10)]),pref_only=true),single(sum([g(),weight(hcg,10)])),single(sum([g(),weight(hcg,10)]),pref_only=true)],boost=100),preferred=[hcg],reopen_closed=false,cost_type=one,bound=BOUND)']),
    # fdss-2018-40 4
    (4, ['--landmarks', 'lmg=lm_zg(use_orders=false)', '--evaluator', 'hlm=lmcount(lmg,admissible=true,pref=false)', '--search', 'eager(single(sum([g(),weight(hlm,3)])),preferred=[],reopen_closed=true,cost_type=one,bound=BOUND)']),
    # fdss-2018-10 7
    (7, ['--evaluator', 'hcg=cg(transform=adapt_costs(one))', '--evaluator', 'hlm=lmcount(lm_reasonable_orders_hps(lm_rhw()),transform=adapt_costs(one))', '--search', 'eager(alt([type_based([g()]),single(hcg),single(hcg,pref_only=true),single(hlm),single(hlm,pref_only=true)]),preferred=[hcg,hlm],cost_type=one,bound=BOUND)']),
    # fdss-2018-24 10
    (10, ['--evaluator', 'hgoalcount=goalcount(transform=adapt_costs(one))', '--evaluator', 'hff=ff(transform=adapt_costs(plusone))', '--evaluator', 'hblind=blind()', '--evaluator', 'hcg=cg()', '--search', 'lazy(alt([single(sum([weight(g(),2),weight(hblind,3)])),single(sum([weight(g(),2),weight(hblind,3)]),pref_only=true),single(sum([weight(g(),2),weight(hff,3)])),single(sum([weight(g(),2),weight(hff,3)]),pref_only=true),single(sum([weight(g(),2),weight(hcg,3)])),single(sum([weight(g(),2),weight(hcg,3)]),pref_only=true),single(sum([weight(g(),2),weight(hgoalcount,3)])),single(sum([weight(g(),2),weight(hgoalcount,3)]),pref_only=true)],boost=3662),preferred=[hff],reopen_closed=true,bound=BOUND)']),
    # fdss-2018-33 24
    (24, ['--landmarks', 'lmg=lm_hm(conjunctive_landmarks=false,use_orders=false,m=1)', '--evaluator', 'hlm=lmcount(lmg,admissible=true)', '--evaluator', 'hff=ff()', '--search', 'lazy(alt([type_based([g()]),single(hlm),single(hlm,pref_only=true),single(hff),single(hff,pref_only=true)],boost=1000),preferred=[hlm,hff],reopen_closed=false,cost_type=one,bound=BOUND)']),
    # fdss-2018-16 7
    (7, ['--landmarks', 'lmg=lm_rhw(only_causal_landmarks=false,disjunctive_landmarks=false,use_orders=true)', '--evaluator', 'hlm=lmcount(lmg,admissible=false,transform=adapt_costs(one))', '--evaluator', 'hff=ff(transform=adapt_costs(one))', '--evaluator', 'hblind=blind()', '--search', 'lazy(alt([type_based([g()]),single(sum([g(),weight(hblind,2)])),single(sum([g(),weight(hblind,2)]),pref_only=true),single(sum([g(),weight(hlm,2)])),single(sum([g(),weight(hlm,2)]),pref_only=true),single(sum([g(),weight(hff,2)])),single(sum([g(),weight(hff,2)]),pref_only=true)],boost=4419),preferred=[hlm],reopen_closed=true,cost_type=one,bound=BOUND)']),
    # fdss-2018-21 7
    (7, ['--evaluator', 'hcea=cea(transform=adapt_costs(one))', '--search', 'eager_greedy([hcea],preferred=[hcea],cost_type=one,bound=BOUND)']),
    # fdss-2018-19 4
    (4, ['--evaluator', 'hcg=cg(transform=adapt_costs(one))', '--evaluator', 'hlm=lmcount(lm_reasonable_orders_hps(lm_rhw()),transform=adapt_costs(one))', '--search', 'lazy(alt([type_based([g()]),single(sum([g(),weight(hcg,3)])),single(sum([g(),weight(hcg,3)]),pref_only=true),single(sum([g(),weight(hlm,3)])),single(sum([g(),weight(hlm,3)]),pref_only=true)]),preferred=[hcg,hlm],cost_type=one,bound=BOUND)']),
    # fdss-2018-02 4
    (4, ['--landmarks', 'lmg=lm_rhw(only_causal_landmarks=false,disjunctive_landmarks=true,use_orders=false)', '--evaluator', 'hlm=lmcount(lmg,admissible=true,transform=adapt_costs(one))', '--evaluator', 'hff=ff(transform=adapt_costs(one))', '--search', 'lazy(alt([type_based([g()]),single(hlm),single(hlm,pref_only=true),single(hff),single(hff,pref_only=true)],boost=0),preferred=[hlm],reopen_closed=false,cost_type=plusone,bound=BOUND)']),
    # fdss-2018-34 5
    (5, ['--landmarks', 'lmg=lm_hm(conjunctive_landmarks=true,use_orders=true,m=1)', '--evaluator', 'hlm=lmcount(lmg,admissible=true)', '--evaluator', 'hff=ff()', '--search', 'lazy(alt([tiebreaking([sum([g(),weight(hlm,10)]),hlm]),tiebreaking([sum([g(),weight(hlm,10)]),hlm],pref_only=true),tiebreaking([sum([g(),weight(hff,10)]),hff]),tiebreaking([sum([g(),weight(hff,10)]),hff],pref_only=true)],boost=200),preferred=[hlm],reopen_closed=true,cost_type=plusone,bound=BOUND)']),
    # fdss-2018-25 5
    (5, ['--evaluator', 'hcg=cg(transform=adapt_costs(plusone))', '--search', 'lazy(alt([single(sum([g(),weight(hcg,10)])),single(sum([g(),weight(hcg,10)]),pref_only=true)],boost=0),preferred=[hcg],reopen_closed=false,cost_type=plusone,bound=BOUND)']),
    # fdss-2018-28 5
    (5, ['--evaluator', 'hblind=blind()', '--evaluator', 'hadd=add()', '--evaluator', 'hcg=cg(transform=adapt_costs(one))', '--evaluator', 'hhmax=hmax()', '--search', 'eager(alt([tiebreaking([sum([g(),weight(hblind,7)]),hblind]),tiebreaking([sum([g(),weight(hhmax,7)]),hhmax]),tiebreaking([sum([g(),weight(hadd,7)]),hadd]),tiebreaking([sum([g(),weight(hcg,7)]),hcg])],boost=2142),preferred=[],reopen_closed=true,bound=BOUND)'])
]
