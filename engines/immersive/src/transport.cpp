// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#include "hush/immersive/transport.hpp"
#include <cstring>
#include <stdexcept>
#include <vector>

#if defined(_WIN32)
#error "UDP transport is POSIX-only in this build"
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace hush {
namespace immersive {

namespace {

constexpr std::uint32_t kMagic = 0x48555348u;  // "HUSH"

void writeLE32(char* p, std::uint32_t v) {
  p[0] = static_cast<char>(v & 0xff);
  p[1] = static_cast<char>((v >> 8) & 0xff);
  p[2] = static_cast<char>((v >> 16) & 0xff);
  p[3] = static_cast<char>((v >> 24) & 0xff);
}

void writeLE16(char* p, std::uint16_t v) {
  p[0] = static_cast<char>(v & 0xff);
  p[1] = static_cast<char>((v >> 8) & 0xff);
}

std::uint32_t readLE32(const char* p) {
  return static_cast<std::uint32_t>(static_cast<unsigned char>(p[0])) |
         (static_cast<std::uint32_t>(static_cast<unsigned char>(p[1])) << 8) |
         (static_cast<std::uint32_t>(static_cast<unsigned char>(p[2])) << 16) |
         (static_cast<std::uint32_t>(static_cast<unsigned char>(p[3])) << 24);
}

std::uint16_t readLE16(const char* p) {
  return static_cast<std::uint16_t>(static_cast<unsigned char>(p[0])) |
         (static_cast<std::uint16_t>(static_cast<unsigned char>(p[1])) << 8);
}

}  // namespace

UdpSender::UdpSender(const std::string& host, int port) {
  sock_ = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (sock_ < 0) throw std::runtime_error("UdpSender: socket failed");

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<std::uint16_t>(port));
  if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
    // try hostname
    hostent* he = gethostbyname(host.c_str());
    if (!he) {
      ::close(sock_);
      throw std::runtime_error("UdpSender: cannot resolve " + host);
    }
    std::memcpy(&addr.sin_addr, he->h_addr_list[0],
                static_cast<std::size_t>(he->h_length));
  }
  if (::connect(sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    ::close(sock_);
    throw std::runtime_error("UdpSender: connect failed");
  }
}

UdpSender::~UdpSender() {
  if (sock_ >= 0) ::close(sock_);
}

bool UdpSender::send(const float* interleaved, int numChannels, int numFrames,
                     int sampleRate) {
  if (!interleaved || numChannels <= 0 || numFrames <= 0) return false;
  const std::size_t header = 16;
  const std::size_t payload =
      static_cast<std::size_t>(numChannels) * numFrames * sizeof(float);
  std::vector<char> buf(header + payload);
  writeLE32(buf.data() + 0, kMagic);
  writeLE32(buf.data() + 4, sequence_++);
  writeLE32(buf.data() + 8, static_cast<std::uint32_t>(sampleRate));
  writeLE16(buf.data() + 12, static_cast<std::uint16_t>(numChannels));
  writeLE16(buf.data() + 14, static_cast<std::uint16_t>(numFrames));
  std::memcpy(buf.data() + header, interleaved, payload);
  const ssize_t n =
      ::send(sock_, buf.data(), buf.size(), 0);
  return n == static_cast<ssize_t>(buf.size());
}

UdpReceiver::UdpReceiver(int port) {
  sock_ = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (sock_ < 0) throw std::runtime_error("UdpReceiver: socket failed");

  int yes = 1;
  ::setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(static_cast<std::uint16_t>(port));
  if (::bind(sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    ::close(sock_);
    throw std::runtime_error("UdpReceiver: bind failed");
  }
}

UdpReceiver::~UdpReceiver() {
  if (sock_ >= 0) ::close(sock_);
}

int UdpReceiver::receive(float* interleaved, int maxChannels, int maxFrames,
                         int& outChannels, int& outSampleRate, int timeoutMs) {
  if (!interleaved) return 0;

  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(sock_, &fds);
  timeval tv{};
  tv.tv_sec = timeoutMs / 1000;
  tv.tv_usec = (timeoutMs % 1000) * 1000;
  if (::select(sock_ + 1, &fds, nullptr, nullptr, &tv) <= 0) return 0;

  std::vector<char> buf(16 + static_cast<std::size_t>(maxChannels) * maxFrames *
                                   sizeof(float));
  sockaddr_in src{};
  socklen_t srcLen = sizeof(src);
  const ssize_t n = ::recvfrom(sock_, buf.data(), buf.size(), 0,
                               reinterpret_cast<sockaddr*>(&src), &srcLen);
  if (n < 16) return 0;

  if (readLE32(buf.data()) != kMagic) return 0;
  outSampleRate = static_cast<int>(readLE32(buf.data() + 8));
  outChannels = static_cast<int>(readLE16(buf.data() + 12));
  const int frames = static_cast<int>(readLE16(buf.data() + 14));
  if (outChannels > maxChannels || frames > maxFrames) return 0;

  const std::size_t payload =
      static_cast<std::size_t>(outChannels) * frames * sizeof(float);
  if (static_cast<std::size_t>(n) < 16 + payload) return 0;
  std::memcpy(interleaved, buf.data() + 16, payload);
  return frames;
}

}  // namespace immersive
}  // namespace hush
