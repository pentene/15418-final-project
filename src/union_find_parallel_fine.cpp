#include "union_find_parallel_fine.hpp"
#include <omp.h>

UnionFindParallelFine::UnionFindParallelFine(int n)
    : parent(n), rank(n, 0), mutexes(n) {
    for (int i = 0; i < n; ++i) 
    {
        parent[i] = i;
    }
}

int UnionFindParallelFine::find(int a) {
    std::lock_guard<std::recursive_mutex> guard(mutexes[a]);
    if (parent[a] != a) 
    {
        parent[a] = find(parent[a]);
    }
    return parent[a];
}

bool UnionFindParallelFine::unionSets(int a, int b) {
    if (a == b) 
    {
        return false; // No need to union the same element.
    }
    std::unique_lock<std::recursive_mutex> lockA(mutexes[a], std::defer_lock);
    std::unique_lock<std::recursive_mutex> lockB(mutexes[b], std::defer_lock);
    std::lock(lockA, lockB); // Lock both mutexes without deadlock

    int rootA = find(a);
    int rootB = find(b);
    if (rootA == rootB) 
    {
        return false;
    }
    if (rank[rootA] < rank[rootB]) 
    {
        parent[rootA] = rootB;
    } else if (rank[rootA] > rank[rootB]) 
    {
        parent[rootB] = rootA;
    } else 
    {
        parent[rootB] = rootA;
        ++rank[rootA];
    }
    return true;
}

void UnionFindParallelFine::processOperations(const std::vector<Operation>& ops, std::vector<int>& results) {
    size_t nOps = ops.size();
    results.resize(nOps);

    // Use OpenMP to process each operation in parallel.
    // Note: Even though iterations run concurrently, each union/find call acquires the same lock,
    // ensuring thread safety.
    #pragma omp parallel for schedule(dynamic)
    for (int i = 0; i < static_cast<int>(nOps); i++) {
        const auto& op = ops[i];
        if (op.type == OperationType::UNION_OP) {
            unionSets(op.a, op.b);
            results[i] = -1;  // For union operations, no 'find' result.
        } else if (op.type == OperationType::FIND_OP) {
            results[i] = find(op.a);
        }
    }
}
