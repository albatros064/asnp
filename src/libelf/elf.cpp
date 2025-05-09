#include "elf.h"

#include <iostream>
#include <fstream>
#include <cstring>

namespace libelf {

void ensureAlignment(uint32_t& value, uint32_t alignment) {
    if (alignment <= 1) {
        return;
    }
    if (value & (alignment - 1)) {
        value = value & ~(alignment - 1);
        value += alignment;
    }
}

Section::Section(): data(0), fileAlignment(0), header({}) {}
Section::~Section() {
    if (data != 0) {
        delete[] data;
        data = 0;
    }
}

bool Section::addSymbol(std::shared_ptr<Section> section, std::string name, Elf32_Word offset) {
    if (!isSymbolTable()) {
        return false;
    }

    if (symbolMap.size() == 0) {
        auto nullSymbol = std::make_shared<Symbol>();
        nullSymbol->index = 0;
        symbols.push_back(nullSymbol);
    }

    if (symbolMap.contains(name)) {
        return false;
    }


    auto symbol = std::make_shared<Symbol>();
    symbol->name = name;
    symbol->header.st_value = offset;
    symbol->header.st_info = ELF32_ST_INFO(STB_GLOBAL, STT_NOTYPE);
    symbol->header.st_shndx = section ? section->index : SHN_UNDEF;
    symbol->section = section;
    symbol->index = symbols.size();

    symbols.push_back(symbol);
    symbolMap[name] = symbol;

    return true;
}

bool Section::addRelocation(std::shared_ptr<Section> section, std::shared_ptr<Symbol> symbol, Elf32_Word offset, uint8_t type) {
    if (!isRelocationTable()) {
        return false;
    }

    auto relocation = std::make_shared<Relocation>();
    relocation->symbol = symbol;
    relocation->offset = offset;
    relocation->type = type;
    relocation->section = section;
    relocations.push_back(relocation);

    return true;
}

std::string Section::getString(Elf32_Word offset) {
    if (!isStringTable() || data == 0) {
        return "";
    }

    if (offset >= header.sh_size) {
        return "";
    }

    return (char *) (data + offset);
}

std::shared_ptr<Symbol> Section::getSymbol(Elf32_Word offset) {
    std::shared_ptr<Symbol> symbol;
    if (!isSymbolTable() || data == 0) {
        return symbol;
    }
    if (offset >= symbols.size()) {
        return symbol;
    }

    return symbols[offset];
}

bool Section::extractData(ElfFile& file) {
    if (data == 0) {
        return false;
    }

    if (isRelocationTable()) {
        auto symbolTable = file.getSection(header.sh_link);
        auto referenceSection = file.getSection(header.sh_info);

        int relocationCount = header.sh_size / sizeof(Elf32_Rel);
        relocations.reserve(relocationCount);

        for (int i = 0; i < relocationCount; i++) {
            auto relocation = std::make_shared<Relocation>();
            Elf32_Rel header = ((Elf32_Rel *)data)[i];

            relocation->symbol = symbolTable->getSymbol(header.r_info >> 8);
            relocation->type = (uint8_t) header.r_info & 0xff;
            relocation->offset = header.r_offset;
            relocation->section = referenceSection;
            
            relocations.push_back(relocation);
        }
    }
    else if (isSymbolTable()) {
        auto symbolNames = file.getSection(header.sh_link);

        int symbolCount = header.sh_size / sizeof(Elf32_Sym);
        symbols.reserve(symbolCount);

        for (int i = 0; i < symbolCount; i++) {
            auto symbol = std::make_shared<Symbol>();
            symbol->header = ((Elf32_Sym *)data)[i];

            symbol->name = symbolNames->getString(symbol->header.st_name);
            symbol->index = symbol->header.st_value;
            if (symbol->header.st_shndx != SHN_UNDEF) {
                symbol->section = file.getSection(symbol->header.st_shndx);
            }

            symbols.push_back(symbol);
            symbolMap[symbol->name] = symbol;
        }
    }

    return true;
}

bool Section::generateData() {
    if (data != 0) {
        return true;
    }

    if (isRelocationTable()) {
        header.sh_size = relocations.size() * sizeof(Elf32_Rel);
        if (link) {
            header.sh_link = link->index;
        }

        data = new char[header.sh_size];

        for (int i = 0; i < relocations.size(); i++) {
            auto relocation = relocations[i];
            Elf32_Word r_info = relocation->symbol->index << 8 | relocation->type;
            Elf32_Rel rel = {
                relocation->offset,
                r_info
            };
            ((Elf32_Rel *) data)[i] = rel;
        }
    }
    else if (isSymbolTable()) {
        header.sh_size = symbols.size() * sizeof(Elf32_Sym);
        if (link) {
            header.sh_link = link->index;
        }

        data = new char[header.sh_size];

        for (int i = 0; i < symbols.size(); i++) {
            auto symbol = symbols[i];

            ((Elf32_Sym *) data)[i] = symbol->header;
        }
    }

    return true;
}

Elf32_Word Symbol::address() {
    if (section) {
        return section->header.sh_addr + header.st_value;
    }
    return header.st_value;
}



uint8_t identBytes[8] = {
    ELFMAG0,ELFMAG1,ELFMAG2,ELFMAG3,
    ELFCLASS32,ELFDATA2LSB,
    EV_CURRENT,
    ELFOSABI_STANDALONE
};

ElfFile::ElfFile(std::string _fileName): fileName(_fileName) {}
ElfFile::ElfFile(Elf32_Half type):header({}) {
    for (int i = 0; i < 8; i++) {
        header.e_ident[i] = identBytes[i];
    }

    header.e_type = type;
    header.e_machine = EM_NONE;
    header.e_version = EV_CURRENT;
    header.e_ehsize = sizeof(header);
    header.e_phentsize = sizeof(Elf32_Phdr);
    header.e_shentsize = sizeof(Elf32_Shdr);
}


std::shared_ptr<Section> ElfFile::addSection(Elf32_Word type, std::shared_ptr<Section> link, std::shared_ptr<Section> info) {
    if (sections.size() == 0) {
        // If we have no sections, section 0 is implied and may be
        // absent. But we're adding a section, so we need to declare
        // it explicitly.
        sections.push_back(std::make_shared<Section>());
    }

    auto section = std::make_shared<Section>();

    section->header.sh_type = type;

    if (type == SHT_REL) {
        section->name = ".rel";
        section->name += info->name;
        section->link = link;
        section->header.sh_info = info->index;
        section->header.sh_entsize = sizeof(Elf32_Rel);
    }
    else if (type == SHT_SYMTAB) {
        section->header.sh_entsize = sizeof(Elf32_Sym);
        section->header.sh_info = 1;
    }

    section->index = sections.size();
    sections.push_back(section);

    return section;
}

std::shared_ptr<Segment> ElfFile::addSegment(Elf32_Word type) {
    auto segment = std::make_shared<Segment>();
    segment->header.p_type = type;

    segments.push_back(segment);

    return segment;
}

bool ElfFile::readHeaders() {
    std::ifstream file(fileName, std::ios::in|std::ios::binary/*|std::ios::ate*/);
    if (!file.is_open()) {
        return false;
    }

    file.read((char *)&header, sizeof(header));
    if (!acceptable(ET_NONE)) {
        return false;
    }

    // preallocate space for program headers
    segments.reserve(header.e_phnum);

    // go to the start of the program header table
    file.seekg(header.e_phoff);
    // read each program header
    for (int i = 0; i < header.e_phnum; i++) {
        auto segment = std::make_shared<Segment>();

        file.read((char *)&(segment->header), header.e_phentsize);

        segments.push_back(segment);
    }

    // preallocate space for section headers
    sections.reserve(header.e_shnum);

    std::shared_ptr<Section> sectionNames;

    // go to the start of the section header table
    file.seekg(header.e_shoff);
    // read each section header
    for (int i = 0; i < header.e_shnum; i++) {
        std::shared_ptr<Section> section = std::make_shared<Section>();

        file.read((char *)&(section->header), header.e_shentsize);

        // keep a copy of the section name strings for later
        if (i == header.e_shstrndx) {
            sectionNames = section;
        }

        sections.push_back(section);
    }

    // is the section name string section actually available?
    if (!sectionNames || sectionNames->header.sh_type != SHT_STRTAB) {
        return false;
    }

    // allocate string space
    sectionNames->data = new char[sectionNames->header.sh_size];
    // go to start of strings
    file.seekg(sectionNames->header.sh_offset);
    // read section name strings
    file.read(sectionNames->data, sectionNames->header.sh_size);

    // resolve names
    for (auto section: sections) {
        section->name = (char *) (sectionNames->data + section->header.sh_name);
    }

    return true;
}

bool ElfFile::readStrings() {
    return readSectionsOfType(SHT_STRTAB);
}

bool ElfFile::readSymbols() {
    return readSectionsOfType(SHT_SYMTAB);
}

bool ElfFile::readRelocations() {
    return readSectionsOfType(SHT_REL);
}

bool ElfFile::readProgBits() {
    return readSectionsOfType(SHT_PROGBITS);
}

bool ElfFile::readSectionsOfType(Elf32_Word type) {
    std::ifstream file(fileName, std::ios::in|std::ios::binary/*|std::ios::ate*/);
    
    for (auto section: sections) {
        if (section->header.sh_type != type) {
            continue;
        }
        if (section->data != 0) {
            continue;
        }
    
        section->data = new char[section->header.sh_size];

        file.seekg(section->header.sh_offset);
        file.read(section->data, section->header.sh_size);

        section->extractData(*this);
    }

    return true;
}

bool ElfFile::generateSymbolStrings(std::shared_ptr<Section> section) {
    std::shared_ptr<Section> nullSection;

    auto strings = addSection(SHT_STRTAB, nullSection, nullSection);
    section->link = strings;
    strings->name = ".strtab";
    int tableLength = 0;
    for (auto symbol: section->symbols) {
        tableLength += symbol->name.length() + 1;
    }
    strings->data = new char[tableLength];
    strings->header.sh_size = tableLength;

    int index = 0;
    for (auto symbol: section->symbols) {
        symbol->header.st_name = index;
        int length = symbol->name.length() + 1;
        std::strcpy(strings->data + index, symbol->name.c_str());
        index += length;
    }

    return true;
}

bool ElfFile::generateSectionNameStrings() {
    std::shared_ptr<Section> nullSection;
    auto strings = addSection(SHT_STRTAB, nullSection, nullSection);
    strings->name = ".shstrtab";
    header.e_shstrndx = strings->index;

    int tableLength = 0;
    for (auto section: sections) {
        tableLength += section->name.length() + 1;
    }
    strings->data = new char[tableLength];
    strings->header.sh_size = tableLength;

    int index = 0;
    for (auto section: sections) {
        section->header.sh_name = index;
        int length = section->name.length() + 1;
        std::strcpy(strings->data + index, section->name.c_str());
        index += length;
    }

    return true;
}

bool ElfFile::generateSectionData() {
    for (auto section: sections) {
        section->generateData();
    }

    return true;
}

bool ElfFile::write(std::string fileName) {
    /*
     *  For our use-case here:
     *  1. program headers immediately follow the file header
     *  2. section data follows program headers
     *  3. section headers follow section data
     *  4. no data byte is outside a section
     */

    std::ofstream file(fileName);

    // Write provisional header
    // (we'll rewrite it later with correct offsets)
    file.write((char *)&header, sizeof(header));
    Elf32_Word offset = sizeof(header);

    // Write segment headers
    for (auto segment: segments) {
        file.write((char *)&segment->header, sizeof(segment->header));
        offset += sizeof(segment->header);
    }

    std::shared_ptr<Section> previousSection;

    // Write section data (which is also segment data, if any)
    for (auto section: sections) {
        if (section->isNoBits() || section->isNull() || section->isProc()) {
            continue;
        }
        auto misalignment = offset;
        ensureAlignment(offset, section->fileAlignment);
        if (misalignment != offset) {
            while (misalignment != offset) {
                file.put(0);
                misalignment++;
            }
        }
        section->header.sh_offset = offset;

        if (section->componentSections.size() > 0) {
            section->header.sh_addr = section->componentSections[0]->header.sh_addr;

            for (auto component: section->componentSections) {
                if (!component->isProgBits()) {
                    continue;
                }
                misalignment = offset;
                ensureAlignment(offset, component->header.sh_addralign);
                if (offset != misalignment) {
                    while (offset != misalignment) {
                        file.put(0);
                        misalignment++;
                    }
                }

                file.write(component->data, component->header.sh_size);
                offset += component->header.sh_size;
                // we've already calculated this size, so we don't
                // want to double up on it
                //section->header.sh_size += component->header.sh_size;
            }
        }
        else {
            auto misalignment = offset;
            ensureAlignment(offset, section->header.sh_addralign);
            if (offset != misalignment) {
                while (offset != misalignment) {
                    file.put(0);
                    misalignment++;
                }
            }
            file.write(section->data, section->header.sh_size);
            offset += section->header.sh_size;
        }
    }

    // Set the final offset for section headers
    if (sections.size() > 0) {
        header.e_shnum = sections.size();
        header.e_shoff = offset;
    }
    // Set the final offset for program headers
    if (segments.size() > 0) {
        header.e_phnum = segments.size();
        header.e_phoff = sizeof(header);
    }

    // Write section headers
    for (auto section: sections) {
        file.write((char *)&(section->header), sizeof(section->header));
    }

    file.seekp(0);
    // Rewrite file header
    file.write((char *)&header, sizeof(header));
    // Rewrite program segment headers
    for (auto segment: segments) {
        file.write((char *)&segment->header, header.e_phentsize);
    }

    // Done!
    file.close();

    return true;
}

std::shared_ptr<Section> ElfFile::getSection(int index) {
    return sections.at(index);
}

std::shared_ptr<Section> ElfFile::findSection(Elf32_Word type, std::string name) {
    for (auto section: sections) {
        if (type != SHT_NULL && type != section->header.sh_type) {
            continue;
        }
        if (!name.empty() && name != section->name) {
            continue;
        }
        return section;
    }

    throw std::out_of_range("ElfFile::findSection");
}
std::shared_ptr<Section> ElfFile::findSection(std::string name) {
    return findSection(SHT_NULL, name);
}

std::vector<std::shared_ptr<Section>> ElfFile::findSections(Elf32_Word type) {
    if (type == SHT_NULL) {
        return sections;
    }
    std::vector<std::shared_ptr<Section>> filteredSections;
    for (auto section: sections) {
        if (section->header.sh_type == type) {
            filteredSections.push_back(section);
        }
    }

    return filteredSections;
}

bool ElfFile::acceptable(Elf32_Half type) {
    // validate ident bytes
    for (int i = 0; i < 8; i++) {
        if (header.e_ident[i] != identBytes[i]) {
            return false;
        }
    }

    // validate ELF version
    if (header.e_version != EV_CURRENT) {
        return false;
    }

    // validate type
    if (type != ET_NONE && header.e_type != type) {
        return false;
    }
    // validate we aren't using a known machine type
    if (header.e_machine != EM_NONE) {
        return false;
    }

    return true;
}

}; // namespace libelf

