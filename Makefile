###############################################################################
# Configuration Section
###############################################################################

# Compiler and flags
CXX         := g++
CXXFLAGS    := -std=c++20 -O3 -Wall -Wextra -Iinclude -fopenmp -Wno-unknown-pragmas

# User configurable options
COARSE      ?= 1
FINE        ?= 0
LOCKFREE    ?= 0
THREAD_COUNT ?= 8

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

# Source files for union-find library (serial and parallel implementations)
SRC_FILES := src/union_find.cpp

ifeq ($(COARSE),1)
    SRC_FILES += src/union_find_parallel_coarse.cpp
endif

ifeq ($(FINE),1)
    SRC_FILES += src/union_find_parallel_fine.cpp
endif

ifeq ($(LOCKFREE),1)
    SRC_FILES += src/union_find_parallel_lockfree.cpp
endif

# Generate object file names from source files.
OBJ_FILES := $(SRC_FILES:.cpp=.o)

# Library output name
LIB_NAME    := libunionfind.a

###############################################################################
# Test Executables
###############################################################################

# Test sources (adjust as needed)
TEST_SERIAL_SRC   := tests/test_serial_correctness.cpp
TEST_PARALLEL_SRC := tests/test_parallel_correctness.cpp

# Executable names
TEST_SERIAL_BIN   := test_serial_correctness
TEST_PARALLEL_BIN := test_parallel_correctness

###############################################################################
# Benchmark Executables
###############################################################################

BENCHMARK_SRC := benchmarks/benchmark.cpp
BENCHMARK_BIN := benchmark

###############################################################################
# Primary Targets
###############################################################################

.PHONY: all clean test run_tests

# Build all targets: library and test executables.
all: $(LIB_NAME) $(TEST_SERIAL_BIN) $(TEST_PARALLEL_BIN) $(BENCHMARK_BIN)

# Build and run the tests.
test: all run_tests

# Run tests sequentially.
run_tests: $(TEST_SERIAL_BIN) $(TEST_PARALLEL_BIN)
	@echo "Running serial test..."
	@./$(TEST_SERIAL_BIN)
	@echo ""
	@echo "Running parallel test..."
	@./$(TEST_PARALLEL_BIN)

# Clean up generated files.
clean:
    @echo "Cleaning..."
    rm -f $(OBJ_FILES) $(LIB_NAME) $(TEST_SERIAL_BIN) $(TEST_PARALLEL_BIN) $(BENCHMARK_BIN) # Add benchmark bin here
    # Also clean any object files specific to benchmark if needed

###############################################################################
# Library Target: Build static library
###############################################################################

$(LIB_NAME): $(OBJ_FILES)
	@echo "Archiving $(LIB_NAME)..."
	ar rcs $(LIB_NAME) $(OBJ_FILES)

###############################################################################
# Pattern Rule for Object Files
###############################################################################

%.o: %.cpp
	@echo "Compiling $< ..."
	$(CXX) $(CXXFLAGS) -c $< -o $@

###############################################################################
# Linking Test Executables
###############################################################################

$(TEST_SERIAL_BIN): $(TEST_SERIAL_SRC) $(LIB_NAME)
	@echo "Linking $@ ..."
	$(CXX) $(CXXFLAGS) $(TEST_SERIAL_SRC) -o $(TEST_SERIAL_BIN) -L. -lunionfind

$(TEST_PARALLEL_BIN): $(TEST_PARALLEL_SRC) $(LIB_NAME)
	@echo "Linking $@ ..."
	$(CXX) $(CXXFLAGS) $(TEST_PARALLEL_SRC) -o $(TEST_PARALLEL_BIN) -L. -lunionfind

###############################################################################
# Linking Benchmark Executable
###############################################################################
# Rule to link the benchmark executable
$(BENCHMARK_BIN): $(BENCHMARK_SRC) $(LIB_NAME)
    @echo "Linking $@ ..."
    $(CXX) $(CXXFLAGS) $(BENCHMARK_SRC) -o $(BENCHMARK_BIN) -L. -lunionfind
