#ifndef UNION_FIND_PARALLEL_FINE_HPP
#define UNION_FIND_PARALLEL_FINE_HPP

#include <vector>
#include <mutex>
#include <numeric> // For std::iota
#include <cassert> // For assertions
#include <algorithm> // For std::min/max
#include <memory> // For potentially managing mutexes if needed (though vector often works)

// Enum to define the type of operation
enum class OperationType {
    UNION_OP,
    FIND_OP,
    SAMESET_OP // Check if two elements are in the same set
};

// Structure to represent an operation
struct Operation {
    OperationType type;
    int a;
    int b; // Used for UNION_OP and SAMESET_OP
};


// --- Fine-Grained Lock Union-Find Class ---

// Fine-Grained Lock Parallel Union-Find implementation using OpenMP.
// Each element has its own mutex, primarily used to lock roots during union operations.
// Path compression in find is best-effort due to potential races without complex traversal locking.
// Union operations are carefully locked to ensure correctness.
class UnionFindParallelFine {
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
        int b; // Used for UNION_OP and SAMESET_OP
    };

    // Constructs a UnionFindParallelFine with n elements (0 .. n-1).
    // Precondition: n >= 0
    explicit UnionFindParallelFine(int n);

    // Finds the representative (root) of the set containing element 'a'.
    // Performs best-effort path compression (potentially racy).
    // Precondition: 0 <= a < size()
    int find(int a);

    // Merges the sets that contain elements 'a' and 'b' using fine-grained locking.
    // Returns true if a merge occurred; false if they were already in the same set.
    // Locks the roots, verifies them, and performs union by rank.
    // Precondition: 0 <= a < size(), 0 <= b < size()
    bool unionSets(int a, int b);

    // Checks if elements 'a' and 'b' are in the same set.
    // Uses the potentially racy find operation. For higher consistency,
    // a locking mechanism similar to unionSets could be used, but is omitted
    // here for simplicity, matching the best-effort nature of find().
    // Precondition: 0 <= a < size(), 0 <= b < size()
    bool sameSet(int a, int b);

    // Processes a list of operations in parallel using OpenMP.
    // Each thread calls the fine-grained find/unionSets/sameSet methods.
    // The results vector is resized to ops.size() and populated as follows:
    // - For FIND_OP: result is the root index found by find(op.a).
    // - For UNION_OP: result is 1 if unionSets(op.a, op.b) returned true (union occurred), 0 otherwise.
    // - For SAMESET_OP: result is 1 if sameSet(op.a, op.b) returned true, 0 otherwise.
    // Precondition: For each op, 0 <= op.a < size(), and if op.type != FIND_OP, 0 <= op.b < size().
    void processOperations(const std::vector<Operation>& ops, std::vector<int>& results);

    // Returns the number of elements (n) the structure was initialized with.
    int size() const;

    // Destructor (default is sufficient)
    ~UnionFindParallelFine() = default;

    // Disable copy and move semantics due to mutex members
    UnionFindParallelFine(const UnionFindParallelFine&) = delete;
    UnionFindParallelFine& operator=(const UnionFindParallelFine&) = delete;
    UnionFindParallelFine(UnionFindParallelFine&&) = delete;
    UnionFindParallelFine& operator=(UnionFindParallelFine&&) = delete;


private:
    // Helper function for find without path compression, used during locked verification.
    int find_root_no_compression(int a) const;

    std::vector<int> parent;
    std::vector<int> rank;
    int num_elements;
    // Vector of mutexes, one for each potential root.
    // std::vector<std::mutex> works in C++11 and later for default construction.
    mutable std::vector<std::mutex> locks;
};

#endif // UNION_FIND_PARALLEL_FINE_HPP
