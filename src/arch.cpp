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
        configRoot.lookupValue("textAddress", textAddress);
        configRoot.lookupValue("dataAddress", dataAddress);

        const libconfig::Setting& csegments = configRoot["segments"];
        int segmentCount = csegments.getLength();
        for (int i = 0; i < segmentCount; i++) {
            const libconfig::Setting& csegment = csegments[i];
            SegmentDescription segment;

            csegment.lookupValue("name", segment.name);
            csegment.lookupValue("start", segment.start);
            csegment.lookupValue("size", segment.size);
            csegment.lookupValue("fill", segment.fill);
            csegment.lookupValue("ephemeral", segment.ephemeral);
            csegment.lookupValue("readOnly", segment.readOnly);

            segments[segment.name] = segment;
        }

        const libconfig::Setting& cfragments = configRoot["fragments"];
        const libconfig::Setting& cformats = configRoot["formats"];
        const libconfig::Setting& cinstructions = configRoot["instructions"];

        int fragmentCount = cfragments.getLength();
        for (int i = 0; i < fragmentCount; i++) {
            const libconfig::Setting& cfragment = cfragments[i];
            Fragment fragment;

            cfragment.lookupValue("name", fragment.name);
            cfragment.lookupValue("type", fragment.type);
            cfragment.lookupValue("width", fragment.width);

            fragment.alignment = 1;
            fragment.owidth = fragment.width;
            fragment.offset = 0;
            cfragment.lookupValue("owidth", fragment.owidth);
            cfragment.lookupValue("alignment", fragment.alignment);
            cfragment.lookupValue("offset", fragment.offset);

            fragments[fragment.name] = fragment;
        }

        int formatCount = cformats.getLength();
        for (int i = 0; i < formatCount; i++) {
            const libconfig::Setting& cformat = cformats[i];
            Format format;
            cformat.lookupValue("name", format.name);
            cformat.lookupValue("width", format.width);

            const libconfig::Setting& cffragments = cformat["fragments"];
            int fSegCount = cffragments.getLength();
            for (int j = 0; j < fSegCount; j++) {
                format.fragments.push_back(cffragments[j]);
            }

            formats[format.name] = format;
        }

        int instructionCount = cinstructions.getLength();
        for (int i = 0; i < instructionCount; i++) {
            const libconfig::Setting& cinstruction = cinstructions[i];
            Instruction instruction;
            cinstruction.lookupValue("mnemonic", instruction.mnemonic);
            cinstruction.lookupValue("format", instruction.format);
            instruction.id = 0;
            cinstruction.lookupValue("id", instruction.id);

            const libconfig::Setting& cifragments = cinstruction["fragments"];
            int fSegCount = cifragments.getLength();
            for (int j = 0; j < fSegCount; j++) {
                instruction.fragments.push_back(cifragments[j]);
            }

            if (cinstruction.exists("components")) {
                const libconfig::Setting& ccomponents = cinstruction["components"];
                int componentCount = ccomponents.getLength();
                for (int c = 0; c < componentCount; c++) {
                    const libconfig::Setting& ccomponent = ccomponents[c];

                    InstructionComponent component;
                    ccomponent.lookupValue("id", component.id);

                    const libconfig::Setting& creplacements = ccomponent["replacements"];
                    int replacementCount = creplacements.getLength();
                    for (int r = 0; r < replacementCount; r++) {
                        const libconfig::Setting& creplacement = creplacements[r];

                        FragmentReplacement replacement;

                        creplacement.lookupValue("source", replacement.source);
                        creplacement.lookupValue("dest", replacement.dest);
                        replacement.shift = 0;
                        if (creplacement.exists("shift")) {
                            creplacement.lookupValue("shift", replacement.shift);
                        }

                        component.replacements.push_back(replacement);
                    }
                    instruction.components.push_back(component);
                }
            }

            if (instruction.format != "composite" && !formats.contains(instruction.format)) {
                throw new ConfigError("unrecognized instruction format '" + instruction.format + "'");
            }
            Format format = formats[instruction.format];
            for (std::string frag: format.fragments) {
                if (cinstruction.exists(frag)) {
                    std::string fragName;
                    cinstruction.lookupValue(frag, fragName);
                    instruction.defaults[frag] = frag;
                }
            }

            if (!instructions.contains(instruction.mnemonic)) {
                std::list<Instruction> iL;
                instructions[instruction.mnemonic] = iL;
            }
            instructions[instruction.mnemonic].push_back(instruction);

            if (instruction.id > 0) {
                indexedInstructions[instruction.id] = instruction;
            }
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
