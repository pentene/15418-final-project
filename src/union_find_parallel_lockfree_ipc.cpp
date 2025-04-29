#include "union_find_parallel_lockfree_ipc.hpp"
#include <omp.h>       
#include <iostream>     
#include <vector>       
#include <atomic>       
#include <utility>      
#include <stdexcept>    
#include <cstdint>      

// Note: Helper functions (is_root, get_rank, make_root_val) are defined
// as static inline members within the class declaration in the header.

// --- Constructor ---
UnionFindParallelLockFreeIPC::UnionFindParallelLockFreeIPC(int n)
    : n_elements(n),
      A(n) // Default-construct n atomic<int> elements
{
    if (n < 0) 
    {
        throw std::invalid_argument("Number of elements cannot be negative.");
    }
    for (int i = 0; i < n; i++) 
    {
        // Initialize each element as a root with rank 0. Store rank 0 as value -1.
        A[i].store(make_root_val(0), std::memory_order_relaxed);
    }
}

// --- Core Lock-Free Operations (Based on original lockfree + IPC) ---

// Internal find operation matching the original lockfree structure.
// Returns {root_index, root_value} where root_value encodes rank if it's a root.
std::pair<int, int> UnionFindParallelLockFreeIPC::find_internal(int u) 
{
    int p_val = A[u].load(std::memory_order_acquire);

    if (is_root(p_val)) 
    {
        return {u, p_val};
    }

    int p_idx = p_val; 
    std::pair<int, int> root_info = find_internal(p_idx);
    int root_idx = root_info.first;

    if (p_idx != root_idx) 
    {
        A[u].compare_exchange_weak(p_val, root_idx,
                                   std::memory_order_release, // Make write visible if successful
                                   std::memory_order_relaxed); // Relaxed on failure is fine
    }

    return root_info;
}


int UnionFindParallelLockFreeIPC::find(int a) 
{
    if (a < 0 || a >= n_elements) 
    {
        throw std::out_of_range("Element index out of range in find().");
    }
    return find_internal(a).first;
}

bool UnionFindParallelLockFreeIPC::unionSets(int a, int b) 
{
    if (a < 0 || a >= n_elements || b < 0 || b >= n_elements)
     {
        throw std::out_of_range("Element index out of range in unionSets().");
    }

    while (true) 
    {
        // --- Immediate Parent Check (IPC) Optimization ---
        int parent_a_ipc = A[a].load(std::memory_order_relaxed);
        int parent_b_ipc = A[b].load(std::memory_order_relaxed);

        // Check if parents are the same and not roots (i.e., they are indices)
        // Also handle the case where one/both might be roots already.
        // If both point to the same non-root node, they are likely in the same set.
        if (!is_root(parent_a_ipc) && parent_a_ipc == parent_b_ipc) {
            // If immediate parents are the same index, they are likely in the same set.
            // The full find/traversal below would confirm, but IPC provides a fast path.
            return false;
        }
        // --- End IPC Optimization ---

        std::pair<int, int> info_a = find_internal(a);
        int root_a_idx = info_a.first;

        std::pair<int, int> info_b = find_internal(b);
        int root_b_idx = info_b.first;

        int current_root_a_val = A[root_a_idx].load(std::memory_order_acquire);
        int current_root_b_val = A[root_b_idx].load(std::memory_order_acquire);

        if (!is_root(current_root_a_val)) 
        {
            continue;
        }
        if (!is_root(current_root_b_val)) 
        {
            continue;
        }

        if (root_a_idx == root_b_idx) 
        {
            return false; // Already in the same set
        }

        int rank_a = get_rank(current_root_a_val);
        int rank_b = get_rank(current_root_b_val);
        int child_root_idx, parent_root_idx;
        int child_val_expected, parent_val_expected;
        int parent_rank_current;

        if (rank_a < rank_b) 
        {
            child_root_idx = root_a_idx; 
            child_val_expected = current_root_a_val;
            parent_root_idx = root_b_idx; 
            parent_val_expected = current_root_b_val;
            parent_rank_current = rank_b;
        } 
        else if (rank_a > rank_b) 
        {
            child_root_idx = root_b_idx; 
            child_val_expected = current_root_b_val;
            parent_root_idx = root_a_idx; 
            parent_val_expected = current_root_a_val;
            parent_rank_current = rank_a;
        } 
        else 
        { 
            if (root_a_idx < root_b_idx) 
            { 
                child_root_idx = root_b_idx; 
                child_val_expected = current_root_b_val;
                parent_root_idx = root_a_idx; 
                parent_val_expected = current_root_a_val;
                parent_rank_current = rank_a;
            } 
            else 
            {
                child_root_idx = root_a_idx; 
                child_val_expected = current_root_a_val;
                parent_root_idx = root_b_idx; 
                parent_val_expected = current_root_b_val;
                parent_rank_current = rank_b;
            }
        }

        // Attempt to link the child root to the parent root index
        if (A[child_root_idx].compare_exchange_weak(child_val_expected, parent_root_idx,
                                                    std::memory_order_release, std::memory_order_relaxed))
        {
            // Successfully linked child to parent.
            // If ranks were equal, attempt to increment the parent's rank.
            if (rank_a == rank_b) 
            {
                int new_parent_rank_val = make_root_val(parent_rank_current + 1);
                A[parent_root_idx].compare_exchange_weak(parent_val_expected, new_parent_rank_val,
                                                         std::memory_order_release, std::memory_order_relaxed);
            }
            return true; // Union successful
        }
        // If CAS failed, loop and retry the entire operation.
    } 
}

