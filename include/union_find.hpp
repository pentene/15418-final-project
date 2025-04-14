#ifndef UNION_FIND_HPP
#define UNION_FIND_HPP

#include <vector>

// Serial Union-Find (Disjoint Set Union) Implementation with Path Compression and Union by Rank
class UnionFind {
public:
    // Constructs a UnionFind data structure with n elements (0 through n-1).
    explicit UnionFind(int n);

    // Finds the root (representative) of the set containing element 'a'.
    // Uses path compression to optimize future queries.
    int find(int a);

    // Merges the sets that contain elements 'a' and 'b'.
    // Returns true if a merge happened (i.e. a and b were in different sets), 
    // or false if they were already in the same set.
    bool unionSets(int a, int b);

private:
    // Parent vector where parent[i] is the parent of element i.
    // If parent[i] == i, then i is the representative (root) of its set.
    std::vector<int> parent;
    
    // Rank vector used to keep trees shallow by attaching smaller trees under larger ones.
    std::vector<int> rank;
};

#endif // UNION_FIND_HPP