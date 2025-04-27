#ifndef UNION_FIND_HPP
#define UNION_FIND_HPP

#include <vector>
#include <cassert> // Include for assertions
#include <numeric> // For std::iota in constructor

// Serial Union-Find (Disjoint Set Union) Implementation with Path Compression
// and Union by Rank. Includes basic input validation via assertions.
// Defines Operation types and a function to process a list of operations,
// consistent with the parallel versions for benchmarking.
class UnionFind {
public:
    // Supported operation types.
    enum class OperationType { UNION_OP, FIND_OP, SAMESET_OP };

    // Operation structure consistent with parallel versions.
    struct Operation {
        OperationType type;
        int a;
        int b; // Used for UNION_OP and SAMESET_OP, ignored for FIND_OP
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

    // Checks if elements 'a' and 'b' are in the same set.
    // Precondition: 0 <= a < size(), 0 <= b < size()
    bool sameSet(int a, int b);

    // Processes a list of operations sequentially.
    // The results vector is resized to ops.size() and populated as follows:
    // - For FIND_OP: result is the root index found by find(op.a).
    // - For UNION_OP: result is 1 if unionSets(op.a, op.b) returned true (union occurred), 0 otherwise.
    // - For SAMESET_OP: result is 1 if sameSet(op.a, op.b) returned true, 0 otherwise.
    // Precondition: For each op, 0 <= op.a < size(), and if op.type != FIND_OP, 0 <= op.b < size().
    void processOperations(const std::vector<Operation>& ops, std::vector<int>& results);

    // Returns the number of elements (n) the structure was initialized with.
    int size() const;

    // Destructor (default is sufficient)
    ~UnionFind() = default;

    // Disable copy/move semantics (optional, but good practice if not needed)
    UnionFind(const UnionFind&) = delete;
    UnionFind& operator=(const UnionFind&) = delete;
    UnionFind(UnionFind&&) = delete;
    UnionFind& operator=(UnionFind&&) = delete;

private:
    std::vector<int> parent;
    std::vector<int> rank;
    int num_elements; // Store the size for bounds checking
};

#endif // UNION_FIND_HPP
