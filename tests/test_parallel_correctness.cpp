#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <stdexcept>
#include <memory> // For std::unique_ptr
#include <cassert> // For assertions
#include <omp.h>   // For omp_set_num_threads
#include <algorithm> // For std::transform
#include <iterator> // For std::back_inserter
#include <iomanip> // For std::setw

// Include the baseline serial implementation
#include "union_find.hpp"

// Conditionally include the parallel implementations based on Makefile flags
#ifdef UNIONFIND_COARSE_ENABLED
#include "union_find_parallel_coarse.hpp"
#endif
#ifdef UNIONFIND_FINE_ENABLED
#include "union_find_parallel_fine.hpp" // Assuming fine-grained might exist
#endif
#ifdef UNIONFIND_LOCKFREE_ENABLED
#include "union_find_parallel_lockfree.hpp"
#endif

// Use the canonical Operation type from the serial version for loading
// NOTE: Serial version only supports UNION_OP and FIND_OP
using CanonicalOperation = UnionFind::Operation;
using CanonicalOperationType = UnionFind::OperationType;

// Helper function to load operations (similar to benchmark)
// MODIFIED: Skips SAMESET_OP as it's not in the baseline UnionFind
bool load_operations_for_test(const std::string& filename, int& n_elements, std::vector<CanonicalOperation>& ops) {
    std::ifstream infile(filename);
    if (!infile) {
        std::cerr << "Test Error: Cannot open file: " << filename << std::endl;
        return false;
    }

    size_t n_ops_in_file; // Total operations listed in the file header
    if (!(infile >> n_elements >> n_ops_in_file)) {
        std::cerr << "Test Error: Could not read header from file: " << filename << std::endl;
        return false;
    }
     if (n_elements <= 0) {
        std::cerr << "Test Error: Invalid number of elements in file: " << n_elements << std::endl;
        return false;
     }

    ops.clear();
    ops.reserve(n_ops_in_file); // Reserve based on file header, actual count might be less
    int type_val, a, b;
    size_t ops_loaded = 0;
    size_t ops_skipped = 0;

    // Define operation types consistent with the header files
    const int UNION_TYPE_VAL = 0; // Assuming 0 is UNION in file
    const int FIND_TYPE_VAL = 1;  // Assuming 1 is FIND in file
    const int SAMESET_TYPE_VAL = 2; // Assuming 2 is SAMESET in file

    for (size_t i = 0; i < n_ops_in_file; ++i) {
        if (!(infile >> type_val >> a >> b)) {
            std::cerr << "Test Error: Failed to read operation " << i+1 << " from file." << std::endl;
            ops.clear();
            return false;
        }

        // Basic validation for 'a'
        if (a < 0 || a >= n_elements) {
            std::cerr << "Test Error: Invalid element 'a' (" << a << ") at line " << i + 2 << " in file " << filename << std::endl;
            ops.clear(); return false;
        }

        CanonicalOperation op;
        op.a = a;
        op.b = b; // Assign b, validate based on type below

        if (type_val == UNION_TYPE_VAL) {
            op.type = CanonicalOperationType::UNION_OP;
            if (b < 0 || b >= n_elements) {
                std::cerr << "Test Error: Invalid element 'b' (" << b << ") for UNION_OP at line " << i + 2 << " in file " << filename << std::endl;
                ops.clear(); return false;
            }
            ops.push_back(op);
            ops_loaded++;
        } else if (type_val == FIND_TYPE_VAL) {
            op.type = CanonicalOperationType::FIND_OP;
            // b is ignored for FIND, no validation needed for b
            ops.push_back(op);
            ops_loaded++;
        } else if (type_val == SAMESET_TYPE_VAL) {
            // SAMESET_OP is not supported by the baseline serial UnionFind.
            // Skip this operation for the correctness test.
            ops_skipped++;
            // Optionally log skipped operations
            // std::cout << "Test Info: Skipping SAMESET_OP at line " << i + 2 << " (not in baseline)." << std::endl;
        }
         else {
            std::cerr << "Test Error: Invalid operation type value (" << type_val << ") at line " << i + 2 << " in file " << filename << std::endl;
            ops.clear(); return false;
         }
    }

    if (ops_loaded + ops_skipped != n_ops_in_file) {
        std::cerr << "Test Warning: Expected " << n_ops_in_file << " operations in file, but processed " << ops_loaded + ops_skipped << " lines from " << filename << "." << std::endl;
    }
    if (ops_skipped > 0) {
    std::cout << "Test Info: Skipped " << ops_skipped << " SAMESET operations as they are not supported by the baseline serial implementation for comparison." << std::endl;
    }
    std::cout << "Loaded " << ops_loaded << " compatible operations (UNION/FIND) for " << n_elements << " elements from " << filename << " for testing." << std::endl;
    return true;
}


