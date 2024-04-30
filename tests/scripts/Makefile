# Execution
NUM_RUNS=30
TIMEOUT=7200

# Commands
CC=clang
CPP=clang++
OPT=opt

# Libraries
LIBS=-lm -lstdc++ -lpthread

# Set the runtime flags
RUNTIME_CFLAGS="-DDEBUG"

# Parallelization options
# 	- To dump the PDG: -noelle-pdg-dump
PARALLELIZATION_OPTIONS=-alloc-aa-verbose=1 -noelle-parallelizer-force 
NOELLE_OPTIONS=-noelle-pdg-verbose=1 -noelle-verbose=2 -noelle-min-hot=0 -noelle-inliner-avoid-hoist-to-main
TOOLS_OPTIONS=
OPT_LEVEL=-O3

# Front-end
INCLUDES=-I../../include/threadpool/include
FRONTEND_OPTIONS=-O1 -Xclang -disable-llvm-passes 
PRE_MIDDLEEND_OPTIONS=-O0 #-mem2reg -simplifycfg -simplifycfg-sink-common=false -instcombine -tailcallelim -loop-simplify -lcssa -licm -globalopt -instcombine -ipsccp -gvn -dse -adce -loop-simplify -lcssa -indvars -loop-deletion -instcombine -indvars
FRONTEND_FLAGS=-emit-llvm $(FRONTEND_OPTIONS)

PARALLEL_PLAIN=parallelized_plain
PARALLEL_SPEC=parallelized_spec
OPTIMIZED=parallelized
OPTIMIZED_AUTOTUNED=parallelized_autotuned
PRINT_OUTPUT_FILE=test.txt
# For benchmarks with more than one source file
BMARK=test
OBJFILES_BC?= $(shell find . -name \*.c  ! -name 'NOELLE_APIs.c' | sed -e "s/\.c/.bc/" ) $(shell find . -name \*.cpp ! -name 'Parallelizer_utils.cpp' | sed -e "s/\.cpp/.bc/" )

# Autotuner
# Input to run the autotuner
INPUT=input.txt
# Time limit for the autotuner to run (in seconds)
TIME=100
# Max number of configurations the autotuner explores
NUM_CONF=500
# Seed configuration the autotuner will start with
SEED_CONF=
# Disable generation of search space (implies the user will provide autotuner_space.info)
NOSEARCHSPACE=
# Disable filtering of search space that forces DOALL on loops that we know are DOALL at compile time
NODOALLFILTERING=
# Disable filtering of search space that checks if parallelizing one DOALL loop at a time yields a better execution time
NODOALLTIMEFILTERING=

PROMPT_DRIVER=/scratch/yc0769/parallelizer-workspace/PROMPT/scripts/prompt_driver.py

all: baseline $(OPTIMIZED)

%.bc: %.c
	$(CC) $(INCLUDES) $(FRONTEND_FLAGS) -c $< -o $@
	opt $(PRE_MIDDLEEND_OPTIONS) $@ -o $@

%.bc: %.cpp
	$(CPP) $(INCLUDES) $(FRONTEND_FLAGS) -c $< -o $@
	opt $(PRE_MIDDLEEND_OPTIONS) $@ -o $@

$(BMARK).bc: $(OBJFILES_BC)
	llvm-link $^ -o $@

baseline: baseline.bc
	$(CPP) $(OPT_LEVEL) $^ -o $@

Parallelizer_utils.bc: Parallelizer_utils.cpp
	$(CPP) $(RUNTIME_CFLAGS) $(INCLUDES) -std=c++14 -emit-llvm $(OPT_LEVEL) -c $^ -o $@

$(OPTIMIZED): test_parallelized.bc
	$(CPP) -std=c++14 -pthreads $(OPT_LEVEL) $< $(LIBS) -o $@

$(OPTIMIZED_AUTOTUNED): test_parallelized_autotuned.bc
	$(CPP) -std=c++14 -pthreads $(OPT_LEVEL) $< $(LIBS) -o $@

test_parallelized.bc: named_with_metadata.bc Parallelizer_utils.bc
#test_parallelized.bc: baseline_with_metadata.bc Parallelizer_utils.bc
	gino $< -o $@ $(NOELLE_OPTIONS) $(PARALLELIZATION_OPTIONS)

