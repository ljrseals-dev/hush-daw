// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#include "hush/composer/neural.hpp"
#include "hush/composer/composer.hpp"
#include "smf.hpp"     // readMidi / writeMidi / SmfError from neural-lite

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace hush::composer;

namespace {

// ---------------------------------------------------------------------------
// Argument parsing
// ---------------------------------------------------------------------------

struct Args {
    std::string command;         // continue | generate
    std::string inPath;
    std::string outPath;
    std::string modelPath;
    std::string style     = "pop";
    int         bars      = 4;
    float       temperature = 1.0f;
    int         topK      = 40;
    float       topP      = 0.95f;
    uint64_t    seed      = 0;
    int         tokens    = 256;
    bool        useCuda   = false;
    bool        help      = false;
    bool        error     = false;
    std::string errorMsg;
};

void printUsage() {
    std::cout <<
        "hush-neural — neural MIDI composer (ONNX Runtime)\n"
        "\n"
        "Usage:\n"
        "  hush-neural continue --in <ctx.mid> --out <out.mid>\n"
        "                       --model <model.onnx> [options]\n"
        "  hush-neural generate --style <p|j|c|a> --bars N\n"
        "                       --out <out.mid> --model <model.onnx> [options]\n"
        "\n"
        "Options:\n"
        "  --in <path>          Input MIDI (continue only)\n"
        "  --out <path>         Output MIDI (required for both verbs)\n"
        "  --model <path>       Path to .onnx model (required)\n"
        "  --style <s>          pop|jazz|classical|ambient or p|j|c|a\n"
        "                       (default pop)\n"
        "  --bars <N>           Bars to generate (default 4)\n"
        "  --temperature <f>    Sampling temperature (default 1.0)\n"
        "  --top-k <N>          Top-k (default 40; 0 = disabled)\n"
        "  --top-p <f>          Top-p / nucleus (default 0.95; 0 = greedy)\n"
        "  --seed <N>           RNG seed; 0 = nondeterministic (default 0)\n"
        "  --tokens <N>         Max new tokens (default 256)\n"
        "  --cuda               Request CUDA EP (requires HUSH_NEURAL_WITH_CUDA)\n"
        "  --help               This message\n";
}

bool parseFloat(const std::string& s, float& out) {
    try {
        size_t c = 0;
        out = std::stof(s, &c);
        return c == s.size();
    } catch (...) { return false; }
}

bool parseInt(const std::string& s, int& out) {
    try {
        size_t c = 0;
        out = std::stoi(s, &c);
        return c == s.size();
    } catch (...) { return false; }
}

bool parseUint64(const std::string& s, uint64_t& out) {
    try {
        size_t c = 0;
        out = std::stoull(s, &c);
        return c == s.size();
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
        const std::string f = argv[i];
        auto need = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                a.error = true;
                a.errorMsg = std::string("missing value for ") + name;
                return nullptr;
            }
            return argv[++i];
        };

