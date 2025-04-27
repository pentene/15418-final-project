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

// Include the baseline serial implementation (ASSUMED TO SUPPORT SAMESET_OP)
#include "union_find.hpp"

// Conditionally include the parallel implementations based on Makefile flags
#ifdef UNIONFIND_COARSE_ENABLED
#include "union_find_parallel_coarse.hpp" // Assumed to support SAMESET_OP
#endif
#ifdef UNIONFIND_FINE_ENABLED
#include "union_find_parallel_fine.hpp" // Assumed to support SAMESET_OP
#endif
#ifdef UNIONFIND_LOCKFREE_ENABLED
#include "union_find_parallel_lockfree.hpp" // Assumed to support SAMESET_OP
#endif
#ifdef UNIONFIND_LOCKFREE_PLAIN_ENABLED // Include the new header
#include "union_find_parallel_lockfree_plain_write.hpp"
#endif

// Use the canonical Operation type from the serial version for loading
// ASSUMPTION: UnionFind::OperationType includes SAMESET_OP
using CanonicalOperation = UnionFind::Operation;
using CanonicalOperationType = UnionFind::OperationType;

// Helper function to load operations (includes SAMESET)
// (Same as the previous version you reviewed, ensuring SAMESET is loaded)
bool load_operations_for_test(const std::string& filename, int& n_elements, std::vector<CanonicalOperation>& ops) {
    std::ifstream infile(filename);
    if (!infile) {
        std::cerr << "Test Error: Cannot open file: " << filename << std::endl;
        return false;
    }

    size_t n_ops_in_file;
    if (!(infile >> n_elements >> n_ops_in_file)) {
        std::cerr << "Test Error: Could not read header from file: " << filename << std::endl;
        return false;
    }
     if (n_elements <= 0) {
        std::cerr << "Test Error: Invalid number of elements in file: " << n_elements << std::endl;
        return false;
      }

    ops.clear();
    ops.reserve(n_ops_in_file);
    int type_val, a, b;
    size_t ops_loaded = 0;

    const int UNION_TYPE_VAL = 0;
    const int FIND_TYPE_VAL = 1;
    const int SAMESET_TYPE_VAL = 2;

    // Optional: Static asserts to ensure enum values match expected file format
    // static_assert(static_cast<int>(CanonicalOperationType::UNION_OP) == UNION_TYPE_VAL, "UNION_OP enum value mismatch");
    // static_assert(static_cast<int>(CanonicalOperationType::FIND_OP) == FIND_TYPE_VAL, "FIND_OP enum value mismatch");
    // static_assert(static_cast<int>(CanonicalOperationType::SAMESET_OP) == SAMESET_TYPE_VAL, "SAMESET_OP enum value mismatch");


    for (size_t i = 0; i < n_ops_in_file; ++i) {
        if (!(infile >> type_val >> a >> b)) {
            std::cerr << "Test Error: Failed to read operation " << i + 1 << " from file." << std::endl;
            ops.clear();
            return false;
        }

        if (a < 0 || a >= n_elements) {
            std::cerr << "Test Error: Invalid element 'a' (" << a << ") at line " << i + 2 << " in file " << filename << std::endl;
            ops.clear(); return false;
        }

        CanonicalOperation op;
        op.a = a;
        op.b = b;

        bool valid_op = true;
        if (type_val == UNION_TYPE_VAL) {
            op.type = CanonicalOperationType::UNION_OP;
            if (b < 0 || b >= n_elements) {
                std::cerr << "Test Error: Invalid element 'b' (" << b << ") for UNION_OP at line " << i + 2 << " in file " << filename << std::endl;
                valid_op = false;
            }
        } else if (type_val == FIND_TYPE_VAL) {
            op.type = CanonicalOperationType::FIND_OP;
            // 'b' is ignored for FIND, no specific validation needed for it here.
        } else if (type_val == SAMESET_TYPE_VAL) {
            op.type = CanonicalOperationType::SAMESET_OP;
             if (b < 0 || b >= n_elements) {
                std::cerr << "Test Error: Invalid element 'b' (" << b << ") for SAMESET_OP at line " << i + 2 << " in file " << filename << std::endl;
                valid_op = false;
            }
        } else {
            std::cerr << "Test Error: Invalid operation type value (" << type_val << ") at line " << i + 2 << " in file " << filename << std::endl;
            valid_op = false;
        }

        if (!valid_op) {
             ops.clear(); return false;
        }

        ops.push_back(op);
        ops_loaded++;
    }

    if (ops_loaded != n_ops_in_file) {
        std::cerr << "Test Warning: Expected " << n_ops_in_file << " operations in file, but loaded " << ops_loaded << " from " << filename << "." << std::endl;
    }

    std::cout << "Loaded " << ops_loaded << " operations (UNION/FIND/SAMESET) for " << n_elements << " elements from " << filename << " for testing." << std::endl;
    return true;
}


