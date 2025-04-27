#include "union_find_parallel_lockfree.hpp"
#include <omp.h> // For OpenMP directives
#include <stdexcept> // For range checks
#include <iostream> // For potential debugging
#include <utility> // For std::pair

// --- Constructor ---

UnionFindParallelLockFree::UnionFindParallelLockFree(int n)
    : n_elements(n),
      A(n) // Use fill constructor to default-construct n atomic<int> elements
{
    if (n < 0) {
        // Use std::invalid_argument for constructor parameter issues
        throw std::invalid_argument("Number of elements cannot be negative.");
    }
    // Initialize each default-constructed atomic individually.
    for (int i = 0; i < n; ++i) {
        // Initialize each element as a root with rank 0. Store rank 0 as value -1.
        // Relaxed memory order is sufficient for initialization before parallel access.
        A[i].store(make_root_val(0), std::memory_order_relaxed);
    }
}

// --- Core Lock-Free Operations (Aligned with Pseudocode) ---

// Internal find operation matching pseudocode structure (lines 16-23)
// Returns {root_index, root_value} where root_value encodes rank if it's a root.
// Performs path compression during recursion unwind.
std::pair<int, int> UnionFindParallelLockFree::find_internal(int u) {
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
    // int root_val = root_info.second; // We have the root's value in root_info.second

    // Lines 20-21: Path Compression: if p != root: CAS(&A[u], p, root)
    // Attempt to update A[u] from p_idx to root_idx if they differ.
    if (p_idx != root_idx) {
        // Read A[u] again just before CAS to use the most recent value in the comparison.
        // Use the value we originally read (p_val which equals p_idx here) as the expected value.
        A[u].compare_exchange_weak(p_val, root_idx,
                                    std::memory_order_release, // Make write visible if successful
                                    std::memory_order_relaxed); // Relaxed on failure is fine
        // We don't retry or loop here; if CAS fails, it means A[u] changed concurrently.
        // The recursive structure ensures we still return the correct root found deeper.
        // Subsequent finds involving 'u' will benefit if the compression succeeded.
    }

    // Line 22: return (root, rank_val)
    // Return the root info found from the recursive call.
    return root_info;
}


// Public find wrapper: Calls internal find and returns only the root index.
int UnionFindParallelLockFree::find(int a) {
     if (a < 0 || a >= n_elements) {
        throw std::out_of_range("Element index out of range in find().");
    }
    // Call the internal recursive find and extract the root index.
    // The internal call handles path compression.
    return find_internal(a).first;
}


