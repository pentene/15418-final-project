#ifndef UNION_FIND_PARALLEL_COARSE_HPP
#define UNION_FIND_PARALLEL_COARSE_HPP

#include <vector>
#include <mutex>
#include <cassert> // Include for assertions

// Coarse-Grained Lock Parallel Union-Find implementation using OpenMP.
// Operations are protected by a single global recursive mutex.
// Includes basic input validation via assertions.
// The interface is consistent with the serial UnionFind class.
class UnionFindParallelCoarse {
public:
    // Supported operation types (consistent with serial version).
    enum class OperationType { UNION_OP, FIND_OP };

    // Operation structure (consistent with serial version).
    struct Operation {
        OperationType type;
        int a;
        int b; // For find operations this field is ignored.
    };

    // Constructs a UnionFindParallelCoarse with n elements (0 .. n-1).
    // Precondition: n >= 0
    explicit UnionFindParallelCoarse(int n);

    // Finds the representative (root) of the set containing element 'a' using path compression.
    // This operation is thread-safe via a coarse-grained lock.
    // Precondition: 0 <= a < size()
    int find(int a);

    // Merges the sets that contain elements 'a' and 'b'.
    // Returns true if a merge occurred; false if they were already in the same set.
    // This operation is thread-safe via a coarse-grained lock.
    // Precondition: 0 <= a < size(), 0 <= b < size()
    bool unionSets(int a, int b);

    // Processes a list of operations in parallel using OpenMP.
    // Each individual find/union call within the parallel loop is protected by the coarse lock.
    // The results vector is resized to ops.size() and for each operation:
    // - For UNION_OP, unionSets() is executed and results[i] is set to -1.
    // - For FIND_OP, find() is executed and its result is stored at results[i].
    // Precondition: For each op, 0 <= op.a < size(), and if op.type == UNION_OP, 0 <= op.b < size().
    void processOperations(const std::vector<Operation>& ops, std::vector<int>& results);

    // Returns the number of elements (n) the structure was initialized with.
    int size() const;

private:
    std::vector<int> parent;
    std::vector<int> rank;
    int num_elements;                 // Store the size for bounds checking
    std::recursive_mutex coarse_lock; // Coarse-grained lock protecting all operations.
                                      // Recursive to allow find() called within unionSets() under the same lock.
};

#endif // UNION_FIND_PARALLEL_COARSE_HPP