// Helper function to convert operations (needed for parallel versions)
// Assumes TargetOp::type can be static_cast from SourceOp::type
template <typename TargetOp, typename SourceOp>
TargetOp convert_operation_test(const SourceOp& source_op) {
    TargetOp target_op;
    target_op.type = static_cast<decltype(target_op.type)>(source_op.type);
    target_op.a = source_op.a;
    target_op.b = source_op.b;
    return target_op;
}

// --- CORRECTNESS TEST FUNCTION ---
// Verifies correctness ONLY by comparing final connectivity state.
template <typename ParallelUF>
bool run_correctness_test(const std::string& impl_name, int n_elements, const std::vector<CanonicalOperation>& canonical_ops) {
    std::cout << "\n--- Testing Correctness: " << impl_name << " (Final Connectivity Verification) ---" << std::endl;

    if (canonical_ops.empty() && n_elements > 0) { // Allow empty ops if n_elements is also 0
        std::cerr << "Test Error: No operations available for testing " << impl_name << " with " << n_elements << " elements." << std::endl;
        return false;
    }
     if (canonical_ops.empty() && n_elements <= 0) {
         std::cout << "Test Info: No operations and no elements. Skipping test for " << impl_name << "." << std::endl;
         return true; // Nothing to test, considered passing.
     }


    // 1. Run Serial Implementation (Baseline)
    UnionFind uf_serial(n_elements);
    std::vector<int> serial_op_results; // Results vector needed for processOperations signature
    serial_op_results.reserve(canonical_ops.size()); // Allocate, but results won't be compared
    std::cout << "Running serial baseline..." << std::endl;
    // Execute all operations (UNION, FIND, SAMESET)
    uf_serial.processOperations(canonical_ops, serial_op_results);
    std::cout << "Serial baseline complete. Processed " << canonical_ops.size() << " operations." << std::endl;


    // 2. Prepare Operations for Parallel Version
    using ParallelOperation = typename ParallelUF::Operation;
    std::vector<ParallelOperation> parallel_ops;
    parallel_ops.reserve(canonical_ops.size());
    std::transform(canonical_ops.begin(), canonical_ops.end(),
                   std::back_inserter(parallel_ops),
                   convert_operation_test<ParallelOperation, CanonicalOperation>);

    // 3. Run Parallel Implementation
    ParallelUF uf_parallel(n_elements);
    std::vector<int> parallel_op_results; // Needed for signature, but results not compared
    parallel_op_results.reserve(parallel_ops.size());
    std::cout << "Running parallel implementation (" << impl_name << ")..." << std::endl;
    // Execute all operations (UNION, FIND, SAMESET) concurrently
    uf_parallel.processOperations(parallel_ops, parallel_op_results);
    std::cout << "Parallel implementation complete. Processed " << parallel_ops.size() << " operations." << std::endl;

    // 4. Get Final Roots for All Elements from Both Implementations
    std::vector<int> serial_final_roots(n_elements);
    std::vector<int> parallel_final_roots(n_elements);

    std::cout << "Calculating final roots for connectivity comparison..." << std::endl;
    // Use the 'find' method of each structure AFTER all operations are done.
    // Run this part serially to avoid races within the verification code itself.
    for (int k = 0; k < n_elements; ++k) {
        // Ensure both serial and parallel classes have a 'find(int)' method
        serial_final_roots[k] = uf_serial.find(k);
        parallel_final_roots[k] = uf_parallel.find(k);
    }
    std::cout << "Final roots calculated." << std::endl;

    // 5. Compare Final Connectivity by Checking All Pairs
    std::cout << "Comparing final connectivity for all pairs..." << std::endl;
    bool connectivity_match = true;
    long long pairs_checked = 0;
    long long conn_mismatches = 0;
    const int report_limit_conn = 10;

    // Optimization: Only check pairs (a, b) where a < b
    for (int a = 0; a < n_elements; ++a) {
        for (int b = a + 1; b < n_elements; ++b) {
            pairs_checked++;
            // Check connectivity using the final roots computed above
            bool serial_connected = (serial_final_roots[a] == serial_final_roots[b]);
            bool parallel_connected = (parallel_final_roots[a] == parallel_final_roots[b]);

            if (serial_connected != parallel_connected) {
                connectivity_match = false;
                conn_mismatches++;
                if (conn_mismatches <= report_limit_conn) {
                    // Report detailed mismatch info
                    std::cerr << "Final Connectivity Mismatch for pair (" << a << ", " << b << "): "
                              << "Serial says " << (serial_connected ? "CONNECTED" : "DISCONNECTED") << " (Roots: " << serial_final_roots[a] << ", " << serial_final_roots[b] << "), "
                              << impl_name << " says " << (parallel_connected ? "CONNECTED" : "DISCONNECTED") << " (Roots: " << parallel_final_roots[a] << ", " << parallel_final_roots[b] << ")" << std::endl;
                }
            }
        }
    }

    std::cout << "Final connectivity comparison complete. Checked " << pairs_checked << " pairs." << std::endl;
    if (connectivity_match) {
        std::cout << "Result: PASS - Final connectivity matches serial baseline." << std::endl;
    } else {
        std::cout << "Result: FAIL - Found " << conn_mismatches << " final connectivity mismatches." << std::endl;
        if (conn_mismatches > report_limit_conn) {
             std::cerr << " (Further mismatch details suppressed)" << std::endl;
        }
    }
    std::cout << "--- Test Complete: " << impl_name << " ---" << std::endl;

    // The overall result ONLY depends on the final connectivity check
    return connectivity_match;
}


