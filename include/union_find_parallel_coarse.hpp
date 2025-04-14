#ifndef UNION_FIND_PARALLEL_COARSE_HPP
#define UNION_FIND_PARALLEL_COARSE_HPP

#include <vector>
#include <mutex>

// Coarse-Grained Lock Parallel Union-Find implementation using OpenMP.
// Operations are protected by a single global (recursive) mutex.
// Also supports batched processing of union/find operations provided in a list.
class UnionFindParallelCoarse {
public:
    // Supported operation types.
    enum class OperationType { UNION_OP, FIND_OP };

    // Operation structure.
    struct Operation {
        OperationType type;
        int a;
        int b; // For find operations this field is ignored.
    };

    // Constructs a UnionFindParallelCoarse with n elements.
    explicit UnionFindParallelCoarse(int n);

    // Finds the representative of the set containing element a, using path compression.
    int find(int a);

    // Merges the sets containing elements a and b.
    bool unionSets(int a, int b);

    // Processes a list of operations in parallel using OpenMP.
    // The results vector is resized to ops.size() and for each operation:
    // - For UNION_OP, unionSets() is executed and results[i] is set to -1.
    // - For FIND_OP, find() is executed and its result is stored at results[i].
    void processOperations(const std::vector<Operation>& ops, std::vector<int>& results);

private:
    std::vector<int> parent;
    std::vector<int> rank;
    std::recursive_mutex lock;  // Coarse-grained lock protecting all operations.
};

#endif // UNION_FIND_PARALLEL_COARSE_HPP
