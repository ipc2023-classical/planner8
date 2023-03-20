-include ../Makefile.config
-include ../Makefile.include

CFLAGS += -I../
LDFLAGS += -L../ -lpddl
LDFLAGS += $(CPOPTIMIZER_LDFLAGS)
LDFLAGS += $(LP_LDFLAGS)
LDFLAGS += $(BLISS_LDFLAGS)
LDFLAGS += $(CLIQUER_LDFLAGS)
LDFLAGS += $(CUDD_LDFLAGS)
LDFLAGS += $(DYNET_LDFLAGS)
LDFLAGS += -pthread -lm

ifeq '$(BIN_STATIC)' 'yes'
  LDFLAGS += -static
endif

TARGETS = pddl
TARGETS += pddl-fdr
TARGETS += pddl-symba
TARGETS += pddl-pddl
TARGETS += pddl-lplan
ifeq '$(USE_DYNET)' 'yes'
  TARGETS += pddl-asnets
endif

OBJS  = opts.o
OBJS += options.o
OBJS += process_strips.o
OBJS += report_lifted_mgroups.o
OBJS += report_mgroups.o
OBJS += lifted_planner.o

ASNETS_OBJS = opts.o

EXP_OBJS = opts.o

all: $(TARGETS)

exp: exp.c $(EXP_OBJS) ../libpddl.a
	$(CC) $(CFLAGS) -o $@ $< $(EXP_OBJS) $(LDFLAGS)
pddl: main.c ../libpddl.a $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)
pddl-fdr: main.c ../libpddl.a $(OBJS)
	$(CC) $(CFLAGS) -DBIN_PDDL_FDR=1 -o $@ $< $(OBJS) $(LDFLAGS)
pddl-symba: main.c ../libpddl.a $(OBJS)
	$(CC) $(CFLAGS) -DBIN_PDDL_SYMBA=1 -o $@ $< $(OBJS) $(LDFLAGS)
pddl-pddl: main.c ../libpddl.a $(OBJS)
	$(CC) $(CFLAGS) -DBIN_PDDL_PDDL=1 -o $@ $< $(OBJS) $(LDFLAGS)
pddl-lplan: main.c ../libpddl.a $(OBJS)
	$(CC) $(CFLAGS) -DBIN_PDDL_LPLAN=1 -o $@ $< $(OBJS) $(LDFLAGS)
pddl-asnets: asnets.c ../libpddl.a $(ASNETS_OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(ASNETS_OBJS) $(LDFLAGS)

report_%.o: report_%.c report.h
	$(CC) $(CFLAGS) -c -o $@ $<
%.o: %.c %.h
	$(CC) $(CFLAGS) -c -o $@ $<
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGETS)
	rm -f *.o

.PHONY: all
