#ifndef UNION_FIND_PARALLEL_LOCKFREE_HPP
#define UNION_FIND_PARALLEL_LOCKFREE_HPP

#include <vector>
#include <atomic>   // For std::atomic
#include <cstdint>  // For std::int64_t
#include <cstddef>  // For std::size_t
#include <numeric>  // For std::iota
#include <cassert>  // For assertions

// Lock-Free Parallel Union-Find implementation using 128-bit Atomics (CMPXCHG16B).
// Assumes target platform (e.g., x86-64 Linux) supports 128-bit atomic operations.
// Uses CAS loops for find (path compression) and union (linking and rank update).
class UnionFindParallelLockFree {
public:
    // Supported operation types (consistent with other versions).
    enum class OperationType { UNION_OP, FIND_OP };

    // Operation structure (consistent with other versions).
    struct Operation {
        OperationType type;
        int a;
        int b; // For find operations this field is ignored.
    };

    // Node structure combining parent and rank for 128-bit atomic operations.
    // Must be 16-byte aligned for CMPXCHG16B.
    struct alignas(16) Node {
        std::int64_t parent; // Use 64-bit for potentially large number of elements
        std::int64_t rank;   // Use 64-bit for simplicity and alignment
    };

    // Constructs a UnionFindParallelLockFree with n elements (0 .. n-1).
    // Precondition: n >= 0
    explicit UnionFindParallelLockFree(int n);

    // Finds the representative (root) of the set containing element 'a' using
    // lock-free path compression with atomic CAS.
    // Precondition: 0 <= a < size()
    int find(int a);

    // Merges the sets that contain elements 'a' and 'b' using lock-free CAS.
    // Returns true if a merge occurred; false if they were already in the same set.
    // Uses 128-bit CAS to attempt atomic linking and rank updates.
    // Precondition: 0 <= a < size(), 0 <= b < size()
    bool unionSets(int a, int b);

    // Processes a list of operations in parallel using OpenMP.
    // Each thread calls the lock-free find/unionSets methods.
    // Precondition: For each op, 0 <= op.a < size(), and if op.type == UNION_OP, 0 <= op.b < size().
    void processOperations(const std::vector<Operation>& ops, std::vector<int>& results);

    // Returns the number of elements (n) the structure was initialized with.
    int size() const;

private:
    // Vector of atomic nodes. std::atomic<Node> relies on the compiler
    // correctly implementing 128-bit atomics for this struct.
    std::vector<std::atomic<Node>> nodes;
    int num_elements;
};

#endif // UNION_FIND_PARALLEL_LOCKFREE_HPP
