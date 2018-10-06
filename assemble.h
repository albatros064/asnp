#ifndef ASSEMBLE_H
#define ASSEMBLE_H

#include <string>

class Assembler {
  public:
    Assembler(std::string, std::string);
    virtual ~Assembler();

    bool assemble();
  private:
    std::string inFile;
    std::string outFile;

    int currentLine;

    bool parseLine(std::string);
    bool parseDirective(std::string, int&);
};

#endif

