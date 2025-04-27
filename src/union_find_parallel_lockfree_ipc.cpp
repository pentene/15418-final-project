#include "union_find_parallel_lockfree_ipc.hpp" // Include the new header
#include <omp.h>        // For OpenMP directives
#include <iostream>     // For std::cerr
#include <vector>       // Included via header
#include <atomic>       // Included via header
#include <utility>      // Included via header
#include <stdexcept>    // Included via header
#include <cstdint>      // Included via header

// Note: Helper functions (is_root, get_rank, make_root_val) are defined
// as static inline members within the class declaration in the header.

// --- Constructor ---

UnionFindParallelLockFreeIPC::UnionFindParallelLockFreeIPC(int n)
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

// --- Core Lock-Free Operations (Based on original lockfree + IPC) ---

// Internal find operation matching the original lockfree structure.
// Returns {root_index, root_value} where root_value encodes rank if it's a root.
// Performs path compression using CAS during recursion unwind.
std::pair<int, int> UnionFindParallelLockFreeIPC::find_internal(int u) {
    // Line 17: p = A[u]
    int p_val = A[u].load(std::memory_order_acquire); // Read current parent index or root value

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
    // int root_val = root_info.second; // Root's value available in root_info

    // Lines 20-21: Path Compression: if p != root: CAS(&A[u], p, root)
    // Attempt to update A[u] from p_idx to root_idx if they differ.
    if (p_idx != root_idx) {
        // Use the value we originally read (p_val which equals p_idx here) as the expected value.
        A[u].compare_exchange_weak(p_val, root_idx,
                                   std::memory_order_release, // Make write visible if successful
                                   std::memory_order_relaxed); // Relaxed on failure is fine
    }

    // Line 22: return (root, rank_val)
    // Return the root info found from the recursive call.
    return root_info;
}


// Public find wrapper: Calls internal find and returns only the root index.
int UnionFindParallelLockFreeIPC::find(int a) {
     if (a < 0 || a >= n_elements) {
        throw std::out_of_range("Element index out of range in find().");
    }
    // Call the internal recursive find and extract the root index.
    // The internal call handles path compression using CAS.
    return find_internal(a).first;
}


// Unites the sets containing elements 'a' and 'b' (lock-free)
// Includes Immediate Parent Check (IPC) optimization.
bool UnionFindParallelLockFreeIPC::unionSets(int a, int b) {
    if (a < 0 || a >= n_elements || b < 0 || b >= n_elements) {
        throw std::out_of_range("Element index out of range in unionSets().");
    }

    while (true) { // Retry loop for the entire operation

        // --- Immediate Parent Check (IPC) Optimization ---
        int parent_a_ipc = A[a].load(std::memory_order_relaxed);
        int parent_b_ipc = A[b].load(std::memory_order_relaxed);

        // Check if parents are the same *and* not roots (i.e., they are indices)
        // Also handle the case where one/both might be roots already.
        // If both point to the same non-root node, they are likely in the same set.
        if (!is_root(parent_a_ipc) && parent_a_ipc == parent_b_ipc) {
            // If immediate parents are the same index, they are likely in the same set.
            // The full find/traversal below would confirm, but IPC provides a fast path.
            // No need to check a != b here, as find() handles that later.
            return false;
        }
        // --- End IPC Optimization ---


        // Lines 2-3: Find roots and their values (which encode ranks)
        std::pair<int, int> info_a = find_internal(a);
        int root_a_idx = info_a.first;
        // int root_a_val = info_a.second; // Value from find might be stale

        std::pair<int, int> info_b = find_internal(b);
        int root_b_idx = info_b.first;
        // int root_b_val = info_b.second; // Value from find might be stale

        // Reload the values directly from the atomic array AT THE ROOTS FOUND.
        int current_root_a_val = A[root_a_idx].load(std::memory_order_acquire);
        int current_root_b_val = A[root_b_idx].load(std::memory_order_acquire);

        // Check if the nodes we identified as roots are *still* roots.
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
        // This logic is from the original lockfree version.
        int child_root_idx, parent_root_idx;
        int child_val_expected, parent_val_expected;
        int parent_rank_current;


        if (rank_a < rank_b) {
            child_root_idx = root_a_idx; child_val_expected = current_root_a_val;
            parent_root_idx = root_b_idx; parent_val_expected = current_root_b_val;
            parent_rank_current = rank_b;
        } else if (rank_a > rank_b) {
            child_root_idx = root_b_idx; child_val_expected = current_root_b_val;
            parent_root_idx = root_a_idx; parent_val_expected = current_root_a_val;
            parent_rank_current = rank_a;
        } else { // Ranks are equal
            if (root_a_idx < root_b_idx) { // Tie-break using index
                 child_root_idx = root_b_idx; child_val_expected = current_root_b_val;
                 parent_root_idx = root_a_idx; parent_val_expected = current_root_a_val;
                 parent_rank_current = rank_a;
            } else {
                 child_root_idx = root_a_idx; child_val_expected = current_root_a_val;
                 parent_root_idx = root_b_idx; parent_val_expected = current_root_b_val;
                 parent_rank_current = rank_b;
            }
        }

        // Attempt to link the child root to the parent root index
        if (A[child_root_idx].compare_exchange_weak(child_val_expected, parent_root_idx,
                                                    std::memory_order_release, std::memory_order_relaxed))
        {
            // Successfully linked child to parent.
            // If ranks were equal, attempt to increment the parent's rank.
            if (rank_a == rank_b) {
                int new_parent_rank_val = make_root_val(parent_rank_current + 1);
                // Attempt to update the parent's rank.
                A[parent_root_idx].compare_exchange_weak(parent_val_expected, new_parent_rank_val,
                                                         std::memory_order_release, std::memory_order_relaxed);
            }
            return true; // Union successful
        }
        // If CAS failed, loop and retry the entire operation.
    } // End while(true)
}

