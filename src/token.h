#ifndef TOKEN_H
#define TOKEN_H

#include <string>

namespace asnp {

enum TokenType {
    Label,
    Identifier,
    Directive,
    Number,
    Punctuator,
    Unknown
};

enum NumberSign {
    DisallowSigned, //    0 - 255
    AllowSigned,    // -128 - 255
    ForceSigned,    // -128 - 127
};

class Token {
    public:
        Token(std::string, int);

        TokenType type;
        std::string content;
        bool error;
        int character;

        uint32_t parseNumber(int, NumberSign sign = NumberSign::DisallowSigned, int skip = 0);
    private:
};

}; // namespace asnp

#endif
