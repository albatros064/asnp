#ifndef ELF_H
#define ELF_H

#include <elf.h>
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace libelf {

#define N16R_REL_JMP    1
#define N16R_REL_B0     4
#define N16R_REL_B1     5
#define N16R_REL_B2     6
#define N16R_REL_B3     7

struct Section;
struct ElfFile;

struct Symbol {
    Symbol(): header({}) {}
    Elf32_Sym header;

    Elf32_Word address();

    std::string name;
    std::shared_ptr<Section> section;
    Elf32_Word index;
};
struct Relocation {
    std::shared_ptr<Symbol> symbol;
    Elf32_Word offset;
    uint8_t type;

    std::shared_ptr<Section> section;
};
struct Section {
    Section();
    ~Section();
    Elf32_Shdr header;
    std::string name;
    Elf32_Word index;

    Elf32_Word fileAlignment;

    bool isNull             () { return header.sh_type == SHT_NULL; }
    bool isProgBits         () { return header.sh_type == SHT_PROGBITS; }
    bool isNoBits           () { return header.sh_type == SHT_NOBITS; }
    bool isSymbolTable      () { return header.sh_type == SHT_SYMTAB; }
    bool isStringTable      () { return header.sh_type == SHT_STRTAB; }
    bool isRelocationTable  () { return header.sh_type == SHT_REL; }
    bool isProc             () { return header.sh_type >= SHT_LOPROC && header.sh_type <= SHT_HIPROC; }

    bool isReadOnly() { return !(header.sh_flags & SHF_WRITE); }
    bool isExecutable() { return (header.sh_flags & SHF_EXECINSTR); }

    bool extractData(ElfFile&);
    bool generateData();

    bool addRelocation(std::shared_ptr<Section>, std::shared_ptr<Symbol>, Elf32_Word, uint8_t);
    bool addSymbol(std::shared_ptr<Section>, std::string, Elf32_Word);

    std::string getString(Elf32_Word);
    std::shared_ptr<Symbol> getSymbol(Elf32_Word);

    char *data;

    std::shared_ptr<Section> link;

    std::map<std::string, std::shared_ptr<Symbol>> symbolMap;
    std::vector<std::shared_ptr<Symbol>> symbols;
    std::vector<std::shared_ptr<Relocation>> relocations;

    std::vector<std::shared_ptr<Section>> componentSections;
};

struct Segment {
    Elf32_Phdr header;
};

struct ElfFile {
    public:
        ElfFile(std::string);
        ElfFile(Elf32_Half);

        bool acceptable(Elf32_Half);

        std::shared_ptr<Section> getSection(int);

        std::shared_ptr<Section> findSection(Elf32_Word, std::string = "");
        std::shared_ptr<Section> findSection(std::string);

        std::vector<std::shared_ptr<Section>> findSections(Elf32_Word);

        std::shared_ptr<Section> addSection(Elf32_Word, std::shared_ptr<Section>, std::shared_ptr<Section>);
        std::shared_ptr<Segment> addSegment(Elf32_Word);

        void setEntryPoint(Elf32_Word entry) { header.e_entry = entry; }

        bool readHeaders();
        bool readStrings();
        bool readSymbols();
        bool readRelocations();
        bool readProgBits();

        bool readSectionsOfType(Elf32_Word);

        bool generateSymbolStrings(std::shared_ptr<Section>);
        bool generateSectionNameStrings();
        bool generateSectionData();

        std::string getFileName() { return fileName; }
        bool write(std::string);
    private:
        std::string fileName;

        Elf32_Ehdr header;

        std::vector<std::shared_ptr<Section>> sections;
        std::vector<std::shared_ptr<Segment>> segments;
};

}; // namespace libelf

#endif
