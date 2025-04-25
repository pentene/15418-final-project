#include "union_find.hpp"
#include <cassert> // Make sure assert is included

// Constructor
UnionFind::UnionFind(int n)
    : parent(n), rank(n, 0), num_elements(n) {
    // Precondition: n should not be negative.
    assert(n >= 0 && "Number of elements cannot be negative.");
    for (int i = 0; i < n; ++i) {
        parent[i] = i; // Each element is initially its own parent.
    }
}

// Find operation with path compression
int UnionFind::find(int a) {
    // Precondition: Element index 'a' must be within bounds.
    assert(a >= 0 && a < num_elements && "Element index out of bounds in find().");

    if (parent[a] != a) {
        // Path compression: Recursively find the root and update the parent directly.
        parent[a] = find(parent[a]);
    }
    return parent[a]; // Return the root of the set.
}

// Union operation with union by rank
bool UnionFind::unionSets(int a, int b) {
    // Precondition: Element indices 'a' and 'b' must be within bounds.
    assert(a >= 0 && a < num_elements && "Element index 'a' out of bounds in unionSets().");
    assert(b >= 0 && b < num_elements && "Element index 'b' out of bounds in unionSets().");

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

// Process a batch of operations
void UnionFind::processOperations(const std::vector<Operation>& ops, std::vector<int>& results) {
    results.resize(ops.size()); // Ensure the results vector has the correct size.

    for (size_t i = 0; i < ops.size(); ++i) {
        const auto& op = ops[i]; // Use a const reference for readability.

        // Precondition check for each operation within the loop
        assert(op.a >= 0 && op.a < num_elements && "Operation element 'a' out of bounds.");
        if (op.type == OperationType::UNION_OP) {
             assert(op.b >= 0 && op.b < num_elements && "Operation element 'b' out of bounds for UNION_OP.");
             unionSets(op.a, op.b);
             results[i] = -1; // Indicate union operation (no specific find result).
        } else { // op.type == OperationType::FIND_OP
             results[i] = find(op.a); // Store the result of the find operation.
        }
    }
}

// Get the size (number of elements)
int UnionFind::size() const {
    return num_elements;
}

