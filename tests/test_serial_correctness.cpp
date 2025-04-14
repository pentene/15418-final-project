#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include "union_find.hpp"  // Serial UnionFind header

// Helper function to trim whitespace.
std::string trim(const std::string &s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end = s.find_last_not_of(" \t\r\n");
    if (start == std::string::npos)
        return "";
    return s.substr(start, end - start + 1);
}

// Structure to hold the test data for serial correctness.
struct SerialTestData {
    int n;  // number of elements
    // List of operations (both union and find) in the order they appear.
    std::vector<UnionFind::Operation> ops;
    // Expected outputs for the find operations (in order).
    std::vector<int> expected;
};

// Loads test data from a given file.
// Expected file format:
//   n u f
// followed by u lines of union operations: "U a b"
// followed by f lines of find operations: "F a expected"
SerialTestData loadSerialTestData(const std::string &filename) {
    std::ifstream infile(filename);
    if (!infile) {
        throw std::runtime_error("Error opening file: " + filename);
    }

    SerialTestData data;
    std::string line;

    // Read header: n, number of union ops (u), number of find ops (f)
    while (std::getline(infile, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        std::istringstream iss(line);
        int uOps, fOps;
        if (!(iss >> data.n >> uOps >> fOps))
            throw std::runtime_error("Invalid header format in test file: " + filename);
        
        // Process union operations.
        for (int i = 0; i < uOps; i++) {
            while (std::getline(infile, line)) {
                line = trim(line);
                if (line.empty() || line[0] == '#') continue;
                std::istringstream opStream(line);
                char op;
                int a, b;
                opStream >> op >> a >> b;
                if (op != 'U')
                    throw std::runtime_error("Expected union operation starting with 'U' in file: " + filename);
                data.ops.push_back({UnionFind::OperationType::UNION_OP, a, b});
                break;
            }
        }
        
        // Process find operations.
        for (int i = 0; i < fOps; i++) {
            while (std::getline(infile, line)) {
                line = trim(line);
                if (line.empty() || line[0] == '#') continue;
                std::istringstream opStream(line);
                char op;
                int a, exp;
                opStream >> op >> a >> exp;
                if (op != 'F')
                    throw std::runtime_error("Expected find operation starting with 'F' in file: " + filename);
                // For a find operation, store the op (b field is unused).
                data.ops.push_back({UnionFind::OperationType::FIND_OP, a, 0});
                data.expected.push_back(exp);
                break;
            }
        }
        break; // Only process one test case per file.
    }
    return data;
}

int main(int argc, char* argv[]) {
    // Require at least one test file as argument.
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <test_file1> [<test_file2> ...]" << std::endl;
        return 1;
    }
    
    bool overallPassed = true;
    
    // Process each test file specified on the command line.
    for (int fileIndex = 1; fileIndex < argc; fileIndex++) {
        std::string filename = argv[fileIndex];
        std::cout << "Processing test file: " << filename << std::endl;
        
        try {
            SerialTestData testData = loadSerialTestData(filename);
            std::cout << "Loaded test data: n = " << testData.n 
                      << ", union ops = " << (testData.ops.size() - testData.expected.size())
                      << ", find ops = " << testData.expected.size() << "\n";
            
            // Create an instance of serial UnionFind.
            UnionFind uf(testData.n);
            std::vector<int> results;
            uf.processOperations(testData.ops, results);
            
            // Verify find operations.
            int findCount = 0;
            bool filePassed = true;
            for (size_t i = 0; i < testData.ops.size(); ++i) {
                if (testData.ops[i].type == UnionFind::OperationType::FIND_OP) {
                    int result = results[i];
                    int expected = testData.expected[findCount];
                    std::cout << "Find op (" << testData.ops[i].a << "): got " << result
                              << ", expected " << expected << "\n";
                    if (result != expected) {
                        std::cerr << "Mismatch in find operation " << findCount << " in file " << filename << "\n";
                        filePassed = false;
                    }
                    ++findCount;
                }
            }
            if (filePassed) {
                std::cout << "Test file '" << filename << "' PASSED.\n";
            } else {
                std::cout << "Test file '" << filename << "' FAILED.\n";
                overallPassed = false;
            }
        } catch (const std::exception &ex) {
            std::cerr << "Error while processing file '" << filename << "': " << ex.what() << "\n";
            overallPassed = false;
        }
        std::cout << "-----------------------------------------\n";
    }
    
    if (!overallPassed) {
        std::cerr << "Some tests failed.\n";
        return 1;
    }
    
    std::cout << "All test files passed.\n";
    return 0;
}
