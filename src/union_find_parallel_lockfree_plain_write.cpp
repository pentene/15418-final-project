#include "union_find_parallel_lockfree_plain_write.hpp"
#include <omp.h>        
#include <iostream>     
#include <vector>      
#include <atomic>       
#include <utility>     
#include <stdexcept>    

// Note: Helper functions (is_root, get_rank, make_root_val) are defined
// as static inline members within the class declaration in the header.

// --- Constructor ---
UnionFindParallelLockFreePlainWrite::UnionFindParallelLockFreePlainWrite(int n)
    : n_elements(n),
      A(n) 
{
    if (n < 0) {
        throw std::invalid_argument("Number of elements cannot be negative.");
    }
    for (int i = 0; i < n; ++i) {
        // Initialize each element as a root with rank 0. Store rank 0 as value -1.
        A[i].store(make_root_val(0), std::memory_order_relaxed);
    }
}

// --- Core Lock-Free Operations (Aligned with Pseudocode + Full Plain Write Optimization) ---
std::pair<int, int> UnionFindParallelLockFreePlainWrite::find_internal(int u) 
{
    int p_val = A[u].load(std::memory_order_relaxed); // Use relaxed read for parent lookup

    if (is_root(p_val)) 
    {
        return {u, p_val}; 
    }
    int p_idx = p_val; 
    std::pair<int, int> root_info = find_internal(p_idx);
    int root_idx = root_info.first;

    // --- Optimization: Path Compaction via Plain Writes ---
    if (p_idx != root_idx) 
    {
        A[u].store(root_idx, std::memory_order_relaxed);
    }

    return root_info;
}

int UnionFindParallelLockFreePlainWrite::find(int a) 
{
    if (a < 0 || a >= n_elements) 
    {
        throw std::out_of_range("Element index out of range in find().");
    }

    return find_internal(a).first;
}

bool UnionFindParallelLockFreePlainWrite::unionSets(int a, int b) 
{
    if (a < 0 || a >= n_elements || b < 0 || b >= n_elements) 
    {
        throw std::out_of_range("Element index out of range in unionSets().");
    }

    while (true) {
        std::pair<int, int> info_a = find_internal(a);
        int root_a_idx = info_a.first;

        std::pair<int, int> info_b = find_internal(b);
        int root_b_idx = info_b.first;

        int current_root_a_val = A[root_a_idx].load(std::memory_order_acquire);
        int current_root_b_val = A[root_b_idx].load(std::memory_order_acquire);

        if (!is_root(current_root_a_val)) 
        {
            continue; // State changed, retry find/union
        }
        if (!is_root(current_root_b_val)) 
        {
            continue; // State changed, retry find/union
        }

        if (root_a_idx == root_b_idx) 
        {
            return false; // Already in the same set
        }

        int rank_a = get_rank(current_root_a_val);
        int rank_b = get_rank(current_root_b_val);

        if (rank_a < rank_b) 
        {
            if (A[root_a_idx].compare_exchange_weak(current_root_a_val, root_b_idx,
                                                    std::memory_order_release, std::memory_order_relaxed)) 
            {
                return true; 
            }
        } 
        else if (rank_a > rank_b) 
        {
            if (A[root_b_idx].compare_exchange_weak(current_root_b_val, root_a_idx,
                                                    std::memory_order_release, std::memory_order_relaxed)) 
            {
                return true; 
            }
        } 
        else 
        {
            if (root_a_idx < root_b_idx) 
            {
                if (A[root_a_idx].compare_exchange_weak(current_root_a_val, root_b_idx,
                                                        std::memory_order_release, std::memory_order_relaxed)) 
                {
                    int new_rank_b_val = make_root_val(rank_b + 1);
                    A[root_b_idx].compare_exchange_weak(current_root_b_val, new_rank_b_val,
                                                        std::memory_order_release, std::memory_order_relaxed);
                    return true;
                }
            }
            else 
            {
                if (A[root_b_idx].compare_exchange_weak(current_root_b_val, root_a_idx,
                                                    std::memory_order_release, std::memory_order_relaxed)) 
                {
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

bool UnionFindParallelLockFreePlainWrite::sameSet(int a, int b) 
{
    if (a < 0 || a >= n_elements || b < 0 || b >= n_elements) 
    {
        throw std::out_of_range("Element index out of range in sameSet().");
    }

    while (true) 
    {
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

void UnionFindParallelLockFreePlainWrite::processOperations(const std::vector<Operation>& ops, std::vector<int>& results) {
    size_t num_ops = ops.size();
    results.resize(num_ops); 

    #pragma omp parallel for schedule(static) 
    for (size_t i = 0; i < num_ops; ++i) {
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
                std::cerr << "Error processing operation " << i << " [" << static_cast<int>(op.type) << "(" << op.a << "," << op.b << ")]: " << e.what() << std::endl;
            }
            results[i] = -1; // Indicate error
        } 
        catch (const std::exception& e) 
        {
            #pragma omp critical
            {
                std::cerr << "Generic error processing operation " << i << " [" << static_cast<int>(op.type) << "(" << op.a << "," << op.b << ")]: " << e.what() << std::endl;
            }
            results[i] = -2; // Indicate generic error
        }
    }
}

int UnionFindParallelLockFreePlainWrite::size() const {
    return n_elements;
}
