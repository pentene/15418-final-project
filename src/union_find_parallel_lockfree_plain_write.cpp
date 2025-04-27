#include "union_find_parallel_lockfree_plain_write.hpp" // Include the new header
#include <omp.h>        // For OpenMP directives
#include <iostream>     // For potential debugging (std::cerr)
#include <vector>       // Included via header, but good practice
#include <atomic>       // Included via header
#include <utility>      // Included via header
#include <stdexcept>    // Included via header

// Note: Helper functions (is_root, get_rank, make_root_val) are defined
// as static inline members within the class declaration in the header.

// --- Constructor ---

UnionFindParallelLockFreePlainWrite::UnionFindParallelLockFreePlainWrite(int n)
    : n_elements(n),
      A(n) // Default-construct n atomic<int> elements
{
    if (n < 0) {
        throw std::invalid_argument("Number of elements cannot be negative.");
    }
    for (int i = 0; i < n; ++i) {
        // Initialize each element as a root with rank 0. Store rank 0 as value -1.
        A[i].store(make_root_val(0), std::memory_order_relaxed);
    }
}

// --- Core Lock-Free Operations (Aligned with Pseudocode + Plain Write Optimization) ---

// Internal find operation matching pseudocode structure (lines 16-23)
// Returns {root_index, root_value} where root_value encodes rank if it's a root.
// Performs path compression using relaxed writes (Optimization based on low contention).
std::pair<int, int> UnionFindParallelLockFreePlainWrite::find_internal(int u) {
    // Line 17: p = A[u]
    int p_val = A[u].load(std::memory_order_acquire); // Read parent/rank info. Acquire needed for sync with union.

    // Line 18: if isRank(p) : return (u, p)
    if (is_root(p_val)) {
        // Base case: u is the root. Return u and its value (which encodes rank).
        return {u, p_val};
    }

    // Line 19: (root, rank_val) = Find(p)
    // Recursive step: Find the root of the parent p_val (which is an index here).
    int p_idx = p_val; // p_val must be an index if not a root
    std::pair<int, int> root_info = find_internal(p_idx);
    int root_idx = root_info.first;
    // int root_val = root_info.second; // Root's value is available if needed

    // --- Optimization: Path Compaction via Plain Writes ---
    // Original pseudocode lines 20-21 used CAS: if p != root: CAS(&A[u], p, root)
    // Optimization: Replace CAS with a relaxed store, assuming low contention makes CAS overhead unnecessary.
    if (p_idx != root_idx) {
        // Directly store the found root index as the new parent for u.
        // Use relaxed memory order as suggested by the optimization text for performance.
        A[u].store(root_idx, std::memory_order_relaxed);
    }
    // --- End Optimization ---

    // Line 22: return (root, rank_val)
    // Return the root info found from the recursive call.
    return root_info;
}


// Public find wrapper: Calls internal find and returns only the root index.
int UnionFindParallelLockFreePlainWrite::find(int a) {
     if (a < 0 || a >= n_elements) {
        throw std::out_of_range("Element index out of range in find().");
    }
    // Call the internal recursive find and extract the root index.
    // The internal call handles path compression (using optimized writes).
    return find_internal(a).first;
}


