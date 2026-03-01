// (C) 2026

#include "StdAfx.h"

#include "OodleEncoder.h"

#include "../../Common/Common.h"
#include "../Common/StreamUtils.h"

#ifndef Z7_EXTRACT_ONLY
namespace NCompress {
namespace NOODLE {

CEncoderBase::CEncoderBase(Int32 compressor):
    _compressor(compressor),
    _processedIn(0),
    _processedOut(0)
{
  _props.clear();
}

CEncoderBase::~CEncoderBase()
{
}

CEncoderKrak::CEncoderKrak():
    CEncoderBase(k_Oodle_Compressor_Kraken)
{
}

CEncoderMrmd::CEncoderMrmd():
    CEncoderBase(k_Oodle_Compressor_Mermaid)
{
}

CEncoderSlke::CEncoderSlke():
    CEncoderBase(k_Oodle_Compressor_Selkie)
{
}

CEncoderLevi::CEncoderLevi():
    CEncoderBase(k_Oodle_Compressor_Leviathan)
{
}

Z7_COM7F_IMF(CEncoderBase::SetCoderProperties(const PROPID *propIDs, const PROPVARIANT *coderProps, UInt32 numProps))
{
  _props.clear();

  for (UInt32 i = 0; i < numProps; i++)
  {
    const PROPVARIANT &prop = coderProps[i];
    const PROPID propID = propIDs[i];
    switch (propID)
    {
      case NCoderPropID::kLevel:
      {
        if (prop.vt != VT_UI4)
          return E_INVALIDARG;
        _props._level = (Byte)NormalizeLevel((UInt32)prop.ulVal);
        break;
      }

      case NCoderPropID::kNumThreads:
      {
        if (prop.vt != VT_UI4)
          return E_INVALIDARG;
        RINOK(SetNumberOfThreads((UInt32)prop.ulVal))
        break;
      }

      default:
        break;
    }
  }

  return S_OK;
}

Z7_COM7F_IMF(CEncoderBase::WriteCoderProperties(ISequentialOutStream *outStream))
{
  return WriteStream(outStream, &_props, sizeof(_props));
}

Z7_COM7F_IMF(CEncoderBase::Code(
    ISequentialInStream *inStream,
    ISequentialOutStream *outStream,
    const UInt64 *inSize,
    const UInt64 * /* outSize */,
    ICompressProgressInfo *progress))
{
  _processedIn = 0;
  _processedOut = 0;

  CByteBuffer rawData;
  RINOK(ReadAllInput(inStream, inSize, rawData, _processedIn))

  CByteBuffer compressedData;
  size_t compressedSize = 0;
  RINOK(OodleCompress(_compressor, _props._level, rawData, compressedData, compressedSize))

  RINOK(WriteStream(outStream, compressedData, compressedSize))
  _processedOut = compressedSize;

  if (progress)
    RINOK(progress->SetRatioInfo(&_processedIn, &_processedOut))

  return S_OK;
}

Z7_COM7F_IMF(CEncoderBase::SetNumberOfThreads(UInt32 /* numThreads */))
{
  return S_OK;
}

}}
#endif
