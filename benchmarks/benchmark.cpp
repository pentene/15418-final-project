#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <chrono>
#include <numeric>     // For std::accumulate
#include <stdexcept>
#include <memory>      // For std::unique_ptr
#include <iomanip>     // For std::fixed, std::setprecision
#include <omp.h>       // For omp_set_num_threads and omp_get_max_threads
#include <algorithm>   // For std::min_element, std::max_element, std::transform
#include <cmath>       // For std::sqrt
#include <type_traits> // For std::remove_reference_t

// Include your Union-Find implementations
// Assuming union_find.hpp defines the canonical OperationType and Operation struct
#include "union_find.hpp" // Serial (defines CanonicalOperation)

#ifdef UNIONFIND_COARSE_ENABLED // Use defines from Makefile
#include "union_find_parallel_coarse.hpp"
#endif
#ifdef UNIONFIND_FINE_ENABLED
#include "union_find_parallel_fine.hpp" // Make sure this ID matches the latest fine-grained hpp
#endif
#ifdef UNIONFIND_LOCKFREE_ENABLED
#include "union_find_parallel_lockfree.hpp"
#endif
#ifdef UNIONFIND_LOCKFREE_PLAIN_ENABLED // Include the new header
#include "union_find_parallel_lockfree_plain_write.hpp"
#endif
#ifdef UNIONFIND_LOCKFREE_IPC_ENABLED // Include the new header
#include "union_find_parallel_lockfree_ipc.hpp"
#endif

// Use the Operation struct and OperationType defined within the canonical UnionFind class.
using CanonicalOperation = UnionFind::Operation;
using CanonicalOperationType = UnionFind::OperationType;


// Function to load operations from a file
// Format:
// <num_elements> <num_operations>
// <type> <a> <b>  (type: 0 for UNION, 1 for FIND, 2 for SAMESET)
// ...
// Loads into std::vector<CanonicalOperation>
bool load_operations(const std::string& filename, int& n_elements, std::vector<CanonicalOperation>& ops) {
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

        // --- Validation ---
        // 1. Check valid type value
        if (type_val < 0 || type_val > 2) { // Now allowing 0, 1, 2
            std::cerr << "Error: Invalid operation type at line " << i + 2 << ": type=" << type_val << " (must be 0, 1, or 2)" << std::endl;
            ops.clear();
            return false;
        }
        // 2. Check index 'a' bounds (required for all ops)
        if (a < 0 || a >= n_elements) {
            std::cerr << "Error: Invalid index 'a' at line " << i + 2 << ": a=" << a << " (n_elements=" << n_elements << ")" << std::endl;
            ops.clear();
            return false;
        }
        // 3. Check index 'b' bounds (required for UNION and SAMESET)
        if (type_val == 0 || type_val == 2) { // UNION or SAMESET
            if (b < 0 || b >= n_elements) {
                std::cerr << "Error: Invalid index 'b' for UNION/SAMESET op at line " << i + 2 << ": b=" << b << " (n_elements=" << n_elements << ")" << std::endl;
                ops.clear();
                return false;
            }
        }
        // --- End Validation ---


        // Create CanonicalOperation object directly
        CanonicalOperation op;
        op.a = a;
        op.b = b; // Store b; it will be ignored by find/unionSets/sameSet if not needed

        // Assign the correct enum type based on the integer value read
        switch (type_val) {
            case 0: op.type = CanonicalOperationType::UNION_OP; break;
            case 1: op.type = CanonicalOperationType::FIND_OP; break;
            case 2: op.type = CanonicalOperationType::SAMESET_OP; break;
            // default case already handled by validation above
        }
        ops.push_back(op);
    }

    if (ops.size() != n_ops) {
        std::cerr << "Warning: Expected " << n_ops << " operations, but read " << ops.size() << "." << std::endl;
        // Decide if this is a fatal error or just a warning
    }

    std::cout << "Successfully loaded " << ops.size() << " operations (UNION=0, FIND=1, SAMESET=2) for "
              << n_elements << " elements from " << filename << std::endl;
    return true;
}

// Helper function to convert between compatible Operation structs
// Assumes the structs have the same members (type, a, b) and compatible enum values.
template <typename TargetOp, typename SourceOp>
TargetOp convert_operation(const SourceOp& source_op) {
    TargetOp target_op;
    // Use static_cast for the enum type conversion. This relies on the enum values
    // (e.g., UNION_OP=0, FIND_OP=1, SAMESET_OP=2) being consistent across implementations.
    target_op.type = static_cast<decltype(TargetOp::type)>(source_op.type);
    target_op.a = source_op.a;
    target_op.b = source_op.b;
    return target_op;
}


