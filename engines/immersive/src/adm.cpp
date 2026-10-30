// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#include "hush/immersive/adm.hpp"
#include "hush/immersive/speakers.hpp"
#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace hush {
namespace immersive {

namespace {

void writeLE16(std::ostream& os, std::uint16_t v) {
  char b[2] = {static_cast<char>(v & 0xff), static_cast<char>((v >> 8) & 0xff)};
  os.write(b, 2);
}

void writeLE32(std::ostream& os, std::uint32_t v) {
  char b[4];
  for (int i = 0; i < 4; ++i) b[i] = static_cast<char>((v >> (i * 8)) & 0xff);
  os.write(b, 4);
}

void writeLE64(std::ostream& os, std::uint64_t v) {
  char b[8];
  for (int i = 0; i < 8; ++i) b[i] = static_cast<char>((v >> (i * 8)) & 0xff);
  os.write(b, 8);
}

void writeFourCC(std::ostream& os, const char* cc) { os.write(cc, 4); }

std::string buildAxml(ChannelFormat bedFormat,
                      const std::vector<AdmObject>& objects, int numChannels) {
  std::ostringstream xml;
  xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  xml << "<ebuCoreMain xmlns=\"urn:ebu:metadata-schema:ebuCore_2016\">\n";
  xml << "  <coreMetadata>\n    <format>\n      <audioFormatExtended>\n";
  xml << "        <audioProgramme audioProgrammeID=\"APR_1001\" "
         "audioProgrammeName=\"Hush Programme\"/>\n";
  xml << "        <audioContent audioContentID=\"ACO_1001\" "
         "audioContentName=\"Main Content\"/>\n";

  const auto& bedSpeakers = speakersFor(bedFormat);
  const int bedCh = static_cast<int>(bedSpeakers.size());
  xml << "        <audioObject audioObjectID=\"AO_1001\" "
         "audioObjectName=\"Bed\"/>\n";
  xml << "        <audioPackFormat audioPackFormatID=\"AP_00010001\" "
         "audioPackFormatName=\"BedPack\" typeDefinition=\"DirectSpeakers\">\n";
  for (int i = 0; i < bedCh; ++i) {
    xml << "          <audioChannelFormatIDRef>AC_0001" << (1000 + i)
        << "</audioChannelFormatIDRef>\n";
  }
  xml << "        </audioPackFormat>\n";

  for (int i = 0; i < bedCh; ++i) {
    const Speaker& sp = bedSpeakers[static_cast<std::size_t>(i)];
    xml << "        <audioChannelFormat audioChannelFormatID=\"AC_0001"
        << (1000 + i) << "\" audioChannelFormatName=\"" << sp.id
        << "\" typeDefinition=\"DirectSpeakers\">\n";
    xml << "          <audioBlockFormat audioBlockFormatID=\"AB_0001"
        << (1000 + i) << "_00000001\">\n";
    xml << "            <position coordinate=\"azimuth\">" << sp.azimuthDeg
        << "</position>\n";
    xml << "            <position coordinate=\"elevation\">" << sp.elevationDeg
        << "</position>\n";
    xml << "            <position coordinate=\"distance\">1</position>\n";
    xml << "          </audioBlockFormat>\n        </audioChannelFormat>\n";
  }

  for (std::size_t oi = 0; oi < objects.size(); ++oi) {
    const AdmObject& obj = objects[oi];
    const int id = 1002 + static_cast<int>(oi);
    xml << "        <audioObject audioObjectID=\"AO_" << id
        << "\" audioObjectName=\"" << obj.name << "\"/>\n";
    xml << "        <audioPackFormat audioPackFormatID=\"AP_0003"
        << (1000 + oi) << "\" audioPackFormatName=\"" << obj.name
        << "Pack\" typeDefinition=\"Objects\">\n";
    xml << "          <audioChannelFormatIDRef>AC_0003" << (1000 + oi)
        << "</audioChannelFormatIDRef>\n        </audioPackFormat>\n";
    xml << "        <audioChannelFormat audioChannelFormatID=\"AC_0003"
        << (1000 + oi) << "\" audioChannelFormatName=\"" << obj.name
        << "\" typeDefinition=\"Objects\">\n";
    xml << "          <audioBlockFormat audioBlockFormatID=\"AB_0003"
        << (1000 + oi) << "_00000001\">\n";
    xml << "            <position coordinate=\"azimuth\">"
        << obj.panner.azimuthDeg << "</position>\n";
    xml << "            <position coordinate=\"elevation\">"
        << obj.panner.elevationDeg << "</position>\n";
    xml << "            <position coordinate=\"distance\">"
        << (obj.panner.distance > 0.f ? obj.panner.distance : 1.f)
        << "</position>\n";
    xml << "          </audioBlockFormat>\n        </audioChannelFormat>\n";
  }

  for (int i = 0; i < numChannels; ++i) {
    xml << "        <audioStreamFormat audioStreamFormatID=\"AS_0001"
        << (1000 + i) << "\" audioStreamFormatName=\"Stream" << i
        << "\" formatDefinition=\"PCM\"/>\n";
    xml << "        <audioTrackFormat audioTrackFormatID=\"AT_0001"
        << (1000 + i) << "\" audioTrackFormatName=\"Track" << i
        << "\" formatDefinition=\"PCM\"/>\n";
  }

  xml << "      </audioFormatExtended>\n    </format>\n  </coreMetadata>\n"
         "</ebuCoreMain>\n";
  return xml.str();
}

}  // namespace

void exportAdm(const std::string& path,
               const std::vector<float>& interleavedSamples, int numChannels,
               int numFrames, int sampleRate, ChannelFormat bedFormat,
               const std::vector<AdmObject>& objects) {
  if (numChannels <= 0 || numFrames <= 0 || sampleRate <= 0) {
    throw std::invalid_argument("exportAdm: invalid dimensions");
  }
  if (static_cast<int>(interleavedSamples.size()) < numChannels * numFrames) {
    throw std::invalid_argument("exportAdm: buffer too small");
  }

  const std::uint64_t dataSize =
      static_cast<std::uint64_t>(numChannels) * numFrames * sizeof(float);

  std::string axml = buildAxml(bedFormat, objects, numChannels);
  if (axml.size() % 2 != 0) axml.push_back('\0');

  const int bedCh = channelCount(bedFormat);
  const int numTracks = 1 + static_cast<int>(objects.size());
  std::vector<char> chnaData;
  auto append16 = [&](std::uint16_t v) {
    chnaData.push_back(static_cast<char>(v & 0xff));
    chnaData.push_back(static_cast<char>((v >> 8) & 0xff));
  };
  auto append32 = [&](std::uint32_t v) {
    for (int i = 0; i < 4; ++i)
      chnaData.push_back(static_cast<char>((v >> (i * 8)) & 0xff));
  };
  append16(static_cast<std::uint16_t>(numTracks));
  append32(static_cast<std::uint32_t>(numTracks));
  append16(1);
  append16(static_cast<std::uint16_t>(bedCh));
  append32(1);
  for (std::size_t i = 0; i < objects.size(); ++i) {
    append16(static_cast<std::uint16_t>(2 + i));
    append16(static_cast<std::uint16_t>(objects[i].numChannels));
    append32(static_cast<std::uint32_t>(2 + i));
  }
  if (chnaData.size() % 2 != 0) chnaData.push_back(0);

  const std::uint16_t audioFormat = 3;
  const std::uint16_t bitsPerSample = 32;
  const std::uint16_t blockAlign =
      static_cast<std::uint16_t>(numChannels * 4);
  const std::uint32_t byteRate =
      static_cast<std::uint32_t>(sampleRate) * blockAlign;

  const std::uint64_t fmtSize = 16;
  const std::uint64_t chnaSize = chnaData.size();
  const std::uint64_t axmlSize = axml.size();
  const std::uint64_t ds64Size = 28;

  std::uint64_t riffSize = 4;
  riffSize += 8 + ds64Size;
  riffSize += 8 + fmtSize;
  riffSize += 8 + chnaSize;
  riffSize += 8 + axmlSize;
  riffSize += 8 + dataSize;

  std::ofstream ofs(path, std::ios::binary);
  if (!ofs) throw std::runtime_error("cannot open ADM output: " + path);

  writeFourCC(ofs, "BW64");
  writeLE32(ofs, 0xFFFFFFFFu);
  writeFourCC(ofs, "WAVE");

  writeFourCC(ofs, "ds64");
  writeLE32(ofs, static_cast<std::uint32_t>(ds64Size));
  writeLE64(ofs, riffSize);
  writeLE64(ofs, dataSize);
  writeLE64(ofs, static_cast<std::uint64_t>(numFrames));
  writeLE32(ofs, 0);

  writeFourCC(ofs, "fmt ");
  writeLE32(ofs, static_cast<std::uint32_t>(fmtSize));
  writeLE16(ofs, audioFormat);
  writeLE16(ofs, static_cast<std::uint16_t>(numChannels));
  writeLE32(ofs, static_cast<std::uint32_t>(sampleRate));
  writeLE32(ofs, byteRate);
  writeLE16(ofs, blockAlign);
  writeLE16(ofs, bitsPerSample);

  writeFourCC(ofs, "chna");
  writeLE32(ofs, static_cast<std::uint32_t>(chnaSize));
  ofs.write(chnaData.data(), static_cast<std::streamsize>(chnaSize));

  writeFourCC(ofs, "axml");
  writeLE32(ofs, static_cast<std::uint32_t>(axmlSize));
  ofs.write(axml.data(), static_cast<std::streamsize>(axmlSize));

  writeFourCC(ofs, "data");
  writeLE32(ofs, static_cast<std::uint32_t>(
                     dataSize > 0xFFFFFFFFull ? 0xFFFFFFFFu : dataSize));
  ofs.write(reinterpret_cast<const char*>(interleavedSamples.data()),
            static_cast<std::streamsize>(dataSize));

  if (!ofs) throw std::runtime_error("error writing ADM file");
}

}  // namespace immersive
}  // namespace hush
