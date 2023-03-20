-include Makefile.config
-include Makefile.include

CFLAGS += -I. -O3

CPPFLAGS += -Wno-ignored-attributes
CPPFLAGS += -I. -O3

CPPCHECK_FLAGS += --platform=unix64 --enable=all -I.

TARGETS  = libpddl.a

OBJS  = alloc
OBJS += err
OBJS += hfunc
OBJS += sha256
OBJS += google-city-hash
OBJS += toml
OBJS += rand
OBJS += sort
OBJS += qsort
OBJS += timsort
OBJS += mergesort
OBJS += heapsort
OBJS += segmarr
OBJS += extarr
OBJS += pairheap
OBJS += hashset
OBJS += rbtree
OBJS += htable
OBJS += fifo
OBJS += lp
OBJS += lp-cplex
OBJS += lp-gurobi
OBJS += lp-glpk
OBJS += lp-highs
OBJS += cp
OBJS += cp-minizinc
OBJS += lisp
OBJS += require_flags
OBJS += type
OBJS += param
OBJS += obj
OBJS += pred
OBJS += fact
OBJS += action
OBJS += prep_action
OBJS += pddl
OBJS += unify
OBJS += compile_in_lifted_mgroup
OBJS += fm
OBJS += fm_arr
OBJS += strips
OBJS += strips_op
OBJS += strips_fact_cross_ref
OBJS += strips_maker
OBJS += strips_conj
OBJS += sql_grounder
OBJS += strips_ground_tree
OBJS += strips_ground
OBJS += strips_ground_sql
OBJS += strips_ground_datalog
OBJS += action_args
OBJS += ground_atom
OBJS += profile
OBJS += lifted_mgroup
OBJS += lifted_mgroup_infer
OBJS += lifted_mgroup_htable
OBJS += mgroup
OBJS += mgroup_projection
OBJS += mutex_pair
OBJS += pddl_file
OBJS += plan_file
OBJS += irrelevance
OBJS += h1
OBJS += h2
OBJS += h3
OBJS += hm
OBJS += disambiguation
OBJS += bitset
OBJS += set
OBJS += fdr_var
OBJS += fdr_part_state
OBJS += fdr_op
OBJS += fdr
OBJS += fdr_state_packer
OBJS += fdr_state_pool
OBJS += fdr_state_space
OBJS += fdr_state_sampler
OBJS += strips_state_space
OBJS += sym
OBJS += famgroup
OBJS += pot
OBJS += lm_cut
OBJS += hpot
OBJS += hflow
OBJS += hmax
OBJS += hadd
OBJS += hff
OBJS += pq
OBJS += mg_strips
OBJS += preprocess
OBJS += cg
OBJS += graph
OBJS += clique
OBJS += biclique
OBJS += fdr_app_op
OBJS += random_walk
OBJS += open_list
OBJS += open_list_splaytree1
OBJS += open_list_splaytree2
OBJS += search
OBJS += search_astar
OBJS += search_lazy
OBJS += lifted_app_action
OBJS += lifted_app_action_sql
OBJS += lifted_app_action_datalog
OBJS += lifted_search
OBJS += plan
OBJS += relaxed_plan
OBJS += heur
OBJS += heur_blind
OBJS += heur_dead_end
OBJS += heur_lm_cut
OBJS += heur_hmax
OBJS += heur_hadd
OBJS += heur_hff
OBJS += heur_flow
OBJS += heur_op_mutex
OBJS += dtg
OBJS += scc
OBJS += ts
OBJS += op_mutex_pair
OBJS += op_mutex_infer
OBJS += op_mutex_infer_ts
OBJS += op_mutex_redundant
OBJS += op_mutex_redundant_greedy
OBJS += op_mutex_redundant_max
OBJS += reversibility
OBJS += invertibility
OBJS += cascading_table
OBJS += transition
OBJS += label
OBJS += labeled_transition
OBJS += trans_system
OBJS += trans_system_abstr_map
OBJS += trans_system_graph
OBJS += bdd
OBJS += bdds
OBJS += symbolic_vars
OBJS += symbolic_constr
OBJS += symbolic_trans
OBJS += symbolic_state
OBJS += symbolic_task
OBJS += symbolic_split_goal
OBJS += cost
OBJS += black_mgroup
OBJS += red_black_fdr
OBJS += outbox
OBJS += datalog
OBJS += datalog_pddl
OBJS += endomorphism_fdr
OBJS += endomorphism_ts
OBJS += endomorphism_lifted
OBJS += homomorphism
OBJS += homomorphism_heur
OBJS += prune_strips
OBJS += objset
OBJS += iset
OBJS += lset
OBJS += cset
OBJS += iarr
OBJS += lifted_heur
OBJS += lifted_heur_relaxed
OBJS += subprocess
OBJS += task
OBJS += asnets_task
OBJS += asnets_train_data

