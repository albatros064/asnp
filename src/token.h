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

class Token {
    public:
        Token(std::string, int);

        TokenType type;
        std::string content;
        bool error;
        int character;

        uint32_t parseNumber(int, int skip = 0);
    private:
};

}; // namespace asnp

#endif