// Helper function to convert operations (needed for parallel versions)
// Assumes TargetOp::type can be static_cast from SourceOp::type (UNION_OP/FIND_OP)
template <typename TargetOp, typename SourceOp>
TargetOp convert_operation_test(const SourceOp& source_op) {
    TargetOp target_op;
    // Only UNION_OP and FIND_OP should be present here due to loader changes.
    assert(source_op.type == CanonicalOperationType::UNION_OP || source_op.type == CanonicalOperationType::FIND_OP);

    // Perform static_cast, assuming enum values for UNION_OP/FIND_OP correspond.
    target_op.type = static_cast<decltype(target_op.type)>(source_op.type);
    target_op.a = source_op.a;
    target_op.b = source_op.b;
    return target_op;
}

// --- MODIFIED CORRECTNESS TEST FUNCTION ---
// Verifies connectivity equivalence using only UNION and FIND operations.
template <typename ParallelUF>
bool run_correctness_test(const std::string& impl_name, int n_elements, const std::vector<CanonicalOperation>& canonical_ops) {
    std::cout << "\n--- Testing Correctness: " << impl_name << " (Connectivity Verification using UNION/FIND) ---" << std::endl;

    // Ensure operations vector is not empty after loading/filtering
    if (canonical_ops.empty()) {
        std::cerr << "Test Error: No compatible (UNION/FIND) operations available for testing " << impl_name << "." << std::endl;
        return false; // Cannot run test without operations
    }

    // 1. Run Serial Implementation (Baseline)
    UnionFind uf_serial(n_elements);
    std::vector<int> serial_op_results; // For processOperations signature
    std::cout << "Running serial baseline..." << std::endl;
    uf_serial.processOperations(canonical_ops, serial_op_results);
    std::cout << "Serial baseline complete." << std::endl;

    // 2. Prepare Operations for Parallel Version
    // Need to ensure the ParallelUF::Operation struct and enum exist and are compatible
    // This requires ParallelUF to be defined when this template is instantiated.
    using ParallelOperation = typename ParallelUF::Operation;
    std::vector<ParallelOperation> parallel_ops;
    parallel_ops.reserve(canonical_ops.size());
    std::transform(canonical_ops.begin(), canonical_ops.end(),
                   std::back_inserter(parallel_ops),
                   convert_operation_test<ParallelOperation, CanonicalOperation>);

    // 3. Run Parallel Implementation
    ParallelUF uf_parallel(n_elements);
    std::vector<int> parallel_op_results; // For processOperations signature
    std::cout << "Running parallel implementation (" << impl_name << ")..." << std::endl;
    uf_parallel.processOperations(parallel_ops, parallel_op_results);
    std::cout << "Parallel implementation complete." << std::endl;

    // 4. Get Final Roots for All Elements from Both Implementations
    std::vector<int> serial_final_roots(n_elements);
    std::vector<int> parallel_final_roots(n_elements);

    std::cout << "Calculating final roots for comparison..." << std::endl;
    // Finding roots serially here to avoid parallel overhead/races in verification itself.
    for (int k = 0; k < n_elements; ++k) {
        serial_final_roots[k] = uf_serial.find(k);
        // Ensure the parallel implementation has a 'find' method
        parallel_final_roots[k] = uf_parallel.find(k);
    }
    std::cout << "Final roots calculated." << std::endl;

    // 5. Compare Connectivity by Checking All Pairs
    std::cout << "Comparing connectivity for all pairs..." << std::endl;
    bool connectivity_match = true;
    long long pairs_checked = 0;
    long long mismatches = 0;
    const int report_limit = 10; // Limit number of reported mismatches

    // Optimization: Only check pairs (a, b) where a < b
    for (int a = 0; a < n_elements; ++a) {
        for (int b = a + 1; b < n_elements; ++b) {
            pairs_checked++;
            bool serial_connected = (serial_final_roots[a] == serial_final_roots[b]);
            // Check connectivity using the parallel structure's final roots
            bool parallel_connected = (parallel_final_roots[a] == parallel_final_roots[b]);

            if (serial_connected != parallel_connected) {
                connectivity_match = false;
                mismatches++;
                if (mismatches <= report_limit) {
                    std::cerr << "Connectivity Mismatch found for pair (" << a << ", " << b << "): "
                              << "Serial says " << (serial_connected ? "CONNECTED" : "DISCONNECTED") << " (Roots: " << serial_final_roots[a] << ", " << serial_final_roots[b] << "), "
                              << impl_name << " says " << (parallel_connected ? "CONNECTED" : "DISCONNECTED") << " (Roots: " << parallel_final_roots[a] << ", " << parallel_final_roots[b] << ")" << std::endl;
                }
            }
        }
    }

    std::cout << "Connectivity comparison complete. Checked " << pairs_checked << " pairs." << std::endl;
    if (connectivity_match) {
        std::cout << "Result: PASS - Connectivity matches serial baseline." << std::endl;
    } else {
        std::cout << "Result: FAIL - Found " << mismatches << " connectivity mismatches." << std::endl;
        if (mismatches > report_limit) {
             std::cerr << " (Further mismatch details suppressed)" << std::endl;
        }
    }
    std::cout << "--- Test Complete: " << impl_name << " ---" << std::endl;
    return connectivity_match;
}


