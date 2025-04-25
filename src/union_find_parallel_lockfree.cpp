#include "union_find_parallel_lockfree.hpp"
#include <omp.h>
#include <vector>
#include <atomic>
#include <cstdint>
#include <cassert>
#include <stdexcept> // For potential exceptions if atomics are not lock-free

// Constructor
UnionFindParallelLockFree::UnionFindParallelLockFree(int n)
    : nodes(n), num_elements(n) {
    // Precondition: n should not be negative.
    assert(n >= 0 && "Number of elements cannot be negative.");

    // Check if the atomic<Node> is indeed lock-free (likely uses CMPXCHG16B)
    // If this assertion fails, the platform/compiler doesn't support the required 128-bit atomics.
    // Note: In C++20, std::atomic<T>::is_always_lock_free could be checked at compile time.
    // For C++17 or earlier, we check at runtime.
    std::atomic<Node> temp_node;
    if (!temp_node.is_lock_free()) {
        // Consider throwing or logging a fatal error if 128-bit atomics aren't supported
        // throw std::runtime_error("128-bit atomic<Node> is not lock-free on this platform.");
        // For now, assert:
        assert(false && "std::atomic<Node> (128-bit) is not lock-free!");
    }


    for (int i = 0; i < n; ++i) {
        // Initialize each node: parent is itself, rank is 0.
        // Use .store() with memory_order_relaxed initially, as no other threads access yet.
        nodes[i].store({ .parent = static_cast<std::int64_t>(i), .rank = 0 }, std::memory_order_relaxed);
    }
}

// Lock-free find operation with path compression using CAS
int UnionFindParallelLockFree::find(int a) {
    // Precondition: Element index 'a' must be within bounds.
    assert(a >= 0 && a < num_elements && "Element index out of bounds in find().");

    int current = a;
    Node current_node;

    // 1. Find the root
    while (true) {
        current_node = nodes[current].load(std::memory_order_acquire); // Read node info
        if (current_node.parent == current) {
            break; // Found the root
        }
        current = static_cast<int>(current_node.parent); // Move up
        // Optional: Add yield/pause here if contention is extremely high
    }
    int root = current; // 'current' now holds the root index

    // 2. Path compression: Make nodes on the path point directly to the root
    current = a; // Start back at the original element
    while (true) {
         Node node_to_update = nodes[current].load(std::memory_order_acquire);
         // If parent is already the root, or if 'current' is the root itself, stop compressing this path segment.
         if (node_to_update.parent == root || current == root) {
            break;
         }

         // Prepare the desired state: parent points to root, rank remains unchanged.
         Node desired_node = { .parent = static_cast<std::int64_t>(root), .rank = node_to_update.rank };

         // Attempt to atomically update the parent pointer using CAS.
         // We use weak because it's okay if it fails spuriously; we'll just retry.
         // memory_order_release on success ensures prior writes are visible,
         // memory_order_acquire on failure ensures we re-read fresh state.
         if (nodes[current].compare_exchange_weak(node_to_update, desired_node,
                                                  std::memory_order_release,
                                                  std::memory_order_acquire)) {
            // Success! Move to the next node in the original path (before compression).
            // We need the parent *before* we potentially updated it.
            // However, the CAS only succeeded if node_to_update was the current value,
            // so node_to_update.parent holds the next element up the original path.
            current = static_cast<int>(node_to_update.parent);
         } else {
            // CAS failed, likely due to a concurrent update.
            // The loop condition (node_to_update.parent == root) or the outer
            // root finding loop will handle converging to the correct state eventually.
            // We might need to re-read the root if the structure changed drastically.
            // For simplicity here, we continue the compression attempt from the current 'current'.
            // A more robust implementation might re-run the root finding part if CAS fails many times.
            // Let's break this inner loop and return the root found initially.
            // The next find() call will continue the compression.
            break; // Exit compression loop for this element if CAS fails
         }

        // Safety break: If current somehow didn't change, exit to prevent infinite loop.
        // This shouldn't happen with the logic above but is a safeguard.
        // if (static_cast<int>(node_to_update.parent) == current) break;
    }

    return root;
}