OBJS += __sqlite3

OBJS_CPP  = cp-cp-optimizer
OBJS_CPP += asnets_dynet

OBJS := $(foreach obj,$(OBJS),.objs/$(obj).o) $(foreach obj,$(OBJS_CPP),.objs/$(obj).cpp.o)

GEN  = pddl/objset.h
GEN += src/objset.c
GEN += pddl/iset.h
GEN += src/iset.c
GEN += pddl/lset.h
GEN += src/lset.c
GEN += pddl/cset.h
GEN += src/cset.c
GEN += pddl/iarr.h
GEN += src/iarr.c

all: $(TARGETS)

bin: libpddl.a
	$(MAKE) -C bin

libpddl.a: $(OBJS) Makefile pddl/version.h
	echo "#include \"pddl/version.h\"" >_version.c
	echo "const char *pddl_build_commit = \"$(shell git rev-parse HEAD)\";" >>_version.c
	echo "const char *pddl_version = PDDL_VERSION_STR \"-$(shell git rev-parse HEAD)\";" >>_version.c
	$(CC) -I. -c -o .objs/_version.o _version.c
	rm -f _version.c
	ar cr $@ $(OBJS) .objs/_version.o
	ranlib $@

pddl/config.h: Makefile Makefile.include
	echo "#ifndef __PDDL_CONFIG_H__" >$@
	echo "#define __PDDL_CONFIG_H__" >>$@
	echo "" >>$@
	if [ "$(DEBUG)" = "yes" ]; then echo "#define PDDL_DEBUG" >>$@; fi
	if [ "$(USE_CLIQUER)" = "yes" ]; then echo "#define PDDL_CLIQUER" >>$@; fi
	if [ "$(USE_CUDD)" = "yes" ]; then echo "#define PDDL_CUDD" >>$@; fi
	if [ "$(USE_BLISS)" = "yes" ]; then echo "#define PDDL_BLISS" >>$@; fi
	if [ "$(USE_CPLEX)" = "yes" ]; then echo "#define PDDL_CPLEX" >>$@; fi
	if [ "$(USE_CPOPTIMIZER)" = "yes" ]; then echo "#define PDDL_CPOPTIMIZER" >>$@; fi
	if [ "$(USE_GUROBI)" = "yes" ]; then echo "#define PDDL_GUROBI" >>$@; fi
	if [ "$(USE_GLPK)" = "yes" ]; then echo "#define PDDL_GLPK" >>$@; fi
	if [ "$(USE_HIGHS)" = "yes" ]; then echo "#define PDDL_HIGHS" >>$@; fi
	if [ "$(USE_CPLEX)" = "yes" ] || [ "$(USE_GUROBI)" = "yes" ] || [ "$(USE_GLPK)" = "yes" ] || [ "$(USE_HIGHS)" = "yes" ]; then echo "#define PDDL_LP" >>$@; fi
	if [ "$(MINIZINC_BIN)" != "" ]; then echo "#define PDDL_MINIZINC" >>$@; fi
	echo "#define PDDL_MINIZINC_BIN \"$(MINIZINC_BIN)\"" >>$@
	echo "#define PDDL_MINIZINC_VERSION \"$(MINIZINC_VERSION)\"" >>$@
	if [ "$(USE_DYNET)" = "yes" ]; then echo "#define PDDL_DYNET" >>$@; fi
	echo "" >>$@
	echo "#endif /* __PDDL_CONFIG_H__ */" >>$@

