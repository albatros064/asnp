#ifndef ERROR_H
#define ERROR_H

#include "token.h"

namespace asnp {

class AssemblyError {
    public:
        AssemblyError(std::string t, std::string m):type(t),message(m) {};

        std::string type;
        std::string message;
};
class CodeError : public AssemblyError {
    public:
        CodeError(std::string t, std::string m, Token toke):AssemblyError(t,m),token(toke) {}
        Token token;
};

class ParseError : public CodeError {
    public:
        ParseError(std::string m, Token toke):CodeError("Parse Error",m,toke) {}
};
class SyntaxError : public CodeError {
    public:
        SyntaxError(std::string m, Token toke):CodeError("Syntax Error",m,toke) {}
};
class ConfigError : public AssemblyError {
    public:
        ConfigError(std::string m):AssemblyError("Config Error",m) {}
};
class ReferenceError : public AssemblyError {
    public:
        ReferenceError(std::string m):AssemblyError("Reference Error",m) {}
};

}; // namespace asnp

#endif