// Main function - unchanged
int main() {
    // --- Configuration ---
    const std::string test_ops_file = "tests/resources/parallel_test_1.txt"; // Example file path

    #ifdef UNIONFIND_DEFAULT_THREADS
        int num_threads = UNIONFIND_DEFAULT_THREADS;
        omp_set_num_threads(num_threads);
        std::cout << "Setting OpenMP threads for test: " << num_threads << std::endl;
    #else
        // Get max threads available if not set, just for info
        int max_threads = omp_get_max_threads();
        std::cout << "Using default OpenMP threads (Max available likely: " << max_threads << ")." << std::endl;
    #endif

    // --- Load Test Data ---
    int n_elements;
    std::vector<CanonicalOperation> operations; // Will only contain UNION/FIND ops
    if (!load_operations_for_test(test_ops_file, n_elements, operations)) {
        return 1; // Failed to load test data
    }
    // Check if any compatible operations were loaded
    if (operations.empty() && n_elements > 0) { // Check n_elements too, in case file only had header
        std::cerr << "Test Error: No compatible (UNION/FIND) operations loaded from file: " << test_ops_file << std::endl;
        std::cerr << "Cannot perform correctness test." << std::endl;
        return 1; // Treat as failure if no ops to test
    }
     if (operations.empty() && n_elements <= 0) {
         // This case should be caught by loader, but double-check
         std::cerr << "Test Error: Invalid input file or no operations." << std::endl;
         return 1;
     }


    // --- Run Tests for Enabled Implementations ---
    bool all_tests_passed = true;
    int tests_run = 0;

    #ifdef UNIONFIND_COARSE_ENABLED
        // Ensure Coarse version has compatible Operation struct/enum
        tests_run++;
        if (!run_correctness_test<UnionFindParallelCoarse>("Coarse-Grained", n_elements, operations)) {
            all_tests_passed = false;
        }
    #endif

    #ifdef UNIONFIND_FINE_ENABLED
        // Assuming UnionFindParallelFine exists and has compatible interface
        tests_run++;
        if (!run_correctness_test<UnionFindParallelFine>("Fine-Grained", n_elements, operations)) {
            all_tests_passed = false;
        }
    #endif

    #ifdef UNIONFIND_LOCKFREE_ENABLED
         // Ensure LockFree version has compatible Operation struct/enum (it should)
        tests_run++;
        if (!run_correctness_test<UnionFindParallelLockFree>("Lock-Free", n_elements, operations)) {
            all_tests_passed = false;
        }
    #endif

    if (tests_run == 0) {
        std::cerr << "\nWarning: No parallel implementations seem to be enabled via Makefile flags (e.g., LOCKFREE=1)." << std::endl;
        std::cerr << "No parallel correctness tests were run." << std::endl;
        return 0; // No tests failed, but none ran
    }

    // --- Final Result ---
    std::cout << "\n========================================" << std::endl;
    if (all_tests_passed) {
        std::cout << "Overall Result: ALL PARALLEL TESTS PASSED (Connectivity Verification)" << std::endl;
        std::cout << "========================================" << std::endl;
        return 0; // Success
    } else {
        std::cout << "Overall Result: SOME PARALLEL TESTS FAILED (Connectivity Verification)" << std::endl;
        std::cout << "========================================" << std::endl;
        return 1; // Failure
    }
}