pddl/objset.h: src/_set_arr.h scripts/fmt_set.sh
	$(BASH) scripts/fmt_set.sh set Set pddl_obj_id_t obj Obj OBJ <$< >$@
src/objset.c: src/_set_arr.c scripts/fmt_set.sh pddl/objset.h
	$(BASH) scripts/fmt_set.sh set Set pddl_obj_id_t obj Obj OBJ <$< >$@
pddl/iset.h: src/_set_arr.h scripts/fmt_set.sh
	$(BASH) scripts/fmt_set.sh set Set int i I I <$< >$@
src/iset.c: src/_set_arr.c scripts/fmt_set.sh pddl/objset.h
	$(BASH) scripts/fmt_set.sh set Set int i I I <$< >$@
pddl/lset.h: src/_set_arr.h scripts/fmt_set.sh
	$(BASH) scripts/fmt_set.sh set Set long l L L <$< >$@
src/lset.c: src/_set_arr.c scripts/fmt_set.sh pddl/objset.h
	$(BASH) scripts/fmt_set.sh set Set long l L L <$< >$@
pddl/cset.h: src/_set_arr.h scripts/fmt_set.sh
	$(BASH) scripts/fmt_set.sh set Set long c C C <$< >$@
src/cset.c: src/_set_arr.c scripts/fmt_set.sh pddl/objset.h
	$(BASH) scripts/fmt_set.sh set Set long c C C <$< >$@
pddl/iarr.h: src/_arr.h scripts/fmt_set.sh
	$(BASH) scripts/fmt_set.sh arr Arr int i I I <$< >$@
src/iarr.c: src/_arr.c scripts/fmt_set.sh
	$(BASH) scripts/fmt_set.sh arr Arr int i I I <$< >$@

.objs/bdd.o: src/bdd.c pddl/bdd.h pddl/config.h $(GEN)
	$(CC) $(CFLAGS) $(CUDD_CFLAGS) -c -o $@ $<
.objs/sym.o: src/sym.c pddl/sym.h pddl/config.h $(GEN)
	$(CC) $(CFLAGS) $(BLISS_CFLAGS) -c -o $@ $<
.objs/clique.o: src/clique.c pddl/clique.h pddl/config.h $(GEN)
	$(CC) $(CFLAGS) $(CLIQUER_CFLAGS) -c -o $@ $<
.objs/asnets_dynet.o: src/asnets_dynet.c pddl/config.h $(GEN)
	$(CC) $(CFLAGS) $(DYNET_CFLAGS) -c -o $@ $<
.objs/lp-%.o: src/lp-%.c src/_lp.h pddl/lp.h pddl/config.h $(GEN)
	$(CC) $(CFLAGS) $(LP_CFLAGS) -c -o $@ $<
.objs/__sqlite3.o: src/sqlite3.c
	$(CC) $(SQLITE_CFLAGS) -c -o $@ $<

.objs/cp-cp-optimizer.cpp.o: src/cp-cp-optimizer.cpp src/_cp.h pddl/cp.h pddl/config.h $(GEN)
	$(CXX) $(CPPFLAGS) $(CPOPTIMIZER_CPPFLAGS) -c -o $@ $<
.objs/asnets_dynet.cpp.o: src/asnets_dynet.cpp pddl/asnets.h pddl/config.h $(GEN)
	$(CXX) $(CPPFLAGS) $(DYNET_CPPFLAGS) -c -o $@ $<

.objs/%.o: src/%.c pddl/%.h pddl/config.h $(GEN)
	$(CC) $(CFLAGS) -c -o $@ $<
.objs/%.o: src/%.c pddl/config.h $(GEN)
	$(CC) $(CFLAGS) -c -o $@ $<
.objs/%.cpp.o: src/%.cpp pddl/%.h pddl/config.h $(GEN)
	$(CXX) $(CPPFLAGS) -c -o $@ $<
.objs/%.cpp.o: src/%.cpp pddl/config.h $(GEN)
	$(CXX) $(CPPFLAGS) -c -o $@ $<

%.h: pddl/config.h
%.c: pddl/config.h


