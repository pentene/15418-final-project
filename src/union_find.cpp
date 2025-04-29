#include "union_find.hpp" 
#include <vector>
#include <cassert>
#include <numeric>

// Constructor
UnionFind::UnionFind(int n)
    : parent(n), rank(n, 0), num_elements(n) 
{
    assert(n >= 0 && "Number of elements cannot be negative.");
    std::iota(parent.begin(), parent.end(), 0);
}

int UnionFind::find(int a) 
{
    assert(a >= 0 && a < num_elements && "Element index out of bounds in find().");

    if (parent[a] != a) 
    {
        parent[a] = find(parent[a]);
    }
    return parent[a];
}

bool UnionFind::unionSets(int a, int b) 
{
    assert(a >= 0 && a < num_elements && "Element index 'a' out of bounds in unionSets().");
    assert(b >= 0 && b < num_elements && "Element index 'b' out of bounds in unionSets().");

    int rootA = find(a);
    int rootB = find(b);

    if (rootA == rootB) 
    {
        return false; 
    }

    if (rank[rootA] < rank[rootB]) 
    {
        parent[rootA] = rootB; 
    } 
    else if (rank[rootA] > rank[rootB]) 
    {
        parent[rootB] = rootA; 
    } 
    else 
    {
        parent[rootB] = rootA;
        ++rank[rootA];
    }
    return true;
}

// Check if two elements are in the same set
bool UnionFind::sameSet(int a, int b) 
{
    assert(a >= 0 && a < num_elements && "Element index 'a' out of bounds in sameSet().");
    assert(b >= 0 && b < num_elements && "Element index 'b' out of bounds in sameSet().");
    return find(a) == find(b);
}

void UnionFind::processOperations(const std::vector<Operation>& ops, std::vector<int>& results) 
{
    size_t nOps = ops.size();
    results.resize(nOps); 

    for (size_t i = 0; i < nOps; i++) 
    {
        const auto& op = ops[i]; 
        assert(op.a >= 0 && op.a < num_elements && "Operation element 'a' out of bounds.");

        switch (op.type) 
        {
            case OperationType::UNION_OP: 
            {
                assert(op.b >= 0 && op.b < num_elements && "Operation element 'b' out of bounds for UNION_OP.");
                bool union_occurred = unionSets(op.a, op.b);
                results[i] = union_occurred ? 1 : 0; 
                
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
                bool are_same = sameSet(op.a, op.b);
                results[i] = are_same ? 1 : 0; 
                break;
            }
            default:
                assert(false && "Unknown operation type encountered.");
                results[i] = -2; // Indicate an error or unexpected state
                break;
        }
    }
}

int UnionFind::size() const 
{
    return num_elements;
}
