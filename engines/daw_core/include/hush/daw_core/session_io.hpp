// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#pragma once

#include "types.hpp"
#include <string>
#include <cstdint>

namespace hush::daw_core {

struct SessionHeader {
    char     magic[8]     = {'H','U','S','H','S','E','S','S'};
    uint32_t version      = 1;
    uint32_t flags        = 0;
    double   sampleRate   = 48000.0;
    int64_t  lengthFrames = 0;
};

class Session;

namespace session_io {
void save(const Session& session, const std::string& path);
Session load(const std::string& path);
} // namespace session_io

} // namespace hush::daw_core