// Unites the sets containing elements 'a' and 'b' (lock-free)
// Aligned with pseudocode lines 1-15
bool UnionFindParallelLockFree::unionSets(int a, int b) {
    if (a < 0 || a >= n_elements || b < 0 || b >= n_elements) {
        throw std::out_of_range("Element index out of range in unionSets().");
    }

    // Line 1: while (true) { ... }
    while (true) {
        // Lines 2-3: Find roots and their values (which encode ranks)
        std::pair<int, int> info_a = find_internal(a);
        int root_a_idx = info_a.first;
        int root_a_val = info_a.second; // This is the value at the root (encodes rank)

        std::pair<int, int> info_b = find_internal(b);
        int root_b_idx = info_b.first;
        int root_b_val = info_b.second; // This is the value at the root (encodes rank)

        // It's possible find_internal returned a node that is no longer a root
        // due to concurrent unions. Reload the values directly from the atomic array
        // to get the most current state *at the potential roots* before proceeding.
        root_a_val = A[root_a_idx].load(std::memory_order_acquire);
        root_b_val = A[root_b_idx].load(std::memory_order_acquire);

        // Check if the nodes we identified as roots are *still* roots.
        // If not, retry the whole operation.
        if (!is_root(root_a_val)) {
            continue; // State changed, retry find
        }
         if (!is_root(root_b_val)) {
            continue; // State changed, retry find
        }

        // Line 4: if u == v : return (where u, v are roots in pseudocode)
        if (root_a_idx == root_b_idx) {
            return false; // Already in the same set
        }

        // Get ranks from the root values
        int rank_a = get_rank(root_a_val);
        int rank_b = get_rank(root_b_val);

        // Lines 5-13: Compare ranks and attempt CAS
        if (rank_a < rank_b) {
            // Line 6: if CAS(&A[u], u_val, v) : return (u is root_a, v is root_b)
            // Attempt to link root_a to root_b
            if (A[root_a_idx].compare_exchange_weak(root_a_val, root_b_idx,
                                                    std::memory_order_release, std::memory_order_relaxed)) {
                return true; // Union successful
            }
            // If CAS fails, loop again (state changed)

        } else if (rank_a > rank_b) {
            // Line 8: if CAS(&A[v], v_val, u) : return (v is root_b, u is root_a)
            // Attempt to link root_b to root_a
            if (A[root_b_idx].compare_exchange_weak(root_b_val, root_a_idx,
                                                    std::memory_order_release, std::memory_order_relaxed)) {
                return true; // Union successful
            }
            // If CAS fails, loop again (state changed)

        } else { // Lines 9-13: Ranks are equal (ru == rv)
            // Use root index as tie-breaker (smaller index becomes parent)
            if (root_a_idx < root_b_idx) {
                // Line 10: if u < v && CAS(&A[u], u_val, v) : ... (u=root_a, v=root_b)
                // Attempt to link root_a to root_b
                 if (A[root_a_idx].compare_exchange_weak(root_a_val, root_b_idx,
                                                        std::memory_order_release, std::memory_order_relaxed)) {
                    // Line 11: CAS(&A[v], rv_val, rv_val + 1) ; return (v=root_b)
                    // Attempt to increment rank of root_b (the new parent)
                    int new_rank_b_val = make_root_val(rank_b + 1);
                    A[root_b_idx].compare_exchange_weak(root_b_val, new_rank_b_val,
                                                        std::memory_order_release, std::memory_order_relaxed);
                    // Return true even if rank update fails, link succeeded.
                    return true;
                 }
                 // If CAS fails, loop again (state changed)

            } else { // root_b_idx < root_a_idx
                 // Line 12: if u > v && CAS(&A[v], v_val, u) : ... (u=root_a, v=root_b)
                 // Attempt to link root_b to root_a
                 if (A[root_b_idx].compare_exchange_weak(root_b_val, root_a_idx,
                                                        std::memory_order_release, std::memory_order_relaxed)) {
                    // Line 13: CAS(&A[u], ru_val, ru_val + 1) ; return (u=root_a)
                    // Attempt to increment rank of root_a (the new parent)
                    int new_rank_a_val = make_root_val(rank_a + 1);
                    A[root_a_idx].compare_exchange_weak(root_a_val, new_rank_a_val,
                                                        std::memory_order_release, std::memory_order_relaxed);
                    // Return true even if rank update fails, link succeeded.
                    return true;
                 }
                 // If CAS fails, loop again (state changed)
            }
        }
        // If any CAS failed, the while(true) loop ensures we retry the entire operation.
    }
}

// Checks if elements 'a' and 'b' are in the same set (lock-free)
// Aligned with pseudocode lines 25-30
bool UnionFindParallelLockFree::sameSet(int a, int b) {
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
        // Check if the first root found is *still* a root. If not, the structure
        // might have changed concurrently making the comparison potentially invalid.
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
// This function remains the same as it relies on the public API,
// which now uses the pseudocode-aligned internal logic.
void UnionFindParallelLockFree::processOperations(const std::vector<Operation>& ops, std::vector<int>& results) {
    size_t num_ops = ops.size();
    results.resize(num_ops); // Ensure results vector has the correct size

    #pragma omp parallel for schedule(static)
    for (size_t i = 0; i < num_ops; ++i) {
        const auto& op = ops[i];
        try {
            if (op.type == OperationType::FIND_OP) {
                // Public find still returns just the root index
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
             results[i] = -2; // Indicate error
        }
    }
}

// --- Utility ---

int UnionFindParallelLockFree::size() const {
    return n_elements;
}
