#include "assemble.h"

#include <iostream>
#include <string>

void showUsage(std::string name) {
    std::cerr << "Usage: " << name << " [-o <out-file>] <in-file>" << std::endl;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        showUsage(argv[0]);
        return -1;
    }

    std::string inFile;
    std::string outFile;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
              case 'o':
                if (i + 1 < argc) {
                    outFile = argv[i + 1];
                }
                break;
              default:
                std::cout << "Warning: Unrecognized flag: '" << argv[i] << "'. Ignoring." << std::endl;
            }
            i++;
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

    //std::cout << "Assembling '" << inFile << "' into '" << outFile << "'." << std::endl;

    asnp::Assembler assembler(inFile, outFile);
    if (!assembler.assemble()) {
        return -1;
    }
    std::cout << "Done." << std::endl;

    return 0;
}
