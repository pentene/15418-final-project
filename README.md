# 15418-final-project

# Parallel Union-Find Implementations in C++/OpenMP

This project implements and compares several sequential and parallel versions of the Union-Find (Disjoint Set Union) data structure using C++20 and OpenMP. The goal is to analyze the performance and scalability trade-offs of different parallelization strategies, including coarse-grained locking, fine-grained locking, and lock-free techniques.

## Features

* **Sequential Baseline:** An optimized serial Union-Find implementation (`UnionFind`).
* **Coarse-Grained Locking:** Parallel execution protected by a single global mutex (`UnionFindParallelCoarse`).
* **Fine-Grained Locking:** Parallel execution using per-element locks (primarily for roots) during union operations, with best-effort path compression (`UnionFindParallelFine`).
* **Lock-Free (Baseline):** Lock-free implementation using `std::atomic<int>` encoding parent/rank and Compare-and-Swap (CAS) based path compression (`UnionFindParallelLockFree`).
* **Lock-Free Optimizations:**
    * Path compaction using plain atomic writes (`UnionFindParallelLockFreePlainWrite`).
    * Immediate Parent Check (IPC) heuristic (`UnionFindParallelLockFreeIPC`).
* **Dataset Generator:** Python script to generate workloads with varying parameters (size, operation mix, contention).
* **Correctness Test:** Verifies parallel implementations against the serial baseline based on final connectivity.
* **Benchmark Suite:** Measures performance (wall-clock time) of different implementations under various workloads and thread counts.

## Requirements

* **Compiler:** A C++ compiler supporting C++20 and OpenMP (e.g., `g++` version 10 or later).
* **Build System:** `make`.
* **Dataset Generation:** Python 3.
* **Operating System:** Developed and tested on Linux. Lock-free implementations rely on efficient native atomic support.

## Directory Structure
.
├── Makefile                # Build configuration
├── README.md               # This file
├── datasets/
├── include/                # Header files (.hpp)
│   ├── union_find.hpp
│   ├── union_find_parallel_coarse.hpp
│   ├── union_find_parallel_fine.hpp
│   ├── union_find_parallel_lockfree.hpp
│   ├── union_find_parallel_lockfree_ipc.hpp
│   └── union_find_parallel_lockfree_plain_write.hpp
├── src/                    # Source files (.cpp)
│   ├── union_find.cpp
│   ├── union_find_parallel_coarse.cpp
│   ├── union_find_parallel_fine.cpp
│   ├── union_find_parallel_lockfree.cpp
│   ├── union_find_parallel_lockfree_ipc.cpp
│   └── union_find_parallel_lockfree_plain_write.cpp
├── tests/                  # Test code and resources
│   ├── benchmark.cpp           # Benchmarking program source
│   ├── test_parallel_correctness.cpp # Correctness test source
│   └── test_serial_correctness.cpp   # Simple serial test source
│   └── resources/            # Directory for storing datasets
│       └── (Example datasets...)
├── scripts/
│   └── generate_operations.py  # Python script for generating datasets

## Building the Code

The project uses a `Makefile` for building the library and executables.

**Enabling Parallel Implementations:**

Before building, you can control which parallel implementations are compiled by setting environment variables or modifying the `Makefile`. The key variables are:

* `COARSE`: Set to `1` to enable the Coarse-Grained implementation.
* `FINE`: Set to `1` to enable the Fine-Grained implementation.
* `LOCKFREE`: Set to `1` to enable the baseline Lock-Free implementation.
* `LOCKFREE_PLAIN`: Set to `1` to enable the Lock-Free (Plain Write) implementation.
* `LOCKFREE_IPC`: Set to `1` to enable the Lock-Free (IPC) implementation.

Example: To enable and build all implementations:
```bash
export COARSE=1 FINE=1 LOCKFREE=1 LOCKFREE_PLAIN=1 LOCKFREE_IPC=1
make
```

**Build Commands:**

Simply use `make` to build the project and `make clean` to delete all compiled files

**Generating Datasets:**

Use the generate_operations.py script to create input files for testing and benchmarking.

`python generate_operations.py <n_elements> <n_operations> <output_file> [options]`

Key arguments:

* n_elements: Number of disjoint elements (e.g., 1000000).
* n_operations: Total number of operations (e.g., 10000000).
* output_file: Path to save the generated file (e.g., tests/resources/ops_1M_10M_c0.5.txt).

Options:

* --find-ratio <float>: Target ratio of FIND operations (default: 0.5).
* --sameset-ratio <float>: Target ratio of SAMESET among non-FIND ops (default: 0.1).
* --contention-level <float>: Focus level for hot element (0.0=uniform, 1.0=high focus, default: 0.0).
* --hot-element <int>: Index of the element for focused contention (default: 0).
* --extreme-contention: Flag to force all operations onto elements 0 and 1.
* --seed <int>: Optional random seed for reproducibility.

**Running Correctness Tests:**

Verify parallel implementations against the serial baseline:

`./test_parallel_correctness <operations_file>`

Example:

`./test_parallel_correctness tests/resources/uniform.txt`

The test will output PASS or FAIL for each enabled parallel implementation.

**Running Benchmarks:**

Measure execution time for different implementations:

`./benchmark <implementation_type> <operations_file> <num_runs> [num_threads]`

* <implementation_type>: serial, coarse, fine, lockfree, lockfree_plain, or lockfree_ipc.
* <operations_file>: Path to the dataset file.
* <num_runs>: Number of benchmark repetitions.
* [num_threads]: (Optional) Number of OpenMP threads. Defaults to maximum available.