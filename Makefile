###############################################################################
# Configuration Section
###############################################################################

# Compiler and flags
CXX         := g++
# Use -O3 for release builds, consider -g for debugging
CXXFLAGS    := -std=c++20 -O3 -Wall -Wextra -Iinclude -fopenmp -Wno-unknown-pragmas

# User configurable options (can be overridden from command line, e.g., make FINE=1)
COARSE      ?= 1
FINE        ?= 1 # Set default to 1 if fine is implemented
LOCKFREE    ?= 1 # Set default to 1 if lockfree is implemented
THREAD_COUNT ?= 8 # Default thread count for parallel tests/benchmarks

# Add preprocessor definitions based on configuration
ifeq ($(COARSE),1)
    CXXFLAGS += -DUNIONFIND_COARSE_ENABLED=1
endif

ifeq ($(FINE),1)
    CXXFLAGS += -DUNIONFIND_FINE_ENABLED=1
endif

ifeq ($(LOCKFREE),1)
    CXXFLAGS += -DUNIONFIND_LOCKFREE_ENABLED=1
    # Add -mcx16 flag if using GCC/Clang on x86-64 to ensure CMPXCHG16B is available/expected
    # This might not be strictly necessary with modern compilers but can help.
    CXXFLAGS += -mcx16
endif

# Pass default thread count (can be used by benchmark/test setup if needed)
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

# Test sources
TEST_SERIAL_SRC   := tests/test_serial_correctness.cpp
TEST_PARALLEL_SRC := tests/test_parallel_correctness.cpp

# Test executable names
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

# Define targets that don't correspond to files
.PHONY: all clean test run_tests

# Build all targets: library, test executables, and benchmark executable.
all: $(LIB_NAME) $(TEST_SERIAL_BIN) $(TEST_PARALLEL_BIN) $(BENCHMARK_BIN)

# Build and run the correctness tests.
# Depends only on the test executables. Builds them if needed.
test: $(TEST_SERIAL_BIN) $(TEST_PARALLEL_BIN)
	@echo "Running serial correctness test..."
	@./$(TEST_SERIAL_BIN)
	@echo ""
	@echo "Running parallel correctness test..."
	@./$(TEST_PARALLEL_BIN)

# Target to explicitly run tests without necessarily rebuilding (if already built).
# Useful if you just want to re-run.
run_tests:
	@echo "Running serial correctness test..."
	@./$(TEST_SERIAL_BIN)
	@echo ""
	@echo "Running parallel correctness test..."
	@./$(TEST_PARALLEL_BIN)

# Clean up generated files.
clean:
	@echo "Cleaning..."
# IMPORTANT: The next line MUST start with a Tab character, not spaces.
	rm -f $(OBJ_FILES) $(LIB_NAME) $(TEST_SERIAL_BIN) $(TEST_PARALLEL_BIN) $(BENCHMARK_BIN) src/*.o tests/*.o benchmarks/*.o *~ core.*

###############################################################################
# Library Target: Build static library
###############################################################################

$(LIB_NAME): $(OBJ_FILES)
	@echo "Archiving $(LIB_NAME)..."
# IMPORTANT: The next line MUST start with a Tab character, not spaces.
	ar rcs $(LIB_NAME) $(OBJ_FILES)

###############################################################################
# Pattern Rule for Object Files (compiles .cpp files into .o files)
###############################################################################

# This rule applies to any .cpp file in src/, tests/, or benchmarks/
# Output .o files will be placed in the same directory as the .cpp file.
%.o: %.cpp
	@echo "Compiling $< ..."
# IMPORTANT: The next line MUST start with a Tab character, not spaces.
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Note: The pattern rule above might place .o files alongside source files.
# If you prefer all .o files in one place (e.g., an 'obj' directory),
# the rules would need adjustment (e.g., using VPATH or more specific rules).
# The current setup is simpler and often sufficient.

###############################################################################
# Linking Test Executables
###############################################################################

$(TEST_SERIAL_BIN): $(TEST_SERIAL_SRC) $(LIB_NAME)
	@echo "Linking $@ ..."
# IMPORTANT: The next line MUST start with a Tab character, not spaces.
	$(CXX) $(CXXFLAGS) $(TEST_SERIAL_SRC) -o $(TEST_SERIAL_BIN) -L. -lunionfind -fopenmp

$(TEST_PARALLEL_BIN): $(TEST_PARALLEL_SRC) $(LIB_NAME)
	@echo "Linking $@ ..."
# IMPORTANT: The next line MUST start with a Tab character, not spaces.
	# Add -latomic if lockfree implementation is enabled and might be tested
	$(CXX) $(CXXFLAGS) $(TEST_PARALLEL_SRC) -o $(TEST_PARALLEL_BIN) -L. -lunionfind -fopenmp $(if $(filter 1,$(LOCKFREE)),-latomic)

###############################################################################
# Linking Benchmark Executable
###############################################################################

# Rule to link the benchmark executable (target name is 'benchmark')
$(BENCHMARK_BIN): $(BENCHMARK_SRC) $(LIB_NAME)
	@echo "Linking $@ ..."
# IMPORTANT: The next line MUST start with a Tab character, not spaces.
	# Ensure benchmark links with OpenMP flags if it uses OpenMP directly
	# Add -latomic explicitly since the benchmark needs to link lockfree code
	$(CXX) $(CXXFLAGS) $(BENCHMARK_SRC) -o $(BENCHMARK_BIN) -L. -lunionfind -fopenmp -latomic

