#include <iostream>
#include <set>

#include "linker.h"

namespace ldnp {

bool Linker::loadFiles(std::vector<std::string> inFileNames) {
    for (auto fileName: inFileNames) {
        auto inFile = std::make_shared<libelf::ElfFile>(fileName);
        if (!inFile->readHeaders()) {
            return false;
        }
        if (!inFile->readStrings()) {
            return false;
        }
        if (!inFile->readSymbols()) {
            return false;
        }
        if (!inFile->readRelocations()) {
            return false;
        }
        if (!inFile->readProgBits()) {
            return false;
        }

        files.push_back(inFile);
    }

    return true;
}

bool Linker::resolveReferences() {
    std::map<std::string, std::shared_ptr<libelf::Symbol>> symbols;

    bool duplicateSymbols = false;
    for (auto file: files) {
        auto symbolTable = file->findSection(SHT_SYMTAB);

        for (auto symbol: symbolTable->symbols) {
            // ignore undefined symbols at this point
            if (!(symbol->section)) {
                continue;
            }

            if (symbols.contains(symbol->name)) {
                duplicateSymbols = true;
                std::cerr << file->getFileName() << ": multiple definition of symbol '" << symbol->name << "'" << std::endl;
                continue;
            }

            symbols[symbol->name] = symbol;
        }
    }

    bool undefinedSymbols = false;
    for (auto file: files) {
        std::set<std::string> previouslyUndefinedSymbols;
        auto sections = file->findSections(SHT_REL);
        for (auto section: sections) {
            for (auto relocation: section->relocations) {
                auto symbol = relocation->symbol;
                auto relSection = file->getSection(section->header.sh_info);
                if (symbols.contains(symbol->name)) {
                    auto reference = symbols[symbol->name];
                    relocation->symbol = reference;
                }
                else if (!previouslyUndefinedSymbols.contains(symbol->name)) {
                    previouslyUndefinedSymbols.insert(symbol->name);
                    undefinedSymbols = true;
                    std::cerr << file->getFileName() << ":(" << relSection->name << "+0x" << std::hex << relocation->offset << std::dec << "): undefined symbol '" << symbol->name << "'" << std::endl;
                }
            }
        }
    }

    if (!symbols.contains("__main")) {
        std::cerr << fileName << ":(.text+0x0): undefined symbol '__main'" << std::endl;
        undefinedSymbols = true;
    }
    else {
        entrySymbol = symbols["__main"];
        exSegment.push_back(entrySymbol->section);
    }

    if (duplicateSymbols || undefinedSymbols) {
        return false;
    }

    return true;
}

void ensureAlignment(uint32_t& value, uint32_t alignment) {
    if (alignment <= 1) {
        return;
    }
    if (value & (alignment - 1)) {
        value = value & ~(alignment - 1);
        value += alignment;
    }
}

bool Linker::positionSegments() {
    int exSize = 0;
    int roSize = 0;
    int rwSize = 0;
    int zeSize = 0;

    Elf32_Addr pageSize = 0;

    // exe progbits containing __main   (.text)
    // exe progbits                     (.text)
    // ro progbits                      (.rodata)
    // rw progbits                      (.data)
    // rw nobits                        (.bss)
    for (auto file: files) {
        for (auto section: file->findSections(SHT_NULL)) {
            if (section == exSegment[0]) {
                exSize += section->header.sh_size;
                continue;
            }
            else if (section->isNoBits()) {
                if (!section->isReadOnly()) {
                    zeSize += section->header.sh_size;
                    zeSegment.push_back(section);
                }
            }
            else if (section->isProgBits()) {
                if (section->isExecutable()) {
                    exSize += section->header.sh_size;
                    exSegment.push_back(section);
                }
                else if (section->isReadOnly()) {
                    roSize += section->header.sh_size;
                    roSegment.push_back(section);
                }
                else {
                    rwSize += section->header.sh_size;
                    rwSegment.push_back(section);
                }
            }
            else if (section->isProc()) {
                if (section->name == ".pagesize") {
                    pageSize = section->header.sh_addr;
                }
            }
        }
    }

    // put the ephemeral parts at the end of the rw segment
    for (auto section: zeSegment) {
        //rwSegment.push_back(section);
    }

    segmentCount = 0;
    if (exSize > 0) {
        segmentCount++;
    }
    if (roSize > 0) {
        segmentCount++;
    }
    if (rwSize > 0 || zeSize > 0) {
        segmentCount++;
    }

    uint32_t memoryOffset = exSegment[0]->header.sh_addr + sizeof(Elf32_Ehdr) + segmentCount * sizeof(Elf32_Phdr);

    for (auto segment: exSegment) {
        ensureAlignment(memoryOffset, segment->header.sh_addralign);
        //segment->fileAlignment = segment->header.sh_addr;
        segment->fileAlignment = pageSize;
        segment->header.sh_addr = memoryOffset;
        memoryOffset += segment->header.sh_size;
    }

    if (roSize > 0) {
        ensureAlignment(memoryOffset, roSegment[0]->header.sh_addr);
    }

    for (auto segment: roSegment) {
        ensureAlignment(memoryOffset, segment->header.sh_addralign);
        segment->fileAlignment = pageSize;
        segment->header.sh_addr = memoryOffset;
        memoryOffset += segment->header.sh_size;
    }

    if (rwSize > 0 || zeSize > 0) {
        ensureAlignment(memoryOffset, pageSize);
        if (rwSize > 0) {
            ensureAlignment(memoryOffset, rwSegment[0]->header.sh_addr);
        }
        if (zeSize > 0) {
            ensureAlignment(memoryOffset, zeSegment[0]->header.sh_addr);
        }
    }

    for (auto segment: rwSegment) {
        ensureAlignment(memoryOffset, segment->header.sh_addralign);
        segment->fileAlignment = pageSize;
        segment->header.sh_addr = memoryOffset;
        memoryOffset += segment->header.sh_size;
    }

    for (auto segment: zeSegment) {
        ensureAlignment(memoryOffset, segment->header.sh_addralign);
        segment->fileAlignment = pageSize;
        segment->header.sh_addr = memoryOffset;
        memoryOffset += segment->header.sh_size;
    }

    return true;
}

bool Linker::relocateSegments() {
    for (auto file: files) {
        auto sections = file->findSections(SHT_REL);
        for (auto section: sections) {
            for (auto relocation: section->relocations) {
                auto offset = relocation->offset;
                auto value = relocation->symbol->address();
                auto data = relocation->section->data;
                uint8_t byte = 0;

                switch (relocation->type) {
                    case N16R_REL_JMP:
                        value = value >> 1;
                        data[offset + 3] = (value & 0xff);
                        value >>= 8;
                        data[offset + 2] = (value & 0xff);
                        value >>= 8;
                        data[offset + 1] = (value & 0xff);
                        value >>= 8;
                        data[offset] = (data[offset] & 0xf0) | (value & 0x0f);
                        break;
                    case N16R_REL_B3:
                        byte++;
                    case N16R_REL_B2:
                        byte++;
                    case N16R_REL_B1:
                        byte++;
                    case N16R_REL_B0:
                        data[offset] = (value >> byte) & 0xff;
                        break;
                    default:
                        break;
                }
            }
        }
    }

    return true;
}

bool Linker::generateOutputFile() {
    std::shared_ptr<libelf::Section> nullSection;

    outFile = std::make_shared<libelf::ElfFile>(ET_EXEC);
    outFile->setEntryPoint(entrySymbol->address());

    auto section = outFile->addSection(SHT_PROGBITS, nullSection, nullSection);
    section->name = ".text";
    section->componentSections = exSegment;
    section->header.sh_addr = exSegment[0]->header.sh_addr;

    auto exSeg = outFile->addSegment(PT_LOAD);
    auto s0 = exSegment[0];
    auto fileOffset = s0->header.sh_addr & (s0->fileAlignment - 1);
    exSeg->header.p_offset = 0;
    exSeg->header.p_flags = PF_X | PF_R;
    exSeg->header.p_filesz = fileOffset;
    exSeg->header.p_memsz  = fileOffset;
    exSeg->header.p_align = s0->header.sh_addralign;
    exSeg->header.p_vaddr = s0->header.sh_addr - fileOffset;
    exSeg->header.p_paddr = s0->header.sh_addr - fileOffset;

    for (auto cSect: exSegment) {
        exSeg->header.p_filesz += cSect->header.sh_size;
        exSeg->header.p_memsz  += cSect->header.sh_size;
    }
    fileOffset = exSeg->header.p_filesz;

    if (roSegment.size() > 0) {
        auto section = outFile->addSection(SHT_PROGBITS, nullSection, nullSection);
        s0 = roSegment[0];
        section->name = ".rodata";
        section->componentSections = roSegment;
        section->header.sh_addr = roSegment[0]->header.sh_addr;

        auto segment = outFile->addSegment(PT_LOAD);

        section->fileAlignment = s0->fileAlignment;
        auto originalOffset = fileOffset;
        ensureAlignment(fileOffset, s0->fileAlignment);
        segment->header.p_offset = fileOffset;
        segment->header.p_flags = PF_R;
        segment->header.p_filesz = 0;
        segment->header.p_memsz = 0;
        segment->header.p_align = s0->header.sh_addralign;
        segment->header.p_vaddr = s0->header.sh_addr;
        segment->header.p_paddr = s0->header.sh_addr;

        for (auto cSect: roSegment) {
            segment->header.p_filesz += cSect->header.sh_size;
            segment->header.p_memsz  += cSect->header.sh_size;
        }

        fileOffset += segment->header.p_filesz;
    }

    if (rwSegment.size() > 0 || zeSegment.size() > 0) {
        auto segment = outFile->addSegment(PT_LOAD);
        if (rwSegment.size() > 0) {
            segment->header.p_vaddr = rwSegment[0]->header.sh_addr;
            segment->header.p_align = rwSegment[0]->header.sh_addralign;
        }
        else if (zeSegment.size() > 0) {
            segment->header.p_vaddr = zeSegment[0]->header.sh_addr;
            segment->header.p_align = zeSegment[0]->header.sh_addralign;
        }

        if (rwSegment.size() > 0) {
            ensureAlignment(fileOffset, rwSegment[0]->fileAlignment);
        }
        if (zeSegment.size() > 0) {
            ensureAlignment(fileOffset, zeSegment[0]->fileAlignment);
        }

        segment->header.p_paddr = segment->header.p_vaddr;
        segment->header.p_offset = fileOffset;
        segment->header.p_flags = PF_R | PF_W;
        segment->header.p_filesz = 0;
        segment->header.p_memsz = 0;

        if (rwSegment.size() > 0) {
            auto section = outFile->addSection(SHT_PROGBITS, nullSection, nullSection);
            section->name = ".data";
            section->componentSections = rwSegment;
            section->fileAlignment = rwSegment[0]->fileAlignment;
            section->header.sh_addr = rwSegment[0]->header.sh_addr;

            for (auto cSect: rwSegment) {
                ensureAlignment(segment->header.p_memsz, cSect->header.sh_addralign);
                ensureAlignment(segment->header.p_filesz, cSect->header.sh_addralign);
                segment->header.p_filesz += cSect->header.sh_size;
                segment->header.p_memsz  += cSect->header.sh_size;

                ensureAlignment(section->header.sh_size, cSect->header.sh_addralign);

                section->header.sh_size += cSect->header.sh_size;
            }
            fileOffset += segment->header.p_filesz;
        }

        if (zeSegment.size() > 0) {
            auto section = outFile->addSection(SHT_NOBITS, nullSection, nullSection);
            section->name = ".bss";
            section->componentSections = zeSegment;
            section->header.sh_addr = zeSegment[0]->header.sh_addr;

            for (auto cSect: zeSegment) {

                ensureAlignment(segment->header.p_memsz, cSect->header.sh_addralign);
                segment->header.p_memsz += cSect->header.sh_size;

                ensureAlignment(section->header.sh_size, cSect->header.sh_addralign);
                section->header.sh_size += cSect->header.sh_size;
            }
            fileOffset += segment->header.p_filesz;
        }
    }

    return true;
}

bool Linker::writeOutputFile(bool outputRaw) {
    outFile->generateSectionNameStrings();
    outFile->generateSectionData();

    return outFile->write(fileName);
}

}; // namespace ldnp
