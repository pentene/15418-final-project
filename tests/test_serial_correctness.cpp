#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <tuple>
#include "union_find.hpp"  // Your serial union-find implementation

// Simple helper to trim leading/trailing whitespace
std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end = s.find_last_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    return s.substr(start, end - start + 1);
}

// Structure to hold our test data
struct TestData {
    int n;  // number of elements
    std::vector<std::pair<int, int>> unions;  // union operations
    std::vector<std::tuple<int, int, bool>> queries;  // queries: (a, b, expectedIsConnected)
};

// Function to load test data from a resource file
TestData loadTestData(const std::string& filename) {
    std::ifstream infile(filename);
    if (!infile.is_open()) {
        throw std::runtime_error("Cannot open test data file: " + filename);
    }

    TestData data;
    std::string line;

    // Read header: n, numUnions, numQueries
    while (std::getline(infile, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        std::istringstream iss(line);
        int n, numUnions, numQueries;
        if (!(iss >> n >> numUnions >> numQueries)) {
            throw std::runtime_error("Invalid test data header format.");
        }
        data.n = n;

        // Read union operations
        for (int i = 0; i < numUnions; ++i) {
            while (std::getline(infile, line)) {
                line = trim(line);
                if (line.empty() || line[0] == '#')
                    continue;
                int a, b;
                std::istringstream issUnion(line);
                if (!(issUnion >> a >> b)) {
                    throw std::runtime_error("Invalid union operation format in test data.");
                }
                data.unions.push_back({a, b});
                break;
            }
        }

        // Read query operations
        for (int i = 0; i < numQueries; ++i) {
            while (std::getline(infile, line)) {
                line = trim(line);
                if (line.empty() || line[0] == '#')
                    continue;
                int a, b, expected;
                std::istringstream issQuery(line);
                if (!(issQuery >> a >> b >> expected)) {
                    throw std::runtime_error("Invalid query operation format in test data.");
                }
                data.queries.push_back(std::make_tuple(a, b, expected == 1));
                break;
            }
        }
        break; // We only process the first test case in this example
    }
    infile.close();
    return data;
}

int main() {
    try {
        // Specify the relative path to the test resource file
        std::string filename = "tests/resources/test_data_serial.txt";
        TestData data = loadTestData(filename);
        std::cout << "Loaded test data: " << data.n << " elements, "
                  << data.unions.size() << " union operations, and "
                  << data.queries.size() << " query operations.\n";

        // Create an instance of your serial UnionFind
        // It is assumed that UnionFind has a constructor taking the number of elements
        UnionFind uf(data.n);

        // Process all union operations from the test data
        for (const auto& op : data.unions) {
            int a = op.first, b = op.second;
            uf.unionSets(a, b);
        }

        // Validate query operations, checking if the connectivity is as expected
        int errors = 0;
        for (const auto& query : data.queries) {
            int a, b;
            bool expected;
            std::tie(a, b, expected) = query;
            bool connected = (uf.find(a) == uf.find(b));
            std::cout << "Query: (" << a << ", " << b << ") - Expected: " << expected
                      << " Got: " << connected << "\n";
            if (connected != expected) {
                std::cerr << "Test failed for query (" << a << ", " << b << ")\n";
                ++errors;
            }
        }

        if (errors > 0) {
            std::cerr << errors << " test queries failed.\n";
            return 1;
        } else {
            std::cout << "All serial correctness tests passed.\n";
            return 0;
        }

    } catch (const std::exception& ex) {
        std::cerr << "An exception occurred: " << ex.what() << "\n";
        return 1;
    }
}