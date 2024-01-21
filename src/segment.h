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
};

class SegmentDescription {
    public:
        static const uint32_t UNDEFINED_OFFSET = 0xffffffff;
        static const uint32_t ALLOCATION_INCREMENT = 16384;
        SegmentDescription() {}
        SegmentDescription(const SegmentDescription &);
        std::string name;
        uint32_t start;
        uint32_t size;
        bool fill;
        bool ephemeral;
        bool readOnly;
};

class Segment : public SegmentDescription {
    public:
        Segment(const SegmentDescription &);
        virtual ~Segment() {}

        uint8_t&  operator[](uint32_t);
        uint32_t& operator[](std::string);

        bool canPlace(int width) { return offset + width < size; }
        const std::list<Reference> getReferences() { return references; }
        const uint32_t getSize() { return data.size(); }
        uint32_t getOffset() { return offset; }
        uint32_t getNext(int width) { return start + offset + width; }

        operator uint32_t() { return start; }
        operator const char *() { return (const char *) data.data(); }

        Segment& operator=(uint32_t);       // set offset
        Segment& operator+=(uint8_t);       // place byte at current offset
        Segment& operator+=(std::string);   // create a label at current offset
        Segment& operator+=(Reference);     // add a reference

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
