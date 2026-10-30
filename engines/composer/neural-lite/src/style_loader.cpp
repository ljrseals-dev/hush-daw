// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#include "style_loader.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>

namespace hush::composer {

// ---------------------------------------------------------------------------
// Roman numeral parsing
// ---------------------------------------------------------------------------
//
// Major: I ii iii IV V vi vii°
// Minor: i ii° III iv v VI VII
//
// We accept both spellings regardless of the surrounding key, because
// a style file is written before the key is known and may legitimately
// mix conventions.

namespace {

struct RomanEntry {
    const char* text;
    int degree;
};

const RomanEntry kRomanTable[] = {
    // Major
    {"I",     0}, {"ii",   1}, {"iii",   2}, {"IV",    3},
    {"V",     4}, {"vi",   5}, {"vii",   6},
    // Minor
    {"i",     0}, {"ii°",  1}, {"III",   2}, {"iv",    3},
    {"v",     4}, {"VI",   5}, {"VII",   6},
    // Common alternates
    {"II",    1}, {"III+", 2}, {"iv°",   3},
};

std::string lower(std::string s) {
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

/// Remove U+00B0 (degree sign), which is 0xC2 0xB0 in UTF-8. Some
/// style files include it, some do not. Normalizing both sides means
/// "vii°" and "vii" both match the same entry.
std::string stripDegreeSign(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (i + 1 < s.size() &&
            static_cast<unsigned char>(s[i])     == 0xC2 &&
            static_cast<unsigned char>(s[i + 1]) == 0xB0) {
            i += 1;
            continue;
        }
        out += s[i];
    }
    return out;
}

} // namespace

int romanToDegree(const std::string& roman) {
    const std::string stripped = stripDegreeSign(roman);
    for (const auto& e : kRomanTable) {
        if (stripDegreeSign(e.text) == stripped) return e.degree;
    }
    return -1;
}

// ---------------------------------------------------------------------------
// Line parsing
// ---------------------------------------------------------------------------

bool parseTransitionLine(const std::string& line,
                         int& fromDegree,
                         int& toDegree,
                         float& weight,
                         std::string& errorOut) {
    std::istringstream is(line);
    std::string kw, fromTok, toTok, weightTok;

    if (!(is >> kw)) { errorOut = "empty line"; return false; }
    if (kw != "transition") {
        errorOut = "expected keyword 'transition', got '" + kw + "'";
        return false;
    }
    if (!(is >> fromTok)) { errorOut = "missing <from>"; return false; }
    if (!(is >> toTok))   { errorOut = "missing <to>";   return false; }
    if (!(is >> weightTok)) { errorOut = "missing <weight>"; return false; }

    fromDegree = romanToDegree(fromTok);
    if (fromDegree < 0) {
        errorOut = "unrecognized from-degree '" + fromTok + "'";
        return false;
    }
    toDegree = romanToDegree(toTok);
    if (toDegree < 0) {
        errorOut = "unrecognized to-degree '" + toTok + "'";
        return false;
    }

    try {
        size_t consumed = 0;
        weight = std::stof(weightTok, &consumed);
        if (consumed != weightTok.size()) {
            errorOut = "trailing characters in weight '" + weightTok + "'";
            return false;
        }
    } catch (const std::exception&) {
        errorOut = "weight is not a number: '" + weightTok + "'";
        return false;
    }

    if (!std::isfinite(weight) || weight < 0.0f) {
        errorOut = "weight must be finite and non-negative";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// File loading
// ---------------------------------------------------------------------------

namespace {

void applyTransition(StyleTables& t, int from, int to, float weight) {
    if (from < 0 || from > 6 || to < 0 || to > 6) return;
    t.w[from][to] = weight;
}

bool loadFromFile(const std::string& path,
                  StyleTables& outTables,
                  std::string& errorOut) {
    std::ifstream in(path);
    if (!in) {
        errorOut = "cannot open " + path;
        return false;
    }

    // Start from a zero table. Rows that the file does not name are
    // filled with the uniform distribution by normalizedRow(), so an
    // incomplete file degrades gracefully rather than producing a
    // row that sums to zero.
    StyleTables t{};

    std::string line;
    int lineNo = 0;
    int parsed = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        const std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#') continue;

        int from = -1, to = -1;
        float w = 0.0f;
        std::string err;
        if (!parseTransitionLine(trimmed, from, to, w, err)) {
            std::ostringstream os;
            os << path << ":" << lineNo << ": " << err;
            errorOut = os.str();
            return false;
        }
        applyTransition(t, from, to, w);
        ++parsed;
    }

    if (parsed == 0) {
        errorOut = path + ": no transition lines found";
        return false;
    }

    outTables = t;
    return true;
}

} // namespace

LoadedStyle loadStyle(const std::string& styleDir,
                      const std::string& style) {
    LoadedStyle result;
    result.style = lower(style.empty() ? std::string("pop") : style);

    // Always start from the built-in table. A successful file load
    // replaces it.
    result.tables   = styleTable(result.style);
    result.fromFile = false;

    if (styleDir.empty()) return result;

    const std::string path = styleDir + "/" + result.style + ".txt";

    {
        std::ifstream probe(path);
        if (!probe) {
            result.fallbackReason = "no override file at " + path;
            return result;
        }
    }

    StyleTables fileTables{};
    std::string err;
    if (!loadFromFile(path, fileTables, err)) {
        // Log once. A malformed file is a soft failure; the built-in
        // table is the correct behavior, and it should not spam on
        // every call.
        std::cerr << "[hush::composer] style override rejected: "
                  << err << " — using built-in '" << result.style
                  << "' table\n";
        result.fallbackReason = err;
        return result;
    }

    result.tables     = fileTables;
    result.fromFile   = true;
    result.sourcePath = path;
    return result;
}

} // namespace hush::composer
