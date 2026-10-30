// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#include "smf.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>

namespace hush::composer {

namespace {

// ---------------------------------------------------------------------------
// Reader — bounds-checked, with unget for running-status handling
// ---------------------------------------------------------------------------

class Reader {
public:
    explicit Reader(const std::vector<uint8_t>& b) : buf_(b) {}

    uint8_t u8() {
        require(1);
        return buf_[pos_++];
    }
    uint16_t u16be() {
        require(2);
        const uint16_t v = (static_cast<uint16_t>(buf_[pos_])     << 8)
                         |  static_cast<uint16_t>(buf_[pos_ + 1]);
        pos_ += 2;
        return v;
    }
    uint32_t u32be() {
        require(4);
        const uint32_t v = (static_cast<uint32_t>(buf_[pos_])     << 24)
                         | (static_cast<uint32_t>(buf_[pos_ + 1]) << 16)
                         | (static_cast<uint32_t>(buf_[pos_ + 2]) <<  8)
                         |  static_cast<uint32_t>(buf_[pos_ + 3]);
        pos_ += 4;
        return v;
    }
    std::vector<uint8_t> bytes(size_t n) {
        require(n);
        std::vector<uint8_t> out(buf_.begin() + pos_,
                                 buf_.begin() + pos_ + n);
        pos_ += n;
        return out;
    }

    uint32_t vlq() {
        uint32_t value = 0;
        for (int i = 0; i < 4; ++i) {
            const uint8_t b = u8();
            value = (value << 7) | (b & 0x7Fu);
            if ((b & 0x80u) == 0) return value;
        }
        throw SmfError("VLQ longer than 4 bytes at offset "
                       + std::to_string(pos_));
    }

    /// Push one byte back. Used to undo a data-byte read when a
    /// running-status event is detected.
    void unget() {
        if (pos_ == 0) throw SmfError("unget with no prior read");
        --pos_;
    }

