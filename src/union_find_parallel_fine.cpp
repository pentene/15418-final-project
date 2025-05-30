#include "union_find_parallel_fine.hpp"
#include <omp.h>
#include <vector>
#include <mutex>
#include <numeric>   
#include <cassert>
#include <algorithm> 
#include <stdexcept> 
// Constructor
UnionFindParallelFine::UnionFindParallelFine(int n)
    : parent(n), rank(n, 0), num_elements(n), locks(n) 
{ 
    assert(n >= 0 && "Number of elements cannot be negative.");
    std::iota(parent.begin(), parent.end(), 0);
}

// Find operation with best-effort path compression (no locks during traversal/compression)
// Because it is hard to implement fine lock the entire path in a parallel environment, this is a best-effort find.
int UnionFindParallelFine::find(int a) 
{
    assert(a >= 0 && a < num_elements && "Element index out of bounds in find().");

    // 1. Find the root (potentially racing with writes)
    int root = a;
    while (parent[root] != root) 
    {
        int next_parent = parent[root];
        root = next_parent;
        assert(root >= 0 && root < num_elements && "Invalid parent index encountered during find.");
    }

    // 2. Path compression (potentially racy writes)
    int current = a;
    while (current != root) 
    {
        int next = parent[current];
        // Racy write: Another thread might be updating parent[current] concurrently.
        parent[current] = root;
        current = next;
    }
    return root;
}

int UnionFindParallelFine::find_root_no_compression(int a) const 
{
    assert(a >= 0 && a < num_elements && "Element index out of bounds in find_root_no_compression().");
    int current = a;
    while (parent[current] != current) 
    {
        current = parent[current];
        assert(current >= 0 && current < num_elements && "Invalid parent index encountered during find_root_no_compression.");
    }
    return current;
}

// Union operation with fine-grained locking and verification
bool UnionFindParallelFine::unionSets(int a, int b) 
{
    assert(a >= 0 && a < num_elements && "Element index 'a' out of bounds in unionSets().");
    assert(b >= 0 && b < num_elements && "Element index 'b' out of bounds in unionSets().");

    while (true) 
    { 
        int rootA = find(a);
        int rootB = find(b);

        if (rootA == rootB) 
        {
            return false; 
        }

        int lock1_idx = std::min(rootA, rootB);
        int lock2_idx = std::max(rootA, rootB);

        std::lock_guard<std::mutex> guard1(locks[lock1_idx]);
        std::lock_guard<std::mutex> guard2(locks[lock2_idx]);

        // *** Critical Section Start ***
        int currentRootA = find_root_no_compression(a);
        int currentRootB = find_root_no_compression(b);

        // If the roots we locked are no longer the *actual* roots of a and b,
        // or if a and b are now in the same set, we must retry.
        if (currentRootA != rootA || currentRootB != rootB || currentRootA == currentRootB) 
        {
            // The structure changed significantly while we were trying to lock,
            // or they got unified concurrently. Release locks (automatically by guards)
            // and retry the outer loop.
            continue;
        }

        // Proceed with the union using the verified roots (rootA, rootB)
        // which are guaranteed to be the correct roots at this point and are different.
        assert(parent[rootA] == rootA && parent[rootB] == rootB); // Assert they are indeed roots

        if (rank[rootA] < rank[rootB]) 
        {
            parent[rootA] = rootB;
        } else if (rank[rootA] > rank[rootB]) 
        {
            parent[rootB] = rootA;
        } else 
        {
            parent[rootB] = rootA;
            rank[rootA]++;
        }
        // *** Critical Section End ***

        return true;
    } 
}

bool UnionFindParallelFine::sameSet(int a, int b) 
{
    // Precondition: Element indices 'a' and 'b' must be within bounds.
    assert(a >= 0 && a < num_elements && "Element index 'a' out of bounds in sameSet().");
    assert(b >= 0 && b < num_elements && "Element index 'b' out of bounds in sameSet().");

    return find(a) == find(b);
}


void UnionFindParallelFine::processOperations(const std::vector<Operation>& ops, std::vector<int>& results) 
{
    size_t nOps = ops.size();
    results.resize(nOps); 

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < static_cast<int>(nOps); ++i) {
        const auto& op = ops[i];

        // Precondition check for element 'a' (common to all ops)
        assert(op.a >= 0 && op.a < num_elements && "Operation element 'a' out of bounds.");

        switch (op.type) 
        {
            case OperationType::UNION_OP: 
            {
                assert(op.b >= 0 && op.b < num_elements && "Operation element 'b' out of bounds for UNION_OP.");
                bool union_occurred = unionSets(op.a, op.b);
                results[i] = union_occurred ? 1 : 0; // Store 1 if union happened, 0 otherwise.
                break;
            }
            case OperationType::FIND_OP: 
            {
                results[i] = find(op.a); // Store the result of the find operation (root).
                break;
            }
            case OperationType::SAMESET_OP: 
            {
                assert(op.b >= 0 && op.b < num_elements && "Operation element 'b' out of bounds for SAMESET_OP.");
                // Calls the fine-grained sameSet
                bool are_same = sameSet(op.a, op.b);
                results[i] = are_same ? 1 : 0; // Store 1 if they are in the same set, 0 otherwise.
                break;
            }
            default:
                assert(false && "Unknown operation type encountered.");
                results[i] = -2; // Indicate an error or unexpected state
                break;
        }
    }
}

int UnionFindParallelFine::size() const 
{
    return num_elements;
}