// Lock-free union operation using 128-bit CAS
bool UnionFindParallelLockFree::unionSets(int a, int b) {
    // Precondition: Element indices 'a' and 'b' must be within bounds.
    assert(a >= 0 && a < num_elements && "Element index 'a' out of bounds in unionSets().");
    assert(b >= 0 && b < num_elements && "Element index 'b' out of bounds in unionSets().");

    while (true) { // Loop until successful union or determined they are already joined.
        int rootA_idx = find(a);
        int rootB_idx = find(b);

        if (rootA_idx == rootB_idx) {
            return false; // Already in the same set.
        }

        // Load the current state of the root nodes. Acquire semantics needed.
        Node rootA_node = nodes[rootA_idx].load(std::memory_order_acquire);
        Node rootB_node = nodes[rootB_idx].load(std::memory_order_acquire);

        // Verify they are still roots. If not, another thread modified them; retry find.
        // Need to check both parent pointers.
        if (rootA_node.parent != rootA_idx || rootB_node.parent != rootB_idx) {
            continue; // State changed concurrently, retry the whole operation.
        }

        // Determine which root becomes the new parent based on rank.
        int child_idx, parent_idx;
        Node child_node, parent_node;

        if (rootA_node.rank < rootB_node.rank) {
            child_idx = rootA_idx; child_node = rootA_node;
            parent_idx = rootB_idx; parent_node = rootB_node;
        } else if (rootB_node.rank < rootA_node.rank) {
            child_idx = rootB_idx; child_node = rootB_node;
            parent_idx = rootA_idx; parent_node = rootA_node;
        } else {
            // Ranks are equal. Arbitrarily choose one, e.g., lower index becomes child.
            // This helps prevent cycles in some edge cases, though path compression complicates guarantees.
            if (rootA_idx < rootB_idx) {
                 child_idx = rootA_idx; child_node = rootA_node;
                 parent_idx = rootB_idx; parent_node = rootB_node;
            } else {
                 child_idx = rootB_idx; child_node = rootB_node;
                 parent_idx = rootA_idx; parent_node = rootA_node;
            }
        }

        // Attempt to link the child root to the parent root using 128-bit CAS.
        // Expected state: child root still points to itself.
        Node expected_child_node = child_node; // The state we read earlier
        // Desired state: child root points to the parent root, rank remains the same.
        Node desired_child_node = { .parent = static_cast<std::int64_t>(parent_idx), .rank = child_node.rank };

        // CAS on the child node. Release semantics on success, Acquire on failure.
        if (nodes[child_idx].compare_exchange_weak(expected_child_node, desired_child_node,
                                                   std::memory_order_release,
                                                   std::memory_order_acquire)) {
            // Successfully linked child to parent!
            // Now, if ranks were equal, attempt to increment the parent's rank.
            if (rootA_node.rank == rootB_node.rank) {
                // Expected state for parent: the state we read earlier (parent_node)
                Node expected_parent_node = parent_node;
                // Desired state: parent still points to itself, rank is incremented.
                Node desired_parent_node = { .parent = static_cast<std::int64_t>(parent_idx), .rank = parent_node.rank + 1 };

                // Attempt to CAS the parent node's rank.
                // It's okay if this fails (e.g., another thread already incremented it
                // or performed another union). The structure remains correct.
                nodes[parent_idx].compare_exchange_weak(expected_parent_node, desired_parent_node,
                                                        std::memory_order_release,
                                                        std::memory_order_relaxed); // Relaxed on failure is okay here
            }
            return true; // Union successful.
        }
        // else: CAS failed. The state of child_idx changed concurrently.
        // The outer loop will retry the entire operation (re-find roots, etc.).
    }
}

// Process a batch of operations in parallel (lock-free)
void UnionFindParallelLockFree::processOperations(const std::vector<Operation>& ops, std::vector<int>& results) {
    size_t nOps = ops.size();
    results.resize(nOps); // Ensure the results vector has the correct size.

    #pragma omp parallel for schedule(dynamic)
    for (int i = 0; i < static_cast<int>(nOps); ++i) {
        const auto& op = ops[i];

        // Precondition checks
        assert(op.a >= 0 && op.a < num_elements && "Operation element 'a' out of bounds.");
        if (op.type == OperationType::UNION_OP) {
            assert(op.b >= 0 && op.b < num_elements && "Operation element 'b' out of bounds for UNION_OP.");
            // Calls the lock-free unionSets
            unionSets(op.a, op.b);
            results[i] = -1;
        } else { // op.type == OperationType::FIND_OP
            // Calls the lock-free find
            results[i] = find(op.a);
        }
    }
}

// Get the size (number of elements)
int UnionFindParallelLockFree::size() const {
    // No atomics needed as num_elements is immutable after construction.
    return num_elements;
}
