/*
 * Octasox.
 * Generate sox command lines that chop samples based on Octatrack slices.
 *
 * Copyright 2020 Johannes Meng
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Code in ot is taken from
 *
 * https://github.com/KaiDrange/OctaChainer,
 *
 * which is released as public domain. Original license text for all code in 
 * namespace ot:
 *
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 *
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * For more information, please refer to <http://unlicense.org>
 */

#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <arpa/inet.h>

namespace ot {

static constexpr unsigned char header_bytes[] = {
  0x46,0x4F,0x52,0x4D,
  0x00,0x00,0x00,0x00,
  0x44,0x50,0x53,0x31,
  0x53,0x4D,0x50,0x41
};

static constexpr unsigned char unknown_bytes[] = {
  0x00,0x00,0x00,0x00,0x00,0x02,0x00
};

enum Loop_t : int32_t {
  NoLoop = 0,
  Loop = 1,
  PIPO = 2
};

enum Stretch_t : int32_t {
  NoStrech = 0,
  Normal = 2,
  Beat = 3
};

enum TrigQuant_t : int32_t {
  Direct = 0xFF,
  Pattern = 0,
  S_1=1,
  S_2=2,
  S_3=3,
  S_4=4,
  S_6=5,
  S_8=6,
  S_12=7,
  S_16=8,
  S_24=9,
  S_32=10,
  S_48=11,
  S_64=12,
  S_96=13,
  S_128=14,
  S_192=15,
  S_256=16
};

#pragma pack(push)
#pragma pack(1)

struct Slice
{
    uint32_t startPoint;
    uint32_t endPoint;
    uint32_t loopPoint;
};

struct OTData
{
    uint8_t header[16];  // 0x00
    uint8_t unknown[7];  // 0x10 (All blank except 0x15 = 2)
    uint32_t tempo;      // 0x17 (BPM*24)
    uint32_t trimLen;    // 0x1B (value*100)
    uint32_t loopLen;    // 0x1F (value*100)
    uint32_t stretch;    // 0x23 (0ff = 0, Normal = 2, Beat = 3)
    uint32_t loop;       // 0x27 (Off = 0, Normal = 1, PingPong = 2)
    uint16_t gain;       // 0x2B (0x30 = 0, 0x60 = 24 (max), 0x00 = -24 (min))
    uint8_t quantize;    // 0x2D (0x00 = Pattern length, 0xFF = Direct, 1-10 = 1,2,3,4,6,8,12,16,24,32,48,64,96,128,192,256)
    uint32_t trimStart;  // 0x2E
    uint32_t trimEnd;    // 0x32
    uint32_t loopPoint;  // 0x36
    Slice slices[64];    // 0x3A - 0x339
    uint32_t sliceCount; // 0x33A
    uint16_t checkSum;   // 0x33E
};

#pragma pack(pop)

} // namespace ot

void load(const char* file, ot::OTData &data)
{
  std::fill(reinterpret_cast<char*>(&data),
            reinterpret_cast<char*>(&data+1),
            0);

  std::ifstream f(file, std::ios::in | std::ios::binary);
  const auto begin = f.tellg();
  f.seekg(0, std::ios::end);
  const auto end = f.tellg();
  const size_t numBytes = end-begin;

  if (numBytes != sizeof(ot::OTData))
    throw std::runtime_error(std::string(file)+" This is not a valid .ot file");

  f.seekg(0);
  f.read(reinterpret_cast<char*>(&data), numBytes);
  f.close();

  // Convert endianness!
  data.tempo = ntohl(data.tempo);
  data.trimLen = ntohl(data.trimLen);
  data.loopLen = ntohl(data.loopLen);
  data.stretch = ntohl(data.stretch);
  data.loop = ntohl(data.loop);
  data.gain = ntohs(data.gain);
  data.trimStart = ntohl(data.trimStart);
  data.trimEnd = ntohl(data.trimEnd);
  data.loopPoint = ntohl(data.loopPoint);
  data.sliceCount = ntohl(data.sliceCount);
  data.checkSum = ntohs(data.checkSum);

  for (uint32_t i = 0; i < data.sliceCount; ++i)
  {
    ot::Slice& s = data.slices[i];
    s.startPoint = ntohl(s.startPoint);
    s.endPoint = ntohl(s.endPoint);
    s.loopPoint = ntohl(s.loopPoint);
  }
};

int main(int argc, char** argv)
{
  if (argc < 2)
  {
    std::cerr << "usage: octasox OTFILE [ OTFILE ...]" << std::endl;
    return 1;
  }

  uint32_t preset = 0;
  for (uint32_t s = 1; s < argc; ++s)
  {
    const std::string settingsFile(argv[s]);
    if (settingsFile.size() < 3
     || (settingsFile.substr(settingsFile.size()-3) != ".ot"))
    {
      std::cerr << "Skipping " << settingsFile 
        << ": Only .ot files are supported." << std::endl;
      continue;
    }

    ot::OTData data;
    load(settingsFile.c_str(), data);

    const std::string name = settingsFile.substr(0, settingsFile.size()-3);
    const std::string input = name + ".wav";
    for (uint32_t i = 0; i < data.sliceCount; ++i)
    {
      ot::Slice& s = data.slices[i];
      std::ostringstream output;
      output << name << std::setw(2) << std::setfill('0') << i << ".wav";
      std::cout << input
                << " "
                << output.str()
                << " "
                << "trim " 
                << s.startPoint << "s "
                << "=" << s.endPoint << "s "
                << std::endl;
    }
  }
  return 0;
}
