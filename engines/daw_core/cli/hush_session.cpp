// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#include "hush/daw_core/session.hpp"
#include <hush/immersive/renderer.hpp>
#include <hush/score/score.hpp>
#include <hush/plugins/plugin_host.hpp>
#include <iostream>
#include <string>

static void usage() {
    std::cout
        << "hush-session — Hush DAW session engine CLI (complete umbrella)\n"
        << "Usage:\n"
        << "  hush-session --help\n"
        << "  hush-session new    <file.hushsess> [--rate 48000] [--length 480000]\n"
        << "  hush-session info   <file.hushsess>\n"
        << "  hush-session render <file.hushsess> <out.wav>\n"
        << "  hush-session demo   <out.wav>          # creates a short demo session and renders it\n"
        << std::endl;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        usage();
        return 0;
    }

    const std::string cmd = argv[1];
    if (cmd == "--help" || cmd == "-h") {
        usage();
        return 0;
    }

    try {
        if (cmd == "new" && argc >= 3) {
            hush::daw_core::Session s;
            double  rate = 48000.0;
            int64_t len  = 480000;

            for (int i = 3; i < argc; ++i) {
                const std::string a = argv[i];
                if (a == "--rate" && i + 1 < argc)
                    rate = std::stod(argv[++i]);
                else if (a == "--length" && i + 1 < argc)
                    len = std::stoll(argv[++i]);
            }

            s.setSampleRate(rate);
            s.setLength(len);
            hush::daw_core::session_io::save(s, argv[2]);
            std::cout << "Created " << argv[2]
                      << " (" << len << " frames @ " << rate << " Hz)\n";
            return 0;
        }

        if (cmd == "info" && argc >= 3) {
            auto s = hush::daw_core::session_io::load(argv[2]);
            std::cout << "Sample rate : " << s.sampleRate() << "\n"
                      << "Length      : " << s.length() << " frames\n"
                      << "Duration    : " << (s.length() / s.sampleRate()) << " s\n";
            return 0;
        }

        if (cmd == "render" && argc >= 4) {
            auto s = hush::daw_core::session_io::load(argv[2]);
            s.renderToWav(argv[3]);
            std::cout << "Wrote " << argv[3] << "\n";
            return 0;
        }

        if (cmd == "demo" && argc >= 3) {
            hush::daw_core::Session s;
            s.setSampleRate(48000.0);
            s.setLength(48000 * 2);          // 2 seconds
            s.setMasterChannels(2);

            // Immersive object slightly left
            auto scene = s.createImmersiveScene("main");
            hush::immersive::Object obj;
            obj.x = -0.4f;
            obj.gain = 0.8f;
            obj.name = "tone";
            s.immersive(scene).addObject(obj);

            // A simple C-major arpeggio
            auto sc = s.createScore("arp");
            const int pitches[] = {60, 64, 67, 72};
            for (int i = 0; i < 4; ++i) {
                hush::score::Note n;
                n.pitch    = pitches[i];
                n.start    = i * 12000;
                n.duration = 10000;
                n.velocity = 0.7f;
                s.score(sc).addNote(n);
            }

            // Mild gain plugin
            auto chain = s.createPluginChain("master");
            s.plugins(chain).setGainDb(-1.5f);

            s.rebuildGraph();
            s.renderToWav(argv[2]);
            std::cout << "Wrote demo " << argv[2] << " (2 s stereo)\n";
            return 0;
        }

        usage();
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