test_parallelized_autotuned.bc: baseline_with_metadata.bc input.txt Parallelizer_utils.bc
	gino-autotuner --parallelizer-args="$< -o $@ $(NOELLE_OPTIONS) $(PARALLELIZATION_OPTIONS)" --libs="$(LIBS)" --input="$(INPUT)" --autotuner-time="$(TIME)" --autotuner-numconf="$(NUM_CONF)" --autotuner-seedconf="$(SEED_CONF)" $(NOSEARCHSPACE) $(NODOALLFILTERING) $(NODOALLTIMEFILTERING)

baseline.bc: $(BMARK).bc
	$(CPP) $(OPT_LEVEL) -c -emit-llvm $^ -o $@

# SLAMP
benchmark.result.slamp.profile: named_with_metadata.bc profile.in
	PROFILEARGS=`cat input.txt` python $(PROMPT_DRIVER) --named code_to_parallelize.bc -m dep-context -t 32 --target-loop $(TARGETLOOP) --target-fcn $(TARGETFCN) --output benchmark.result.slamp.profile

result.slamp.profile: benchmark.result.slamp.profile
	cat benchmark.result.slamp.profile >> result.slamp.profile

named_with_metadata.bc: baseline_with_metadata.bc
	python $(PROMPT_DRIVER) baseline_with_metadata.bc -m dep-context --skip-run --stop-at-named 
	mv baseline_with_metadata.named.bc named_with_metadata.bc

baseline_with_metadata.bc: default.profraw baseline_pre.bc
	noelle-meta-prof-embed $^ -o $@
	noelle-meta-pdg-embed $(NOELLE_OPTIONS) $@ -o $@

baseline_with_runtime_prof: test.bc
	noelle-prof-coverage $^ $@ $(LIBS)

pre_profiles.profraw: baseline_with_runtime_prof input.txt
	./$< `cat input.txt` 
	mv default.profraw $@

baseline_pre.bc: pre_profiles.profraw test.bc
	noelle-meta-prof-embed $^ -o $@
	gino-pre $@ $(NOELLE_OPTIONS) $(TOOLS_OPTIONS)

default.profraw: baseline_pre_prof input.txt
	./$< `cat input.txt` 

baseline_pre_prof: baseline_pre.bc
	noelle-prof-coverage $^ $@ $(LIBS)

input.txt:
	@../../scripts/create_input.sh $@

test_correctness: input.txt baseline $(OPTIMIZED)
	@../../scripts/test_correctness.sh $(NUM_RUNS)

check_correctness: input.txt baseline $(PARALLEL_PLAIN) $(PARALLEL_SPEC)
	./baseline `cat input.txt` > output_baseline.txt 2>&1 
	./$(PARALLEL_PLAIN) `cat input.txt` > output_parallelized_wo_spec.txt 2>&1
	./$(PARALLEL_SPEC) `cat input.txt` > output_parallelized_spec.txt 2>&1

check_performance: perf_input.txt baseline $(PARALLEL_PLAIN) $(PARALLEL_SPEC)
	regressions-watchdog $(TIMEOUT) baseline.time ./baseline `cat perf_input.txt`
	regressions-watchdog $(TIMEOUT) parallel.plain.time ./$(PARALLEL_PLAIN) `cat perf_input.txt`
	regressions-watchdog $(TIMEOUT) parallel.spec.time ./$(PARALLEL_SPEC) `cat perf_input.txt`

check_gino: perf_input.txt baseline $(OPTIMIZED)
	regressions-watchdog $(TIMEOUT) baseline.time ./baseline `cat perf_input.txt`
	regressions-watchdog $(TIMEOUT) parallel.plain.time ./$(OPTIMIZED) `cat perf_input.txt`

clean:
	rm -f *.bc *.dot *.jpg *.ll *.S *.s *.o baseline $(OPTIMIZED) $(OPTIMIZED_AUTOTUNED) *.prof *.profraw *prof .*.dot
	rm -f time_parallelized.txt compiler_output.txt ;
	rm -f output*.txt ;
	rm -f OUT ;
	rm -f *.exe
	rm -rf binaryToRun baselineTime.txt executionTime.txt exploredConfs.json opentuner.db opentuner.log ;

clean-prompt:
	rm *result.slamp.profile target_list slamp.time

.PHONY: test_correctness clean
