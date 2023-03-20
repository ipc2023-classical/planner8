# cpddl

**cpddl** is a library for automated planning.

## License

cpddl is licensed under OSI-approved 3-clause BSD License, text of license
is distributed along with source code in BSD-LICENSE file.

The library depends on [sqlite](https://www.sqlite.org/index.html) which is
[public-domain](https://www.sqlite.org/copyright.html) and it is already part
of the source code. Other than that, cpddl can be compiled without any other
dependecies. However, certain functionalities require external libraries:
 - symmetries (``pddl/sym.h``) require
 [bliss](https://users.aalto.fi/~tjunttil/bliss) library licensed under LGPL
 - binary decision diagrams (``pddl/bdd.h``) require
 [cudd](https://davidkebo.com/cudd) library licensed under BSD 3
 - (I)LP solver (``pddl/lp.h``) requires
 [CPLEX Optimizer](https://www.ibm.com/analytics/cplex-optimizer),
 [Gurobi](https://www.gurobi.com/),
 [GLPK](https://www.gnu.org/software/glpk/), or
 [HiGHS](https://highs.dev). CPLEX Optimizer and
 Gurobi are commercial products, but it is possible to obtain an academic
 license, GLPK is licensed under GPLv3, and HiGHS is licensed under MIT
 license.
 - constraint optimization (``pddl/cp.h``) requires either
 [CPLEX CP Optimizer](https://www.ibm.com/analytics/cplex-cp-optimizer), or
 [minizinc](https://www.minizinc.org/). CPLEX CP Optimizer is a commercial
 library, but it is possible to obtain an academic license, and minizinc is
 licensed under Mozilla Public License v2.0 (and itself depends on other
 solvers), but it is called as a subprocess from cpddl, i.e., it is never
 statically or dynamically linked to cpddl.

## Compile

Easiest way to compile the library and the binaries that come with the
library:
```sh
  $ ./scripts/build.sh
```
This builds the library with [bliss](https://users.aalto.fi/~tjunttil/bliss)
and [cudd](https://davidkebo.com/cudd) libraries which are compiled from local
copies in ``third-party/`` directory. It also tries to automatically find
[GLPK](https://www.gnu.org/software/glpk/) and
[minizinc](https://www.minizinc.org/) installed on your system.

You can change default configuration by adding ``Makefile.config`` file
containing the new configuration (see ``Makefile.config.tpl``):
 - The easiest way to integrate CPLEX is to install
 [IBM ILOG CPLEX Optimization Studio](https://www.ibm.com/products/ilog-cplex-optimization-studio)
 and set the variable ``IBM_CPLEX_ROOT`` to the top installation directory
 of the CPLEX studio. However, you can also set ``CPLEX_CFLAGS``,
 ``CPLEX_LDFLAGS``, ``CPOPTIMIZER_CPPFLAGS``, and ``CPOPTIMIZER_LDFLAGS``
 variables separately.
 - Gurobi can be used by setting up the ``GUROBI_CFLAGS`` and
 ``GUROBI_LDFLAGS`` variables.
 - If GLPK is not automatically found, you can explicitly set ``GLPK_CFLAGS``
 and ``GLPK_LDFLAGS`` variables.
 - If minizinc is not automatically found, set ``MINIZINC_BIN`` variable to
 the absolute path of the minizinc program.

You can check the current configuration by
```sh
  $ make help
```


## References
The inference of **fact-alternating mutex groups** (``pddl/famgroup.h``) is
described in
 - Daniel Fišer, Antonín Komenda.
Fact-Alternating Mutex Groups for Classical Planning,
JAIR 61: 475-521 (2018)

The inference of **lifted fact-alternating mutex groups**
(``pddl/lifted_mgroup*.h``) is described in
- Daniel Fišer.
Lifted Fact-Alternating Mutex Groups and Pruned Grounding of Classical
Planning Problems, AAAI 2020

Pruning of unreachable and dead-end operators on the PDDL level
(``pddl/compile_in_lifted_mgroup.h``) is described in
- Daniel Fišer.
Operator Pruning using Lifted Mutex Groups via Compilation on Lifted Level,
ICAPS 2023

**Operator mutexes** (``pddl/op_mutex*.h``) are described in
 - Daniel Fišer, Álvaro Torralba, Alexander Shleyfman.
Operator Mutexes and Symmetries for Simplifying Planning Tasks,
AAAI 2019, 7586-7593

**Multi-fact disambiguations** and potential heuristics strenghtened with
disambiguations (``pddl/{pot.h,hpot.h,disambiguation.h}``) are described in
 - Daniel Fišer, Rostislav Horčík, Antonín Komenda.
Strengthening Potential Heuristics with Mutexes and Disambiguations,
ICAPS 2020

**Endomorphisms/Homomorphism**
(``pddl/endomorphism.h``, ``pddl/homomorphism*.h``) are described in
 - Rostislav Horčík, Daniel Fišer, Álvaro Torralba
Homomorphisms of Lifted Planning Tasks: The Case for Delete-free Relaxation Heuristics,
AAAI 2022
 - Rostislav Horčík, Daniel Fišer.
Endomorphisms of Classical Planning Tasks,
AAAI 2021
 - Rostislav Horčík, Daniel Fišer.
Endomorphisms of Lifted Planning Problems,
ICAPS 2021

**Custom-design FDR encodings** (``pddl/red_black_fdr.h``)
 - Daniel Fišer, Daniel Gnad, Michael Katz, Jörg Hoffmann
Custom-Design of FDR Encodings: The Case of Red-Black Planning,
IJCAI 2021

**Symbolic search** (``pddl/symbolic*.h``)
 - Daniel Fišer, Álvaro Torralba, Jörg Hoffmann.
Operator-Potentials in Symbolic Search: From Forward to Bi-Directional Search,
ICAPS 2022
 - Daniel Fišer, Álvaro Torralba, Jörg Hoffmann.
Operator-Potential Heuristics for Symbolic Search,
AAAI 2022

Please refer to these papers when documenting work that uses the corresponding
parts of cpddl.

