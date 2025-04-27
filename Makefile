###############################################################################
# Configuration Section
###############################################################################

# Compiler and flags
CXX         := g++
# Use -O3 for release builds, consider -g for debugging
# Added -pthread for potential std::thread usage if needed, though OpenMP is primary
CXXFLAGS_BASE := -std=c++20 -O3 -g -Wall -Wextra -Iinclude -fopenmp -pthread -Wno-unknown-pragmas

# User configurable options (can be overridden from command line, e.g., make FINE=1)
COARSE          ?= 1 # Enable Coarse-grained locking version
FINE            ?= 1 # Enable Fine-grained locking version
LOCKFREE        ?= 1 # Enable original Lock-free version (CAS path compression)
LOCKFREE_PLAIN  ?= 1 # Enable Lock-free version with Plain Write path compaction
THREAD_COUNT    ?= 8 # Default thread count for parallel tests/benchmarks


###############################################################################
# Source Files & Object Files Determination
###############################################################################

# Start with base source file (assuming a non-parallel base exists or is needed)
# If no base serial implementation, remove this line or adjust as needed.
# SRC_FILES := src/union_find_serial.cpp # Example if you have a serial base

# Initialize SRC_FILES (start empty if no base serial)
SRC_FILES := src/union_find.cpp 

# Initialize CXXFLAGS with base flags
CXXFLAGS := $(CXXFLAGS_BASE)

# --- Conditionally add implementations ---

ifeq ($(strip $(COARSE)),1)
    SRC_FILES += src/union_find_parallel_coarse.cpp
    CXXFLAGS += -DUNIONFIND_COARSE_ENABLED=1
endif

ifeq ($(strip $(FINE)),1)
    SRC_FILES += src/union_find_parallel_fine.cpp
    CXXFLAGS += -DUNIONFIND_FINE_ENABLED=1
endif

# Check if *any* lockfree version is enabled for common flags/libs
ANY_LOCKFREE := 0
ifeq ($(strip $(LOCKFREE)),1)
    ANY_LOCKFREE := 1
    SRC_FILES += src/union_find_parallel_lockfree.cpp
    CXXFLAGS += -DUNIONFIND_LOCKFREE_ENABLED=1
endif
ifeq ($(strip $(LOCKFREE_PLAIN)),1)
    ANY_LOCKFREE := 1
    SRC_FILES += src/union_find_parallel_lockfree_plain_write.cpp
    CXXFLAGS += -DUNIONFIND_LOCKFREE_PLAIN_ENABLED=1
endif

# Add flags/libs needed for lockfree implementations
ifeq ($(strip $(ANY_LOCKFREE)),1)
    # Add -mcx16 flag if using GCC/Clang on x86-64 for CMPXCHG16B support
    # Check your architecture/compiler if needed. Assumed x86-64 GCC/Clang here.
    CXXFLAGS += -mcx16
    # Linker flag needed for atomic operations library
    LDFLAGS_ATOMIC := -latomic
else
    LDFLAGS_ATOMIC :=
endif


# Now, *after* SRC_FILES is fully determined, define OBJ_FILES for the library
OBJ_FILES := $(SRC_FILES:.cpp=.o)

# Add other flags AFTER conditional ones if needed
CXXFLAGS += -DUNIONFIND_DEFAULT_THREADS=$(THREAD_COUNT)

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
.PHONY: all clean test run_tests benchmark run_benchmark

# Build all targets: library, test executables, and benchmark executable.
all: $(LIB_NAME) $(TEST_SERIAL_BIN) $(TEST_PARALLEL_BIN) $(BENCHMARK_BIN)

# Build and run the correctness tests.
# Depends only on the test executables. Builds them if needed.
test: $(TEST_SERIAL_BIN) $(TEST_PARALLEL_BIN)
	@echo "Running serial correctness test..."
	@./$(TEST_SERIAL_BIN)
	@echo ""
	@echo "Running parallel correctness test..."
	@./$(TEST_PARALLEL_BIN) $(THREAD_COUNT) # Pass thread count if test uses it

# Target to explicitly run tests without necessarily rebuilding (if already built).
# Useful if you just want to re-run.
run_tests:
	@echo "Running serial correctness test..."
	@./$(TEST_SERIAL_BIN)
	@echo ""
	@echo "Running parallel correctness test..."
	@./$(TEST_PARALLEL_BIN) $(THREAD_COUNT) # Pass thread count if test uses it

# Build the benchmark executable
benchmark: $(BENCHMARK_BIN)

# Build and run the benchmark executable
run_benchmark: $(BENCHMARK_BIN)
	@echo "Running benchmark with $(THREAD_COUNT) threads..."
	@./$(BENCHMARK_BIN) $(THREAD_COUNT) # Pass thread count if benchmark uses it

# Clean up generated files.
clean:
	@echo "Cleaning..."
# IMPORTANT: The next line MUST start with a Tab character, not spaces.
	rm -f $(OBJ_FILES) $(LIB_NAME) $(TEST_SERIAL_BIN) $(TEST_PARALLEL_BIN) $(BENCHMARK_BIN) src/*.o tests/*.o benchmarks/*.o *~ core.*

###############################################################################
# Library Target: Build static library
###############################################################################

# Depends on ALL object files determined above
$(LIB_NAME): $(OBJ_FILES)
	@echo "Archiving $(LIB_NAME)..."
# IMPORTANT: The next line MUST start with a Tab character, not spaces.
	ar rcs $(LIB_NAME) $(OBJ_FILES) # Use the final OBJ_FILES list

###############################################################################
# Pattern Rule for Object Files (compiles .cpp files into .o files)
###############################################################################

# This rule applies to any .cpp file found as a prerequisite.
# Output .o files will be placed in the same directory as the .cpp file.
%.o: %.cpp
	@echo "Compiling $< ..."
# IMPORTANT: The next line MUST start with a Tab character, not spaces.
	$(CXX) $(CXXFLAGS) -c $< -o $@

###############################################################################
# Linking Test Executables
###############################################################################

# Link serial test (usually doesn't need OpenMP or atomic, but include CXXFLAGS for consistency)
$(TEST_SERIAL_BIN): $(TEST_SERIAL_SRC) $(LIB_NAME)
	@echo "Linking $@ ..."
# IMPORTANT: The next line MUST start with a Tab character, not spaces.
	$(CXX) $(CXXFLAGS) $(TEST_SERIAL_SRC) -o $(TEST_SERIAL_BIN) -L. -lunionfind

# Link parallel test (needs OpenMP, and atomic if any lockfree enabled)
$(TEST_PARALLEL_BIN): $(TEST_PARALLEL_SRC) $(LIB_NAME)
	@echo "Linking $@ ..."
# IMPORTANT: The next line MUST start with a Tab character, not spaces.
	$(CXX) $(CXXFLAGS) $(TEST_PARALLEL_SRC) -o $(TEST_PARALLEL_BIN) -L. -lunionfind -fopenmp $(LDFLAGS_ATOMIC)

###############################################################################
# Linking Benchmark Executable
###############################################################################

# Link the benchmark executable (needs OpenMP, and atomic if any lockfree enabled)
$(BENCHMARK_BIN): $(BENCHMARK_SRC) $(LIB_NAME)
	@echo "Linking $@ ..."
# IMPORTANT: The next line MUST start with a Tab character, not spaces.
	$(CXX) $(CXXFLAGS) $(BENCHMARK_SRC) -o $(BENCHMARK_BIN) -L. -lunionfind -fopenmp $(LDFLAGS_ATOMIC)