clean:
	rm -f .objs/*.o
	rm -f $(TARGETS)
	rm -f pddl/config.h
	rm -f src/*.pb.{cc,h}
	rm -f $(GEN)
	if [ -d bin ]; then $(MAKE) -C bin clean; fi;
	if [ -f t/Makefile ]; then $(MAKE) -C t clean; fi;

c:
	rm -f .objs/[a-zA-Z0-9]*.o
	rm -f .objs/_[a-zA-Z0-9]*.o
	rm -f $(TARGETS)
	rm -f pddl/config.h
	rm -f $(GEN)
	if [ -d bin ]; then $(MAKE) -C bin clean; fi;
	if [ -f t/Makefile ]; then $(MAKE) -C t clean; fi;

mrproper: clean third-party-clean

check check-all check-valgrind check-all-valgrind check-segfault check-all-segfault check-gdb check-all-gdb:
	if [ -f t/Makefile ]; then $(MAKE) -C t $@; fi
static-check:
	$(CPPCHECK) $(CPPCHECK_FLAGS) pddl/ src/

doc:
	$(MAKE) -C doc

analyze: clean
	$(SCAN_BUILD) $(MAKE)

tidy:
	find src/ -name '*.c' \
              -a -not -name google-city-hash.c \
              -a -not -name sqlite3.c \
              -a -not -name toml.c \
              -a -not -name sha256.c \
        | xargs -n1 bash scripts/tidy-code.sh
	find pddl/ -name '*.h' \
              -a -not -name google-city-hash.c \
              -a -not -name sqlite3.c \
              -a -not -name toml.c \
              -a -not -name sha256.c \
        | xargs -n1 bash scripts/tidy-code.sh

list-global-symbols: libpddl.a
	readelf -s libpddl.a \
        | grep GLOBAL \
        | awk '{print $$8}' \
        | sort \
        | uniq \
        | grep -v '^pddl' \
        | grep -v '^_pddl' \
        | grep -v '^__pddl' \
        | grep -v '^_Z.*Ilo' \
        | grep -v '^_Z.*Ilo' \
        | grep -v '^_Z.*dynet' \
        | grep -v '^CPX' \
        | grep -v '^GRB' \
        | grep -v '^glp_' \
        | grep -v '^bliss_' \
        | grep -v '^Cudd_' \
        | less

third-party: bliss cudd
third-party-clean: bliss-clean cudd-clean

bliss: third-party/bliss/libbliss.a
bliss-clean:
	$(MAKE) -C third-party/bliss clean
	rm -f third-party/bliss/libbliss.a
	rm -f third-party/bliss/bliss_C.h
third-party/bliss/libbliss.a:
	$(MAKE) CC=$(CXX) -C third-party/bliss lib_static
	cp third-party/bliss/src/bliss_C.h third-party/bliss/
	mv third-party/bliss/libbliss_static.a $@

cudd: third-party/cudd/libcudd.a
cudd-clean:
	git clean -fdx third-party/cudd
	rm -f third-party/cudd/lib*.a
	rm -f third-party/cudd/cudd.h
third-party/cudd/libcudd.a:
	cd third-party/cudd && aclocal
	cd third-party/cudd && autoconf
	cd third-party/cudd && automake
	cd third-party/cudd && ./configure --disable-shared CC=$(CC) CXX=$(CXX)
	$(MAKE) -C third-party/cudd
	cp third-party/cudd/cudd/.libs/libcudd.a $@
	cp third-party/cudd/cudd/cudd.h third-party/cudd/cudd.h

sqlite-amalgam:
	unzip $(SQLITE_SRC_ZIP)
	mv sqlite-src-*/ sqlite
	cd sqlite/ && ./configure --disable-json --disable-load-extension
	cd sqlite/ && make OPTS="$(SQLITE_GEN_CFLAGS)" sqlite3.c
	cat sqlite/sqlite3.c | sed 's/sqlite3/pddl_sqlite3/g' >src/sqlite3.c
	cat sqlite/sqlite3.h | sed 's/sqlite3/pddl_sqlite3/g' >src/sqlite3.h
	rm -rf sqlite/

.PHONY: all bin clean help doc install analyze \
  examples mrproper \
  check check-all \
  check-valgrind check-all-valgrind \
  check-segfault check-all-segfault \
  check-gdb check-all-gdb \
  third-party third-party-clean \
  bliss bliss-clean \
  sqlite-amalgam
