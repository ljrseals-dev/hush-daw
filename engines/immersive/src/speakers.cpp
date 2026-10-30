// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#include "hush/immersive/speakers.hpp"
#include <cmath>
#include <stdexcept>

namespace hush {
namespace immersive {

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kDeg2Rad = kPi / 180.f;

Speaker makeSpeaker(const char* id, float azDeg, float elDeg, bool lfe = false) {
  Speaker s;
  s.id = id;
  s.azimuthDeg = azDeg;
  s.elevationDeg = elDeg;
  s.isLfe = lfe;
  s.isAmbisonic = false;
  const float az = azDeg * kDeg2Rad;
  const float el = elDeg * kDeg2Rad;
  const float cosEl = std::cos(el);
  s.x = cosEl * std::sin(az);
  s.y = cosEl * std::cos(az);
  s.z = std::sin(el);
  return s;
}

Speaker makeAmbisonic(const char* id) {
  Speaker s;
  s.id = id;
  s.azimuthDeg = 0.f;
  s.elevationDeg = 0.f;
  s.x = 0.f;
  s.y = 0.f;
  s.z = 0.f;
  s.isLfe = false;
  s.isAmbisonic = true;
  return s;
}

const std::vector<Speaker>& layoutFor(ChannelFormat f) {
  static const std::vector<Speaker> mono = {makeSpeaker("M", 0.f, 0.f)};
  static const std::vector<Speaker> stereo = {
    makeSpeaker("L", 30.f, 0.f), makeSpeaker("R", -30.f, 0.f)};
  static const std::vector<Speaker> lcr = {
    makeSpeaker("L", 30.f, 0.f), makeSpeaker("C", 0.f, 0.f), makeSpeaker("R", -30.f, 0.f)};
  static const std::vector<Speaker> quad = {
    makeSpeaker("L", 30.f, 0.f), makeSpeaker("R", -30.f, 0.f),
    makeSpeaker("Ls", 90.f, 0.f), makeSpeaker("Rs", -90.f, 0.f)};
  static const std::vector<Speaker> fivePointOne = {
    makeSpeaker("L", 30.f, 0.f), makeSpeaker("R", -30.f, 0.f),
    makeSpeaker("C", 0.f, 0.f), makeSpeaker("Lfe", 0.f, -90.f, true),
    makeSpeaker("Ls", 90.f, 0.f), makeSpeaker("Rs", -90.f, 0.f)};
  static const std::vector<Speaker> sevenPointOne = {
    makeSpeaker("L", 30.f, 0.f), makeSpeaker("R", -30.f, 0.f),
    makeSpeaker("C", 0.f, 0.f), makeSpeaker("Lfe", 0.f, -90.f, true),
    makeSpeaker("Ls", 90.f, 0.f), makeSpeaker("Rs", -90.f, 0.f),
    makeSpeaker("Lrs", 150.f, 0.f), makeSpeaker("Rrs", -150.f, 0.f)};
  static const std::vector<Speaker> fivePointOneFour = {
    makeSpeaker("L", 30.f, 0.f), makeSpeaker("R", -30.f, 0.f),
    makeSpeaker("C", 0.f, 0.f), makeSpeaker("Lfe", 0.f, -90.f, true),
    makeSpeaker("Ls", 90.f, 0.f), makeSpeaker("Rs", -90.f, 0.f),
    makeSpeaker("Ltf", 45.f, 45.f), makeSpeaker("Rtf", -45.f, 45.f),
    makeSpeaker("Ltr", 135.f, 45.f), makeSpeaker("Rtr", -135.f, 45.f)};
  static const std::vector<Speaker> sevenPointOneFour = {
    makeSpeaker("L", 30.f, 0.f), makeSpeaker("R", -30.f, 0.f),
    makeSpeaker("C", 0.f, 0.f), makeSpeaker("Lfe", 0.f, -90.f, true),
    makeSpeaker("Ls", 90.f, 0.f), makeSpeaker("Rs", -90.f, 0.f),
    makeSpeaker("Lrs", 150.f, 0.f), makeSpeaker("Rrs", -150.f, 0.f),
    makeSpeaker("Ltf", 45.f, 45.f), makeSpeaker("Rtf", -45.f, 45.f),
    makeSpeaker("Ltr", 135.f, 45.f), makeSpeaker("Rtr", -135.f, 45.f)};
  static const std::vector<Speaker> ninePointOneSix = {
    makeSpeaker("L", 30.f, 0.f), makeSpeaker("R", -30.f, 0.f),
    makeSpeaker("C", 0.f, 0.f), makeSpeaker("Lfe", 0.f, -90.f, true),
    makeSpeaker("Lw", 60.f, 0.f), makeSpeaker("Rw", -60.f, 0.f),
    makeSpeaker("Ls", 90.f, 0.f), makeSpeaker("Rs", -90.f, 0.f),
    makeSpeaker("Lrs", 150.f, 0.f), makeSpeaker("Rrs", -150.f, 0.f),
    makeSpeaker("Ltf", 45.f, 45.f), makeSpeaker("Rtf", -45.f, 45.f),
    makeSpeaker("Ltm", 90.f, 45.f), makeSpeaker("Rtm", -90.f, 45.f),
    makeSpeaker("Ltr", 135.f, 45.f), makeSpeaker("Rtr", -135.f, 45.f)};
  static const std::vector<Speaker> foa = {
    makeAmbisonic("W"), makeAmbisonic("Y"), makeAmbisonic("Z"), makeAmbisonic("X")};
  static const std::vector<Speaker> soa = {
    makeAmbisonic("W"), makeAmbisonic("Y"), makeAmbisonic("Z"), makeAmbisonic("X"),
    makeAmbisonic("V"), makeAmbisonic("T"), makeAmbisonic("R"), makeAmbisonic("S"),
    makeAmbisonic("U")};
  static const std::vector<Speaker> hoa3 = {
    makeAmbisonic("ACN0"),  makeAmbisonic("ACN1"),  makeAmbisonic("ACN2"),  makeAmbisonic("ACN3"),
    makeAmbisonic("ACN4"),  makeAmbisonic("ACN5"),  makeAmbisonic("ACN6"),  makeAmbisonic("ACN7"),
    makeAmbisonic("ACN8"),  makeAmbisonic("ACN9"),  makeAmbisonic("ACN10"), makeAmbisonic("ACN11"),
    makeAmbisonic("ACN12"), makeAmbisonic("ACN13"), makeAmbisonic("ACN14"), makeAmbisonic("ACN15")};

  switch (f) {
    case ChannelFormat::Mono:              return mono;
    case ChannelFormat::Stereo:            return stereo;
    case ChannelFormat::LCR:               return lcr;
    case ChannelFormat::Quad:              return quad;
    case ChannelFormat::FivePointOne:      return fivePointOne;
    case ChannelFormat::SevenPointOne:     return sevenPointOne;
    case ChannelFormat::FivePointOneFour:  return fivePointOneFour;
    case ChannelFormat::SevenPointOneFour: return sevenPointOneFour;
    case ChannelFormat::NinePointOneSix:   return ninePointOneSix;
    case ChannelFormat::AmbisonicsFOA:     return foa;
    case ChannelFormat::AmbisonicsSOA:     return soa;
    case ChannelFormat::AmbisonicsHOA3:    return hoa3;
    default:
      throw std::invalid_argument("unknown ChannelFormat");
  }
}

}  // namespace

int channelCount(ChannelFormat format) {
  return static_cast<int>(layoutFor(format).size());
}

const std::vector<Speaker>& speakersFor(ChannelFormat format) {
  return layoutFor(format);
}

Speaker speakerFor(ChannelFormat format, int index) {
  const auto& lay = layoutFor(format);
  if (index < 0 || index >= static_cast<int>(lay.size())) {
    throw std::invalid_argument("speaker index out of range");
  }
  return lay[static_cast<std::size_t>(index)];
}

}  // namespace immersive
}  // namespace hush
