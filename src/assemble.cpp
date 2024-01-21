#include <iostream>
#include <iomanip>
#include <fstream>
#include <bit>
#include <algorithm>
#include <vector>
#include <filesystem>
#include <ctype.h>

#include "error.h"
#include "assemble.h"

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

bool Assembler::link() {
    try {
        std::cout << "Resolving references" << std::endl;
        processReferences();

        std::cout << "Writing data" << std::endl;
        auto out = std::ofstream(outFile, std::ios::binary);
        for (auto seg: segments) {
            if (seg.second->ephemeral) {
                continue;
            }
            out.write((const char *) (*seg.second), seg.second->getSize());
        }
    }
    catch (AssemblyError *e) {
        std::cerr << e->type << ": " << e->message << std::endl;

        return false;
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
    else if (token.content == ".segment") {
        Token newSegment = tokens.front();
        tokens.pop_front();

        if (!segments.contains(newSegment.content)) {
            throw new SyntaxError("unrecognized segment '" + newSegment.content + "'", newSegment);
        }
        segment = segments[newSegment.content];
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
                        candidates[c].pendingReferences[fragment] = content;
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
                    value = token.parseNumber(seg.width, 0, sign);
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
                else if (seg.owidth > seg.width) {
                    value <<= (seg.owidth - seg.width);
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

        auto values = candidate.values;
        auto pendingReferences = candidate.pendingReferences;
        auto option = candidate.instruction;

        auto format = architecture->formats[option.format];
        int instructionWidth = format.width / 8;

        if (!segment->canPlace(instructionWidth)) {
            throw new SyntaxError("text fragment size exceeded", token);
        }

        // Pack instruction fragments into bytes
        uint8_t buffer[16];
        std::fill(std::begin(buffer), std::end(buffer), 0);
        uint32_t startingOffset = segment->getOffset();
        int bit = 0;
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
                std::string label = pendingReferences[fragmentName];
                Reference reference;
                reference.label = label;
                reference.offset = segment->getOffset() - 1;
                reference.bit    = bit;
                reference.width  = fragment.owidth;
                reference.shift  = fragment.alignment - 1;
                reference.relative = fragment.type == "raddress" ? startingOffset : 0;

                *segment += reference;
            }

            segment->pack(value, fragment.owidth, Segment::UNDEFINED_OFFSET, bit);
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

    *segment += token.content;
}

void Assembler::processReferences() {
    for (auto segment: segments) {
        for (auto reference: segment.second->getReferences()) {
            std::string label = reference.label;

            if (!labels.contains(label)) {
                throw new ReferenceError("undefined reference to '" + label + "'");
            }

            auto labelSegment = labels[label];
            uint32_t value = (*labelSegment)[label] + (uint32_t) *labelSegment;

            int bit = reference.bit;

            uint32_t modifiedValue = value;
            if (reference.relative != 0) {
                modifiedValue = (*labelSegment)[label] - reference.relative;
            }

            if (reference.shift > 0) {
                modifiedValue >>= reference.shift;
            }

            segment.second->pack(modifiedValue, reference.width, reference.offset, bit);
        }
    }
}

Token::Token(std::string token, int start): content(token), character(start), error(false) {
    auto ch = token.front();

    if (ch== '.') {
        type = Directive;
    }
    else if (ch == '"') {
        type = String;
        if (token.back() != '"' || token[token.length() - 2] == '\\') {
            error = true;
        }
    }
    else if (ch == '-' || isdigit(ch)) {
        type = Number;
    }
    else if (ch == '_' || ch == '$' || isalpha(ch)) {
        type = Identifier;
    }
    else if (ch == ',' || ch == '(' || ch == ')') {
        type = Punctuator;
    }
    else {
        type = Unknown;
    }

    if (token.back() == ':') {
        if (type == Identifier) {
            type = Label;
            content = token.substr(0,token.length() - 1);
        }
        else {
            error = true;
        }
    }
}

uint32_t Token::parseNumber(int maxBits, int subtract, NumberSign sign, int skip) {
    auto str = content.substr(skip);
    if (str == "0" || str == "-0") {
        uint32_t value = 0 - subtract;
        int bitsCount = 32 - std::countl_zero(value);
        if (bitsCount > maxBits) {
            // actually out of bits range
            throw new SyntaxError("number out of range", *this);
        }
        if (sign != NumberSign::ForceUnsigned && (value) > 1 << (maxBits - 1)) {
            // negative values < -128 (for 8 bits)
            throw new SyntaxError("number out of range", *this);
        }
        if (sign == NumberSign::ForceSigned && (value) > (1 << (maxBits - 1)) - 1) {
            // signed values > 127 (for 8 bits)
            throw new SyntaxError("number out of range", *this);
        }
        return value - subtract;
    }

    int offset = 0;
    int base = 10;
    bool negative = false;
    if (str[0] == '-') {
        negative = true;
        offset++;
        if (sign == NumberSign::ForceUnsigned) {
            throw new SyntaxError("number out of range", *this);
        }
    }
    else if (str[0] == '0') {
        offset++;
        if (str[1] == 'x' || str[1] == 'X') {
            offset++;
            base = 16;
        }
        else if (str[1] == 'b' || str[1] == 'B') {
            offset++;
            base = 2;
        }
        else {
            base = 8;
        }
    }
    str = str.substr(offset);

    uint32_t value = 0;
    int bitsCount = 0;
    for (int i = 0; i < str.length(); i++) {
        char ch = str[i];
        if (ch == '_') {
            continue;
        }
        if (base == 16) {
            value <<= 4;
            if (ch >= '0' && ch <= '9') {
                value += ch - '0';
            }
            else if (ch >= 'A' && ch <= 'F') {
                value += ch - 'A' + 10;
            }
            else if (ch >= 'a' && ch <= 'f') {
                value += ch - 'a' + 10;
            }
            else {
                throw new ParseError("malformed number", *this);
            }
        }
        else if (base == 10) {
            value *= 10;
            if (ch >= '0' && ch <= '9') {
                value += ch - '0';
            }
            else {
                throw new ParseError("malformed number", *this);
            }
        }
        else if (base == 8) {
            value <<= 3;
            if (ch >= '0' && ch <= '7') {
                value += ch - '0';
            }
            else {
                throw new ParseError("malformed number", *this);
            }
        }
        else {
            value <<= 1;
            value += ch - '0';
            if (ch != '0' && ch != '1') {
                throw new ParseError("malformed number", *this);
            }
        }

        bitsCount = 32 - std::countl_zero(value);
        if (bitsCount >= 32) {
            // we exit out if we hit the limit...
            if (i < str.length() - 1) {
                throw new SyntaxError("malformed number ", *this);
            }
            break;
        }
    }

    if (sign == NumberSign::ForceUnsigned) {
        if (negative) {
            throw new SyntaxError("number out of range0", *this);
        }

        value -= subtract;
        bitsCount = 32 - std::countl_zero(value);
    }

    if (bitsCount > maxBits) {
        // actually out of bits range
        throw new SyntaxError("number out of range1", *this);
    }
    if (sign != NumberSign::ForceUnsigned && negative && value > 1 << (maxBits - 1)) {
        // negative values < -128 (for 8 bits)
        throw new SyntaxError("number out of range2", *this);
    }
    if (sign == NumberSign::ForceSigned && !negative && value > (1 << (maxBits - 1)) - 1) {
        // signed values > 127 (for 8 bits)
        throw new SyntaxError("number out of range3", *this);
    }

    if (bitsCount == 0) {
        //throw new ParseError("malformed number", *this);
    }

    if (negative) {
        value = ~value;
        value += 1;
    }

    return value;
}


}; // namespace asnp
