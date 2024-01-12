#ifndef ASSEMBLE_H
#define ASSEMBLE_H

#include <string>
#include <list>
#include <memory>

#include "arch.h"
#include "token.h"
#include "error.h"

#define SECTION_MAX_SIZE 0x10000

namespace asnp {

enum LineState {
    LabelState,
    ActionState,
    DoneState
};
enum Segment {
    DataSegment,
    TextSegment,
    NoSegment
};

class Reference {
    public:
        uint16_t offset;
        uint8_t bit;
        int width;
        int shift;
        uint32_t relative;
};

class InstructionCandidate {
    public:
        arch::Instruction instruction;
        std::map<std::string, uint32_t> values;
        std::map<std::string, std::string> pendingReferences;

        int matchedTokens;
        SyntaxError *error;
};

class Assembler {
    public:
        Assembler(std::string, std::string);
        virtual ~Assembler();

        bool assemble();
    private:
        std::string inFile;
        std::string outFile;

        int currentLine;
        std::string line;

        Segment segment;
        LineState lineState;
        std::list<Token> tokens;
        std::unique_ptr<arch::Arch> architecture;

        uint32_t dataStart;
        uint32_t textStart;
        uint16_t dataOffset;
        uint16_t textOffset;

        uint8_t data[SECTION_MAX_SIZE];
        uint8_t text[SECTION_MAX_SIZE];
        

        std::map<std::string, std::pair<Segment, uint16_t>> labels;
        std::map<std::string, std::list<Reference>> references;

        void tokenize(std::string);
        
        void processDirective(Token &);
        void processInstruction(Token &);
        void processLabel(Token &);
        void processReferences();
};

}; // namespace asnp

#endif

