// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace hush {
namespace immersive {

// Minimal real-time UDP PCM transport.
// Packet format (little-endian):
//   uint32 magic = 0x48555348  ("HUSH")
//   uint32 sequence
//   uint32 sampleRate
//   uint16 numChannels
//   uint16 numFrames
//   float32 samples[numChannels * numFrames]  interleaved
//
// This is intentionally simple and complete for low-latency local networks;
// it does not implement FEC, encryption or clock recovery.

class UdpSender {
public:
  UdpSender(const std::string& host, int port);
  ~UdpSender();

  // Send one interleaved block. Returns false on socket error.
  bool send(const float* interleaved, int numChannels, int numFrames,
            int sampleRate);

private:
  int sock_{-1};
  std::uint32_t sequence_{0};
};

class UdpReceiver {
public:
  explicit UdpReceiver(int port);
  ~UdpReceiver();

  // Blocking receive with timeout_ms. Returns number of frames received,
  // or 0 on timeout / error. Fills interleaved buffer (must be large enough).
  int receive(float* interleaved, int maxChannels, int maxFrames,
              int& outChannels, int& outSampleRate, int timeoutMs = 50);

private:
  int sock_{-1};
};

}  // namespace immersive
}  // namespace hush
