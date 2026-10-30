// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#include "hush/daw_core/session_io.hpp"
#include "hush/daw_core/session.hpp"

#include <fstream>
#include <cstring>
#include <stdexcept>

namespace hush::daw_core {
namespace session_io {

namespace {

void writeExact(std::ostream& os, const void* data, size_t n) {
    os.write(static_cast<const char*>(data), static_cast<std::streamsize>(n));
    if (!os)
        throw std::runtime_error("session_io: write failed");
}

void readExact(std::istream& is, void* data, size_t n) {
    is.read(static_cast<char*>(data), static_cast<std::streamsize>(n));
    if (!is || is.gcount() != static_cast<std::streamsize>(n))
        throw std::runtime_error("session_io: read failed or truncated");
}

} // namespace

void save(const Session& session, const std::string& path) {
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs)
        throw std::runtime_error("session_io::save: cannot open " + path);

    SessionHeader hdr;
    hdr.sampleRate   = session.sampleRate();
    hdr.lengthFrames = session.length();
    writeExact(ofs, &hdr, sizeof(hdr));
}

Session load(const std::string& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs)
        throw std::runtime_error("session_io::load: cannot open " + path);

    SessionHeader hdr;
    readExact(ifs, &hdr, sizeof(hdr));

    if (std::memcmp(hdr.magic, "HUSHSESS", 8) != 0)
        throw std::runtime_error("session_io::load: bad magic");
    if (hdr.version != 1)
        throw std::runtime_error("session_io::load: unsupported version");

    Session s;
    s.setSampleRate(hdr.sampleRate);
    s.setLength(hdr.lengthFrames);
    return s;
}

} // namespace session_io
} // namespace hush::daw_core
