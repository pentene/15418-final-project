#include "union_find.hpp" // Use quotes for local header
#include <vector>
#include <cassert>
#include <numeric> // For std::iota

// Constructor
UnionFind::UnionFind(int n)
    : parent(n), rank(n, 0), num_elements(n) { // Initialize num_elements
    // Precondition: n should not be negative.
    assert(n >= 0 && "Number of elements cannot be negative.");
    // Initialize parent array: each element is its own parent initially.
    std::iota(parent.begin(), parent.end(), 0);
}

// Find operation with path compression
int UnionFind::find(int a) {
    // Precondition: Element index 'a' must be within bounds.
    assert(a >= 0 && a < num_elements && "Element index out of bounds in find().");

    if (parent[a] != a) {
        // Path compression: update parent to the root recursively.
        parent[a] = find(parent[a]);
    }
    return parent[a];
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

// Check if two elements are in the same set
bool UnionFind::sameSet(int a, int b) {
    // Precondition: Element indices 'a' and 'b' must be within bounds.
    assert(a >= 0 && a < num_elements && "Element index 'a' out of bounds in sameSet().");
    assert(b >= 0 && b < num_elements && "Element index 'b' out of bounds in sameSet().");

    // Find roots without modifying structure further (find already does compression)
    return find(a) == find(b);
}


// Process a batch of operations sequentially, storing results as specified
void UnionFind::processOperations(const std::vector<Operation>& ops, std::vector<int>& results) {
    size_t nOps = ops.size();
    results.resize(nOps); // Ensure the results vector has the correct size.

    for (size_t i = 0; i < nOps; ++i) {
        const auto& op = ops[i]; // Use a const reference for readability.

        // Precondition checks (can be done inside the loop for serial)
        assert(op.a >= 0 && op.a < num_elements && "Operation element 'a' out of bounds.");

        switch (op.type) {
            case OperationType::UNION_OP: {
                assert(op.b >= 0 && op.b < num_elements && "Operation element 'b' out of bounds for UNION_OP.");
                bool union_occurred = unionSets(op.a, op.b);
                results[i] = union_occurred ? 1 : 0; // Store 1 if union happened, 0 otherwise.
                break;
            }
            case OperationType::FIND_OP: {
                results[i] = find(op.a); // Store the result of the find operation (root).
                break;
            }
            case OperationType::SAMESET_OP: {
                 assert(op.b >= 0 && op.b < num_elements && "Operation element 'b' out of bounds for SAMESET_OP.");
                bool are_same = sameSet(op.a, op.b);
                results[i] = are_same ? 1 : 0; // Store 1 if they are in the same set, 0 otherwise.
                break;
            }
             default:
                 // Should not happen if input validation is correct
                 assert(false && "Unknown operation type encountered.");
                 results[i] = -2; // Indicate an error or unexpected state
                 break;
        }
    }
}

// Get the size (number of elements)
int UnionFind::size() const {
    return num_elements;
}