// Main function - unchanged from previous version, but interpretation of results is clearer
int main() {
    // --- Configuration ---
    const std::string test_ops_file = "tests/resources/ops_10k_100k_f0.4_c0.0_s0.5.txt"; // Example file path

    #ifdef UNIONFIND_DEFAULT_THREADS
        int num_threads = UNIONFIND_DEFAULT_THREADS;
        omp_set_num_threads(num_threads);
        std::cout << "Setting OpenMP threads for test: " << num_threads << std::endl;
    #else
        int max_threads = omp_get_max_threads();
        std::cout << "Using default OpenMP threads (Max available likely: " << max_threads << ")." << std::endl;
    #endif

    // --- Load Test Data ---
    int n_elements;
    std::vector<CanonicalOperation> operations; // Includes UNION, FIND, SAMESET
    if (!load_operations_for_test(test_ops_file, n_elements, operations)) {
        return 1; // Failed to load test data
    }
    // Handle case of empty operations list (check n_elements too)
     if (operations.empty()) {
        if (n_elements > 0) {
            std::cerr << "Test Error: No operations loaded from file: " << test_ops_file << " for " << n_elements << " elements." << std::endl;
            std::cerr << "Cannot perform correctness test." << std::endl;
            return 1;
        } else {
            std::cout << "Test Info: No operations loaded, and n_elements is 0. Skipping tests." << std::endl;
            // Report overall success as no tests failed (because none could run)
            std::cout << "\n========================================" << std::endl;
            std::cout << "Overall Result: NO TESTS RUN (No operations/elements)" << std::endl;
            std::cout << "========================================" << std::endl;
            return 0;
        }
    }


    // --- Run Tests for Enabled Implementations ---
    bool all_tests_passed = true;
    int tests_run = 0;

    #ifdef UNIONFIND_COARSE_ENABLED
        tests_run++;
        // Pass the full list of operations (including SAMESET)
        if (!run_correctness_test<UnionFindParallelCoarse>("Coarse-Grained", n_elements, operations)) {
            all_tests_passed = false;
        }
    #endif

    #ifdef UNIONFIND_FINE_ENABLED
        tests_run++;
        // Pass the full list of operations (including SAMESET)
        if (!run_correctness_test<UnionFindParallelFine>("Fine-Grained", n_elements, operations)) {
            all_tests_passed = false;
        }
    #endif

    #ifdef UNIONFIND_LOCKFREE_ENABLED
        tests_run++;
        // Pass the full list of operations (including SAMESET)
        if (!run_correctness_test<UnionFindParallelLockFree>("Lock-Free", n_elements, operations)) {
            all_tests_passed = false;
        }
    #endif

    #ifdef UNIONFIND_LOCKFREE_PLAIN_ENABLED
        tests_run++;
        // Pass the full list of operations (including SAMESET)
        if (!run_correctness_test<UnionFindParallelLockFreePlainWrite>("Lock-Free Plain Write", n_elements, operations)) {
            all_tests_passed = false;
        }
    #endif

    if (tests_run == 0) {
        std::cerr << "\nWarning: No parallel implementations seem to be enabled via Makefile flags (e.g., LOCKFREE=1)." << std::endl;
        std::cerr << "No parallel correctness tests were run." << std::endl;
        // Report overall success as no tests failed (because none ran)
        std::cout << "\n========================================" << std::endl;
        std::cout << "Overall Result: NO PARALLEL TESTS ENABLED" << std::endl;
        std::cout << "========================================" << std::endl;
        return 0;
    }

    // --- Final Result ---
    std::cout << "\n========================================" << std::endl;
    if (all_tests_passed) {
        std::cout << "Overall Result: ALL ENABLED PARALLEL TESTS PASSED (Final Connectivity Verification)" << std::endl;
        std::cout << "========================================" << std::endl;
        return 0; // Success
    } else {
        std::cout << "Overall Result: SOME ENABLED PARALLEL TESTS FAILED (Final Connectivity Verification)" << std::endl;
        std::cout << "========================================" << std::endl;
        return 1; // Failure
    }
}