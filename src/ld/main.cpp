#include <iostream>

#include "linker.h"

void showUsage(std::string name) {
    std::cerr << "Usage: " << name << " [-o <out-file>] [-s] [-r] <in-file> [<in-file> ... ]" << std::endl;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        showUsage(argv[0]);
        return -1;
    }

    std::vector<std::string> inFiles;
    std::string outFile = "a.out";
    bool outputSymbols = false;
    bool outputRaw = false;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
              case 'o': // set output file
                i++;
                if (i < argc) {
                    outFile = argv[i];
                }
                break;
              case 's': // output symbol locations
                outputSymbols = true;
                break;
              case 'r': // 
                outputRaw = true;
                break;
              default:
                std::cout << "Warning: Unrecognized flag: '" << argv[i] << "'. Ignoring." << std::endl;
                break;
            }
        }
        else {
            inFiles.push_back(argv[i]);
        }
    }

    if (inFiles.size() == 0) {
        showUsage(argv[0]);
        return -1;
    }

    ldnp::Linker linker(outFile);
    if (!linker.loadFiles(inFiles)) {
        return -1;
    }
    if (!linker.resolveReferences()) {
        return -1;
    }
    if (!linker.positionSegments()) {
        return -1;
    }
    if (!linker.relocateSegments()) {
        return -1;
    }
    if (!linker.generateOutputFile()) {
        return -1;
    }
    if (!linker.writeOutputFile(outputRaw)) {
        return -1;
    }
    std::cout << "Done." << std::endl;
    
    

    return 0;
}
