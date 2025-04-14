#include "union_find.hpp"

UnionFind::UnionFind(int size) : parent(size), rank(size, 0) {
    for (int i = 0; i < size; ++i) {
        parent[i] = i;
    }
}

int UnionFind::find(int x) {
    if (parent[x] != x) {
        parent[x] = find(parent[x]);  // Path compression
    }
    return parent[x];
}

void UnionFind::unionSets(int a, int b) {
    int rootA = find(a);
    int rootB = find(b);
    if (rootA != rootB) {
        if (rank[rootA] < rank[rootB]) {
            parent[rootA] = rootB;
        } else {
            parent[rootB] = rootA;
            if (rank[rootA] == rank[rootB]) {
                rank[rootA]++;
            }
        }
    }
}
