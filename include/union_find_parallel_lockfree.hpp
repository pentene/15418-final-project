#ifndef UNION_FIND_PARALLEL_LOCKFREE_HPP
#define UNION_FIND_PARALLEL_LOCKFREE_HPP

#include <vector>
#include <atomic>
#include <numeric> // For std::iota
#include <stdexcept> // For std::runtime_error

// --- Lock-Free Union-Find Class ---

class UnionFindParallelLockFree 
{
private:

    // Represents the parent/rank information.
    // If A[i] >= 0, it's the parent index.
    // If A[i] < 0, i is a root, and -(A[i] + 1) is its rank.
    int n_elements;
    std::vector<std::atomic<int>> A;

    // Helper to check if a value represents a root (negative value)
    // Corresponds to isRank() in the pseudocode, but checks the value itself
    static inline bool is_root(int val) 
    {
        return val < 0;
    }

    // Helper to get the rank from a root's value
    static inline int get_rank(int root_val) 
    {
        // Assumes is_root(root_val) is true
        return -(root_val + 1);
    }

    // Helper to create the value to store for a root with a given rank
    static inline int make_root_val(int rank) 
    {
        return -(rank + 1);
    }

    // Internal find operation with path compression.
    // Returns the root index.
    // Corresponds to the Find function in the pseudocode (lines 16-23)
    // but simplified to only return the root, rank is handled separately if needed.
    std::pair<int, int> find_internal(int u);

public:
    enum class OperationType 
    {
        UNION_OP,
        FIND_OP,
        SAMESET_OP // Check if two elements are in the same set
    };

    // Structure to represent an operation
    struct Operation 
    {
        OperationType type;
        int a;
        int b; // Ignored for FIND_OP
    };
    // Constructs a UnionFindParallelLockFree with n elements (0 .. n-1).
    // Precondition: n >= 0
    explicit UnionFindParallelLockFree(int n);

    // Finds the representative (root) of the set containing element 'a'.
    // Performs path compression.
    // Returns the root index.
    int find(int a);

    // Unites the sets containing elements 'a' and 'b'.
    // Uses union by rank.
    // Returns true if 'a' and 'b' were in different sets (union performed), false otherwise.
    // Corresponds to the Union function in the pseudocode (lines 1-15)
    bool unionSets(int a, int b);

    // Checks if elements 'a' and 'b' are in the same set.
    // Corresponds to the SameSet function in the pseudocode (lines 25-30)
    bool sameSet(int a, int b);

    // Processes a list of operations in parallel using OpenMP.
    // Each thread calls the lock-free find/unionSets/sameSet methods.
    // Results vector will be resized and populated.
    // For FIND_OP, result is the root.
    // For UNION_OP, result is 1 if union occurred, 0 otherwise.
    // For SAMESET_OP, result is 1 if they are in the same set, 0 otherwise.
    // Precondition: For each op, 0 <= op.a < size(), and if op.type != FIND_OP, 0 <= op.b < size().
    void processOperations(const std::vector<Operation>& ops, std::vector<int>& results);

    // Returns the number of elements (n) the structure was initialized with.
    int size() const;

    // Destructor (default is sufficient as std::vector and std::atomic manage their own resources)
    ~UnionFindParallelLockFree() = default;

    // Disable copy and move semantics for simplicity, as copying atomics needs care
    UnionFindParallelLockFree(const UnionFindParallelLockFree&) = delete;
    UnionFindParallelLockFree& operator=(const UnionFindParallelLockFree&) = delete;
    UnionFindParallelLockFree(UnionFindParallelLockFree&&) = delete;
    UnionFindParallelLockFree& operator=(UnionFindParallelLockFree&&) = delete;
};

#endif // UNION_FIND_PARALLEL_LOCKFREE_HPP
