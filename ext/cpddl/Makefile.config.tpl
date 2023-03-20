# Set up specific compiler
#CC = clang
#CXX = clang++

# Turn on debug build
#DEBUG = yes

# Compile with -Werror
#WERROR = yes

# Turn on compilation with profiling
#PROFIL = yes

# Configuration of IBM CPLEX Optimization Studio
#IBM_CPLEX_ROOT = /opt/cplex/v12.10
#CPLEX_CFLAGS = -I/opt/cplex1271/cplex/include
#CPLEX_LDFLAGS = -L/opt/cplex1271/cplex/lib/x86-64_linux/static_pic/ -lcplex
#CPOPTIMIZER_CPPFLAGS = -I/opt/cplex/v12.10/cpoptimizer/include -I/opt/cplex/v12.10/concert/include/ -I/opt/cplex/v12.10/cplex/include
#CPOPTIMIZER_LDFLAGS = -L/opt/cplex/v12.10/cpoptimizer/lib/x86-64_linux/static_pic/ -lcp -L/opt/cplex/v12.10/concert/lib/x86-64_linux/static_pic/ -lconcert -lstdc++
#USE_CPLEX = no # Do not use CPLEX library
#USE_CPOPTIMIZER = no # Do not use CPOptimizer library

# Configuration of Gurobi optimizer
#GUROBI_CFLAGS = -I/opt/gurobi951/linux64/include
#GUROBI_LDFLAGS = -L/opt/gurobi951/linux64/lib -Wl,-rpath=/opt/gurobi951/linux64/lib -lgurobi95
#USE_GUROBI = no # Do not use Gurobi library

# Configuration of HiGHS library https://highs.dev
#HIGHS_ROOT = /opt/HiGHS
#HIGHS_CFLAGS = -I/opt/HiGHS/include
#HIGHS_LDFLAGS = -L/opt/HiGHS/lib -lhighs
#USE_HIGHS = no # Do not use HiGHS library

# Configuration of Minizinc optimizer
#MINIZINC_BIN = /opt/minizinc/bin/minizinc

# Configuration of GLPK
#USE_GLPK = no # Do not use GLPK library

# Configuration of dynet library https://github.com/clab/dynet.git
#DYNET_ROOT = /opt/dynet
#DYNET_CPPFLAGS = -I/opt/dynet/include
#DYNET_LDFLAGS = -L/opt/dynet/lib -ldynet
