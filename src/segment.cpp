#include "segment.h"

namespace asnp {

SegmentDescription::SegmentDescription(const SegmentDescription& original) {
    name        = original.name;
    start       = original.start;
    size        = original.size;
    fill        = original.fill;
    ephemeral   = original.ephemeral;
    readOnly    = original.readOnly;
}

Segment::Segment(const SegmentDescription &base):SegmentDescription(base),offset(0) {
    uint32_t allocation = size;
    if (size > ALLOCATION_INCREMENT) {
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

uint32_t& Segment::operator[](std::string index) {
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

    if (newOffset > size) {
        throw new SegmentError("segment '" + name + "' max size of '" + std::to_string(size) + "' exceeded");
    }

    offset = newOffset;

    return *this;
}

Segment& Segment::operator+=(uint8_t datum) {
    if (offset >= size) {
// throw
    }

    if (offset > data.capacity()) {
        uint32_t allocation = offset - data.capacity();
        if (allocation < ALLOCATION_INCREMENT) {
            allocation = ALLOCATION_INCREMENT;
        }
        data.reserve(std::min(size, (uint32_t) (data.capacity() + allocation)));
    }
    if (offset >= data.size()) {
        data.resize(offset + 1);
    }

    data[offset++] = datum;

    return *this;
}

Segment& Segment::operator+=(std::string label) {
    labels[label] = offset;

    return *this;
}

Segment& Segment::operator+=(Reference newRef) {
    references.push_back(newRef);

    return *this;
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