// Unites the sets containing elements 'a' and 'b' (lock-free)
// Aligned with pseudocode lines 1-15
// Uses CAS for critical updates (linking roots, incrementing rank).
bool UnionFindParallelLockFreePlainWrite::unionSets(int a, int b) {
    if (a < 0 || a >= n_elements || b < 0 || b >= n_elements) {
        throw std::out_of_range("Element index out of range in unionSets().");
    }

    // Line 1: while (true) { ... }
    while (true) {
        // Lines 2-3: Find roots and their values (which encode ranks)
        std::pair<int, int> info_a = find_internal(a);
        int root_a_idx = info_a.first;

        std::pair<int, int> info_b = find_internal(b);
        int root_b_idx = info_b.first;

        // Reload the values directly from the atomic array AT THE ROOTS FOUND.
        // Acquire semantics are crucial here to synchronize with other union/find ops.
        int current_root_a_val = A[root_a_idx].load(std::memory_order_acquire);
        int current_root_b_val = A[root_b_idx].load(std::memory_order_acquire);

        // Check if the nodes we identified as roots are *still* roots based on the reloaded values.
        if (!is_root(current_root_a_val)) {
            continue; // State changed, retry find/union
        }
         if (!is_root(current_root_b_val)) {
            continue; // State changed, retry find/union
        }

        // Line 4: if u == v : return (where u, v are roots)
        if (root_a_idx == root_b_idx) {
            return false; // Already in the same set
        }

        // Get ranks from the reloaded, confirmed root values
        int rank_a = get_rank(current_root_a_val);
        int rank_b = get_rank(current_root_b_val);

        // Lines 5-13: Compare ranks and attempt CAS for linking and rank updates.
        if (rank_a < rank_b) {
            // Line 6: Attempt to link root_a to root_b
            if (A[root_a_idx].compare_exchange_weak(current_root_a_val, root_b_idx,
                                                    std::memory_order_release, std::memory_order_relaxed)) {
                return true; // Union successful
            }
        } else if (rank_a > rank_b) {
            // Line 8: Attempt to link root_b to root_a
            if (A[root_b_idx].compare_exchange_weak(current_root_b_val, root_a_idx,
                                                    std::memory_order_release, std::memory_order_relaxed)) {
                return true; // Union successful
            }
        } else { // Lines 9-13: Ranks are equal (ru == rv)
            // Use root index as tie-breaker (smaller index becomes parent)
            if (root_a_idx < root_b_idx) {
                // Line 10: Attempt to link root_a to root_b
                if (A[root_a_idx].compare_exchange_weak(current_root_a_val, root_b_idx,
                                                        std::memory_order_release, std::memory_order_relaxed)) {
                    // Line 11: Attempt to increment rank of root_b (new parent)
                    int new_rank_b_val = make_root_val(rank_b + 1);
                    A[root_b_idx].compare_exchange_weak(current_root_b_val, new_rank_b_val,
                                                        std::memory_order_release, std::memory_order_relaxed);
                    return true;
                }
            } else { // root_b_idx < root_a_idx
                 // Line 12: Attempt to link root_b to root_a
                if (A[root_b_idx].compare_exchange_weak(current_root_b_val, root_a_idx,
                                                        std::memory_order_release, std::memory_order_relaxed)) {
                    // Line 13: Attempt to increment rank of root_a (new parent)
                    int new_rank_a_val = make_root_val(rank_a + 1);
                    A[root_a_idx].compare_exchange_weak(current_root_a_val, new_rank_a_val,
                                                        std::memory_order_release, std::memory_order_relaxed);
                    return true;
                }
            }
        }
        // If any linking CAS failed, the while(true) loop ensures we retry the entire operation.
    }
}

// Checks if elements 'a' and 'b' are in the same set (lock-free)
// Aligned with pseudocode lines 25-30
bool UnionFindParallelLockFreePlainWrite::sameSet(int a, int b) {
     if (a < 0 || a >= n_elements || b < 0 || b >= n_elements) {
        throw std::out_of_range("Element index out of range in sameSet().");
    }

    // Line 25: while (true) { ... }
    while (true) {
        // Line 26: (u, _) = Find(u) ; (v, _) = Find(v) (u,v are roots here)
        int root_a_idx = find_internal(a).first; // Get root via internal find
        int root_b_idx = find_internal(b).first; // Get root via internal find

        // Line 27: if u == v : return true
        if (root_a_idx == root_b_idx) {
            return true; // Definitely in the same set
        }

        // Line 28: if isRank(A[u]) : // still a root? (u is root_a_idx)
        // Check if the first root found is *still* a root. Acquire necessary.
        int current_val_at_root_a = A[root_a_idx].load(std::memory_order_acquire);
        if (is_root(current_val_at_root_a)) {
            // Line 29: return false
            // If roots were different, and root_a is confirmed to still be a root,
            // they were not in the same set *at this moment*.
            return false;
        }

        // Line 28 leads to retry if !isRank(A[u])
        // If root_a is no longer a root, retry the whole operation.
        continue;
    }
}


// --- Parallel Processing ---
// Processes operations in parallel using OpenMP.
void UnionFindParallelLockFreePlainWrite::processOperations(const std::vector<Operation>& ops, std::vector<int>& results) {
    size_t num_ops = ops.size();
    results.resize(num_ops); // Ensure results vector has the correct size

    #pragma omp parallel for schedule(dynamic) // Dynamic schedule often good for variable op times
    for (size_t i = 0; i < num_ops; ++i) {
        const auto& op = ops[i];
        try {
            if (op.type == OperationType::FIND_OP) {
                results[i] = find(op.a);
            } else if (op.type == OperationType::UNION_OP) {
                bool success = unionSets(op.a, op.b);
                results[i] = success ? 1 : 0;
            } else if (op.type == OperationType::SAMESET_OP) {
                 bool same = sameSet(op.a, op.b);
                 results[i] = same ? 1 : 0;
            }
        } catch (const std::out_of_range& e) {
            // Use omp critical for thread-safe error output
            #pragma omp critical
            {
                 std::cerr << "Error processing operation " << i << " [" << static_cast<int>(op.type) << "(" << op.a << "," << op.b << ")]: " << e.what() << std::endl;
            }
            results[i] = -1; // Indicate error
        } catch (const std::exception& e) {
             #pragma omp critical
            {
                 std::cerr << "Generic error processing operation " << i << " [" << static_cast<int>(op.type) << "(" << op.a << "," << op.b << ")]: " << e.what() << std::endl;
            }
             results[i] = -2; // Indicate generic error
        }
    }
}

// --- Utility ---

int UnionFindParallelLockFreePlainWrite::size() const {
    return n_elements;
}
