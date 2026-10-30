// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#pragma once
#include "harmony.hpp"

#include <string>

namespace hush::composer {

/// Result of a style load. `fromFile` is true if the tables came from
/// a user-provided directory. False if the built-in tables were used
/// (either because no dir was given, or because the file for the
/// requested style was absent or malformed).
struct LoadedStyle {
    StyleTables tables;
    std::string style;           // normalized lower-case name
    bool        fromFile = false;
    std::string sourcePath;      // empty when fromFile == false
    std::string fallbackReason;  // non-empty when a file load failed
};

/// Load a style table. If `styleDir` is non-empty, look for
/// <styleDir>/<style>.txt and parse it. On any failure — directory
/// missing, file missing, file malformed — fall back to the built-in
/// table for that style and record the reason.
///
/// Never throws. A bad directory is a soft failure.
LoadedStyle loadStyle(const std::string& styleDir,
                      const std::string& style);

/// Parse one transition line:
///     transition <from> <to> <weight>
/// `from` and `to` are Roman numerals. Returns true on success. On
/// failure, `errorOut` names the problem.
bool parseTransitionLine(const std::string& line,
                         int& fromDegree,
                         int& toDegree,
                         float& weight,
                         std::string& errorOut);

/// Roman numeral to degree in [0,6], or -1 if unrecognized. Accepts
/// both the major and minor spellings, plus the UTF-8 degree sign.
int romanToDegree(const std::string& roman);

} // namespace hush::composer
