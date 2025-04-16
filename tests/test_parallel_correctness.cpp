#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <omp.h>

// Include the parallel union find header.
#include "union_find_parallel_coarse.hpp"
// #include "union_find_parallel_fine.hpp"
// #include "union_find_parallel_lockfree.hpp"

// Helper function to trim whitespace from both ends.
std::string trim(const std::string &s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end = s.find_last_not_of(" \t\r\n");
    if (start == std::string::npos)
        return "";
    return s.substr(start, end - start + 1);
}

// Structure to store a union operation in the test file.
struct UnionOp {
    int a;
    int b;
};

// Structure for connectivity query.
struct Query {
    int a;
    int b;
    int expected;  // 1 if connected, 0 if not.
};

// Structure to hold the complete test data.
struct ParallelTestData {
    int n;                          // number of elements
    std::vector<UnionOp> unionOps;  // union operations to be executed concurrently
    std::vector<Query> queries;     // connectivity queries (executed after all unions)
};

// Function to load test data from a file.
// Expected file format (example):
//   # first line: n u q
//   10 4 3
//   # union operations:
//   U 0 1
//   U 1 2
//   U 3 4
//   U 6 7
//   # connectivity queries:
//   Q 0 2 1
//   Q 0 3 0
//   Q 6 7 1
ParallelTestData loadTestData(const std::string &filename) {
    std::ifstream infile(filename);
    if (!infile)
        throw std::runtime_error("Error opening test file: " + filename);

    ParallelTestData data;
    std::string line;

    // Read header line: n u q
    while (std::getline(infile, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream header(line);
        int uCount, qCount;
        if (!(header >> data.n >> uCount >> qCount))
            throw std::runtime_error("Invalid header in test file: " + filename);

        // Read union operations.
        for (int i = 0; i < uCount; i++) {
            while (std::getline(infile, line)) {
                line = trim(line);
                if (line.empty() || line[0] == '#')
                    continue;
                std::istringstream opStream(line);
                char op;
                int a, b;
                opStream >> op >> a >> b;
                if (op != 'U')
                    throw std::runtime_error("Expected a union op (U) in file: " + filename);
                data.unionOps.push_back({a, b});
                break;
            }
        }
        // Read connectivity queries.
        for (int i = 0; i < qCount; i++) {
            while (std::getline(infile, line)) {
                line = trim(line);
                if (line.empty() || line[0] == '#')
                    continue;
                std::istringstream qStream(line);
                char q;
                int a, b, expected;
                qStream >> q >> a >> b >> expected;
                if (q != 'Q')
                    throw std::runtime_error("Expected a query op (Q) in file: " + filename);
                data.queries.push_back({a, b, expected});
                break;
            }
        }
        break; // Process only one test case per file.
    }

    return data;
}

// Template helper function to run the test on one file using a given Union-Find type.
// UFType must provide a constructor UFType(int), unionSets(int, int),
// find(int), and processOperations(const std::vector<Operation>&, std::vector<int>&).
template <typename UFType>
bool runTestForFile(const std::string &filename) {
    std::cout << "Processing test file: " << filename << std::endl;
    ParallelTestData data = loadTestData(filename);
    UFType uf(data.n);

    // Convert test file's union operations into the library's Operation type.
    std::vector<typename UFType::Operation> ops;
    for (const auto &u : data.unionOps) {
        // Explicitly construct an object of type UFType::Operation.
        typename UFType::Operation op(UFType::OperationType::UNION_OP, u.a, u.b);
        ops.push_back(op);
    }

    // Process union operations concurrently using the library's processOperations method.
    std::vector<int> results;
    uf.processOperations(ops, results);

    // Now verify connectivity queries sequentially.
    bool passed = true;
    for (size_t i = 0; i < data.queries.size(); i++) {
        Query q = data.queries[i];
        // Check: if q.expected == 1 then uf.find(q.a) should equal uf.find(q.b);
        // otherwise, they should differ.
        bool connected = (uf.find(q.a) == uf.find(q.b));
        std::cout << "Query (" << q.a << ", " << q.b << ") -> got "
                  << (connected ? "connected" : "not connected")
                  << ", expected " << (q.expected == 1 ? "connected" : "not connected") << "\n";
        if (connected != (q.expected == 1)) {
            std::cerr << "Mismatch for query (" << q.a << ", " << q.b << ")\n";
            passed = false;
        }
    }

    if (passed)
        std::cout << "Test file '" << filename << "' PASSED.\n";
    else
        std::cout << "Test file '" << filename << "' FAILED.\n";
    std::cout << "-----------------------------------------\n";
    return passed;
}

int main(int argc, char* argv[]) {
    // Usage: executable <strategy> <test_file1> [test_file2 ...]
    // <strategy> must be one of: coarse, fine, lockfree
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <strategy> <test_file1> [test_file2 ...]\n";
        return 1;
    }

    std::string strategy = argv[1];
    bool overallPassed = true;

    // Process each test file provided on the command line.
    for (int i = 2; i < argc; i++) {
        std::string testFile = argv[i];
        bool result = false;
        if (strategy == "coarse") {
            result = runTestForFile<UnionFindParallelCoarse>(testFile);
        } else if (strategy == "fine") {
            // Uncomment below if you have implemented the fine-grained version.
            // result = runTestForFile<UnionFindParallelFine>(testFile);
        } else if (strategy == "lockfree") {
            // Uncomment below if you have implemented the lock-free version.
            // result = runTestForFile<UnionFindParallelLockFree>(testFile);
        } else {
            std::cerr << "Unknown strategy: " << strategy << "\n";
            return 1;
        }
        overallPassed = overallPassed && result;
    }

    if (!overallPassed) {
        std::cerr << "Some tests FAILED.\n";
        return 1;
    }
    std::cout << "All tests PASSED.\n";
    return 0;
}
