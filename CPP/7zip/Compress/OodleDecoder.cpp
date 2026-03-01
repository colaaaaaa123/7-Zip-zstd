// (C) 2026

#include "StdAfx.h"

#include "OodleDecoder.h"

#include "../../Common/Common.h"
#include "../Common/StreamUtils.h"

namespace NCompress {
namespace NOODLE {

CDecoderBase::CDecoderBase():
    _processedIn(0),
    _processedOut(0)
{
  _props.clear();
}

CDecoderBase::~CDecoderBase()
{
}

Z7_COM7F_IMF(CDecoderBase::SetDecoderProperties2(const Byte *prop, UInt32 size))
{
  _props.clear();

  if (size == 0)
    return S_OK;

  if (size != 3 && size != 5)
    return E_NOTIMPL;

  _props._ver_major = prop[0];
  _props._ver_minor = prop[1];
  _props._level = prop[2];
  if (size == 5)
  {
    _props._reserved[0] = prop[3];
    _props._reserved[1] = prop[4];
  }
  return S_OK;
}

Z7_COM7F_IMF(CDecoderBase::SetNumberOfThreads(UInt32 /* numThreads */))
{
  return S_OK;
}

HRESULT CDecoderBase::SetOutStreamSizeResume(const UInt64 * /* outSize */)
{
  _processedOut = 0;
  return S_OK;
}

Z7_COM7F_IMF(CDecoderBase::SetOutStreamSize(const UInt64 *outSize))
{
  _processedIn = 0;
  RINOK(SetOutStreamSizeResume(outSize))
  return S_OK;
}

HRESULT CDecoderBase::CodeSpec(
    ISequentialInStream *inStream,
    ISequentialOutStream *outStream,
    const UInt64 *inSize,
    const UInt64 *outSize,
    ICompressProgressInfo *progress)
{
  if (!outSize)
    return E_INVALIDARG;

  CByteBuffer compressedData;
  RINOK(ReadAllInput(inStream, inSize, compressedData, _processedIn))

  CByteBuffer rawData;
  size_t rawSize = 0;
  RINOK(OodleDecompress(compressedData, *outSize, rawData, rawSize))
  if (rawSize != (size_t)*outSize)
    return E_FAIL;

  RINOK(WriteStream(outStream, rawData, rawSize))
  _processedOut = rawSize;

  if (progress)
    RINOK(progress->SetRatioInfo(&_processedIn, &_processedOut))

  return S_OK;
}

Z7_COM7F_IMF(CDecoderBase::Code(
    ISequentialInStream *inStream,
    ISequentialOutStream *outStream,
    const UInt64 *inSize,
    const UInt64 *outSize,
    ICompressProgressInfo *progress))
{
  RINOK(SetOutStreamSize(outSize))
  return CodeSpec(inStream, outStream, inSize, outSize, progress);
}

#ifndef Z7_NO_READ_FROM_CODER
Z7_COM7F_IMF(CDecoderBase::SetInStream(ISequentialInStream *inStream))
{
  _inStream = inStream;
  return S_OK;
}

Z7_COM7F_IMF(CDecoderBase::ReleaseInStream())
{
  _inStream.Release();
  return S_OK;
}
#endif

HRESULT CDecoderBase::CodeResume(
    ISequentialOutStream *outStream,
    const UInt64 *outSize,
    ICompressProgressInfo *progress)
{
  RINOK(SetOutStreamSizeResume(outSize))
  return CodeSpec(_inStream, outStream, NULL, outSize, progress);
}

}}
