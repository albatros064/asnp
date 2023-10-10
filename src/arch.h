#ifndef ARCH_H
#define ARCH_H

#include <string>
#include <list>
#include <map>

#include "token.h"

namespace asnp {
namespace arch {

class Segment {
    public:
        std::string name;
        std::string type;
        int width;
};
class Format {
    public:
        std::string name;
        int width;
        std::list<std::string> segments;
};
class Instruction {
    public:
        std::string mnemonic;
        std::string format;
        std::list<std::string> segments;
        std::map<std::string,std::string> defaults;
};

class Arch {
    public:
        Arch(std::string);

        std::map<std::string, Format> formats;
        std::map<std::string, Segment> segments;
        std::map<std::string, std::list<Instruction>> instructions;

        int dataWidth;
        int addressWidth;
        int addressableWidth;
};

}; // namespace arch
}; // namespace asnp

#endif
