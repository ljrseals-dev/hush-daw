// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#pragma once
#include "hush/notation/score.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace hush::composer {

/// Errors from MIDI I/O. Always carries a message naming the byte
/// offset or the field that failed.
class SmfError : public std::runtime_error {
public:
    explicit SmfError(const std::string& msg)
        : std::runtime_error(msg) {}
};

/// Read a Standard MIDI File from disk. Supports format 0 and 1.
/// Format 2 is rejected (its semantics do not map onto one Score).
/// All tracks are merged; notes sorted by start tick then pitch.
notation::Score readMidi(const std::string& path);

/// Serialize a Score as a format-0 SMF and write it to disk.
/// PPQ and tempo come from the Score. Running status is not used on
/// write (every event carries its full status byte).
void writeMidi(const std::string& path, const notation::Score& score);

/// In-memory variants. Useful for tests and round-tripping without
/// touching the filesystem.
notation::Score      parseMidi(const std::vector<uint8_t>& bytes);
std::vector<uint8_t> serialiseMidi(const notation::Score& score);

} // namespace hush::composer
