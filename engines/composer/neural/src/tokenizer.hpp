// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#pragma once
#include "hush/notation/score.hpp"

#include <stdexcept>
#include <string>
#include <vector>

namespace hush::composer::neural {

// ---------------------------------------------------------------------------
// Fixed token layout — the contract between the tokenizer and the
// ONNX model. Changing any index requires regenerating the model.
//
//   0        BOS
//   1        EOS
//   2        STYLE (followed by a single style byte)
//   3        BAR  (precedes every measure, including the first)
//   4..131   MIDI pitch 0..127
//   132..139 duration bucket 0..7
//   140..143 velocity bucket 0..3
//   144      REST
//   145      END (reserved)
//   146      PAD  (trailing only; ignored by decoder)
//
// Total vocab: 147.
// ---------------------------------------------------------------------------

constexpr int kVocabSize   = 147;
constexpr int kTokBos      = 0;
constexpr int kTokEos      = 1;
constexpr int kTokStyle    = 2;
constexpr int kTokBar      = 3;
constexpr int kTokPitchBase = 4;
constexpr int kTokDurBase  = 132;
constexpr int kTokVelBase  = 140;
constexpr int kTokRest     = 144;
constexpr int kTokEnd      = 145;
constexpr int kTokPad      = 146;

// Style bytes used after the STYLE token.
constexpr int kStylePop       = 'p';
constexpr int kStyleJazz      = 'j';
constexpr int kStyleClassical = 'c';
constexpr int kStyleAmbient   = 'a';

/// Thrown on malformed token streams.
class TokenizerError : public std::invalid_argument {
public:
    explicit TokenizerError(const std::string& msg)
        : std::invalid_argument(msg) {}
};

/// Encode a Score into a token stream.
/// Layout: [BOS][STYLE s][BAR][pitch dur vel]* ...[BAR]...[EOS][PAD]*
/// If the encoding exceeds `maxTokens`, it is truncated and closed
/// with EOS so the stream remains well-formed.
std::vector<int> tokenize(const notation::Score& score,
                          int maxTokens,
                          int styleByte);

/// Decode a token stream. Strict: throws TokenizerError on any
/// structural violation with the offending index.
notation::Score detokenize(const std::vector<int>& tokens);

/// Duration bucket index 0..7 for a tick count at the given PPQ.
int durationBucket(int durationTicks, int ppq);

/// Tick count for a duration bucket at the given PPQ.
int bucketToTicks(int bucket, int ppq);

/// Velocity bucket 0..3 for a MIDI velocity 1..127.
int velocityBucket(int velocity);

/// Representative velocity for a bucket 0..3.
int bucketToVelocity(int bucket);

} // namespace hush::composer::neural
