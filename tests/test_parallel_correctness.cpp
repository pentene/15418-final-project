#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <omp.h>

// Include parallel union find headers.
#include "union_find_parallel_coarse.hpp"
#include "union_find_parallel_fine.hpp"
#include "union_find_parallel_lockfree.hpp"

// Helper function to trim whitespace from both ends.
std::string trim(const std::string &s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end = s.find_last_not_of(" \t\r\n");
    if (start == std::string::npos)
        return "";
    return s.substr(start, end - start + 1);
}

// Structure to store a union operation.
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
    std::vector<UnionOp> unionOps;  // union operations to be performed concurrently
    std::vector<Query> queries;     // connectivity queries to be checked after all unions are done
};

// Function to load test data from a file.
// The expected file format is as described above.
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
// UFType must provide a constructor UFType(int), a unionSets(int, int), and find(int).
template <typename UFType>
bool runTestForFile(const std::string &filename) {
    std::cout << "Processing test file: " << filename << std::endl;
    ParallelTestData data = loadTestData(filename);
    UFType uf(data.n);

    // Process union operations concurrently.
    std::vector<int> results(data.unionOps.size(), -1); // Initialize results for union operations.
    uf.processOperations(data.unionOps, results);

    // Now verify connectivity queries.
    bool passed = true;
    for (size_t i = 0; i < data.queries.size(); i++) {
        Query q = data.queries[i];
        // In both sequential and parallel versions, the check is that if q.expected==1 then
        // uf.find(a) should equal uf.find(b); if q.expected==0 they should differ.
        bool connected = (uf.find(q.a) == uf.find(q.b));
        std::cout << "Query (" << q.a << ", " << q.b << ") -> got " << connected
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
            // result = runTestForFile<UnionFindParallelFine>(testFile);
        } else if (strategy == "lockfree") {
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
