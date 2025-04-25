#include "union_find_parallel_fine.hpp"
#include <omp.h>
#include <vector>
#include <mutex>
#include <numeric>
#include <cassert>
#include <algorithm> // For std::swap

// Constructor
UnionFindParallelFine::UnionFindParallelFine(int n)
    : parent(n), rank(n, 0), num_elements(n), locks(n) { // Initialize locks vector with n default mutexes
    // Precondition: n should not be negative.
    assert(n >= 0 && "Number of elements cannot be negative.");
    // Initialize parent array: each element is its own parent initially.
    std::iota(parent.begin(), parent.end(), 0);
}

// Find operation with best-effort path compression (no locks during traversal/compression)
int UnionFindParallelFine::find(int a) {
    // Precondition: Element index 'a' must be within bounds.
    assert(a >= 0 && a < num_elements && "Element index out of bounds in find().");

    // 1. Find the root
    int root = a;
    // Note: Reading parent[root] here without locks might race with a concurrent union.
    // However, the union operation locks roots, so eventually, this traversal
    // should stabilize towards the correct root.
    while (parent[root] != root) {
        root = parent[root];
    }

    // 2. Path compression: Make all nodes on the path point directly to the root.
    // This part is potentially racy if a union modifies the path concurrently.
    // It's "best effort" - might not fully compress or point to the *absolute*
    // latest root in highly contested scenarios, but helps subsequent finds.
    int current = a;
    while (current != root) {
        int next = parent[current];
        // Potentially racy write without locking 'current'.
        parent[current] = root;
        current = next;
    }
    return root;
}

// Helper function: Finds the root without performing path compression.
// Used inside unionSets critical section where locks are already held.
// Marked const as it doesn't intend to modify state directly (path compression).
// The 'mutable' on 'locks' allows locking but isn't strictly needed here as we
// assume the caller holds the necessary locks.
int UnionFindParallelFine::find_root_no_compression(int a) const {
     assert(a >= 0 && a < num_elements && "Element index out of bounds in find_root_no_compression().");
     int current = a;
     while (parent[current] != current) {
        current = parent[current];
     }
     return current;
}


// Union operation with fine-grained locking and verification
bool UnionFindParallelFine::unionSets(int a, int b) {
    // Precondition: Element indices 'a' and 'b' must be within bounds.
    assert(a >= 0 && a < num_elements && "Element index 'a' out of bounds in unionSets().");
    assert(b >= 0 && b < num_elements && "Element index 'b' out of bounds in unionSets().");

    while (true) { // Loop to handle potential retries if roots change during locking
        int rootA = find(a); // Find potential roots (might race)
        int rootB = find(b);

        if (rootA == rootB) {
            return false; // Already in the same set (or were recently)
        }

        // Acquire locks in a fixed order (smaller index first) to prevent deadlock.
        int lock1_idx = std::min(rootA, rootB);
        int lock2_idx = std::max(rootA, rootB);

        std::lock_guard<std::mutex> guard1(locks[lock1_idx]);
        // Check if the root we intended to lock (lock1_idx) is still a root.
        // If not, another union might have happened. Release lock and retry.
        // Note: This check itself isn't strictly necessary if the verification below is done,
        // but can potentially save acquiring the second lock unnecessarily.
        // if (parent[lock1_idx] != lock1_idx) continue; // Optional optimization

        std::lock_guard<std::mutex> guard2(locks[lock2_idx]);
        // Check if the second root we intended to lock is still a root.
        // if (parent[lock2_idx] != lock2_idx) continue; // Optional optimization


        // *** Critical Section Start ***
        // Re-verify the roots *while holding the locks* using non-compressing find.
        // This ensures we are working with the *actual* current roots.
        int currentRootA = find_root_no_compression(a);
        int currentRootB = find_root_no_compression(b);

        // If roots changed and are now the same, or if the elements we locked
        // are no longer the roots we need to work with, we might need to retry.
        // However, if the *current* roots are the ones we locked, we can proceed.
        // If current roots are different from locked roots (rootA, rootB), it implies
        // a concurrent union happened. The simplest is often to just retry the whole operation.
        // Let's refine: we only care if the *current* roots are different now.
        if (currentRootA == currentRootB) {
            // They are now in the same set after acquiring locks. Unlock and return.
            // Guards automatically unlock on exit.
            return false;
        }

        // Ensure we are operating on the *actual* current roots we verified under lock.
        // If the verified roots (currentRootA, currentRootB) are different from the
        // roots whose locks we hold (rootA, rootB), it indicates a complex concurrent
        // modification. Retrying the whole operation is the safest approach.
        if (currentRootA != rootA || currentRootB != rootB) {
            // The structure changed significantly while we were trying to lock.
            // Release locks (automatically by guards) and retry the outer loop.
            continue;
        }

        // Proceed with the union using the verified roots (rootA, rootB)
        // which are equal to (currentRootA, currentRootB) at this point.
        if (rank[rootA] < rank[rootB]) {
            parent[rootA] = rootB;
        } else if (rank[rootA] > rank[rootB]) {
            parent[rootB] = rootA;
        } else {
            parent[rootB] = rootA;
            rank[rootA]++;
        }
        // *** Critical Section End ***

        return true; // Union successful
    } // End while(true)
}

// Process a batch of operations in parallel (fine-grained locking)
void UnionFindParallelFine::processOperations(const std::vector<Operation>& ops, std::vector<int>& results) {
    size_t nOps = ops.size();
    results.resize(nOps); // Ensure the results vector has the correct size.

    #pragma omp parallel for schedule(dynamic)
    for (int i = 0; i < static_cast<int>(nOps); ++i) {
        const auto& op = ops[i];

        // Precondition checks
        assert(op.a >= 0 && op.a < num_elements && "Operation element 'a' out of bounds.");
        if (op.type == OperationType::UNION_OP) {
            assert(op.b >= 0 && op.b < num_elements && "Operation element 'b' out of bounds for UNION_OP.");
            // Calls the fine-grained unionSets
            unionSets(op.a, op.b);
            results[i] = -1;
        } else { // op.type == OperationType::FIND_OP
            // Calls the fine-grained find
            results[i] = find(op.a);
        }
    }
}

// Get the size (number of elements)
int UnionFindParallelFine::size() const {
    // No lock needed as num_elements is immutable after construction.
    return num_elements;
}
