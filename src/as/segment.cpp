#include <cstring>
#include "segment.h"

namespace asnp {

SegmentDescription::SegmentDescription(): relocatable(true), start(0), size(0), align(0), fill(false), ephemeral(false), readOnly(false), executable(false) {}

SegmentDescription::SegmentDescription(const SegmentDescription& original) {
    name        = original.name;
    start       = original.start;
    size        = original.size;
    fill        = original.fill;
    ephemeral   = original.ephemeral;
    readOnly    = original.readOnly;
    executable  = original.executable;
    align       = original.align;
}

Segment::Segment(const SegmentDescription &base):SegmentDescription(base),offset(0) {
    uint32_t allocation = size;
    if (size > ALLOCATION_INCREMENT || size == 0) {
        allocation = ALLOCATION_INCREMENT;
    }
    data.reserve(allocation);
}

uint8_t& Segment::operator[](uint32_t index) {
    if (index >= data.size()) {
        throw new SegmentError("subscript access past pre-established data size");
    }

    return data[index];
}

char *Segment::getData(bool copy) {
    char *segmentData = (char *) data.data();
    if (!copy) {
        return segmentData;
    }

    char *newSegmentData = new char[getSize()];

    std::memcpy(newSegmentData, segmentData, getSize());
    return newSegmentData;
};

uint32_t& Segment::getLabelOffset(std::string index) {
    if (!labels.contains(index)) {
        labels[index] = UNDEFINED_OFFSET;
    }

    return labels[index];
}

Segment& Segment::operator=(uint32_t newOffset) {
    if (newOffset < start) {
        throw new SegmentError("invalid offset '" + std::to_string(newOffset) + "'");
    }
    newOffset -= start;

    if (size > 0 && newOffset > size) {
        throw new SegmentError("segment '" + name + "' max size of '" + std::to_string(size) + "' exceeded");
    }

    offset = newOffset;

    return *this;
}

Segment& Segment::operator+=(uint8_t datum) {
    if (size > 0 && offset >= size) {
// throw
    }

    if (offset > data.capacity()) {
        uint32_t allocation = offset - data.capacity();
        if (allocation < ALLOCATION_INCREMENT) {
            allocation = ALLOCATION_INCREMENT;
        }
        allocation += data.capacity();
        if (size > 0 && allocation > size) {
            allocation = size;
        }
        data.reserve(allocation);
    }

    if (offset >= data.size()) {
        data.resize(offset + 1);
    }

    data[offset++] = datum;

    return *this;
}

void Segment::addLabel(std::string label) {
    labels[label] = offset;
}

void Segment::addReference(Reference newRef) {
    references.push_back(newRef);
}

void Segment::pack(uint32_t value, int width, uint32_t byte, int &bit) {
    do {
        int startBit = bit;
        int bitsForByte = 8 - bit;
        uint32_t mask = 0xff;
        uint32_t val = value;
        if (bitsForByte > width) {
            mask >>= (8 - width);
            val &= mask;
            val <<= (bitsForByte - width);
            bit += width;
            width = 0;
        }
        else {
            val = (val >> (width - bitsForByte)) & mask;
            width -= bitsForByte;
            bit += bitsForByte;
        }

        if (byte == UNDEFINED_OFFSET) {
            if (startBit == 0) {
               *this += (uint8_t) val;
            }
            else {
                data[offset - 1] |= (uint8_t) val;
            }
        }
        else {
            data[byte] |= (uint8_t) val;
        }

        if (bit >= 8) { // shouldn't ever be > 8, but meh
            bit = 0;
            if (byte != UNDEFINED_OFFSET) {
                byte++;
            }
        }
    } while (width > 0);
}

};
