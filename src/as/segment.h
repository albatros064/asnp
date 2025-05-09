#ifndef SEGMENT_H
#define SEGMENT_H

#include <string>
#include <vector>
#include <list>
#include <map>

#include "error.h"

namespace asnp {

class Reference {
    public:
        std::string label;
        uint32_t offset;
        uint8_t bit;
        int width;
        int shift;
        uint32_t relative;
        uint8_t type;
};

class SegmentDescription {
    public:
        static const uint32_t UNDEFINED_OFFSET = 0xffffffff;
        static const uint32_t ALLOCATION_INCREMENT = 16384;
        SegmentDescription();
        SegmentDescription(const SegmentDescription &);
        std::string name;
        uint32_t start;
        uint32_t size;
        uint32_t align;
        bool relocatable;
        bool fill;
        bool ephemeral;
        bool readOnly;
        bool executable;
};

class Segment : public SegmentDescription {
    public:
        Segment(const SegmentDescription &);
        virtual ~Segment() {}

        uint8_t&  operator[](uint32_t);
        uint32_t& getLabelOffset(std::string);

        bool canPlace(int width) { return size == 0 || (offset + width < size); }
        const std::list<Reference> getReferences() { return references; }
        const std::map<std::string, uint32_t> getLabels() { return labels; }
        const uint32_t getSize() { return data.size(); }
        uint32_t getOffset() { return offset; }
        uint32_t getNext(int width) { return start + offset + width; }

        uint32_t getStartAddress() { return start; }
        char * getData(bool copy = false);

        Segment& operator=(uint32_t);       // set offset
        Segment& operator+=(uint8_t);       // place byte at current offset
        void addLabel(std::string);         // create a label at current offset
        void addReference(Reference);       // add a reference

        void pack(uint32_t, int, uint32_t, int&);
    private:
        uint32_t offset;

        std::vector<uint8_t> data;
        std::map<std::string, uint32_t> labels;
        std::list<Reference> references;
};

class SegmentError : public AssemblyError {
    public:
        SegmentError(std::string m):AssemblyError("Segment Error",m) {}
};

}; // namespace asnp

#endif