// Checks if elements 'a' and 'b' are in the same set (lock-free)
// Includes Immediate Parent Check (IPC) optimization.
bool UnionFindParallelLockFreeIPC::sameSet(int a, int b) {
    if (a < 0 || a >= n_elements || b < 0 || b >= n_elements) {
        throw std::out_of_range("Element index out of range in sameSet().");
    }

    while (true) { // Retry loop for the entire operation

        // --- Immediate Parent Check (IPC) Optimization ---
        int parent_a_ipc = A[a].load(std::memory_order_relaxed);
        int parent_b_ipc = A[b].load(std::memory_order_relaxed);

        if (a == b) return true; // Handle trivial case first

        // Check if parents are the same *and* not roots (i.e., they are indices)
        if (!is_root(parent_a_ipc) && parent_a_ipc == parent_b_ipc) {
            // If immediate parents point to the same node index, they are likely in the same set.
            return true;
        }
        // --- End IPC Optimization ---


        // Line 26: (u, _) = Find(u) ; (v, _) = Find(v) (u,v are roots here)
        int root_a_idx = find_internal(a).first; // Get root via internal find
        int root_b_idx = find_internal(b).first; // Get root via internal find

        // Line 27: if u == v : return true
        if (root_a_idx == root_b_idx) {
            return true; // Definitely in the same set
        }

        // Line 28: if isRank(A[u]) : // still a root? (u is root_a_idx)
        // Check if the first root found is *still* a root using acquire semantics.
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
    } // End while(true)
}


// --- Parallel Processing ---
// Processes operations in parallel using OpenMP. Uses static scheduling.
void UnionFindParallelLockFreeIPC::processOperations(const std::vector<Operation>& ops, std::vector<int>& results) {
    size_t num_ops = ops.size();
    results.resize(num_ops);

    // Use static scheduling as requested based on user finding
    #pragma omp parallel for schedule(static)
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
            #pragma omp critical
            {
                 std::cerr << "Error processing operation " << i << ": " << e.what() << std::endl;
            }
            results[i] = -1; // Indicate error
        } catch (const std::exception& e) {
             #pragma omp critical
            {
                 std::cerr << "Generic error processing operation " << i << ": " << e.what() << std::endl;
            }
             results[i] = -2; // Indicate generic error
        }
    }
}

// --- Utility ---
int UnionFindParallelLockFreeIPC::size() const {
    return n_elements;
}
