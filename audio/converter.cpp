#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <iomanip>

void convertToCSV(const std::string& inputFileName, const std::string& outputFileName, int& numValues) {
    std::ifstream inputFile(inputFileName);
    if (!inputFile.is_open()) {
        std::cerr << "Error: Unable to open input file " << inputFileName << std::endl;
        return;
    }

    std::ofstream outputFile(outputFileName);
    if (!outputFile.is_open()) {
        std::cerr << "Error: Unable to create output file " << outputFileName << std::endl;
        return;
    }

    std::string line;
    while (std::getline(inputFile, line)) {
        std::istringstream iss(line);
        std::string word;
        bool firstWord = true;
        while (iss >> word) {
            if (firstWord) {
                firstWord = false;
                continue; // Skip the first word (byte count)
            }
            // Check if the word is a hexadecimal value
            if (word.find_first_not_of("0123456789abcdefABCDEF") == std::string::npos) {
                // Convert the hexadecimal value to an integer
                unsigned int hexValue = std::stoul(word, nullptr, 16);
                // Write the hexadecimal value with "0x" prefix to the output file
                outputFile << "0x" << std::setw(8) << std::setfill('0') << std::hex << hexValue;
            } else {
                // Write non-hexadecimal values directly to the output file
                outputFile << word;
            }
            // Add comma after each value except the last one
            if (!iss.eof()) {
                outputFile << ",";
            }
            numValues++; // Increment count of values
        }
        outputFile << "," << std::endl; // Add comma at the end of each line
    }

    inputFile.close();
    outputFile.close();

    std::cout << "Conversion completed. Output saved to " << outputFileName << std::endl;
}

int main() {
    std::string inputFileName = "output32b.txt"; // Change this to your input file name
    std::string outputFileName = "output32b.csv"; // Change this to your desired output file name
    int numValues = 0;
    convertToCSV(inputFileName, outputFileName, numValues);
    std::cout << "Number of values in the CSV: " << numValues << std::endl;
    return 0;
}
