#ifndef UNION_FIND_HPP
#define UNION_FIND_HPP

#include <vector>
#include <cassert> // Include for assertions

// Serial Union-Find (Disjoint Set Union) Implementation with Path Compression
// and Union by Rank. Includes basic input validation via assertions.
// In addition to the traditional interface, we define an
// Operation type and a function to process a list of operations.
class UnionFind {
public:
    // Supported operation types.
    enum class OperationType { UNION_OP, FIND_OP };

    // Each operation is either a union (two elements to merge) or a find (one element).
    // For union operations, both 'a' and 'b' are used.
    // For find operations, only 'a' is used (the result of find(a) is returned).
    struct Operation {
        OperationType type;
        int a;
        int b; // For find operations this field is ignored.
    };

    // Constructs a UnionFind data structure with n elements (0 .. n-1).
    // Precondition: n >= 0
    explicit UnionFind(int n);

    // Finds the representative (root) of the set containing element 'a' using path compression.
    // Precondition: 0 <= a < size()
    int find(int a);

    // Merges the sets that contain elements 'a' and 'b'.
    // Returns true if a merge occurred; false if they were already in the same set.
    // Precondition: 0 <= a < size(), 0 <= b < size()
    bool unionSets(int a, int b);

    // Processes a list of operations sequentially.
    // The results vector is resized to ops.size() and for each operation:
    // - For UNION_OP, unionSets() is called and results[i] is set to -1.
    // - For FIND_OP, find() is called and its result is stored at results[i].
    // Precondition: For each op, 0 <= op.a < size(), and if op.type == UNION_OP, 0 <= op.b < size().
    void processOperations(const std::vector<Operation>& ops, std::vector<int>& results);

    // Returns the number of elements (n) the structure was initialized with.
    int size() const;

private:
    std::vector<int> parent;
    std::vector<int> rank;
    int num_elements; // Store the size for bounds checking
};

#endif // UNION_FIND_HPP
