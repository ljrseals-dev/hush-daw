// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#include "hush/daw_core/session.hpp"
#include <hush/immersive/renderer.hpp>
#include <hush/score/score.hpp>
#include <hush/plugins/plugin_host.hpp>
#include <iostream>
#include <cstdlib>
#include <fstream>
#include <cstdio>
#include <cmath>

#define CHECK(cond) do { \
    if (!(cond)) { \
        std::cerr << "FAIL: " << #cond << " at " << __FILE__ << ":" << __LINE__ << "\n"; \
        std::exit(1); \
    } \
} while (0)

int main() {
    using namespace hush::daw_core;

    // ------------------------------------------------------------------
    // Happy path: create → configure → save → load
    // ------------------------------------------------------------------
    {
        Session s;
        s.setSampleRate(48000.0);
        s.setLength(96000);
        CHECK(s.sampleRate() == 48000.0);
        CHECK(s.length() == 96000);

        session_io::save(s, "test_session.hushsess");
        Session t = session_io::load("test_session.hushsess");
        CHECK(t.sampleRate() == 48000.0);
        CHECK(t.length() == 96000);
        std::remove("test_session.hushsess");
    }

    // ------------------------------------------------------------------
    // Edge: zero-length session — renderToWav must fail
    // ------------------------------------------------------------------
    {
        Session s;
        s.setSampleRate(44100.0);
        s.setLength(0);
        bool threw = false;
        try { s.renderToWav("should_fail.wav"); } catch (...) { threw = true; }
        CHECK(threw);
    }

    // ------------------------------------------------------------------
    // Failure mode: negative sample rate
    // ------------------------------------------------------------------
    {
        Session s;
        bool threw = false;
        try { s.setSampleRate(-1.0); } catch (const std::invalid_argument&) { threw = true; }
        CHECK(threw);
    }

    // ------------------------------------------------------------------
    // Failure mode: bad magic on load
    // ------------------------------------------------------------------
    {
        {
            std::ofstream ofs("bad.hushsess", std::ios::binary);
            ofs.write("BADMAGIC", 8);
        }
        bool threw = false;
        try { session_io::load("bad.hushsess"); } catch (...) { threw = true; }
        CHECK(threw);
        std::remove("bad.hushsess");
    }

    // ------------------------------------------------------------------
    // Transport
    // ------------------------------------------------------------------
    {
        Transport tr;
        tr.setSampleRate(48000.0);
        tr.setPosition(100);
        CHECK(tr.position() == 100);
        tr.play();
        CHECK(tr.state() == TransportState::Playing);
        auto adv = tr.advance(50);
        CHECK(adv.begin == 100);
        CHECK(adv.end   == 150);
        CHECK(tr.position() == 150);
        tr.stop();
        CHECK(tr.state() == TransportState::Stopped);
    }

    // ------------------------------------------------------------------
    // Transport loop
    // ------------------------------------------------------------------
    {
        Transport tr;
        tr.setSampleRate(48000.0);
        tr.setLoop(0, 1000);
        tr.setPosition(900);
        auto adv = tr.advance(200);
        CHECK(adv.looped == true);
        CHECK(adv.end == 1000);
        CHECK(tr.position() == 100);
    }

    // ------------------------------------------------------------------
    // Full integration: immersive + score + plugin → non-silent render
    // ------------------------------------------------------------------
    {
        Session s;
        s.setSampleRate(48000.0);
        s.setLength(4800);               // 100 ms
        s.setMasterChannels(2);

        auto scene = s.createImmersiveScene();
        hush::immersive::Object obj;
        obj.x = 0.3f;
        obj.gain = 1.0f;
        s.immersive(scene).addObject(obj);

        auto sc = s.createScore();
        hush::score::Note n;
        n.pitch = 69;                    // A4
        n.start = 0;
        n.duration = 4800;
        n.velocity = 0.8f;
        s.score(sc).addNote(n);

        auto chain = s.createPluginChain();
        s.plugins(chain).setGainDb(0.0f);

        s.rebuildGraph();
        auto buf = s.render(0, 4800);
        CHECK(buf.channels == 2);
        CHECK(buf.frames == 4800);
        CHECK(buf.data.size() == 9600);

        // Must contain energy (not pure silence)
        float energy = 0.f;
        for (float v : buf.data) energy += v * v;
        CHECK(energy > 1e-6f);
    }

    // ------------------------------------------------------------------
    // RenderGraph empty path produces silence
    // ------------------------------------------------------------------
    {
        RenderGraph g;
        g.setMasterChannels(2);
        auto buf = g.render(0, 128);
        CHECK(buf.channels == 2);
        CHECK(buf.frames == 128);
        for (float v : buf.data) CHECK(v == 0.f);
    }

    // ------------------------------------------------------------------
    // Plugin state round-trip
    // ------------------------------------------------------------------
    {
        hush::plugins::PluginHost host;
        host.setGainDb(-6.0f);
        auto state = host.saveState();
        host.setGainDb(0.0f);
        host.restoreState(state);
        CHECK(std::abs(host.gainDb() + 6.0f) < 0.01f);
    }

    std::cout << "All daw_core tests passed\n";
    return 0;
}
