#ifndef UNION_FIND_PARALLEL_LOCKFREE_IPC_HPP
#define UNION_FIND_PARALLEL_LOCKFREE_IPC_HPP

#include <vector>
#include <atomic>
#include <utility> // For std::pair
#include <stdexcept> // For std::runtime_error, std::out_of_range, std::invalid_argument

// --- Operation Definition ---
// Assuming these are defined globally or in a shared header.
// If not, define them here or include the relevant header.


// --- Lock-Free Union-Find Class with Plain Write Path Compaction ---

class UnionFindParallelLockFreeIPC {
private:
    // Represents the parent/rank information.
    // If A[i] >= 0, it's the parent index.
    // If A[i] < 0, i is a root, and -(A[i] + 1) is its rank.
    int n_elements;
    std::vector<std::atomic<int>> A;

    // Helper to check if a value represents a root (negative value)
    static inline bool is_root(int val) {
        return val < 0;
    }

    // Helper to get the rank from a root's value
    static inline int get_rank(int root_val) {
        // Assumes is_root(root_val) is true
        return -(root_val + 1);
    }

    // Helper to create the value to store for a root with a given rank
    static inline int make_root_val(int rank) {
        return -(rank + 1);
    }

    // Internal find operation matching pseudocode structure.
    // Returns {root_index, root_value} where root_value encodes rank if it's a root.
    // Performs path compression using relaxed writes (Optimization).
    std::pair<int, int> find_internal(int u);


public:
    enum class OperationType {
        UNION_OP,
        FIND_OP,
        SAMESET_OP // Check if two elements are in the same set
    };

    // Structure to represent an operation
    struct Operation {
        OperationType type;
        int a;
        int b; // Ignored for FIND_OP
    };
    // Constructs a UnionFindParallelLockFreePlainIP with n elements (0 .. n-1).
    // Precondition: n >= 0
    explicit UnionFindParallelLockFreeIPC(int n);

    // Finds the representative (root) of the set containing element 'a'.
    // Performs path compression (using optimized writes internally).
    // Returns the root index.
    int find(int a);

    // Unites the sets containing elements 'a' and 'b'.
    // Uses union by rank and CAS for critical updates.
    // Returns true if 'a' and 'b' were in different sets (union performed), false otherwise.
    bool unionSets(int a, int b);

    // Checks if elements 'a' and 'b' are in the same set.
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

    // Destructor (default is sufficient)
    ~UnionFindParallelLockFreeIPC() = default;

    // Disable copy and move semantics
    UnionFindParallelLockFreeIPC(const UnionFindParallelLockFreeIPC&) = delete;
    UnionFindParallelLockFreeIPC& operator=(const UnionFindParallelLockFreeIPC&) = delete;
    UnionFindParallelLockFreeIPC(UnionFindParallelLockFreeIPC&&) = delete;
    UnionFindParallelLockFreeIPC& operator=(UnionFindParallelLockFreeIPC&&) = delete;
};

#endif // UNION_FIND_PARALLEL_LOCKFREE_PLAIN_WRITE_HPP
