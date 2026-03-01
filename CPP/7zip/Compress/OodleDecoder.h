// (C) 2026

#ifndef ZIP7_INC_OODLE_DECODER_H
#define ZIP7_INC_OODLE_DECODER_H

#include "OodleCodec.h"

#include "../../Common/MyCom.h"

namespace NCompress {
namespace NOODLE {

class CDecoderBase:
  public ICompressCoder,
  public ICompressSetDecoderProperties2,
  public ICompressSetCoderMt,
  public CMyUnknownImp
{
  CMyComPtr<ISequentialInStream> _inStream;

public:
  CProps _props;
  UInt64 _processedIn;
  UInt64 _processedOut;

  CDecoderBase();
  ~CDecoderBase();

  HRESULT CodeSpec(
      ISequentialInStream *inStream,
      ISequentialOutStream *outStream,
      const UInt64 *inSize,
      const UInt64 *outSize,
      ICompressProgressInfo *progress);
  HRESULT CodeResume(
      ISequentialOutStream *outStream,
      const UInt64 *outSize,
      ICompressProgressInfo *progress);
  HRESULT SetOutStreamSizeResume(const UInt64 *outSize);

  Z7_COM_QI_BEGIN2(ICompressCoder)
  Z7_COM_QI_ENTRY(ICompressSetDecoderProperties2)
  Z7_COM_QI_ENTRY(ICompressSetCoderMt)
  Z7_COM_QI_END
  Z7_COM_ADDREF_RELEASE

  Z7_IFACE_COM7_IMP(ICompressCoder)
  Z7_IFACE_COM7_IMP(ICompressSetDecoderProperties2)
public:
  Z7_IFACE_COM7_IMP(ICompressSetCoderMt)
  Z7_COM7F_IMF(SetOutStreamSize(const UInt64 *outSize));
#ifndef Z7_NO_READ_FROM_CODER
  Z7_COM7F_IMF(SetInStream(ISequentialInStream *inStream));
  Z7_COM7F_IMF(ReleaseInStream());
  UInt64 GetInputProcessedSize() const { return _processedIn; }
#endif
};

class CDecoderKrak Z7_final:
  public CDecoderBase
{
public:
  CDecoderKrak() {}
};

class CDecoderMrmd Z7_final:
  public CDecoderBase
{
public:
  CDecoderMrmd() {}
};

class CDecoderSlke Z7_final:
  public CDecoderBase
{
public:
  CDecoderSlke() {}
};

class CDecoderLevi Z7_final:
  public CDecoderBase
{
public:
  CDecoderLevi() {}
};

}}

#endif
