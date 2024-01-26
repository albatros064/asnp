#include <bit>
#include "token.h"
#include "error.h"

namespace asnp {

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
