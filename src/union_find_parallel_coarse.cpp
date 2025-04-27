#include "union_find_parallel_coarse.hpp"
#include <omp.h>    // Include OpenMP header
#include <cassert>  // Include for assertions
#include <vector>   // Ensure vector is included

// Constructor
UnionFindParallelCoarse::UnionFindParallelCoarse(int n)
    : parent(n), rank(n, 0), num_elements(n) {
    // Precondition: n should not be negative.
    assert(n >= 0 && "Number of elements cannot be negative.");
    for (int i = 0; i < n; ++i) {
        parent[i] = i; // Each element is initially its own parent.
    }
    // Note: The mutex 'coarse_lock' is default-initialized.
}

// Find operation with path compression (thread-safe via coarse lock)
int UnionFindParallelCoarse::find(int a) {
    // Precondition: Element index 'a' must be within bounds.
    // Note: Assertion is outside the lock for faster failure detection if possible,
    // but the core logic requires the lock.
    assert(a >= 0 && a < num_elements && "Element index out of bounds in find().");

    std::lock_guard<std::recursive_mutex> guard(coarse_lock); // Lock before accessing shared data
    if (parent[a] != a) {
        // Path compression: Recursively find the root and update the parent directly.
        // Recursive call is safe because we use a recursive_mutex.
        parent[a] = find(parent[a]);
    }
    return parent[a]; // Return the root of the set.
}

// Union operation with union by rank (thread-safe via coarse lock)
bool UnionFindParallelCoarse::unionSets(int a, int b) {
    // Precondition: Element indices 'a' and 'b' must be within bounds.
    // Assertions outside the lock.
    assert(a >= 0 && a < num_elements && "Element index 'a' out of bounds in unionSets().");
    assert(b >= 0 && b < num_elements && "Element index 'b' out of bounds in unionSets().");

    std::lock_guard<std::recursive_mutex> guard(coarse_lock); // Lock before accessing shared data

    // find() calls will acquire the same lock recursively.
    int rootA = find(a); // Find the root of the set containing 'a'.
    int rootB = find(b); // Find the root of the set containing 'b'.

    if (rootA == rootB) {
        return false; // 'a' and 'b' are already in the same set.
    }

    // Union by rank: Attach the shorter tree to the root of the taller tree.
    if (rank[rootA] < rank[rootB]) {
        parent[rootA] = rootB; // Make rootB the parent of rootA.
    } else if (rank[rootA] > rank[rootB]) {
        parent[rootB] = rootA; // Make rootA the parent of rootB.
    } else {
        // Ranks are equal: arbitrarily make rootA the parent of rootB.
        parent[rootB] = rootA;
        // Increment the rank of the new root (rootA).
        ++rank[rootA];
    }
    return true; // A union occurred.
}

// Process a batch of operations in parallel (coarse-grained locking)
void UnionFindParallelCoarse::processOperations(const std::vector<Operation>& ops, std::vector<int>& results) {
    size_t nOps = ops.size();
    results.resize(nOps); // Ensure the results vector has the correct size.

    // Use OpenMP to parallelize the loop over operations.
    // Each thread will execute some iterations of the loop.
    // The schedule(dynamic) clause can help balance load if operations take variable time,
    // though with coarse locking, contention might be the bigger factor.
    #pragma omp parallel for schedule(dynamic)
    for (int i = 0; i < static_cast<int>(nOps); ++i) { // Use int for loop variable as common practice with OpenMP
        const auto& op = ops[i]; // Use a const reference for readability.

        // Precondition check for each operation within the loop
        // These assertions run within each thread before potentially calling find/unionSets.
        assert(op.a >= 0 && op.a < num_elements && "Operation element 'a' out of bounds.");

        if (op.type == OperationType::UNION_OP) {
            assert(op.b >= 0 && op.b < num_elements && "Operation element 'b' out of bounds for UNION_OP.");
            // unionSets acquires the global lock, serializing the core work.
            unionSets(op.a, op.b);
            results[i] = -1; // Indicate union operation (no specific find result).
        } else { // op.type == OperationType::FIND_OP
            // find acquires the global lock, serializing the core work.
            results[i] = find(op.a); // Store the result of the find operation.
        }
    }
}

// Get the size (number of elements)
int UnionFindParallelCoarse::size() const {
    // No lock needed as num_elements is immutable after construction.
    return num_elements;
}