#include <iostream>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <vector>
#include <filesystem>
#include <ctype.h>

#include "error.h"
#include "assemble.h"

#include "../libelf/elf.h"

namespace asnp {

Assembler::Assembler(std::string out)
  :outFile(out) {
}
Assembler::~Assembler() {}

#define LENGTH_LINEBUFFER 256
bool Assembler::assemble(std::string inDir, std::string inFile) {
    if (inFile.empty()) {
        std::cerr << "Could not open input file '" << inFile << "'. Aborting." << std::endl;
        return false;
    }
    if (inFile.front() != '/') {
        inFile = inDir + inFile;
    }

    std::filesystem::path filePath(inFile);
    std::string directory = filePath.parent_path();

    if (!directory.empty() && directory.back() != '/') {
        directory += '/';
    }

    std::ifstream reader(inFile);

    if (!reader.is_open()) {
        std::cerr << "Could not open input file '" << inFile << "'. Aborting." << std::endl;
        return false;
    }

    try {
        currentLine = 1;

        char lineBuffer[LENGTH_LINEBUFFER];
        while (!reader.eof()) {
            line = "";
            reader.getline(lineBuffer, LENGTH_LINEBUFFER);
            line.append(lineBuffer);
            while (reader.fail() && !reader.eof()) {
                reader.getline(lineBuffer, LENGTH_LINEBUFFER);
            }

            tokenize(line);

            lineState = LabelState;

            while (!tokens.empty()) {
                Token token = tokens.front();
                tokens.pop_front();

                if (lineState == LabelState) {
                    if (token.type == Directive) {
                        processDirective(token, directory);
                        lineState = DoneState;
                    }
                    else if (!segment.get()) {
                        throw new SyntaxError("unexpected token '" + token.content + "'", token);
                    }
                    else if (token.type == Label) {
                        processLabel(token);
                        lineState = ActionState;
                    }
                    else if (token.type == Identifier) {
                        processInstruction(token);
                        lineState = DoneState;
                    }
                    else {
                        throw new SyntaxError("unexpected token '" + token.content + "'", token);
                    }
                }
                else if (lineState == ActionState) {
                    if (token.type == Directive) {
                        processDirective(token, directory);
                        lineState = DoneState;
                    }
                    else if (!segment.get()) {
                        throw new SyntaxError("unexpected token '" + token.content + "'", token);
                    }
                    else if (token.type == Identifier) {
                        processInstruction(token);
                        lineState = DoneState;
                    }
                    else {
                        throw new SyntaxError("unexpected token '" + token.content + "'", token);
                    }
                }
                else {
                    if (token.content.length() > 0 && token.content[0] == '"') {
                        throw new SyntaxError("unexpected string " + token.content, token);
                    }
                    else {
                        throw new SyntaxError("unexpected token '" + token.content + "'", token);
                    }
                }
            }

            currentLine++;
        }
    }
    catch (CodeError *e) {
        std::cerr << "[" << inFile << ":" << currentLine << "] ";
        std::cerr << e->type << ": " << e->message;
        std::cerr << " at char " << (e->token.character + 1) << ":" << std::endl;
        std::cerr << line << std::endl;
        std::string blanks(e->token.character, ' ');
        std::cerr << blanks << '^' << std::endl;

        return false;
    }
    catch (AssemblyError *e) {
        std::cerr << "[" << inFile << ":" << currentLine << "] ";
        std::cerr << e->type << ": " << e->message << std::endl;

        return false;
    }

    return true;
}

bool Assembler::link(bool outputSymbols, bool forbidExternalSymbols) {
    try {
        std::cout << "Resolving references" << std::endl;
        auto references = processReferences(!forbidExternalSymbols);

        if (outputSymbols) {
            std::cout << "Writing symbols" << std::endl;
            auto symbolOut = std::ofstream(outFile + ".sym", std::ios::binary);
            for (auto reference: references) {
                symbolOut << "0x" << std::setw(8) << std::setfill('0') << std::hex << reference.second << " " << reference.first << std::endl;
            }
        }
    }
    catch (AssemblyError *e) {
        std::cerr << e->type << ": " << e->message << std::endl;

        return false;
    }

    return true;
}

bool Assembler::write(bool raw) {
    std::cout << "Writing data" << std::endl;
    if (raw) {
        // output unadorned machine code
        auto out = std::ofstream(outFile, std::ios::binary);

        for (auto seg: segments) {
            if (seg.second->ephemeral) {
                continue;
            }
            out.write(seg.second->getData(), seg.second->getSize());
        }
    }
    else {
        // output elf

        libelf::ElfFile file(ET_REL);

        std::shared_ptr<libelf::Section> nullSection;
        
        auto symbolSection = file.addSection(SHT_SYMTAB, nullSection, nullSection);
        symbolSection->name = ".symtab";

        int usedSegmentCount = 0;
        for (auto seg: segments) {
            auto segment = seg.second;

            if (!usedSegments.contains(segment->name)) {
                continue;
            }
            usedSegmentCount++;

            Elf32_Word sectionType;
            if (segment->ephemeral) {
                sectionType = SHT_NOBITS;
            }
            else {
                sectionType = SHT_PROGBITS;
            }

            Elf32_Word sectionFlags = SHF_ALLOC;
            if (!segment->readOnly) {
                sectionFlags |= SHF_WRITE;
            }
            if (segment->executable) {
                sectionFlags |= SHF_EXECINSTR;
            }

            auto section = file.addSection(sectionType, nullSection, nullSection);
            section->name = "." + segment->name;
            section->header.sh_flags = sectionFlags;
            section->header.sh_addralign = segment->align;
            section->header.sh_addr = segment->start;

            if (!segment->ephemeral) {
                section->data = segment->getData(true);
                section->header.sh_size = segment->getSize();
            }
            else {
                section->header.sh_size = segment->getOffset();
            }

            auto labels = segment->getLabels();
            for (auto label: labels) {
                // all symbols are globals right now...
                symbolSection->addSymbol(section, label.first, label.second);
            }

            if (segment->getReferences().size() == 0) {
                continue;
            }

            auto relocSection = file.addSection(SHT_REL, symbolSection, section);
            relocSection->header.sh_info = section->index;

            for (auto reference: segment->getReferences()) {
                if (!labels.contains(reference.label)) {
                    // create new undefined symbol
                    symbolSection->addSymbol(nullSection, reference.label, 0);
                }
                auto symbol = symbolSection->symbolMap[reference.label];
                relocSection->addRelocation(section, symbol, reference.offset, reference.type);
            }
        }

        if (architecture->pageSize > 0) {
            auto pageSizeSection = file.addSection(SHT_LOPROC, nullSection, nullSection);
            pageSizeSection->name = ".pagesize";
            pageSizeSection->header.sh_addr= architecture->pageSize;
        }

        file.generateSymbolStrings(symbolSection);
        file.generateSectionNameStrings();
        file.generateSectionData();

        file.write(outFile);
    }
    return true;
}

bool _eat_whitespace(std::string line, int &index) {
    while (isspace(line[index])) {
        index++;
        if (index >= line.length()) {
            return false;
        }
    }
    if (index >= line.length()) {
        return false;
    }
    return true;
}
std::string _read_word(std::string line, int &index) {
    int start = index;
    while (!isspace(line[index]) &&
            line[index] != ',' && line[index] != ';' &&
            line[index] != '(' && line[index] != ')' &&
            line[index] != '"') {
        index++;
        if (index >= line.length()) {
            break;
        }
        if (line[index] == ':') {
            index++;
            break;
        }
    }
    return line.substr(start, index - start);
}
std::string _read_punctuator(std::string line, int &index) {
    return line.substr(index++, 1);
}
std::string _read_string(std::string line, int &index) {
    int start = index;
    index++;

    while (index < line.length()) {
        if (line[index] == '\\') {
            index++;
        }
        else if (line[index] == '"') {
            index++;
            break;
        }
        index++;
    }

    return line.substr(start, index - start);
}

void Assembler::tokenize(std::string line) {
    int current = 0;
    if (!_eat_whitespace(line, current)) {
        return;
    }

    tokens.clear();

    while (current < line.length()) {
        int tokenStart = current;
        if (line[current] == ';') {
            // Rest of the line is a comment
            break;
        }

        std::string tokenString;
        if (line[current] == '"') {
            tokenString = _read_string(line, current);
        }
        else if (line[current] == ',' || line[current] == '(' || line[current] == ')') {
            tokenString = _read_punctuator(line, current);
        }
        else {
            tokenString = _read_word(line, current);
        }

        if (tokenString.length() > 0) {
            Token token(tokenString, tokenStart);
            tokens.push_back(token);
        }

        _eat_whitespace(line, current);
    }
}

void Assembler::processDirective(Token &token, std::string directory) {
    if (token.content == ".arch") {
        if (architecture) {
            throw new SyntaxError("cannot redefine architecture", token);
        }

        if (tokens.empty()) {
            throw new SyntaxError("missing architecture", token);
        }
            
        Token archArg = tokens.front();
        tokens.pop_front();
        
        architecture = std::make_unique<arch::Arch>(archArg.content);

        for (auto seg: architecture->segments) {
            segments[seg.first] = std::make_shared<Segment>(seg.second);
        }
    }
    else if (!architecture) {
        throw new SyntaxError("architecture not defined", token);
    }
    else if (token.content == ".org" || token.content == ".origin") {
        if (tokens.empty()) {
            throw new SyntaxError("missing argument for directive '" + token.content + "'", token);
        }

        Token directiveArg = tokens.front();
        tokens.pop_front();

        if (directiveArg.type != TokenType::Number) {
            throw new SyntaxError("unexpected token '" + directiveArg.content + "'", directiveArg);
        }
        *segment = directiveArg.parseNumber(32, 0,  NumberSign::ForceUnsigned);
    }
    else if (token.content == ".segment" || token.content == ".data" || token.content == ".text" || token.content == ".rodata" || token.content == ".bss") {
        std::string segmentName;

        if (token.content == ".segment") {
            Token newSegment = tokens.front();
            tokens.pop_front();

            if (!segments.contains(newSegment.content)) {
                throw new SyntaxError("unrecognized segment '" + newSegment.content + "'", newSegment);
            }
            segmentName = newSegment.content;
        }
        else {
            segmentName = token.content.substr(1);
        }

        segment = segments[segmentName];
        if (!usedSegments.contains(segmentName)) {
            usedSegments.insert(segmentName);
        }
    }
    else if (token.content == ".byte" || token.content == ".word" || token.content == ".dword") {
        uint32_t value = 0;
        int width = token.content == ".byte" ? 8 : (token.content == ".word" ? 16 : 32);
        int bytes = width >> 1;
        if (!tokens.empty()) {
            Token directiveArg = tokens.front();
            tokens.pop_front();

            if (directiveArg.type != TokenType::Number) {
                throw new SyntaxError("unexpected token '" + directiveArg.content + "'", directiveArg);
            }

            value = (uint32_t) directiveArg.parseNumber(width, 0, NumberSign::AllowSigned);
        }

        *segment += (uint8_t) (value & 0xff);
        if (width > 8) {
            *segment += (uint8_t) ((value >> 8) & 0xff);
            if (width > 16) {
                *segment += (uint8_t) ((value >> 16) & 0xff);
                *segment += (uint8_t) ((value >> 24) & 0xff);
            }
        }
    }
    else if (token.content == ".string" || token.content == ".stringz") {
        Token directiveArg = tokens.front();
        tokens.pop_front();

        if (directiveArg.type != TokenType::String) {
            throw new SyntaxError("unexpected token '" + directiveArg.content + "'", directiveArg);
        }
        if (directiveArg.error) {
            throw new SyntaxError("unterminated string '" + directiveArg.content + "'", directiveArg);
        }

        for (int i = 1; i < directiveArg.content.length() - 1; i++) {
            uint8_t character = directiveArg.content[i];
            if (character == '\\') {
                i++;
                character = directiveArg.content[i];
                if (std::isdigit(character)) {
                    character = character - '0';
                }
                else {
                    switch (character) {
                        case 'a':
                            character = '\a';
                            break;
                        case 'b':
                            character = '\b';
                            break;
                        case 'f':
                            character = '\f';
                            break;
                        case 'n':
                            character = '\n';
                            break;
                        case 'r':
                            character = '\r';
                            break;
                        case 't':
                            character = '\t';
                            break;
                        case 'v':
                            character = '\v';
                            break;
                        case '\\':
                        case '\'':
                        case '"':
                            break;
                        default:
                            break;
                    }
                }
            }

            *segment += character;
        }
        if (token.content == ".stringz") {
            *segment += (uint8_t) 0;
        }
    }
    else if (token.content == ".include") {
        Token directiveArg = tokens.front();
        tokens.pop_front();

        if (directiveArg.type != TokenType::String) {
            throw new SyntaxError("unexpected token '" + directiveArg.content + "'", directiveArg);
        }
        if (directiveArg.error) {
            throw new SyntaxError("unterminated string '" + directiveArg.content + "'", directiveArg);
        }

        if (!tokens.empty()) {
            directiveArg = tokens.front();
            throw new SyntaxError("unexpected token '" + directiveArg.content + "'", directiveArg);
        }

        lineStack.push_back(currentLine);
        if (!assemble(directory, directiveArg.content.substr(1, directiveArg.content.length() - 2))) {
            currentLine = lineStack.back();
            lineStack.pop_back();
            throw new NestedError("error(s) encountered in file included on line " + std::to_string(currentLine));
        }
        currentLine = lineStack.back();
        lineStack.pop_back();
    }
    else {
        throw new SyntaxError("unrecognized directive '" + token.content + "'", token);
    }
}
void Assembler::processInstruction(Token &token) {
    if (!architecture->instructions.contains(token.content)) {
        throw new SyntaxError("unexpected identifier '" + token.content + "'", token);
    }

    auto fragments = architecture->fragments;
    auto relocations = architecture->relocations;

    std::vector<InstructionCandidate> candidates;
    for (auto option: architecture->instructions[token.content]) {
        if (option.fragments.size() != tokens.size()) {
            continue;
        }

        InstructionCandidate candidate;
        candidate.matchedTokens = -1;
        candidate.instruction = option;
        candidates.push_back(candidate);
    }

    int tokenCount = tokens.size();
    for (int t = 0; t < tokenCount; t++) {
        auto token = tokens.front();
        tokens.pop_front();
        std::string content = token.content;

        for (int c = 0; c < candidates.size(); c++) {
            auto candidate = candidates[c];
            if (candidate.matchedTokens >= 0) {
                continue;
            }

            auto fragment = candidate.instruction.fragments[t];

            bool needPunctuator = false;
            if (fragment[0] == ':') {
                fragment = fragment.substr(1);
                needPunctuator = true;
            }

            try {
                if (needPunctuator) {
                    if (token.type != TokenType::Punctuator) {
                        throw new SyntaxError("unexpected token '" + content + "'. Expecting '" + fragment + "'", token);
                    }
                    if (token.content != fragment) {
                        throw new SyntaxError("unexpected punctuator '" + content + "'. Expecting '" + fragment + "'", token);
                    }
                    // Punctuator matches. Next token.
                    continue;
                }

                uint32_t value = 0;
                auto seg = fragments[fragment];
                if (seg.type == "address" || seg.type == "raddress") {
                    if (token.type == TokenType::Number) {
                        NumberSign sign = seg.type == "address" ? NumberSign::ForceUnsigned : NumberSign::ForceSigned;
                        value = token.parseNumber(seg.width, 0, sign);
                    }
                    else if (token.type == TokenType::Identifier) {
                        value = 0;
                        PendingReference pendingReference;
                        pendingReference.label = content;
                        pendingReference.shift = seg.alignment - 1;
                        if (!seg.relocation.empty()) {
                            pendingReference.relocation = seg.relocation;
                        }
                        candidates[c].pendingReferences[fragment] = pendingReference;
                    }
                    else {
                        throw new SyntaxError("unexpected token '" + content + "'. Expecting '" + fragment + "'", token);
                    }
                }
                else if (seg.type == "reg") {
                    if (token.type != TokenType::Identifier) {
                        throw new SyntaxError("unexpected token '" + content + "'. Expecting '" + fragment + "'", token);
                    }
                    if (content[0] != '$') {
                        throw new SyntaxError("unexpected token '" + content + "'. Expecting '" + fragment + "'", token);
                    }

                    value = token.parseNumber(seg.width, seg.offset, NumberSign::ForceUnsigned, 1);
                }
                else if (seg.type == "signed" || seg.type == "unsigned") {
                    if (token.type != TokenType::Number) {
                        throw new SyntaxError("unexpected token '" + content + "'. Expecting '" + fragment + "'", token);
                    }

                    NumberSign sign = seg.type == "signed" ? NumberSign::ForceSigned : NumberSign::ForceUnsigned;
                    value = token.parseNumber(seg.width, seg.offset, sign);
                }

                if (seg.alignment > 1) {
                    uint32_t mask = (1 << (seg.alignment - 1)) - 1;
                    if (value & mask) {
                        throw new SyntaxError("number must be divisible by " + std::to_string(1 << (seg.alignment - 1)), token);
                    }
                }

                if (seg.owidth < seg.width) {
                    value >>= (seg.width - seg.owidth);
                }
                else if (seg.owidth > seg.width && !seg.rightAlign) {
                    value <<= (seg.owidth - seg.width);
                }

                if (seg.group != "") {
                    //candidates[c].values[seg.group] = value;
                    fragment = seg.group;
                }
                candidates[c].values[fragment] = value;
            }
            catch (SyntaxError *e) {
                candidates[c].error = e;
                candidates[c].matchedTokens = t;
            }
        }
    }

    int maxMatchedTokens = -1;
    auto error = new SyntaxError("unresolved instruction variant", token);
    for (auto candidate: candidates) {
        if (candidate.matchedTokens >= 0) {
            if (candidate.matchedTokens > maxMatchedTokens) {
                maxMatchedTokens = candidate.matchedTokens;
                error = candidate.error;
            }
            continue;
        }

        std::list<InstructionCandidate> options;
        if (candidate.instruction.format == "composite") {
            for (auto component: candidate.instruction.components) {
                InstructionCandidate componentInstruction;
                componentInstruction.instruction        = architecture->indexedInstructions[component.id];
                componentInstruction.values             = candidate.values;
                componentInstruction.pendingReferences  = candidate.pendingReferences;

                for (auto replacement: component.replacements) {
                    if (candidate.pendingReferences.contains(replacement.source)) {
                        componentInstruction.values[replacement.dest] = candidate.values[replacement.source];
                        auto newReference = candidate.pendingReferences[replacement.source];
                        newReference.shift = replacement.shift;
                        if (!replacement.relocation.empty()) {
                            newReference.relocation = replacement.relocation;
                        }
                        componentInstruction.pendingReferences[replacement.dest] = newReference;
                    }
                    else {
                        componentInstruction.values[replacement.dest] = candidate.values[replacement.source] >> replacement.shift;
                    }
                }
                options.push_back(componentInstruction);
            }
        }
        else {
            options.push_back(candidate);
        }

        for (auto candidate: options) {
            auto option = candidate.instruction;

            auto format = architecture->formats[option.format];
            auto values = candidate.values;
            auto pendingReferences = candidate.pendingReferences;

            int instructionWidth = format.width / 8;

            if (!segment->canPlace(instructionWidth)) {
                throw new SyntaxError("segment size exceeded", token);
            }

            // Pack instruction fragments into bytes
            uint8_t buffer[16];
            std::fill(std::begin(buffer), std::end(buffer), 0);
            uint32_t startingOffset = segment->getOffset();
            int bit = 0;
int totalWidth = 0;
            for (auto fragmentName: format.fragments) {
                uint32_t value;
                if (values.contains(fragmentName)) {
                    value = values[fragmentName];
                }
                else if (option.defaults.contains(fragmentName)) {
                    auto defaultValue = option.defaults[fragmentName];
                    if (defaultValue == "%next%") {
                        value = segment->getNext(instructionWidth);
                    }
                    else {
                        value = std::stoi(defaultValue);
                    }
                }
                else {
                    continue;
                }

                auto fragment = fragments[fragmentName];
                if (pendingReferences.contains(fragmentName)) {
                    auto pendingReference = pendingReferences[fragmentName];

                    uint8_t relocationType = 0;
                    if (!pendingReference.relocation.empty()) {
                        relocationType = relocations[pendingReference.relocation].type;
                    }
                    Reference reference;
                    reference.label  = pendingReference.label;
                    reference.offset = segment->getOffset();
                    reference.bit    = bit;
                    reference.width  = fragment.owidth;
                    reference.shift  = pendingReference.shift;
                    reference.type = relocationType;
                    reference.relative = fragment.type == "raddress" ? startingOffset : 0;
                    if (bit != 0) {
                        reference.offset -= 1;
                    }

                    segment->addReference(reference);
                }

                segment->pack(value, fragment.owidth, Segment::UNDEFINED_OFFSET, bit);
            }
        }

        return;
    }

    throw error;
}
void Assembler::processLabel(Token &token) {
    if (labels.contains(token.content)) {
        throw new SyntaxError("duplicate label '" + token.content + "'", token);
    }

    labels[token.content] = segment;

    segment->addLabel(token.content);
}

std::map<std::string, uint32_t> Assembler::processReferences(bool onlyRelative) {
    std::map<std::string, uint32_t> symbols;
    for (auto segment: segments) {
        for (auto reference: segment.second->getReferences()) {
            std::string label = reference.label;

            if (!labels.contains(label)) {
                if (onlyRelative) {
                    continue;
                }

                throw new ReferenceError("undefined reference to '" + label + "'");
            }

            auto labelSegment = labels[label];
            uint32_t value = labelSegment->getLabelOffset(label) + labelSegment->getStartAddress();

            int bit = reference.bit;

            uint32_t modifiedValue = value;
            if (reference.relative != 0) {
                modifiedValue = labelSegment->getLabelOffset(label) - reference.relative;
            }
            else if (onlyRelative) {
                // if it's not relative, and that's what we're doing, skip it.
                continue;
            }

            symbols[label] = value;

            if (reference.shift > 0) {
                modifiedValue >>= reference.shift;
            }

            segment.second->pack(modifiedValue, reference.width, reference.offset, bit);
        }
    }

    return symbols;
}

}; // namespace asnp
