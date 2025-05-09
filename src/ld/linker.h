#ifndef LINKER_H
#define LINKER_H

#include <string>
#include <vector>

#include "../libelf/elf.h"

namespace ldnp {

class Linker {
    public:
        Linker(std::string _fileName): fileName(_fileName) {}
        ~Linker() {}

        bool loadFiles(std::vector<std::string>);
        bool resolveReferences();
        bool positionSegments();
        bool relocateSegments();
        bool generateOutputFile();
        bool writeOutputFile(bool);
    private:
        std::string fileName;
        std::shared_ptr<libelf::ElfFile> outFile;

        std::shared_ptr<libelf::Symbol> entrySymbol;

        std::vector<std::shared_ptr<libelf::ElfFile>> files;

        std::vector<std::shared_ptr<libelf::Section>> exSegment;
        std::vector<std::shared_ptr<libelf::Section>> roSegment;
        std::vector<std::shared_ptr<libelf::Section>> rwSegment;
        std::vector<std::shared_ptr<libelf::Section>> zeSegment;
        int segmentCount;
};

}; // namespace ldnp

#endif
