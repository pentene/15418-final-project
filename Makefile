CXX       := g++
CXXFLAGS  := -std=c++20 -O3 -Wall -Wextra -Iinclude -fopenmp -Wno-unknown-pragmas

# We can allow the user to enable/disable parallel variants (coarse/fine/lockfree)
# and set a default thread count at compile time.
# Usage example:
#   make all COARSE=1 FINE=1 LOCKFREE=0 THREAD_COUNT=8
COARSE    ?= 1
FINE      ?= 1
LOCKFREE  ?= 1
THREAD_COUNT ?= 4

# If you’re using Google Test or Google Benchmark installed system-wide, 
# you may need to tweak these to point to the correct include/library paths.
# GTEST_LIB  := -lgtest -lgtest_main -lpthread
# GBENCH_LIB := -lbenchmark -lpthread

###############################################################################
# Derived Compiler Flags
###############################################################################

ifeq ($(COARSE),1)
  CXXFLAGS += -DUNIONFIND_COARSE_ENABLED=1
endif

ifeq ($(FINE),1)
  CXXFLAGS += -DUNIONFIND_FINE_ENABLED=1
endif

ifeq ($(LOCKFREE),1)
  CXXFLAGS += -DUNIONFIND_LOCKFREE_ENABLED=1
endif

CXXFLAGS += -DUNIONFIND_DEFAULT_THREADS=$(THREAD_COUNT)

###############################################################################
# Source Files & Object Files
###############################################################################

# Serial
SRC_FILES := src/union_find.cpp

# Parallel implementations (conditionally included if user sets them ON)
ifeq ($(COARSE),1)
  SRC_FILES += src/union_find_parallel_coarse.cpp
endif

ifeq ($(FINE),1)
  SRC_FILES += src/union_find_parallel_fine.cpp
endif

ifeq ($(LOCKFREE),1)
  SRC_FILES += src/union_find_parallel_lockfree.cpp
endif

# Object files for the library
OBJ_FILES := $(SRC_FILES:.cpp=.o)

# Test sources
TEST_SERIAL_SRC   := tests/test_serial_correctness.cpp
TEST_PARALLEL_SRC := tests/test_parallel_correctness.cpp

# Benchmark source
BENCH_SRC := benchmarks/bench_union_find.cpp

# Binaries
LIB_NAME   := libunionfind.a
TEST_SERIAL_BIN   := test_serial_correctness
TEST_PARALLEL_BIN := test_parallel_correctness
BENCH_BIN  := bench_union_find

###############################################################################
# Primary Targets
###############################################################################

.PHONY: all clean test bench

# 'all' will build the library, the tests, and the benchmark
all: $(LIB_NAME) $(TEST_SERIAL_BIN) $(TEST_PARALLEL_BIN) $(BENCH_BIN)

# Build just the tests (after building the library)
test: $(TEST_SERIAL_BIN) $(TEST_PARALLEL_BIN)

# Build just the benchmark (after building the library)
bench: $(BENCH_BIN)

# Clean up
clean:
	@echo "Cleaning..."
	rm -f $(OBJ_FILES) \
	      $(TEST_SERIAL_BIN) \
	      $(TEST_PARALLEL_BIN) \
	      $(BENCH_BIN) \
	      $(LIB_NAME)

###############################################################################
# Library Target
###############################################################################

# Static library from our union-find implementations
$(LIB_NAME): $(OBJ_FILES)
	@echo "Archiving $@"
	ar rcs $@ $^

###############################################################################
# Object File Rules
###############################################################################

# Pattern rule to compile .cpp to .o
%.o: %.cpp
	@echo "Compiling $< ..."
	$(CXX) $(CXXFLAGS) -c $< -o $@

###############################################################################
# Test Executables
###############################################################################

# $(TEST_SERIAL_BIN): $(LIB_NAME) $(TEST_SERIAL_SRC)
# 	@echo "Linking $@"
# 	$(CXX) $(CXXFLAGS) $(TEST_SERIAL_SRC) -o $@ -L. -lunionfind $(GTEST_LIB)

# $(TEST_PARALLEL_BIN): $(LIB_NAME) $(TEST_PARALLEL_SRC)
# 	@echo "Linking $@"
# 	$(CXX) $(CXXFLAGS) $(TEST_PARALLEL_SRC) -o $@ -L. -lunionfind $(GTEST_LIB)

###############################################################################
# Benchmark Executable
###############################################################################

# $(BENCH_BIN): $(LIB_NAME) $(BENCH_SRC)
# 	@echo "Linking $@"
# 	$(CXX) $(CXXFLAGS) $(BENCH_SRC) -o $@ -L. -lunionfind $(GBENCH_LIB)

###############################################################################
# Convenience "run" Targets
###############################################################################

# Shortcut to compile and run tests
run_tests: test
	@echo "Running serial tests:"
	@./$(TEST_SERIAL_BIN)
	@echo "Running parallel tests:"
	@./$(TEST_PARALLEL_BIN)

# Shortcut to compile and run benchmark
run_bench: bench
	@echo "Running benchmark:"
	@./$(BENCH_BIN)