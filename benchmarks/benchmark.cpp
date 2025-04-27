#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <chrono>
#include <numeric>
#include <stdexcept>
#include <memory> // For std::unique_ptr
#include <iomanip> // For std::fixed, std::setprecision
#include <omp.h>   // For omp_set_num_threads and omp_get_max_threads
#include <algorithm>  // Added for std::min_element, std::max_element
#include <cmath>      // Added for std::sqrt

// Include your Union-Find implementations
#include "union_find.hpp" // Serial - We'll use its Operation type as canonical
#ifdef UNIONFIND_COARSE_ENABLED // Use defines from Makefile
#include "union_find_parallel_coarse.hpp"
#endif
#ifdef UNIONFIND_FINE_ENABLED
#include "union_find_parallel_fine.hpp"
#endif
#ifdef UNIONFIND_LOCKFREE_ENABLED
#include "union_find_parallel_lockfree.hpp"
#endif

// Use the Operation struct and OperationType defined within the UnionFind class.
// This assumes all implementations (coarse, fine, lockfree) have a compatible
// nested Operation struct and OperationType enum, as intended by the design.
using Operation = UnionFind::Operation;
using OperationType = UnionFind::OperationType;


// Function to load operations from a file
// Format:
// <num_elements> <num_operations>
// <type> <a> <b>  (type: 0 for UNION, 1 for FIND)
// ...
// Now uses std::vector<UnionFind::Operation>
bool load_operations(const std::string& filename, int& n_elements, std::vector<Operation>& ops) {
    std::ifstream infile(filename);
    if (!infile) {
        std::cerr << "Error: Cannot open file: " << filename << std::endl;
        return false;
    }

    size_t n_ops;
    if (!(infile >> n_elements >> n_ops)) {
        std::cerr << "Error: Could not read number of elements and operations from file: " << filename << std::endl;
        return false;
    }
     if (n_elements <= 0) {
        std::cerr << "Error: Invalid number of elements read from file: " << n_elements << std::endl;
        return false;
     }

    ops.clear(); // Clear any previous content
    ops.reserve(n_ops);
    int type_val, a, b;

    for (size_t i = 0; i < n_ops; ++i) {
        if (!(infile >> type_val >> a >> b)) {
            std::cerr << "Error: Failed to read operation " << i+1 << " from file." << std::endl;
            ops.clear(); // Clear partial data
            return false;
        }
        // Basic validation
        // Check element indices against n_elements read from file
        if ((type_val != 0 && type_val != 1) || a < 0 || a >= n_elements || b < 0 || (type_val == 0 && b >= n_elements)) {
            std::cerr << "Error: Invalid operation data at line " << i + 2 << ": type=" << type_val << ", a=" << a << ", b=" << b << " (n_elements=" << n_elements << ")" << std::endl;
            ops.clear();
            return false;
        }

        // Create UnionFind::Operation object directly
        Operation op;
        op.type = (type_val == 0) ? OperationType::UNION_OP : OperationType::FIND_OP;
        op.a = a;
        op.b = b; // b is ignored for FIND_OP by the implementations
        ops.push_back(op);
    }

    if (ops.size() != n_ops) {
         std::cerr << "Warning: Expected " << n_ops << " operations, but read " << ops.size() << "." << std::endl;
         // Decide if this is a fatal error or just a warning
    }

    std::cout << "Successfully loaded " << ops.size() << " operations for " << n_elements << " elements from " << filename << std::endl;
    return true;
}


