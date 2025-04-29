#ifndef UNION_FIND_PARALLEL_COARSE_HPP
#define UNION_FIND_PARALLEL_COARSE_HPP

#include <vector>
#include <mutex>
#include <cassert> // Include for assertions
#include <numeric>

// Enum to define the type of operatio
// --- Coarse-Grained Lock Union-Find Class ---

// Coarse-Grained Lock Parallel Union-Find implementation using OpenMP.
// Operations are protected by a single global recursive mutex.
// Includes basic input validation via assertions.
// The interface is consistent with the UnionFindParallelLockFree class.
class UnionFindParallelCoarse 
{
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
        int b; // Used for UNION_OP and SAMESET_OP
    };

    // Constructs a UnionFindParallelCoarse with n elements (0 .. n-1).
    // Precondition: n >= 0
    explicit UnionFindParallelCoarse(int n);

    // Finds the representative (root) of the set containing element 'a' using path compression.
    // This operation is thread-safe via a coarse-grained lock.
    // Precondition: 0 <= a < size()
    int find(int a);

    // Merges the sets that contain elements 'a' and 'b'.
    // Returns true if a merge occurred; false if they were already in the same set.
    // This operation is thread-safe via a coarse-grained lock.
    // Precondition: 0 <= a < size(), 0 <= b < size()
    bool unionSets(int a, int b);

    // Checks if elements 'a' and 'b' are in the same set.
    // This operation is thread-safe via a coarse-grained lock.
    // Precondition: 0 <= a < size(), 0 <= b < size()
    bool sameSet(int a, int b);

    // Processes a list of operations in parallel using OpenMP.
    // Each individual find/unionSets/sameSet call within the parallel loop is protected by the coarse lock.
    // The results vector is resized to ops.size() and populated as follows:
    // - For FIND_OP: result is the root index found by find(op.a).
    // - For UNION_OP: result is 1 if unionSets(op.a, op.b) returned true (union occurred), 0 otherwise.
    // - For SAMESET_OP: result is 1 if sameSet(op.a, op.b) returned true, 0 otherwise.
    // Precondition: For each op, 0 <= op.a < size(), and if op.type != FIND_OP, 0 <= op.b < size().
    void processOperations(const std::vector<Operation>& ops, std::vector<int>& results);

    // Returns the number of elements (n) the structure was initialized with.
    int size() const;

    // Destructor (default is sufficient)
    ~UnionFindParallelCoarse() = default;

    // Disable copy and move semantics for simplicity and safety with mutex
    UnionFindParallelCoarse(const UnionFindParallelCoarse&) = delete;
    UnionFindParallelCoarse& operator=(const UnionFindParallelCoarse&) = delete;
    UnionFindParallelCoarse(UnionFindParallelCoarse&&) = delete;
    UnionFindParallelCoarse& operator=(UnionFindParallelCoarse&&) = delete;

private:
    std::vector<int> parent;
    std::vector<int> rank;
    int num_elements;                   // Store the size for bounds checking
    mutable std::recursive_mutex coarse_lock; // Coarse-grained lock protecting all operations.
                                        // Recursive to allow find() called within unionSets()/sameSet() under the same lock.
                                        // Marked mutable to allow locking in const methods like size() if needed.
};
#endif // UNION_FIND_PARALLEL_COARSE_HPP
