#ifndef ARCH_H
#define ARCH_H

#include <string>
#include <list>
#include <vector>
#include <map>

#include "token.h"

namespace asnp {
namespace arch {

class Fragment {
    public:
        std::string name;
        std::string type;
        int width;
        int owidth;
        int alignment;
        int offset;
};
class Format {
    public:
        std::string name;
        int width;
        std::list<std::string> fragments;
};
class Instruction {
    public:
        std::string mnemonic;
        std::string format;
        std::vector<std::string> fragments;
        std::map<std::string,std::string> defaults;
};

class Arch {
    public:
        Arch(std::string);

        std::map<std::string, Format> formats;
        std::map<std::string, Fragment> fragments;
        std::map<std::string, std::list<Instruction>> instructions;

        int dataWidth;
        int addressWidth;
        int addressableWidth;
        uint32_t entryPoint;
};

}; // namespace arch
}; // namespace asnp

#endif
