#include <ryml.hpp>
#include <c4/std/string.hpp>

#include <iostream>
#include <fstream>

#include "error.h"
#include "arch.h"

namespace asnp {
namespace arch {

Arch::Arch(std::string name) {
    std::string noStr = "";
    int32_t noInt = 0;
    uint32_t noUint = 0;
    try {
        std::ifstream file(name + ".arch.yaml", std::ios::in|std::ios::binary|std::ios::ate);

        if (!file.is_open()) {
            std::cout << "Configuration file not found." << std::endl;
            return;
        }

        size_t size = file.tellg();
        file.seekg(0);

        char *fileContent = new char[size];
        file.read(fileContent, size);
        file.close();

        ryml::Tree tree = ryml::parse_in_place({fileContent, size});
        ryml::NodeRef config = tree.rootref();

        config["dataWidth"] >> dataWidth;
        config["addressWidth"] >> addressWidth;
        config["addressableWidth"] >> addressableWidth;
        config.get_if("pageSize", &pageSize, 0);

        auto csegments      = config["segments"];
        auto cfragments     = config["fragments"];
        auto cformats       = config["formats"];
        auto cinstructions  = config["instructions"];
        auto crelocations   = config["relocations"];

        int segmentCount = csegments.num_children();
        for (int i = 0; i < segmentCount; i++) {
            auto csegment = csegments[i];
            SegmentDescription segment;

            if (!csegment["name"].readable()) {
                std::cout << "Missing name" << std::endl;
            }
            csegment["name"] >> segment.name;
            csegment.get_if("start",        &segment.start,         noUint);
            csegment.get_if("size",         &segment.size,          noUint);
            csegment.get_if("align",        &segment.align,         noUint);
            csegment.get_if("relocatable",  &segment.relocatable,   true);
            csegment.get_if("fill",         &segment.fill,          false);
            csegment.get_if("ephemeral",    &segment.ephemeral,     false);
            csegment.get_if("readOnly",     &segment.readOnly,      false);
            csegment.get_if("executable",   &segment.executable,    false);

            segments[segment.name] = segment;
        }

        int relocationCount = crelocations.num_children();
        for (int i = 0; i < relocationCount; i++) {
            auto crelocation = crelocations[i];
            Relocation relocation;

            crelocation["name"] >> relocation.name;
            crelocation["type"] >> relocation.type;

            relocations[relocation.name] = relocation;
        }

        int fragmentCount = cfragments.num_children();
        for (int i = 0; i < fragmentCount; i++) {
            auto cfragment = cfragments[i];
            Fragment fragment;

            cfragment["name"] >> fragment.name;
            cfragment["type"] >> fragment.type;
            cfragment["width"] >> fragment.width;
            cfragment.get_if("relocation",  &fragment.relocation,   noStr);
            cfragment.get_if("group",       &fragment.group,        noStr);
            cfragment.get_if("alignment",   &fragment.alignment,    1);
            cfragment.get_if("owidth",      &fragment.owidth,       fragment.width);
            cfragment.get_if("offset",      &fragment.offset,       0);
            cfragment.get_if("right",       &fragment.rightAlign,   false);

            fragments[fragment.name] = fragment;
        }

        int formatCount = cformats.num_children();
        for (int i = 0; i < formatCount; i++) {
            auto cformat = cformats[i];
            Format format;

            cformat["name"] >> format.name;
            cformat["width"] >> format.width;

            std::string fragmentName;
            auto cffragments = cformat["fragments"];
            int fSegCount = cffragments.num_children();
            for (int j = 0; j < fSegCount; j++) {
                auto fseg = cffragments[j];
                fseg >> fragmentName;
                format.fragments.push_back(fragmentName);
            }

            formats[format.name] = format;
        }

        int instructionCount = cinstructions.num_children();
        for (int i = 0; i < instructionCount; i++) {
            auto cinstruction = cinstructions[i];

            Instruction instruction;
            instruction.id = 0;

            if (!cinstruction["mnemonic"].readable()) {
            }
            cinstruction["mnemonic"] >> instruction.mnemonic;
            if (!cinstruction["format"].readable()) {
            }
            cinstruction["format"] >> instruction.format;
            cinstruction.get_if("id", &instruction.id, 0);

            std::string fragmentName;
            auto cifragments = cinstruction["fragments"];
            int fSegCount = cifragments.num_children();
            for (int j = 0; j < fSegCount; j++) {
                auto fseg = cifragments[j];
                fseg >> fragmentName;
                instruction.fragments.push_back(fragmentName);
            }

            auto ccomponents = cinstruction["components"];
            if (ccomponents.readable()) {
                //const libconfig::Setting& ccomponents = cinstruction["components"];
                int componentCount = ccomponents.num_children();
                for (int c = 0; c < componentCount; c++) {
                    auto ccomponent = ccomponents[c];

                    InstructionComponent component;
                    ccomponent["id"] >> component.id;

                    auto creplacements = ccomponent["replacements"];
                    int replacementCount = creplacements.num_children();
                    for (int r = 0; r < replacementCount; r++) {
                        auto creplacement = creplacements[r];

                        FragmentReplacement replacement;
                        creplacement["source"] >> replacement.source;
                        creplacement["dest"] >> replacement.dest;
                        creplacement.get_if("relocation", &replacement.relocation, noStr);
                        creplacement.get_if("shift", &replacement.shift, noInt);

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
                auto cfrag = cinstruction[frag.c_str()];
                if (cfrag.readable()) {
                    std::string fragName;
                    cfrag >> fragName;
                    instruction.defaults[frag] = fragName;
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
    catch (int e) {
        //throw new ConfigError("config file '" + name + ".arch' on line " + std::to_string(e.getLine()) +  ": " + e.getError());
    }
}

}; // namespace arch
}; // namespace asnp
