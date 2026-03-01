// (C) 2026

#ifndef ZIP7_INC_OODLE_CODEC_H
#define ZIP7_INC_OODLE_CODEC_H

#include "../../Common/MyBuffer.h"

#include "../ICoder.h"

namespace NCompress {
namespace NOODLE {

const UInt32 k_Oodle_Version_Major = 9;
const UInt32 k_Oodle_Version_Minor = 0;
const UInt32 k_Oodle_Level_Default = 5;
const UInt32 k_Oodle_Level_Max = 9;
const UInt32 k_Oodle_SeekChunkLen = 0x40000;

const Int32 k_Oodle_Compressor_Kraken = 8;
const Int32 k_Oodle_Compressor_Mermaid = 9;
const Int32 k_Oodle_Compressor_Selkie = 11;
const Int32 k_Oodle_Compressor_Leviathan = 13;

struct CProps
{
  CProps() { clear(); }
  void clear()
  {
    memset(this, 0, sizeof(*this));
    _ver_major = (Byte)k_Oodle_Version_Major;
    _ver_minor = (Byte)k_Oodle_Version_Minor;
    _level = (Byte)k_Oodle_Level_Default;
  }

  Byte _ver_major;
  Byte _ver_minor;
  Byte _level;
  Byte _reserved[2];
};

UInt32 NormalizeLevel(UInt32 level);

HRESULT ReadAllInput(ISequentialInStream *inStream, const UInt64 *inSize, CByteBuffer &data, UInt64 &processedIn);

HRESULT OodleCompress(
    Int32 compressor,
    UInt32 level,
    const CByteBuffer &rawData,
    CByteBuffer &compressedData,
    size_t &compressedSize);

HRESULT OodleDecompress(
    const CByteBuffer &compressedData,
    UInt64 outSize,
    CByteBuffer &rawData,
    size_t &rawSize);

}}

#endif