    void skip(size_t n) {
        require(n);
        pos_ += n;
    }
    size_t pos() const { return pos_; }
    size_t remaining() const { return buf_.size() - pos_; }

private:
    void require(size_t n) {
        if (pos_ + n > buf_.size()) {
            throw SmfError("read past end at offset "
                           + std::to_string(pos_) + " (need "
                           + std::to_string(n) + ", have "
                           + std::to_string(remaining()) + ")");
        }
    }
    const std::vector<uint8_t>& buf_;
    size_t pos_ = 0;
};

// ---------------------------------------------------------------------------
// Writer
// ---------------------------------------------------------------------------

class Writer {
public:
    void u8(uint8_t v) { buf_.push_back(v); }
    void u16be(uint16_t v) {
        buf_.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        buf_.push_back(static_cast<uint8_t>( v       & 0xFF));
    }
    void u32be(uint32_t v) {
        buf_.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
        buf_.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        buf_.push_back(static_cast<uint8_t>((v >>  8) & 0xFF));
        buf_.push_back(static_cast<uint8_t>( v        & 0xFF));
    }
    void tag(const char* s) {
        for (int i = 0; i < 4; ++i)
            buf_.push_back(static_cast<uint8_t>(s[i]));
    }
    void vlq(uint32_t v) {
        uint8_t tmp[5];
        int n = 0;
        tmp[n++] = static_cast<uint8_t>(v & 0x7Fu);
        v >>= 7;
        while (v) {
            tmp[n++] = static_cast<uint8_t>((v & 0x7Fu) | 0x80u);
            v >>= 7;
        }
        for (int i = n - 1; i >= 0; --i) buf_.push_back(tmp[i]);
    }
    std::vector<uint8_t>&       data()       { return buf_; }
    const std::vector<uint8_t>& data() const { return buf_; }

private:
    std::vector<uint8_t> buf_;
};

// ---------------------------------------------------------------------------
// Note collection
// ---------------------------------------------------------------------------

struct RawNote {
    int pitch;
    int velocity;
    int startTick;
    int endTick;
};

struct PendingNote {
    int pitch;
    int velocity;
    int startTick;
};

struct Event {
    uint8_t status   = 0;
    uint8_t data1    = 0;
    uint8_t data2    = 0;
    bool    hasData2 = false;
};

/// Read one event from a track. Handles running status by holding the
/// last status byte across calls.
Event readEvent(Reader& r, uint8_t& runningStatus, int trackIndex) {
    Event e;
    const uint8_t first = r.u8();
    if (first & 0x80u) {
        e.status = first;
        if (e.status < 0xF0u) runningStatus = e.status;
    } else {
        if (runningStatus == 0)
            throw SmfError("running status with no prior status in track "
                           + std::to_string(trackIndex));
        e.status = runningStatus;
        r.unget();
    }

    if (e.status >= 0xF0u) return e;

    const uint8_t type = e.status & 0xF0u;
    e.data1 = r.u8();
    // Program change (C0) and channel pressure (D0) have one data byte.
    if (type != 0xC0u && type != 0xD0u) {
        e.data2 = r.u8();
        e.hasData2 = true;
    }
    return e;
}

} // namespace

// ---------------------------------------------------------------------------
// Parse
// ---------------------------------------------------------------------------

notation::Score parseMidi(const std::vector<uint8_t>& bytes) {
    Reader r(bytes);

    {
        const auto hdr = r.bytes(4);
        if (hdr != std::vector<uint8_t>{'M','T','h','d'})
            throw SmfError("not a Standard MIDI File: missing MThd");
    }

    const uint32_t hdrLen = r.u32be();
    if (hdrLen != 6)
        throw SmfError("MThd length must be 6, got "
                       + std::to_string(hdrLen));

    const uint16_t format = r.u16be();
    if (format == 2)
        throw SmfError("SMF format 2 is not supported");
    if (format != 0 && format != 1)
        throw SmfError("unknown SMF format " + std::to_string(format));

    const uint16_t nTracks  = r.u16be();
    const uint16_t division = r.u16be();
    if (division & 0x8000u)
        throw SmfError("SMPTE time division is not supported");
    const int ppq = division;

    notation::Score score;
    score.ppq = ppq;

    std::vector<RawNote> allNotes;
    int firstTempo = 120;

    for (uint16_t t = 0; t < nTracks; ++t) {
        {
            const auto tag = r.bytes(4);
            if (tag != std::vector<uint8_t>{'M','T','r','k'})
                throw SmfError("expected MTrk at track "
                               + std::to_string(t));
        }
        const uint32_t len = r.u32be();
        const size_t trackStart = r.pos();
        const size_t trackEnd   = trackStart + len;
        if (trackEnd > bytes.size())
            throw SmfError("MTrk at track " + std::to_string(t)
                           + " runs past end of file");

        std::vector<PendingNote> pending;
        uint32_t tick = 0;
        uint8_t  runningStatus = 0;

        while (r.pos() < trackEnd) {
            tick += r.vlq();
            const Event e = readEvent(r, runningStatus, t);

            if (e.status == 0xFFu) {
                const uint8_t  metaType = r.u8();
                const uint32_t metaLen  = r.vlq();
                const auto     meta     = r.bytes(metaLen);
                if (metaType == 0x51 && metaLen == 3) {
                    const uint32_t usPerQuarter =
                        (static_cast<uint32_t>(meta[0]) << 16)
                      | (static_cast<uint32_t>(meta[1]) <<  8)
                      |  static_cast<uint32_t>(meta[2]);
                    if (usPerQuarter > 0) {
                        firstTempo = static_cast<int>(
                            (60000000.0 / usPerQuarter) + 0.5);
                    }
                }
                continue;
            }

            if (e.status == 0xF0u || e.status == 0xF7u) {
                const uint32_t sysexLen = r.vlq();
                r.skip(sysexLen);
                continue;
            }

            const uint8_t type = e.status & 0xF0u;

            if (type == 0x90u) {          // note-on
                if (e.data2 > 0) {
                    pending.push_back(PendingNote{
                        e.data1, e.data2, static_cast<int>(tick)});
                } else {
                    // note-on with velocity 0 is note-off
                    auto it = std::find_if(
                        pending.rbegin(), pending.rend(),
                        [&](const PendingNote& p) {
                            return p.pitch == e.data1;
                        });
                    if (it != pending.rend()) {
                        allNotes.push_back(RawNote{
                            it->pitch, it->velocity, it->startTick,
                            static_cast<int>(tick)});
                        pending.erase(std::next(it).base());
                    }
                }
            } else if (type == 0x80u) {   // note-off
                auto it = std::find_if(
                    pending.rbegin(), pending.rend(),
                    [&](const PendingNote& p) {
                        return p.pitch == e.data1;
                    });
                if (it != pending.rend()) {
                    allNotes.push_back(RawNote{
                        it->pitch, it->velocity, it->startTick,
                        static_cast<int>(tick)});
                    pending.erase(std::next(it).base());
                }
            }
        }

        // Orphan notes: a note-on with no matching note-off inside
        // this track is closed at the track's final tick. This keeps
        // truncated or malformed files parseable rather than
        // silently dropping their content.
        for (const auto& p : pending) {
            allNotes.push_back(RawNote{
                p.pitch, p.velocity, p.startTick,
                static_cast<int>(tick)});
        }

        r.skip(trackEnd - r.pos());
    }

    score.tempoBpm = firstTempo;

    // Sort by start tick, then pitch, for deterministic measure layout.
    std::sort(allNotes.begin(), allNotes.end(),
              [](const RawNote& a, const RawNote& b) {
                  if (a.startTick != b.startTick)
                      return a.startTick < b.startTick;
                  return a.pitch < b.pitch;
              });

    const int ticksPerMeasure = ppq * 4;
    int lastEnd = 0;
    for (const auto& n : allNotes)
        lastEnd = std::max(lastEnd, n.endTick);

    const int nMeasures = std::max(
        1, (lastEnd + ticksPerMeasure - 1) / ticksPerMeasure);

    score.measures.resize(static_cast<size_t>(nMeasures));
    for (auto& m : score.measures) m.ticksPerMeasure = ticksPerMeasure;

    for (const auto& n : allNotes) {
        const int mIndex = n.startTick / ticksPerMeasure;
        if (mIndex < 0 || mIndex >= nMeasures) continue;
        const int startInMeasure = n.startTick - mIndex * ticksPerMeasure;
        const int dur = std::max(1, n.endTick - n.startTick);

        notation::Note nn;
        nn.pitch         = n.pitch;
        nn.velocity      = n.velocity;
        nn.durationTicks = dur;
        nn.startTick     = startInMeasure;
        score.measures[static_cast<size_t>(mIndex)].notes.push_back(nn);
    }

    return score;
}

// ---------------------------------------------------------------------------
// Serialise
// ---------------------------------------------------------------------------

std::vector<uint8_t> serialiseMidi(const notation::Score& score) {
    const int ppq = score.ppq > 0 ? score.ppq : 480;
    const int usPerQuarter =
        score.tempoBpm > 0
            ? static_cast<int>(60000000.0 / score.tempoBpm + 0.5)
            : 500000;

    struct AbsNote {
        int start;
        int end;
        int pitch;
        int velocity;
    };
    std::vector<AbsNote> notes;
    int absTick = 0;
    for (const auto& m : score.measures) {
        for (const auto& n : m.notes) {
            AbsNote a;
            a.start    = absTick + n.startTick;
            a.end      = a.start + std::max(1, n.durationTicks);
            a.pitch    = std::clamp(n.pitch, 0, 127);
            a.velocity = std::clamp(n.velocity, 1, 127);
            notes.push_back(a);
        }
        absTick += m.ticksPerMeasure;
    }

    struct Ev {
        int     tick;
        bool    isOn;
        uint8_t pitch;
        uint8_t velocity;
    };
    std::vector<Ev> evs;
    evs.reserve(notes.size() * 2);
    for (const auto& n : notes) {
        evs.push_back({n.start, true,
                       static_cast<uint8_t>(n.pitch),
                       static_cast<uint8_t>(n.velocity)});
        evs.push_back({n.end, false,
                       static_cast<uint8_t>(n.pitch), 0});
    }
    std::sort(evs.begin(), evs.end(), [](const Ev& a, const Ev& b) {
        if (a.tick != b.tick) return a.tick < b.tick;
        return a.isOn < b.isOn;   // off before on at the same tick
    });

    Writer track;
    {
        // Track name (empty).
        track.vlq(0);
        track.u8(0xFF); track.u8(0x03); track.vlq(0);

        // Set tempo.
        track.vlq(0);
        track.u8(0xFF); track.u8(0x51); track.vlq(3);
        track.u8(static_cast<uint8_t>((usPerQuarter >> 16) & 0xFF));
        track.u8(static_cast<uint8_t>((usPerQuarter >>  8) & 0xFF));
        track.u8(static_cast<uint8_t>( usPerQuarter        & 0xFF));

        int last = 0;
        for (const auto& e : evs) {
            track.vlq(static_cast<uint32_t>(e.tick - last));
            last = e.tick;
            if (e.isOn) {
                track.u8(0x90); track.u8(e.pitch); track.u8(e.velocity);
            } else {
                track.u8(0x80); track.u8(e.pitch); track.u8(0x00);
            }
        }

        // End of track.
        track.vlq(0);
        track.u8(0xFF); track.u8(0x2F); track.vlq(0);
    }

    Writer out;
    out.tag("MThd");
    out.u32be(6);
    out.u16be(0);                              // format 0
    out.u16be(1);                              // one track
    out.u16be(static_cast<uint16_t>(ppq));

    out.tag("MTrk");
    out.u32be(static_cast<uint32_t>(track.data().size()));
    for (uint8_t b : track.data()) out.u8(b);

    return out.data();
}

// ---------------------------------------------------------------------------
// File wrappers
// ---------------------------------------------------------------------------

notation::Score readMidi(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        throw SmfError("cannot open " + path);

    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());
    if (bytes.empty())
        throw SmfError(path + " is empty");
    return parseMidi(bytes);
}

void writeMidi(const std::string& path, const notation::Score& score) {
    const auto bytes = serialiseMidi(score);
    std::ofstream out(path, std::ios::binary);
    if (!out)
        throw SmfError("cannot write " + path);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    if (!out)
        throw SmfError("write failed for " + path);
}

} // namespace hush::composer
