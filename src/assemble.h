#ifndef ASSEMBLE_H
#define ASSEMBLE_H

#include <string>
#include <list>
#include <memory>
#include <set>

#include "arch.h"
#include "segment.h"
#include "token.h"
#include "error.h"

namespace asnp {

enum LineState {
    LabelState,
    ActionState,
    DoneState
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
        Assembler(std::string);
        virtual ~Assembler();

        bool assemble(std::string, std::string);
        bool link(bool);
    private:
        std::string outFile;

        int currentLine;
        std::string line;
        std::list<int> lineStack;

        LineState lineState;
        std::list<Token> tokens;
        std::unique_ptr<arch::Arch> architecture;

        std::map<std::string, std::shared_ptr<Segment>> segments;
        std::shared_ptr<Segment> segment;
        std::map<std::string, std::shared_ptr<Segment>> labels;

        void tokenize(std::string);
        
        void processDirective(Token &, std::string);
        void processInstruction(Token &);
        void processLabel(Token &);
        std::map<std::string, uint32_t> processReferences();
};

}; // namespace asnp

#endif

