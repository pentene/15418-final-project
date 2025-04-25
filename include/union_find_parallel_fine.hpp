#ifndef UNION_FIND_PARALLEL_FINE_HPP
#define UNION_FIND_PARALLEL_FINE_HPP

#include <vector>
#include <mutex>
#include <numeric> // For std::iota
#include <cassert> // For assertions
#include <algorithm> // For std::min/max

// Fine-Grained Lock Parallel Union-Find implementation using OpenMP.
// Each element has its own mutex, primarily used to lock roots during union operations.
// Path compression in find is best-effort due to potential races without complex traversal locking.
// Union operations are carefully locked to ensure correctness.
class UnionFindParallelFine {
public:
    // Supported operation types (consistent with other versions).
    enum class OperationType { UNION_OP, FIND_OP };

    // Operation structure (consistent with other versions).
    struct Operation {
        OperationType type;
        int a;
        int b; // For find operations this field is ignored.
    };

    // Constructs a UnionFindParallelFine with n elements (0 .. n-1).
    // Precondition: n >= 0
    explicit UnionFindParallelFine(int n);

    // Finds the representative (root) of the set containing element 'a'.
    // Performs path compression, but this part might race with concurrent unions
    // without more complex locking during traversal. The root finding itself
    // should be safe assuming unionSets locks correctly.
    // Precondition: 0 <= a < size()
    int find(int a);

    // Merges the sets that contain elements 'a' and 'b' using fine-grained locking.
    // Returns true if a merge occurred; false if they were already in the same set.
    // Locks the roots, verifies them, and performs union by rank.
    // Precondition: 0 <= a < size(), 0 <= b < size()
    bool unionSets(int a, int b);

    // Processes a list of operations in parallel using OpenMP.
    // Each thread calls the fine-grained find/unionSets methods.
    // Precondition: For each op, 0 <= op.a < size(), and if op.type == UNION_OP, 0 <= op.b < size().
    void processOperations(const std::vector<Operation>& ops, std::vector<int>& results);

    // Returns the number of elements (n) the structure was initialized with.
    int size() const;

private:
    // Helper function for find without path compression, used during locked verification.
    int find_root_no_compression(int a) const;

    std::vector<int> parent;
    std::vector<int> rank;
    int num_elements;
    // Use a vector of mutexes, one for each element.
    // We primarily lock the mutexes corresponding to the roots.
    // Using mutable allows locking in const methods like find_root_no_compression if needed,
    // but here it's mainly for organization. std::mutex itself is not copyable/movable.
    // We need a dynamically sized container of non-movable mutexes.
    // A vector of unique_ptr<mutex> or a custom allocator might be needed,
    // or simply accept that the vector won't be efficiently resizable after init.
    // For simplicity here, we'll assume std::vector<std::mutex> works as needed post C++11
    // (it's often implemented with non-movable internal storage).
    mutable std::vector<std::mutex> locks;
};

#endif // UNION_FIND_PARALLEL_FINE_HPP