#include <iostream>
#include <iomanip>
#include <fstream>
#include <bit>
#include <ctype.h>

#include "error.h"
#include "assemble.h"

namespace asnp {

Assembler::Assembler(std::string in, std::string out)
  :inFile(in),outFile(out) {
}
Assembler::~Assembler() {
}

#define LENGTH_LINEBUFFER 256
bool Assembler::assemble() {
    std::ifstream reader(inFile);

    if (!reader.is_open()) {
        std::cerr << "Could not open input file '" << inFile << "'. Aborting." << std::endl;
        return false;
    }

    try {
        currentLine = 1;
        section = NoSection;

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
                        processDirective(token);
                        lineState = DoneState;
                    }
                    else if (section == NoSection) {
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
                        processDirective(token);
                        lineState = DoneState;
                    }
                    else if (section == NoSection) {
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
                    throw new SyntaxError("unexpected token '" + token.content + "'", token);
                }
            }

            currentLine++;
        }

        if (dataOffset > 0) {
            dataStart = textStart + textOffset;
        }

        processReferences();

        auto out = std::ofstream(outFile, std::ios::binary);
        out.write((const char *) text, textOffset);
        if (dataOffset > 0) {
            out.write((const char *) data, dataOffset);
        }
        //std::ofstream(outFile, std::ios::binary).write((const char *) text, textOffset);
    }
    catch (CodeError *e) {
        std::cerr << e->type << ": " << e->message;
        std::cerr << " on line " << currentLine << " at char " << (e->token.character + 1) << ":" << std::endl;
        std::cerr << line << std::endl;
        std::string blanks(e->token.character, ' ');
        std::cerr << blanks << '^' << std::endl;
    }
    catch (AssemblyError *e) {
        std::cerr << e->type << ": " << e->message << std::endl;
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
    std::string word;
    int start = index;
    while (!isspace(line[index]) &&
            line[index] != ',' && line[index] != ';' &&
            line[index] != '(' && line[index] != ')') {
        index++;
        if (index >= line.length()) {
            break;
        }
        if (line[index] == ':') {
            index++;
            break;
        }
    }
    word = line.substr(start, index - start);
    _eat_whitespace(line, index);

    return word;
}
std::string _read_punctuator(std::string line, int &index) {
    return line.substr(index++, 1);
}

void _pack_bits(uint8_t *buffer, uint32_t value, int width, int& byte, int& bit) {
    do {
        int bitsForByte = 8 - bit;
        uint32_t mask = 0xff;
        uint32_t val = value;
        if (bitsForByte > width) {
            mask >>= (8 - width);
            val &= mask;
            val <<= (bitsForByte - width);
            bit += width;
            width = 0;
        }
        else {
            val = (val >> (width - bitsForByte)) & mask;
            width -= bitsForByte;
            bit += bitsForByte;
        }
        buffer[byte] |= (uint8_t) val;

        if (bit >= 8) { // shouldn't ever be > 8, but meh
            bit = 0;
            byte++;
        }
    } while (width > 0);
}

void Assembler::tokenize(std::string line) {
    int current = 0;
    if (!_eat_whitespace(line, current)) {
        return;
    }

    tokens.clear();

    while (true && current < line.length()) {
        int tokenStart = current;
        std::string word = _read_word(line, current);
        if (word.length() > 0) {
            Token identifier(word, tokenStart);
            tokens.push_back(identifier);
        }

        if (line[current] == ',' || line[current] == '(' || line[current] == ')') {
            tokenStart = current;
            Token punctuator(_read_punctuator(line, current), tokenStart);
            tokens.push_back(punctuator);
        }
        if (line[current] == ';') {
            // Rest of the line is a comment
            break;
        }
    }
}