// --- Main Benchmark Function ---
int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <implementation_type> <operations_file> <num_runs> [num_threads]" << std::endl;
        std::cerr << "  implementation_type: serial, coarse, fine, lockfree" << std::endl;
        std::cerr << "  operations_file: Path to the file containing operations." << std::endl;
        std::cerr << "  num_runs: Number of times to run processOperations for timing." << std::endl;
        std::cerr << "  num_threads (optional): Number of threads for parallel versions (default: max available)." << std::endl;
        return 1;
    }

    std::string impl_type = argv[1];
    std::string ops_file = argv[2];
    int num_runs = std::stoi(argv[3]);
    int num_threads = omp_get_max_threads(); // Default to max threads

    if (argc > 4) {
        num_threads = std::stoi(argv[4]);
        if (num_threads <= 0) {
            std::cerr << "Warning: Invalid number of threads specified (" << argv[4] << "). Using default (" << omp_get_max_threads() << ")." << std::endl;
            num_threads = omp_get_max_threads();
        }
    }

    if (num_runs <= 0) {
        std::cerr << "Error: Number of runs must be positive." << std::endl;
        return 1;
    }


    // --- Load Operations ---
    int n_elements;
    // Use the alias UnionFind::Operation here
    std::vector<Operation> operations;
    if (!load_operations(ops_file, n_elements, operations)) {
        return 1; // Error loading data
    }
    if (operations.empty()) {
        std::cerr << "Error: No operations loaded." << std::endl;
        return 1;
    }


    // --- Configure OpenMP ---
    if (impl_type != "serial") {
        omp_set_num_threads(num_threads);
        std::cout << "Using OpenMP with " << num_threads << " threads." << std::endl;
    } else {
        num_threads = 1; // Serial runs on 1 thread
        std::cout << "Running serial implementation (1 thread)." << std::endl;
    }


    // --- Benchmarking ---
    std::vector<double> durations; // Store durations in milliseconds
    durations.reserve(num_runs);
    std::vector<int> results; // To store results from processOperations

    std::cout << "\nStarting benchmark..." << std::endl;
    std::cout << "Implementation: " << impl_type << std::endl;
    std::cout << "Element Count:  " << n_elements << std::endl;
    std::cout << "Operation Count:" << operations.size() << std::endl;
    std::cout << "Number of Runs: " << num_runs << std::endl;
    std::cout << "Threads:        " << num_threads << std::endl;


    // Lambda to run the benchmark for a given UF type
    // The type of uf_instance will be deduced, and its processOperations
    // should accept std::vector<UnionFind::Operation> (or compatible).
    auto run_benchmark = [&](auto& uf_instance_prototype) {
        // Warm-up run (optional but recommended)
        { // Scope for temporary warm-up instance
            // Create instance based on prototype type
            auto temp_uf = std::make_unique<std::remove_reference_t<decltype(uf_instance_prototype)>>(n_elements);
            std::cout << "Performing warm-up run..." << std::endl;
            temp_uf->processOperations(operations, results); // Pass the vector<UnionFind::Operation>
            std::cout << "Warm-up complete." << std::endl;
        } // temp_uf destroyed here

        // Timed runs
        for (int i = 0; i < num_runs; ++i) {
            // Create a fresh instance for each run to reset state
            auto current_uf = std::make_unique<std::remove_reference_t<decltype(uf_instance_prototype)>>(n_elements);

            auto start_time = std::chrono::high_resolution_clock::now();
            // Pass the vector<UnionFind::Operation> - this should now match expectations
            current_uf->processOperations(operations, results);
            auto end_time = std::chrono::high_resolution_clock::now();

            std::chrono::duration<double, std::milli> duration_ms = end_time - start_time;
            durations.push_back(duration_ms.count());
            std::cout << "Run " << (i + 1) << ": " << duration_ms.count() << " ms" << std::endl;
        }
    };


    // --- Select Implementation and Run Benchmark ---
    try {
        if (impl_type == "serial") {
            UnionFind uf_proto(n_elements); // Create a prototype instance
            run_benchmark(uf_proto);
        }
        #ifdef UNIONFIND_COARSE_ENABLED
        else if (impl_type == "coarse") {
            UnionFindParallelCoarse uf_proto(n_elements);
            run_benchmark(uf_proto);
        }
        #endif
        #ifdef UNIONFIND_FINE_ENABLED
        else if (impl_type == "fine") {
            UnionFindParallelFine uf_proto(n_elements);
            run_benchmark(uf_proto);
        }
        #endif
        #ifdef UNIONFIND_LOCKFREE_ENABLED
        else if (impl_type == "lockfree") {
            UnionFindParallelLockFree uf_proto(n_elements);
            run_benchmark(uf_proto);
        }
        #endif
        else {
            std::cerr << "Error: Unknown implementation type '" << impl_type << "'." << std::endl;
            std::cerr << "Supported types: serial";
            #ifdef UNIONFIND_COARSE_ENABLED
            std::cerr << ", coarse";
            #endif
            #ifdef UNIONFIND_FINE_ENABLED
            std::cerr << ", fine";
            #endif
            #ifdef UNIONFIND_LOCKFREE_ENABLED
            std::cerr << ", lockfree";
            #endif
            std::cerr << std::endl;
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "An exception occurred during benchmarking: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "An unknown exception occurred during benchmarking." << std::endl;
        return 1;
    }


    // --- Calculate and Print Results ---
    if (durations.empty()) {
        std::cerr << "Error: No benchmark runs were completed successfully." << std::endl;
        return 1;
    }

    double total_duration = std::accumulate(durations.begin(), durations.end(), 0.0);
    double avg_duration = total_duration / durations.size();
    double min_duration = *std::min_element(durations.begin(), durations.end());
    double max_duration = *std::max_element(durations.begin(), durations.end());

    // Calculate standard deviation
    double sq_sum = 0.0;
    for(double d : durations) {
        sq_sum += (d - avg_duration) * (d - avg_duration);
    }
    double std_dev = (durations.size() > 1) ? std::sqrt(sq_sum / (durations.size() - 1)) : 0.0; // Use n-1 for sample std dev


    std::cout << "\n--- Benchmark Summary ---" << std::endl;
    std::cout << std::fixed << std::setprecision(4); // Format output
    std::cout << "Implementation: " << impl_type << std::endl;
    std::cout << "Threads:        " << num_threads << std::endl;
    std::cout << "Element Count:  " << n_elements << std::endl;
    std::cout << "Operation Count:" << operations.size() << std::endl;
    std::cout << "Number of Runs: " << num_runs << std::endl;
    std::cout << "-------------------------" << std::endl;
    std::cout << "Avg Time:       " << avg_duration << " ms" << std::endl;
    std::cout << "Min Time:       " << min_duration << " ms" << std::endl;
    std::cout << "Max Time:       " << max_duration << " ms" << std::endl;
    std::cout << "Std Dev:        " << std_dev << " ms" << std::endl;
    std::cout << "-------------------------" << std::endl;

    std::cout << "\nNote on Cache Metrics:" << std::endl;
    std::cout << "To measure cache performance (e.g., cache misses), use external tools." << std::endl;
    std::cout << "On Linux, try 'perf stat':" << std::endl;
    std::cout << "  perf stat -e cache-references,cache-misses,instructions,cycles ./" << argv[0] << " "
              << impl_type << " " << ops_file << " " << num_runs << " " << num_threads << std::endl;
    std::cout << "Alternatively, consider using libraries like PAPI (Performance Application Programming Interface)." << std::endl;


    return 0;
}
