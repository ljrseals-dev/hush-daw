// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

namespace hush {
namespace immersive {

enum class ChannelFormat : std::uint8_t {
  Mono = 0,
  Stereo,
  LCR,
  Quad,
  FivePointOne,
  SevenPointOne,
  FivePointOneFour,
  SevenPointOneFour,
  NinePointOneSix,
  AmbisonicsFOA,
  AmbisonicsSOA,
  AmbisonicsHOA3,
  Count
};

inline const char* channelFormatName(ChannelFormat f) {
  switch (f) {
    case ChannelFormat::Mono:              return "Mono";
    case ChannelFormat::Stereo:            return "Stereo";
    case ChannelFormat::LCR:               return "LCR";
    case ChannelFormat::Quad:              return "Quad";
    case ChannelFormat::FivePointOne:      return "FivePointOne";
    case ChannelFormat::SevenPointOne:     return "SevenPointOne";
    case ChannelFormat::FivePointOneFour:  return "FivePointOneFour";
    case ChannelFormat::SevenPointOneFour: return "SevenPointOneFour";
    case ChannelFormat::NinePointOneSix:   return "NinePointOneSix";
    case ChannelFormat::AmbisonicsFOA:     return "AmbisonicsFOA";
    case ChannelFormat::AmbisonicsSOA:     return "AmbisonicsSOA";
    case ChannelFormat::AmbisonicsHOA3:    return "AmbisonicsHOA3";
    default:
      throw std::invalid_argument("unknown ChannelFormat");
  }
}

inline ChannelFormat channelFormatFromName(const std::string& name) {
  if (name == "Mono")              return ChannelFormat::Mono;
  if (name == "Stereo")            return ChannelFormat::Stereo;
  if (name == "LCR")               return ChannelFormat::LCR;
  if (name == "Quad")              return ChannelFormat::Quad;
  if (name == "FivePointOne")      return ChannelFormat::FivePointOne;
  if (name == "SevenPointOne")     return ChannelFormat::SevenPointOne;
  if (name == "FivePointOneFour")  return ChannelFormat::FivePointOneFour;
  if (name == "SevenPointOneFour") return ChannelFormat::SevenPointOneFour;
  if (name == "NinePointOneSix")   return ChannelFormat::NinePointOneSix;
  if (name == "AmbisonicsFOA")     return ChannelFormat::AmbisonicsFOA;
  if (name == "AmbisonicsSOA")     return ChannelFormat::AmbisonicsSOA;
  if (name == "AmbisonicsHOA3")    return ChannelFormat::AmbisonicsHOA3;
  throw std::invalid_argument("unknown ChannelFormat name: " + name);
}

}  // namespace immersive
}  // namespace hush
