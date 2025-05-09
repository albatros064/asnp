#include "assemble.h"

#include <iostream>
#include <string>

void showUsage(std::string name) {
    std::cerr << "Usage: " << name << " [-o <out-file>] [-s] [-r] <in-file>" << std::endl;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        showUsage(argv[0]);
        return -1;
    }

    std::string inFile;
    std::string outFile;
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
            inFile = argv[i];
        }
    }

    if (inFile.length() <= 0) {
        showUsage(argv[0]);
        return -1;
    }

    if (outFile.length() <= 0) {
        outFile = inFile;
        outFile.append(".o");
    }

    asnp::Assembler assembler(outFile);
    if (!assembler.assemble("", inFile)) {
        return -1;
    }
    if (!assembler.link(outputSymbols, outputRaw)) {
        return -1;
    }
    if (!assembler.write(outputRaw)) {
        return -1;
    }
    std::cout << "Done." << std::endl;

    return 0;
}