void Assembler::processDirective(Token &token) {
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
        textStart = architecture->entryPoint;
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
        uint32_t value = directiveArg.parseNumber(32, NumberSign::ForceUnsigned);

        if (section == DataSection) {
            throw new SyntaxError("data segment may not be manually located (for now)", directiveArg);
        }
        else {
            if (textOffset == 0) {
                textStart = value;
            }
            else if (value < textStart) {
                throw new SyntaxError("invalid offset '" + directiveArg.content + "'", directiveArg);
            }
            else if (value - textStart > SECTION_MAX_SIZE) {
                throw new SyntaxError("text segment size exceeded", directiveArg);
            }
            else {
                textOffset = value - textStart;
            }
        }
    }
    else if (token.content == ".data") {
        section = DataSection;
    }
    else if (token.content == ".text") {
        section = TextSection;
    }
    else if (token.content == ".byte" || token.content == ".word") {
        uint32_t value = 0;
        int width = token.content == ".byte" ? 8 : 16;
        int bytes = width >> 1;
        if (!tokens.empty()) {
            Token directiveArg = tokens.front();
            tokens.pop_front();

            if (directiveArg.type != TokenType::Number) {
                throw new SyntaxError("unexpected token '" + directiveArg.content + "'", directiveArg);
            }

            value = (uint16_t) directiveArg.parseNumber(width, NumberSign::AllowSigned);
        }

        if (section == DataSection) {
            if (dataOffset + bytes >= SECTION_MAX_SIZE) {
                throw new SyntaxError("data segment size exceeded", token);
            }

            data[dataOffset++] = (uint8_t) (value & 0xff);
            if (width > 8) {
                data[dataOffset++] = (uint8_t) ((value >> 8) & 0xff);
            }
        }
        else {
            if (textOffset + bytes >= SECTION_MAX_SIZE) {
                throw new SyntaxError("text segment size exceeded", token);
            }

            text[textOffset++] = (uint8_t) (value & 0xff);
            if (width > 8) {
                text[textOffset++] = (uint8_t) ((value >> 8) & 0xff);
            }
        }
    }
    else {
        throw new SyntaxError("unrecognized directive '" + token.content + "'", token);
    }
}
void Assembler::processInstruction(Token &token) {
    if (!architecture->instructions.contains(token.content)) {
        throw new SyntaxError("unexpected identifier '" + token.content + "'", token);
    }

    auto segments = architecture->segments;
    auto options = architecture->instructions[token.content];

    for (auto option: options) {
        if (option.segments.size() != tokens.size()) {
            continue;
        }

        std::map<std::string, uint32_t> values;
        std::map<std::string, std::string> pendingReferences;

        for (std::string segment: option.segments) {
            bool needPunctuator = false;
            if (segment[0] == ':') {
                segment = segment.substr(1);
                needPunctuator = true;
            }
            Token token = tokens.front();
            std::string content = token.content;
            tokens.pop_front();

            if (needPunctuator) {
                if (token.type != TokenType::Punctuator) {
                    throw new SyntaxError("unexpected token '" + content + "'. Expecting '" + segment + "'", token);
                }
                if (token.content != segment) {
                    throw new SyntaxError("unexpected punctuator '" + content + "'. Expecting '" + segment + "'", token);
                }
                // Punctuator matches. Next token.
                continue;
            }

            uint32_t value = 0;
            arch::Segment seg = segments[segment];
            if (seg.type == "address" || seg.type == "raddress") {
                if (token.type == TokenType::Number) {
                    NumberSign sign = seg.type == "address" ? NumberSign::ForceUnsigned : NumberSign::ForceSigned;
                    value = token.parseNumber(seg.width, sign);
                }
                else if (token.type == TokenType::Identifier) {
                    value = 0;
                    pendingReferences[segment] = content;
                }
                else {
                    throw new SyntaxError("unexpected token '" + content + "'. Expecting '" + segment + "'", token);
                }
            }
            else if (seg.type == "reg") {
                if (token.type != TokenType::Identifier) {
                    throw new SyntaxError("unexpected token '" + token.content + "'. Expecting '" + segment + "'", token);
                }
                if (content[0] != '$') {
                    throw new SyntaxError("unexpected token '" + token.content + "'. Expecting '" + segment + "'", token);
                }

                value = token.parseNumber(seg.width, NumberSign::ForceUnsigned, 1);
            }
            else if (seg.type == "signed" || seg.type == "unsigned") {
                if (token.type != TokenType::Number) {
                    throw new SyntaxError("unexpected token '" + token.content + "'. Expecting '" + segment + "'", token);
                }

                NumberSign sign = seg.type == "signed" ? NumberSign::ForceSigned : NumberSign::ForceUnsigned;
                value = token.parseNumber(seg.width, sign);
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

            values[segment] = value;
        }
        if (tokens.size() > 0) {
            throw new SyntaxError("trailing token '" + token.content + "'", tokens.front());
        }

        auto format = architecture->formats[option.format];
        int instructionWidth = format.width / 8;

        if (instructionWidth + textOffset >= SECTION_MAX_SIZE) {
            throw new SyntaxError("text segment size exceeded", token);
        }

        // Pack instruction segments into bytes
        uint8_t buffer[16];
        std::fill(std::begin(buffer), std::end(buffer), 0);
        int byte = 0;
        int bit = 0;
        for (auto segmentName: format.segments) {
            uint32_t value;
            if (values.contains(segmentName)) {
                value = values[segmentName];
            }
            else if (option.defaults.contains(segmentName)) {
                auto defaultValue = option.defaults[segmentName];
                if (defaultValue == "%next%") {
                    value = textStart + textOffset + instructionWidth;
                }
                else {
                    value = std::stoi(defaultValue);
                }
            }
            else {
                continue;
            }

            auto segment = segments[segmentName];
            if (pendingReferences.contains(segmentName)) {
                std::string label = pendingReferences[segmentName];
                Reference reference;
                reference.offset = textOffset + byte;
                reference.bit    = bit;
                reference.width  = segment.owidth;
                reference.shift  = segment.alignment - 1;
                reference.relative = segment.type == "raddress" ? textOffset : 0;

                if (!references.contains(label)) {
                    std::list<Reference> rL;
                    references[label] = rL;
                }
                references[label].push_back(reference);
            }

            _pack_bits(buffer, value, segment.owidth, byte, bit);
        }

        for (int i = 0; i < instructionWidth; i++) {
            text[textOffset++] = buffer[i];
        }

        return;
    }

    throw new SyntaxError("unresolved instruction variant", token);
}
void Assembler::processLabel(Token &token) {
    if (labels.contains(token.content)) {
        throw new SyntaxError("duplicate label '" + token.content + "'", token);
    }

    labels[token.content] = std::pair<Section, uint16_t>(section, section == DataSection ? dataOffset : textOffset);
}

void Assembler::processReferences() {
    for (auto iter = references.begin(); iter != references.end(); iter++) {
        std::string label = iter->first;
        if (!labels.contains(label)) {
            throw new ReferenceError("undefined reference to '" + label + "'");
        }

        auto deref = labels[label];
        uint32_t value;
        if (deref.first == Section::TextSection) {
            value = textStart + deref.second;
        }
        else {
            value = dataStart + deref.second;
        }

        for (Reference ref: iter->second) {
            int byte = ref.offset;
            int bit = ref.bit;

            uint32_t modifiedValue = value;
            if (ref.relative != 0) {
                modifiedValue = deref.second - ref.relative;
            }
            if (ref.shift > 0) {
                modifiedValue >>= ref.shift;
            }

            _pack_bits(text, modifiedValue, ref.width, byte, bit);
        }
    }
}

Token::Token(std::string token, int start): content(token), character(start), error(false) {
    auto ch = token.front();

    if (ch== '.') {
        type = Directive;
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

uint32_t Token::parseNumber(int maxBits, NumberSign sign, int skip) {
    auto str = content.substr(skip);
    if (str == "0" || str == "-0") {
        return 0;
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
        if (bitsCount > maxBits) {
            // actually out of bits range
            throw new SyntaxError("number out of range", *this);
        }
        if (sign != NumberSign::ForceUnsigned && negative && value > 1 << (maxBits - 1)) {
            // negative values < -128 (for 8 bits)
            throw new SyntaxError("number out of range", *this);
        }
        if (sign == NumberSign::ForceSigned && !negative && value > (1 << (maxBits - 1)) - 1) {
            // signed values > 127 (for 8 bits)
            throw new SyntaxError("number out of range", *this);
        }
    }

    if (bitsCount == 0) {
        throw new ParseError("malformed number", *this);
    }

    if (negative) {
        value = ~value;
        value += 1;
    }

    return value;
}

}; // namespace asnp