        if (f == "--in") {
            const char* v = need("--in");
            if (!v) return a;
            a.inPath = v;
        } else if (f == "--out") {
            const char* v = need("--out");
            if (!v) return a;
            a.outPath = v;
        } else if (f == "--model") {
            const char* v = need("--model");
            if (!v) return a;
            a.modelPath = v;
        } else if (f == "--style") {
            const char* v = need("--style");
            if (!v) return a;
            a.style = v;
        } else if (f == "--bars") {
            const char* v = need("--bars");
            if (!v) return a;
            if (!parseInt(v, a.bars) || a.bars <= 0) {
                a.error = true;
                a.errorMsg = "invalid --bars value: " + std::string(v);
                return a;
            }
        } else if (f == "--temperature") {
            const char* v = need("--temperature");
            if (!v) return a;
            if (!parseFloat(v, a.temperature) || a.temperature < 0.0f) {
                a.error = true;
                a.errorMsg = "invalid --temperature value: "
                             + std::string(v);
                return a;
            }
        } else if (f == "--top-k") {
            const char* v = need("--top-k");
            if (!v) return a;
            if (!parseInt(v, a.topK) || a.topK < 0) {
                a.error = true;
                a.errorMsg = "invalid --top-k value: " + std::string(v);
                return a;
            }
        } else if (f == "--top-p") {
            const char* v = need("--top-p");
            if (!v) return a;
            if (!parseFloat(v, a.topP) || a.topP < 0.0f || a.topP > 1.0f) {
                a.error = true;
                a.errorMsg = "invalid --top-p value (must be 0.0..1.0): "
                             + std::string(v);
                return a;
            }
        } else if (f == "--seed") {
            const char* v = need("--seed");
            if (!v) return a;
            if (!parseUint64(v, a.seed)) {
                a.error = true;
                a.errorMsg = "invalid --seed value: " + std::string(v);
                return a;
            }
        } else if (f == "--tokens") {
            const char* v = need("--tokens");
            if (!v) return a;
            if (!parseInt(v, a.tokens) || a.tokens <= 0) {
                a.error = true;
                a.errorMsg = "invalid --tokens value: " + std::string(v);
                return a;
            }
        } else if (f == "--cuda") {
            a.useCuda = true;
        } else if (f == "--help" || f == "-h") {
            a.help = true;
            return a;
        } else {
            a.error = true;
            a.errorMsg = "unknown option: " + f;
            return a;
        }
    }
    return a;
}

Style parseStyle(const std::string& s) {
    if (s == "jazz"      || s == "j") return Style::Jazz;
    if (s == "classical" || s == "c") return Style::Classical;
    if (s == "ambient"   || s == "a") return Style::Ambient;
    return Style::Pop;   // "pop", "p", or anything else
}

// ---------------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------------

int cmdContinue(const Args& a) {
    if (a.inPath.empty()) {
        std::cerr << "error: continue requires --in <ctx.mid>\n";
        return 2;
    }
    if (a.outPath.empty()) {
        std::cerr << "error: continue requires --out <out.mid>\n";
        return 2;
    }
    if (a.modelPath.empty()) {
        std::cerr << "error: continue requires --model <model.onnx>\n";
        return 2;
    }

    notation::Score prompt;
    try {
        prompt = readMidi(a.inPath);
    } catch (const SmfError& e) {
        std::cerr << "error reading " << a.inPath << ": "
                  << e.what() << "\n";
        return 1;
    }

    SampleConfig sc;
    sc.temperature  = a.temperature;
    sc.topK         = a.topK;
    sc.topP         = a.topP;
    sc.seed         = a.seed;
    sc.maxNewTokens = a.tokens;
    sc.style        = a.style;
    sc.bars         = a.bars;

    try {
        NeuralComposer c(a.modelPath, a.useCuda);
        notation::Score out = c.continueFrom(prompt, sc);

        try {
            writeMidi(a.outPath, out);
        } catch (const SmfError& e) {
            std::cerr << "error writing " << a.outPath << ": "
                      << e.what() << "\n";
            return 1;
        }

        std::cout << "wrote " << a.outPath
                  << " (" << out.measures.size() << " measures)\n";
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}

int cmdGenerate(const Args& a) {
    if (a.outPath.empty()) {
        std::cerr << "error: generate requires --out <out.mid>\n";
        return 2;
    }
    if (a.modelPath.empty()) {
        std::cerr << "error: generate requires --model <model.onnx>\n";
        return 2;
    }

    SampleConfig sc;
    sc.temperature  = a.temperature;
    sc.topK         = a.topK;
    sc.topP         = a.topP;
    sc.seed         = a.seed;
    sc.maxNewTokens = a.tokens;
    sc.style        = a.style;
    sc.bars         = a.bars;

    try {
        NeuralComposer c(a.modelPath, a.useCuda);
        notation::Score out = c.generate(parseStyle(a.style),
                                         a.bars, sc);

        try {
            writeMidi(a.outPath, out);
        } catch (const SmfError& e) {
            std::cerr << "error writing " << a.outPath << ": "
                      << e.what() << "\n";
            return 1;
        }

        std::cout << "wrote " << a.outPath
                  << " (" << out.measures.size() << " measures)\n";
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
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
        if      (a.command == "continue") return cmdContinue(a);
        else if (a.command == "generate") return cmdGenerate(a);
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
