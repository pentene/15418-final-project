#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <stdexcept>
#include <memory> 
#include <cassert> 
#include <iomanip> 

#include "union_find.hpp"

using CanonicalOperation = UnionFind::Operation;
using CanonicalOperationType = UnionFind::OperationType;

bool load_operations_for_test(const std::string& filename, int& n_elements, std::vector<CanonicalOperation>& ops) 
{
    std::ifstream infile(filename);
    if (!infile) 
    {
        std::cerr << "Test Error: Cannot open file: " << filename << std::endl;
        return false;
    }

    size_t n_ops_in_file; // Total operations listed in the file header
    if (!(infile >> n_elements >> n_ops_in_file)) 
    {
        std::cerr << "Test Error: Could not read header from file: " << filename << std::endl;
        return false;
    }
    if (n_elements <= 0) 
    {
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

    for (size_t i = 0; i < n_ops_in_file; i++) 
    {
        if (!(infile >> type_val >> a >> b)) 
        {
            std::cerr << "Test Error: Failed to read operation " << i+1 << " from file." << std::endl;
            ops.clear();
            return false;
        }

        // --- Validation ---
        // 1. Check valid type value
        if (type_val < 0 || type_val > 2) 
        {
            std::cerr << "Test Error: Invalid operation type at line " << i + 2 << ": type=" << type_val << " (must be 0, 1, or 2)" << std::endl;
            ops.clear(); 
            return false;
        }
        // 2. Check index 'a' bounds (required for all ops)
        if (a < 0 || a >= n_elements) 
        {
            std::cerr << "Test Error: Invalid index 'a' at line " << i + 2 << ": a=" << a << " (n_elements=" << n_elements << ")" << std::endl;
            ops.clear(); 
            return false;
        }
        // 3. Check index 'b' bounds (required for UNION and SAMESET)
        if (type_val == UNION_TYPE_VAL || type_val == SAMESET_TYPE_VAL) 
        {
            if (b < 0 || b >= n_elements) 
            {
                std::cerr << "Test Error: Invalid index 'b' for UNION/SAMESET op at line " << i + 2 << ": b=" << b << " (n_elements=" << n_elements << ")" << std::endl;
                ops.clear(); 
                return false;
            }
        }
        // --- End Validation ---

        CanonicalOperation op;
        op.a = a;
        op.b = b; // Store b; it will be ignored by find if not needed

        switch (type_val) 
        {
            case UNION_TYPE_VAL: op.type = CanonicalOperationType::UNION_OP; break;
            case FIND_TYPE_VAL: op.type = CanonicalOperationType::FIND_OP; break;
            case SAMESET_TYPE_VAL: op.type = CanonicalOperationType::SAMESET_OP; break;
        }
        ops.push_back(op);
        ops_loaded++;
    }

    if (ops_loaded != n_ops_in_file) 
    {
        std::cerr << "Test Warning: Expected " << n_ops_in_file << " operations in file header, but loaded " << ops_loaded << " operations from " << filename << "." << std::endl;
    }

    std::cout << "Loaded " << ops_loaded << " operations (UNION/FIND/SAMESET) for "
              << n_elements << " elements from " << filename << " for serial testing." << std::endl;
    return true;
}


int main(int argc, char* argv[]) 
{
    // --- Configuration ---
    // Expect exactly one argument: the test file path
    if (argc != 2) 
    {
        std::cerr << "Usage: " << argv[0] << " <test_operations_file>" << std::endl;
        std::cerr << "  File format: <n_elements> <n_ops>" << std::endl;
        std::cerr << "               <type> <a> <b> (type: 0=UNION, 1=FIND, 2=SAMESET)" << std::endl;
        return 1;
    }
    const std::string test_ops_file = argv[1];

    std::cout << "--- Testing Serial UnionFind Correctness ---" << std::endl;
    std::cout << "Test File: " << test_ops_file << std::endl;

    // --- Load Test Data ---
    int n_elements;
    std::vector<CanonicalOperation> operations;
    if (!load_operations_for_test(test_ops_file, n_elements, operations)) 
    {
        std::cerr << "Test Setup FAILED: Could not load test data." << std::endl;
        return 1;
    }
    if (operations.empty()) 
    {
        std::cerr << "Test Setup Warning: No operations loaded from file: " << test_ops_file << std::endl;
        std::cerr << "Test considered trivially PASSED as there's nothing to execute." << std::endl;
        return 0;
    }

    // --- Run Serial Implementation ---
    bool test_passed = true;
    std::vector<int> serial_op_results;
    try 
    {
        std::cout << "Instantiating UnionFind(" << n_elements << ")..." << std::endl;
        UnionFind uf_serial(n_elements);

        std::cout << "Running serial processOperations (" << operations.size() << " ops)..." << std::endl;
        uf_serial.processOperations(operations, serial_op_results);
        std::cout << "Serial processOperations complete." << std::endl;

        // --- Basic Verification ---
        // Check if the results vector has the expected size
        if (serial_op_results.size() != operations.size()) 
        {
             std::cerr << "Result Size Mismatch! Expected: " << operations.size()
                       << ", Got: " << serial_op_results.size() << std::endl;
             test_passed = false;
        } 
        else 
        {
            std::cout << "Result vector size matches operation count (" << serial_op_results.size() << ")." << std::endl;
            // Optional: Print first few results for basic sanity check
            // size_t print_limit = std::min((size_t)10, serial_op_results.size());
            // std::cout << "First " << print_limit << " results: ";
            // for(size_t i=0; i<print_limit; ++i) std::cout << serial_op_results[i] << " ";
            // std::cout << std::endl;
        }
    } 
    catch (const std::exception& e) 
    {
        std::cerr << "Test FAILED: An exception occurred during serial execution: " << e.what() << std::endl;
        test_passed = false;
    } 
    catch (...) 
    {
        std::cerr << "Test FAILED: An unknown exception occurred during serial execution." << std::endl;
        test_passed = false;
    }

    // --- Final Result ---
    std::cout << "\n========================================" << std::endl;
    if (test_passed) 
    {
        std::cout << "Overall Result: SERIAL TEST PASSED" << std::endl;
        std::cout << "========================================" << std::endl;
        return 0; // Success
    } 
    else 
    {
        std::cout << "Overall Result: SERIAL TEST FAILED" << std::endl;
        std::cout << "========================================" << std::endl;
        return 1; // Failure
    }
}