// Checks if elements 'a' and 'b' are in the same set (lock-free)
// Includes Immediate Parent Check (IPC) optimization.
bool UnionFindParallelLockFreeIPC::sameSet(int a, int b) 
{
    if (a < 0 || a >= n_elements || b < 0 || b >= n_elements) 
    {
        throw std::out_of_range("Element index out of range in sameSet().");
    }

    while (true) 
    {
        // --- Immediate Parent Check (IPC) Optimization ---
        int parent_a_ipc = A[a].load(std::memory_order_relaxed);
        int parent_b_ipc = A[b].load(std::memory_order_relaxed);

        if (a == b) return true; // Handle trivial case first
        if (!is_root(parent_a_ipc) && parent_a_ipc == parent_b_ipc) 
        {
            return true;
        }
        // --- End IPC Optimization ---

        int root_a_idx = find_internal(a).first; 
        int root_b_idx = find_internal(b).first;

        if (root_a_idx == root_b_idx) 
        {
            return true;
        }

        int current_val_at_root_a = A[root_a_idx].load(std::memory_order_acquire);
        if (is_root(current_val_at_root_a)) 
        {
            return false;
        }
        continue;
    } 
}

void UnionFindParallelLockFreeIPC::processOperations(const std::vector<Operation>& ops, std::vector<int>& results) 
{
    size_t num_ops = ops.size();
    results.resize(num_ops);

    #pragma omp parallel for schedule(static)
    for (size_t i = 0; i < num_ops; i++) 
    {
        const auto& op = ops[i];
        try 
        {
            if (op.type == OperationType::FIND_OP) 
            {
                results[i] = find(op.a);
            } 
            else if (op.type == OperationType::UNION_OP) 
            {
                bool success = unionSets(op.a, op.b);
                results[i] = success ? 1 : 0;
            } 
            else if (op.type == OperationType::SAMESET_OP) 
            {
                bool same = sameSet(op.a, op.b);
                results[i] = same ? 1 : 0;
            }
        } 
        catch (const std::out_of_range& e) 
        {
            #pragma omp critical
            {
                std::cerr << "Error processing operation " << i << ": " << e.what() << std::endl;
            }
            results[i] = -1; // Indicate error
        } 
        catch (const std::exception& e) 
        {
            #pragma omp critical
            {
                std::cerr << "Generic error processing operation " << i << ": " << e.what() << std::endl;
            }
            results[i] = -2; // Indicate generic error
        }
    }
}

int UnionFindParallelLockFreeIPC::size() const 
{
    return n_elements;
}
