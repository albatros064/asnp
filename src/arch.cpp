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

            const libconfig::Setting& cifragments = cinstruction["fragments"];
            int fSegCount = cifragments.getLength();
            for (int j = 0; j < fSegCount; j++) {
                instruction.fragments.push_back(cifragments[j]);
            }

            if (!formats.contains(instruction.format)) {
                throw new ConfigError("unrecognized instruction format '" + instruction.format + "'");
            }
            Format format = formats[instruction.format];
            for (std::string seg: format.fragments) {
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
