#include "union_find.hpp"

UnionFind::UnionFind(int n)
    : parent(n), rank(n, 0) {
    for (int i = 0; i < n; ++i) {
        parent[i] = i;
    }
}

int UnionFind::find(int a) {
    if (parent[a] != a) {
        // Path compression: update parent to the root.
        parent[a] = find(parent[a]);
    }
    return parent[a];
}

bool UnionFind::unionSets(int a, int b) {
    int rootA = find(a);
    int rootB = find(b);
    if (rootA == rootB)
        return false; // Already in the same set.
    
    // Union by rank: attach smaller tree under larger.
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

void UnionFind::processOperations(const std::vector<Operation>& ops, std::vector<int>& results) {
    results.resize(ops.size());
    for (size_t i = 0; i < ops.size(); i++) {
        if (ops[i].type == OperationType::UNION_OP) {
            unionSets(ops[i].a, ops[i].b);
            results[i] = -1; // Indicate that union op has no "find" result.
        } else if (ops[i].type == OperationType::FIND_OP) {
            results[i] = find(ops[i].a);
        }
    }
}
