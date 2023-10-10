#include "libconfig.h++"
#include <iostream>

#include "error.h"
#include "arch.h"

namespace asnp {
namespace arch {

Arch::Arch(std::string name) {
    libconfig::Config config;
    try {
        config.readFile((name + ".arch").c_str());

        const libconfig::Setting& configRoot = config.getRoot();

        configRoot.lookupValue("dataWidth", dataWidth);
        configRoot.lookupValue("addressWidth", addressWidth);
        configRoot.lookupValue("addressableWidth", addressableWidth);
        configRoot.lookupValue("entryPoint", entryPoint);

        const libconfig::Setting& csegments = configRoot["segments"];
        const libconfig::Setting& cformats = configRoot["formats"];
        const libconfig::Setting& cinstructions = configRoot["instructions"];

        int segmentCount = csegments.getLength();
        for (int i = 0; i < segmentCount; i++) {
            const libconfig::Setting& csegment = csegments[i];
            Segment segment;

            csegment.lookupValue("name", segment.name);
            csegment.lookupValue("type", segment.type);
            csegment.lookupValue("width", segment.width);

            segment.alignment = 1;
            segment.owidth = segment.width;
            csegment.lookupValue("owidth", segment.owidth);
            csegment.lookupValue("alignment", segment.alignment);

            segments[segment.name] = segment;
        }

        int formatCount = cformats.getLength();
        for (int i = 0; i < formatCount; i++) {
            const libconfig::Setting& cformat = cformats[i];
            Format format;
            cformat.lookupValue("name", format.name);
            cformat.lookupValue("width", format.width);

            const libconfig::Setting& cfsegments = cformat["segments"];
            int fSegCount = cfsegments.getLength();
            for (int j = 0; j < fSegCount; j++) {
                format.segments.push_back(cfsegments[j]);
            }

            formats[format.name] = format;
        }

        int instructionCount = cinstructions.getLength();
        for (int i = 0; i < instructionCount; i++) {
            const libconfig::Setting& cinstruction = cinstructions[i];
            Instruction instruction;
            cinstruction.lookupValue("mnemonic", instruction.mnemonic);
            cinstruction.lookupValue("format", instruction.format);

            const libconfig::Setting& cisegments = cinstruction["segments"];
            int fSegCount = cisegments.getLength();
            for (int j = 0; j < fSegCount; j++) {
                instruction.segments.push_back(cisegments[j]);
            }

            if (!formats.contains(instruction.format)) {
                throw new ConfigError("unrecognized instruction format '" + instruction.format + "'");
            }
            Format format = formats[instruction.format];
            for (std::string seg: format.segments) {
                if (cinstruction.exists(seg)) {
                    std::string segName;
                    cinstruction.lookupValue(seg, segName);
                    instruction.defaults[seg] = segName;
                }
            }

            if (!instructions.contains(instruction.mnemonic)) {
                std::list<Instruction> iL;
                instructions[instruction.mnemonic] = iL;
            }
            instructions[instruction.mnemonic].push_back(instruction);
        }
    }
    catch (libconfig::FileIOException e) {
        throw new ConfigError("cannot read file '" + name + ".arch'");
    }
    catch (libconfig::ParseException e) {
        throw new ConfigError("config file '" + name + ".arch' on line " + std::to_string(e.getLine()) +  ": " + e.getError());
    }
}

}; // namespace arch
}; // namespace asnp
