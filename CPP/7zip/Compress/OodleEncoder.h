// (C) 2026

#ifndef ZIP7_INC_OODLE_ENCODER_H
#define ZIP7_INC_OODLE_ENCODER_H

#ifndef Z7_EXTRACT_ONLY

#include "OodleCodec.h"

#include "../../Common/MyCom.h"

namespace NCompress {
namespace NOODLE {

class CEncoderBase:
  public ICompressCoder,
  public ICompressSetCoderMt,
  public ICompressSetCoderProperties,
  public ICompressWriteCoderProperties,
  public CMyUnknownImp
{
  Int32 _compressor;

public:
  CProps _props;
  UInt64 _processedIn;
  UInt64 _processedOut;

  CEncoderBase(Int32 compressor);
  ~CEncoderBase();

  Z7_COM_QI_BEGIN2(ICompressCoder)
  Z7_COM_QI_ENTRY(ICompressSetCoderMt)
  Z7_COM_QI_ENTRY(ICompressSetCoderProperties)
  Z7_COM_QI_ENTRY(ICompressWriteCoderProperties)
  Z7_COM_QI_END
  Z7_COM_ADDREF_RELEASE

  Z7_IFACE_COM7_IMP(ICompressCoder)
  Z7_IFACE_COM7_IMP(ICompressSetCoderMt)
  Z7_IFACE_COM7_IMP(ICompressSetCoderProperties)
  Z7_IFACE_COM7_IMP(ICompressWriteCoderProperties)
};

class CEncoderKrak Z7_final:
  public CEncoderBase
{
public:
  CEncoderKrak();
};

class CEncoderMrmd Z7_final:
  public CEncoderBase
{
public:
  CEncoderMrmd();
};

class CEncoderSlke Z7_final:
  public CEncoderBase
{
public:
  CEncoderSlke();
};

class CEncoderLevi Z7_final:
  public CEncoderBase
{
public:
  CEncoderLevi();
};

}}

#endif

#endif
