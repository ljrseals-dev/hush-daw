// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#include "hush/composer/lite.hpp"
#include "harmony.hpp"
#include "smf.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace hush::composer;

namespace {

// ---------------------------------------------------------------------------
// Argument parsing
// ---------------------------------------------------------------------------

struct Args {
    std::string command;
    std::string inPath;
    std::string outPath;
    std::string style    = "pop";
    std::string styleDir;
    int         bars     = 4;
    int         count    = 4;
    uint64_t    seed     = 0;
    bool        help     = false;
    bool        error    = false;
    std::string errorMsg;
};

void printUsage() {
    std::cout <<
        "hush-neural-lite — statistical MIDI composer\n"
        "\n"
        "Usage:\n"
        "  hush-neural-lite suggest  --in <ctx.mid> [options]\n"
        "  hush-neural-lite continue --in <ctx.mid> --out <out.mid> [options]\n"
        "  hush-neural-lite generate --style <s> --bars N --out <out.mid> [options]\n"
        "  hush-neural-lite analyze  --in <score.mid>\n"
        "\n"
        "Options:\n"
        "  --in <path>          Input MIDI file (context or score)\n"
        "  --out <path>         Output MIDI file (continue / generate)\n"
        "  --style <s>          pop | jazz | classical | ambient (default pop)\n"
        "  --bars <N>           Bars to generate (default 4)\n"
        "  --count <K>          Number of suggestions (suggest only, default 4)\n"
        "  --seed <N>           RNG seed; 0 = nondeterministic (default 0)\n"
        "  --style-dir <path>   Directory of <style>.txt override files\n"
        "  --help               This message\n"
        "\n"
        "Examples:\n"
        "  hush-neural-lite suggest  --in motif.mid --style jazz --bars 4 --count 4\n"
        "  hush-neural-lite continue --in motif.mid --style pop --bars 8 --out out.mid\n"
        "  hush-neural-lite generate --style ambient --bars 16 --out pad.mid --seed 42\n"
        "  hush-neural-lite analyze  --in score.mid\n";
}

bool parseUint64(const std::string& s, uint64_t& out) {
    try {
        size_t consumed = 0;
        out = std::stoull(s, &consumed);
        return consumed == s.size();
    } catch (...) { return false; }
}

bool parseInt(const std::string& s, int& out) {
    try {
        size_t consumed = 0;
        out = std::stoi(s, &consumed);
        return consumed == s.size();
    } catch (...) { return false; }
}

Args parseArgs(int argc, char** argv) {
    Args a;
    if (argc < 2) { a.help = true; return a; }

    a.command = argv[1];
    if (a.command == "--help" || a.command == "-h") {
        a.help = true;
        return a;
    }

    for (int i = 2; i < argc; ++i) {
        const std::string flag = argv[i];
        auto needValue = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                a.error = true;
                a.errorMsg = std::string("missing value for ") + name;
                return nullptr;
            }
            return argv[++i];
        };

        if (flag == "--in") {
            const char* v = needValue("--in");
            if (!v) return a;
            a.inPath = v;
        } else if (flag == "--out") {
            const char* v = needValue("--out");
            if (!v) return a;
            a.outPath = v;
        } else if (flag == "--style") {
            const char* v = needValue("--style");
            if (!v) return a;
            a.style = v;
        } else if (flag == "--bars") {
            const char* v = needValue("--bars");
            if (!v) return a;
            if (!parseInt(v, a.bars) || a.bars <= 0) {
                a.error = true;
                a.errorMsg = "invalid --bars value: " + std::string(v);
                return a;
            }
        } else if (flag == "--count") {
            const char* v = needValue("--count");
            if (!v) return a;
            if (!parseInt(v, a.count) || a.count <= 0) {
                a.error = true;
                a.errorMsg = "invalid --count value: " + std::string(v);
                return a;
            }
        } else if (flag == "--seed") {
            const char* v = needValue("--seed");
            if (!v) return a;
            if (!parseUint64(v, a.seed)) {
                a.error = true;
                a.errorMsg = "invalid --seed value: " + std::string(v);
                return a;
            }
        } else if (flag == "--style-dir") {
            const char* v = needValue("--style-dir");
            if (!v) return a;
            a.styleDir = v;
        } else if (flag == "--help" || flag == "-h") {
            a.help = true;
            return a;
        } else {
            a.error = true;
            a.errorMsg = "unknown option: " + flag;
            return a;
        }
    }
    return a;
}

// ---------------------------------------------------------------------------
// Engraved text output
// ---------------------------------------------------------------------------

std::string pitchName(int midi) {
    static const char* names[] = {
        "C","C#","D","D#","E","F","F#","G","G#","A","A#","B"
    };
    const int pc  = ((midi % 12) + 12) % 12;
    const int oct = (midi / 12) - 1;
    return std::string(names[pc]) + std::to_string(oct);
}

void printEngraved(const notation::Score& s, const std::string& title) {
    std::cout << "=== " << title << " ===\n";
    std::cout << "ppq=" << s.ppq
              << " tempo=" << s.tempoBpm
              << " measures=" << s.measures.size() << "\n";
    for (size_t i = 0; i < s.measures.size(); ++i) {
        const auto& m = s.measures[i];
        std::cout << "  m" << (i + 1) << ": ";
        if (m.notes.empty()) {
            std::cout << "(empty)";
        } else {
            for (size_t j = 0; j < m.notes.size(); ++j) {
                if (j) std::cout << " ";
                std::cout << pitchName(m.notes[j].pitch);
            }
        }
        std::cout << "\n";
    }
}

