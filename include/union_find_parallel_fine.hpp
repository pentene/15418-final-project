#ifndef UNION_FIND_PARALLEL_FINE_HPP
#define UNION_FIND_PARALLEL_FINE_HPP

#include <vector>
#include <mutex>

class UnionFindParallelFine {
public:
    // Supported operation types.
    enum class OperationType { UNION_OP, FIND_OP };

    // Operation structure.
    struct Operation {
        OperationType type;
        int a;
        int b; // For find operations this field is ignored.
    };
    
    // Constructs a UnionFindParallelFine with n elements.
    explicit UnionFindParallelFine(int n);

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
    std::vector<std::recursive_mutex> mutexes;  // one mutex per node      
};

#endif // UNION_FIND_PARALLEL_FINE_HPP



