#ifndef ASSEMBLE_H
#define ASSEMBLE_H

#include <string>
#include <list>
#include <memory>

#include "arch.h"
#include "token.h"

#define SECTION_MAX_SIZE 0x10000

namespace asnp {

enum LineState {
    LabelState,
    ActionState,
    DoneState
};
enum Section {
    DataSection,
    TextSection,
    NoSection
};

class Reference {
    public:
        uint16_t offset;
        uint8_t bit;
        int width;
        int shift;
        uint32_t relative;
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

        Section section;
        LineState lineState;
        std::list<Token> tokens;
        std::unique_ptr<arch::Arch> architecture;

        uint32_t dataStart;
        uint32_t textStart;
        uint16_t dataOffset;
        uint16_t textOffset;

        uint8_t data[SECTION_MAX_SIZE];
        uint8_t text[SECTION_MAX_SIZE];
        

        std::map<std::string, std::pair<Section, uint16_t>> labels;
        std::map<std::string, std::list<Reference>> references;

        void tokenize(std::string);
        
        void processDirective(Token &);
        void processInstruction(Token &);
        void processLabel(Token &);
        void processReferences();
};

}; // namespace asnp

#endif