// ---------------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------------

int cmdSuggest(const Args& a) {
    if (a.inPath.empty()) {
        std::cerr << "error: suggest requires --in <ctx.mid>\n";
        return 2;
    }

    notation::Score context;
    try {
        context = readMidi(a.inPath);
    } catch (const SmfError& e) {
        std::cerr << "error reading " << a.inPath << ": "
                  << e.what() << "\n";
        return 1;
    }

    LiteComposer c(a.styleDir);

    const auto key = ::hush::composer::detectKey(context);
    std::cout << "context key: " << key.name() << "\n";
    std::cout << "style: " << a.style << "\n\n";

    const auto candidates = c.suggest(context, a.style, a.count,
                                      a.bars, a.seed);
    if (candidates.empty()) {
        std::cerr << "no candidates produced\n";
        return 1;
    }

    for (size_t i = 0; i < candidates.size(); ++i) {
        std::ostringstream title;
        title << "suggestion " << (i + 1) << " / " << candidates.size();
        printEngraved(candidates[i], title.str());
        std::cout << "\n";
    }
    return 0;
}

int cmdContinue(const Args& a) {
    if (a.inPath.empty()) {
        std::cerr << "error: continue requires --in <ctx.mid>\n";
        return 2;
    }
    if (a.outPath.empty()) {
        std::cerr << "error: continue requires --out <out.mid>\n";
        return 2;
    }

    notation::Score context;
    try {
        context = readMidi(a.inPath);
    } catch (const SmfError& e) {
        std::cerr << "error reading " << a.inPath << ": "
                  << e.what() << "\n";
        return 1;
    }

    SampleConfig sample;
    sample.bars  = a.bars;
    sample.seed  = a.seed;
    sample.style = a.style;

    LiteComposer c(a.styleDir);
    notation::Score result = c.continueFrom(context, sample);

    try {
        writeMidi(a.outPath, result);
    } catch (const SmfError& e) {
        std::cerr << "error writing " << a.outPath << ": "
                  << e.what() << "\n";
        return 1;
    }

    printEngraved(result, "continued");
    std::cout << "\nwrote " << a.outPath << "\n";
    return 0;
}

int cmdGenerate(const Args& a) {
    if (a.outPath.empty()) {
        std::cerr << "error: generate requires --out <out.mid>\n";
        return 2;
    }

    Style style = Style::Pop;
    if      (a.style == "jazz")      style = Style::Jazz;
    else if (a.style == "classical") style = Style::Classical;
    else if (a.style == "ambient")   style = Style::Ambient;
    else if (a.style != "pop") {
        std::cerr << "error: unknown style '" << a.style
                  << "'. Use pop, jazz, classical, or ambient.\n";
        return 2;
    }

    SampleConfig sample;
    sample.bars  = a.bars;
    sample.seed  = a.seed;
    sample.style = a.style;

    LiteComposer c(a.styleDir);
    notation::Score result = c.generate(style, a.bars, sample);

    try {
        writeMidi(a.outPath, result);
    } catch (const SmfError& e) {
        std::cerr << "error writing " << a.outPath << ": "
                  << e.what() << "\n";
        return 1;
    }

    printEngraved(result, "generated");
    std::cout << "\nwrote " << a.outPath << "\n";
    return 0;
}

int cmdAnalyze(const Args& a) {
    if (a.inPath.empty()) {
        std::cerr << "error: analyze requires --in <score.mid>\n";
        return 2;
    }

    notation::Score score;
    try {
        score = readMidi(a.inPath);
    } catch (const SmfError& e) {
        std::cerr << "error reading " << a.inPath << ": "
                  << e.what() << "\n";
        return 1;
    }

    const auto key      = ::hush::composer::detectKey(score);
    const auto analysis = analyzeMeasures(score, key, true);

    std::cout << "file:  " << a.inPath << "\n";
    std::cout << "key:   " << key.name() << "\n";
    std::cout << "ppq:   " << score.ppq << "\n";
    std::cout << "tempo: " << score.tempoBpm << " bpm\n";
    std::cout << "bars:  " << score.measures.size() << "\n\n";

    for (size_t i = 0; i < analysis.size(); ++i) {
        const auto& ma = analysis[i];
        std::cout << "  m" << (i + 1) << ": " << ma.chord.text;
        if (ma.chord.inversion) std::cout << " (inv)";
        if (ma.isCadence)       std::cout << " [cadence]";
        std::cout << "\n";
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    const Args a = parseArgs(argc, argv);

    if (a.help) {
        printUsage();
        return 0;
    }
    if (a.error) {
        std::cerr << "error: " << a.errorMsg << "\n\n";
        printUsage();
        return 2;
    }

    try {
        if      (a.command == "suggest")  return cmdSuggest(a);
        else if (a.command == "continue") return cmdContinue(a);
        else if (a.command == "generate") return cmdGenerate(a);
        else if (a.command == "analyze")  return cmdAnalyze(a);
        else {
            std::cerr << "error: unknown command '" << a.command
                      << "'\n\n";
            printUsage();
            return 2;
        }
    } catch (const std::exception& e) {
        std::cerr << "fatal: " << e.what() << "\n";
        return 1;
    }
}
