#include "union_find_parallel_coarse.hpp"
#include <omp.h>    
#include <cassert> 
#include <vector>   

// Constructor
UnionFindParallelCoarse::UnionFindParallelCoarse(int n)
    : parent(n), rank(n, 0), num_elements(n) 
{
    // Precondition: n should not be negative.
    assert(n >= 0 && "Number of elements cannot be negative.");
    for (int i = 0; i < n; i++) 
    {
        parent[i] = i; // Each element is initially its own parent.
    }
    // Note: The mutex 'coarse_lock' is default-initialized.
}

// Find operation with path compression (thread-safe via coarse lock)
int UnionFindParallelCoarse::find(int a) 
{
    assert(a >= 0 && a < num_elements && "Element index out of bounds in find().");

    std::lock_guard<std::recursive_mutex> guard(coarse_lock); // Lock before accessing shared data
    if (parent[a] != a) {
        parent[a] = find(parent[a]);
    }
    return parent[a]; // Return the root of the set.
}

// Union operation with union by rank (thread-safe via coarse lock)
bool UnionFindParallelCoarse::unionSets(int a, int b) 
{
    assert(a >= 0 && a < num_elements && "Element index 'a' out of bounds in unionSets().");
    assert(b >= 0 && b < num_elements && "Element index 'b' out of bounds in unionSets().");

    std::lock_guard<std::recursive_mutex> guard(coarse_lock);

    int rootA = find(a);
    int rootB = find(b); 

    if (rootA == rootB) {
        return false; 
    }

    if (rank[rootA] < rank[rootB]) {
        parent[rootA] = rootB; 
    } else if (rank[rootA] > rank[rootB]) {
        parent[rootB] = rootA; 
    } else {
        parent[rootB] = rootA;
        ++rank[rootA];
    }
    return true;
}

bool UnionFindParallelCoarse::sameSet(int a, int b) {
    assert(a >= 0 && a < num_elements && "Element index 'a' out of bounds in sameSet().");
    assert(b >= 0 && b < num_elements && "Element index 'b' out of bounds in sameSet().");

    std::lock_guard<std::recursive_mutex> guard(coarse_lock); 
    return find(a) == find(b); 
}

// Process a batch of operations in parallel (coarse-grained locking)
void UnionFindParallelCoarse::processOperations(const std::vector<Operation>& ops, std::vector<int>& results) 
{
    size_t nOps = ops.size();
    results.resize(nOps); 

    // Use OpenMP to parallelize the loop over operations.
    // Each thread will execute some iterations of the loop.
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < static_cast<int>(nOps); i++) 
    {
        const auto& op = ops[i]; // Use a const reference for readability.
        assert(op.a >= 0 && op.a < num_elements && "Operation element 'a' out of bounds.");

        switch (op.type) 
        {
            case OperationType::UNION_OP: 
            {
                assert(op.b >= 0 && op.b < num_elements && "Operation element 'b' out of bounds for UNION_OP.");
                bool union_occurred = unionSets(op.a, op.b);
                results[i] = union_occurred ? 1 : 0; 
                break;
            }
            case OperationType::FIND_OP: 
            {
                results[i] = find(op.a);
                break;
            }
            case OperationType::SAMESET_OP: 
            {
                assert(op.b >= 0 && op.b < num_elements && "Operation element 'b' out of bounds for SAMESET_OP.");
                bool are_same = sameSet(op.a, op.b);
                results[i] = are_same ? 1 : 0;
                break;
            }
            default:
                assert(false && "Unknown operation type encountered.");
                results[i] = -2; // Indicate an error or unexpected state
                break;
        }
    }
}

// Get the size (number of elements)
int UnionFindParallelCoarse::size() const 
{
    return num_elements;
}