// --- Main Benchmark Function ---
int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <implementation_type> <operations_file> <num_runs> [num_threads]" << std::endl;
        std::cerr << "  implementation_type: serial, coarse, fine, lockfree, lockfree_plain, lockfree_ipc" << std::endl;
        std::cerr << "  operations_file: Path to the file containing operations (Type: 0=UNION, 1=FIND, 2=SAMESET)." << std::endl;
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
    // Load into the canonical vector type
    std::vector<CanonicalOperation> canonical_operations;
    if (!load_operations(ops_file, n_elements, canonical_operations)) {
        return 1; // Error loading data
    }
    if (canonical_operations.empty()) {
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
    std::cout << "Operation Count:" << canonical_operations.size() << std::endl;
    std::cout << "Number of Runs: " << num_runs << std::endl;
    std::cout << "Threads:        " << num_threads << std::endl;


    // Lambda to run the benchmark for a given UF type
    // Takes a prototype instance just to deduce the type
    auto run_benchmark = [&](auto& uf_instance_prototype) {
        // Deduce the specific Operation type required by this UF implementation
        using SpecificUF = std::remove_reference_t<decltype(uf_instance_prototype)>;
        using SpecificOperation = typename SpecificUF::Operation; // Get the nested Operation type

        // --- Perform conversion once before warm-up and timed runs ---
        std::vector<SpecificOperation> specific_operations;
        specific_operations.reserve(canonical_operations.size());
        std::transform(canonical_operations.begin(), canonical_operations.end(),
                       std::back_inserter(specific_operations),
                       convert_operation<SpecificOperation, CanonicalOperation>);
        // --- Conversion complete ---


        // Warm-up run
        {
            // Use unique_ptr for automatic memory management
            auto temp_uf = std::make_unique<SpecificUF>(n_elements);
            std::cout << "Performing warm-up run..." << std::endl;
            // Pass the correctly typed vector
            temp_uf->processOperations(specific_operations, results); // Results vector is populated but not used here
            std::cout << "Warm-up complete." << std::endl;
        }


        // Timed runs
        for (int i = 0; i < num_runs; ++i) {
            // Create a fresh instance for each run
            auto current_uf = std::make_unique<SpecificUF>(n_elements);

            // --- Timing starts HERE ---
            auto start_time = std::chrono::high_resolution_clock::now();

            // Pass the correctly typed vector
            current_uf->processOperations(specific_operations, results); // Results populated here

            auto end_time = std::chrono::high_resolution_clock::now();
            // --- Timing ends HERE ---


            std::chrono::duration<double, std::milli> duration_ms = end_time - start_time;
            durations.push_back(duration_ms.count());
            std::cout << "Run " << (i + 1) << ": " << duration_ms.count() << " ms" << std::endl;

            // Optional: Add basic validation check on results size after first run
            if (i == 0 && results.size() != specific_operations.size()) {
                 std::cerr << "Warning: Results vector size (" << results.size()
                           << ") does not match operations vector size (" << specific_operations.size()
                           << ") after first run." << std::endl;
            }
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
        #ifdef UNIONFIND_LOCKFREE_PLAIN_ENABLED // New implementation
        else if (impl_type == "lockfree_plain") {
            UnionFindParallelLockFreePlainWrite uf_proto(n_elements);
            run_benchmark(uf_proto);
        }
        #endif
        #ifdef UNIONFIND_LOCKFREE_IPC_ENABLED // New implementation
        else if (impl_type == "lockfree_ipc") {
            UnionFindParallelLockFreeIPC uf_proto(n_elements);
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
            #ifdef UNIONFIND_LOCKFREE_PLAIN_ENABLED // New implementation
            std::cerr << ", lockfree_plain";
            #endif
            #ifdef UNIONFIND_LOCKFREE_IPC_ENABLED // New implementation
            std::cerr << ", lockfree_ipc";
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
    // Use n-1 for sample standard deviation, handle n=1 case.
    double std_dev = (durations.size() > 1) ? std::sqrt(sq_sum / (durations.size() - 1)) : 0.0;


    std::cout << "\n--- Benchmark Summary ---" << std::endl;
    std::cout << std::fixed << std::setprecision(4); // Format output
    std::cout << "Implementation: " << impl_type << std::endl;
    std::cout << "Threads:        " << num_threads << std::endl;
    std::cout << "Element Count:  " << n_elements << std::endl;
    std::cout << "Operation Count:" << canonical_operations.size() << std::endl;
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
    // Construct the perf command string dynamically for clarity
    std::string perf_command = "perf stat -e cache-references,cache-misses,instructions,cycles ./";
    perf_command += argv[0]; // Executable name
    perf_command += " " + impl_type;
    perf_command += " " + ops_file;
    perf_command += " " + std::to_string(num_runs);
    if (argc > 4) { // Include thread count if provided
         perf_command += " " + std::to_string(num_threads);
    }
    std::cout << "  " << perf_command << std::endl;
    std::cout << "Alternatively, consider using libraries like PAPI (Performance Application Programming Interface)." << std::endl;


    return 0;
}
