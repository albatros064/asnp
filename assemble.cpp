#include "assemble.h"

#include <iostream>
#include <fstream>
#include <ctype.h>

Assembler::Assembler(std::string in, std::string out)
  :inFile(in),outFile(out) {
}
Assembler::~Assembler() {
}

#define LENGTH_LINEBUFFER 256
bool Assembler::assemble() {
    std::ifstream reader(inFile);

    if (!reader.is_open()) {
        std::cerr << "Could not open input file '" << inFile << "'. Aborting." << std::endl;
        return false;
    }

    currentLine = 1;
    char lineBuffer[LENGTH_LINEBUFFER];
    while (!reader.eof()) {
        std::string line;
        reader.getline(lineBuffer, LENGTH_LINEBUFFER);
        line.append(lineBuffer);
        while (reader.fail() && !reader.eof()) {
            reader.getline(lineBuffer, LENGTH_LINEBUFFER);
        }

        if (!parseLine(line)) {
            return false;
        }

        currentLine++;
    }
    return true;
}

bool _eat_whitespace(std::string line, int &index) {
    while (isspace(line[index])) {
        index++;
        if (index >= line.length()) {
            return false;
        }
    }
    return true;
}
std::string _read_word(std::string line, int &index) {
    std::string word;
    int start = index;
    while (!isspace(line[index]) && line[index] != ':' && line[index] != ',' && line[index] != ';') {
        index++;
        if (index >= line.length()) {
            break;
        }
    }
    word = line.substr(start, index - start);
    _eat_whitespace(line, index);

    return word;
}
std::string _read_punctuator(std::string line, int &index) {
    //std::string 
    return line.substr(index++, 1);
}

bool Assembler::parseLine(std::string line) {
    int current = 0;
    if (!_eat_whitespace(line, current)) {
        return true;
    }

    std::queue<string> line_queue;

    while (true) {
        std::string word = _read_word(line, current);
        line_queue.append(word);
        if (line[current] == ':' || line[current] == ',') {
            line_queue.append(_read_punctuator(line, current));
        }
        if (line[current] == ';';
    }

    return true;
}

bool Assembler::parseDirective(std::string directive, std::string line, int &current) {
    std::cout << "Unsupported directive: " << directive << std::endl;

    return true;
}
