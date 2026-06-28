// (C) 2026

#include "StdAfx.h"

#include "../Common/RegisterCodec.h"
#include "../Common/StreamUtils.h"

#include "../../Common/MyBuffer.h"
#include "../../Common/MyCom.h"

#if defined(_WIN64)

namespace NCompress {
namespace NOodle {

namespace {

const UInt32 kMethodKrak = 0x4F71107;
const UInt32 kMethodMrmd = 0x4F71108;
const UInt32 kMethodSlke = 0x4F71109;
const UInt32 kMethodLevi = 0x4F7110A;

const Byte kVersionMajor = 9;
const Byte kVersionMinor = 0;
const Byte kDefaultLevel = 5;
const Int32 kLevelMin = -4;
const Int32 kLevelMax = 9;
const UInt32 kFastLevelMax = 4;
const Int32 kSeekChunkLen = 0x40000;

const Int32 kCompressorKraken = 8;
const Int32 kCompressorMermaid = 9;
const Int32 kCompressorSelkie = 11;
const Int32 kCompressorLeviathan = 13;

const Int32 kFuzzSafeYes = 1;
const Int32 kCheckCrcNo = 0;
const Int32 kVerbosityNone = 0;
const Int32 kThreadPhaseAll = 3;
const UInt64 kOodleMaxSize = ((UInt64)1 << 63) - 1;

struct CProps
{
  Byte VersionMajor;
  Byte VersionMinor;
  Byte Level;
  Byte Reserved[2];

  CProps()
  {
    Clear();
  }

  void Clear()
  {
    memset(this, 0, sizeof(*this));
    VersionMajor = kVersionMajor;
    VersionMinor = kVersionMinor;
    Level = kDefaultLevel;
  }
};

struct COodleLZ_CompressOptions
{
  UInt32 verbosity;
  Int32 minMatchLen;
  Int32 seekChunkReset;
  Int32 seekChunkLen;
  Int32 profile;
  Int32 dictionarySize;
  Int32 spaceSpeedTradeoffBytes;
  Int32 maxHuffmansPerChunk;
  Int32 sendQuantumCRCs;
  Int32 maxLocalDictionarySize;
  Int32 makeLongRangeMatcher;
  Int32 matchTableSizeLog2;
};

static HMODULE LoadOodleLibrary()
{
  static const WCHAR kName[] = L"oo2core_9_win64.dll";

  HMODULE module = ::LoadLibraryW(kName);
  if (module)
    return module;

  WCHAR path[MAX_PATH + 2];
  const DWORD len = ::GetModuleFileNameW(NULL, path, MAX_PATH + 1);
  if (len == 0 || len > MAX_PATH)
    return NULL;

  unsigned dirEnd = 0;
  for (unsigned i = len; i != 0; i--)
  {
    const WCHAR c = path[i - 1];
    if (c == L'\\' || c == L'/')
    {
      dirEnd = i;
      break;
    }
  }

  if (dirEnd == 0)
    return NULL;

  const unsigned nameLen = (unsigned)(sizeof(kName) / sizeof(kName[0])) - 1;
  if (dirEnd + nameLen + 1 > MAX_PATH + 1)
    return NULL;

  for (unsigned i = 0; i <= nameLen; i++)
    path[dirEnd + i] = kName[i];

  return ::LoadLibraryW(path);
}

class COodle
{
  HMODULE _module;
  bool _isLoaded;

public:
  typedef Int64 (WINAPI *FuncCompress)(
      Int32 compressor,
      const void *rawBuf,
      Int64 rawLen,
      void *compBuf,
      Int32 level,
      const COodleLZ_CompressOptions *options,
      const void *dictionaryBase,
      const void *lrm,
      void *scratchMem,
      Int64 scratchSize);

  typedef Int64 (WINAPI *FuncDecompress)(
      const void *compBuf,
      Int64 compBufSize,
      void *rawBuf,
      Int64 rawLen,
      Int32 fuzzSafe,
      Int32 checkCRC,
      Int32 verbosity,
      const void *decBufBase,
      Int64 decBufSize,
      void *fpCallback,
      void *callbackUserData,
      void *decoderMemory,
      Int64 decoderMemorySize,
      Int32 threadPhase);

  typedef Int64 (WINAPI *FuncGetCompressedBufferSizeNeeded)(Byte compressor, Int64 rawSize);
  typedef Int64 (WINAPI *FuncGetDecodeBufferSize)(Byte compressor, Int64 rawSize, Int32 corruptionPossible);
  typedef const COodleLZ_CompressOptions * (WINAPI *FuncCompressOptionsGetDefault)();

  FuncCompress Compress;
  FuncDecompress Decompress;
  FuncGetCompressedBufferSizeNeeded GetCompressedBufferSizeNeeded;
  FuncGetDecodeBufferSize GetDecodeBufferSize;
  FuncCompressOptionsGetDefault CompressOptionsGetDefault;

  COodle():
      _module(NULL),
      _isLoaded(false),
      Compress(NULL),
      Decompress(NULL),
      GetCompressedBufferSizeNeeded(NULL),
      GetDecodeBufferSize(NULL),
      CompressOptionsGetDefault(NULL)
  {
  }

  bool Load()
  {
    if (_isLoaded)
      return true;

    _isLoaded = true;

    _module = LoadOodleLibrary();
    if (!_module)
      return false;

    Compress = Z7_GET_PROC_ADDRESS(FuncCompress, _module, "OodleLZ_Compress");
    Decompress = Z7_GET_PROC_ADDRESS(FuncDecompress, _module, "OodleLZ_Decompress");
    GetCompressedBufferSizeNeeded =
        Z7_GET_PROC_ADDRESS(FuncGetCompressedBufferSizeNeeded, _module, "OodleLZ_GetCompressedBufferSizeNeeded");
    GetDecodeBufferSize =
        Z7_GET_PROC_ADDRESS(FuncGetDecodeBufferSize, _module, "OodleLZ_GetDecodeBufferSize");
    CompressOptionsGetDefault =
        Z7_GET_PROC_ADDRESS(FuncCompressOptionsGetDefault, _module, "OodleLZ_CompressOptions_GetDefault");

    return true;
  }

  bool CanEncode()
  {
    return Load()
        && Compress
        && GetCompressedBufferSizeNeeded
        && CompressOptionsGetDefault;
  }

  bool CanDecode()
  {
    return Load()
        && Decompress
        && GetDecodeBufferSize;
  }
};

static COodle &GetOodle()
{
  static COodle oodle;
  return oodle;
}

static Int32 NormalizeLevel(Int32 level)
{
  if (level < kLevelMin)
    return kLevelMin;
  if (level > kLevelMax)
    return kLevelMax;
  return level;
}

static Int32 DecodeStoredLevel(UInt32 level)
{
  if (level > Z7_ZSTD_FAST_LEV_INC && level <= Z7_ZSTD_FAST_LEV_INC + kFastLevelMax)
    return -(Int32)(level - Z7_ZSTD_FAST_LEV_INC);
  if (level > (UInt32)kLevelMax)
    return kLevelMax;
  return (Int32)level;
}

static Byte EncodeStoredLevel(Int32 level)
{
  level = NormalizeLevel(level);
  if (level < 0)
    return (Byte)(Z7_ZSTD_FAST_LEV_INC + (UInt32)(-level));
  return (Byte)level;
}

static HRESULT ReadInput(ISequentialInStream *inStream, const UInt64 *inSize,
    CByteBuffer &data, UInt64 &processedIn)
{
  processedIn = 0;
  data.Free();

  if (inSize && *inSize != (UInt64)(Int64)-1)
  {
    if (*inSize > (UInt64)(size_t)-1 || *inSize > kOodleMaxSize)
      return E_OUTOFMEMORY;

    const size_t size = (size_t)*inSize;
    data.Alloc(size);
    size_t processed = size;
    RINOK(ReadStream(inStream, data.NonConstData(), &processed))
    processedIn = (UInt64)processed;
    return processed == size ? S_OK : S_FALSE;
  }

  size_t size = 0;
  size_t capacity = 0;
  Byte temp[1 << 15];

  for (;;)
  {
    size_t cur = sizeof(temp);
    RINOK(ReadStream(inStream, temp, &cur))
    if (cur == 0)
      break;

    if (size > (size_t)-1 - cur)
      return E_OUTOFMEMORY;

    const size_t newSize = size + cur;
    if (newSize > capacity)
    {
      size_t newCapacity = capacity ? capacity : (1 << 20);
      while (newCapacity < newSize)
      {
        if (newCapacity > ((size_t)-1 >> 1))
        {
          newCapacity = newSize;
          break;
        }
        newCapacity <<= 1;
      }
      data.ChangeSize_KeepData(newCapacity, size);
      capacity = newCapacity;
    }

    memcpy(data.NonConstData() + size, temp, cur);
    size = newSize;
    processedIn += cur;

    if ((UInt64)size > kOodleMaxSize)
      return E_OUTOFMEMORY;
  }

  if (size != capacity)
    data.ChangeSize_KeepData(size, size);
  return S_OK;
}

static HRESULT Compress(Int32 compressor, Byte level, const CByteBuffer &src,
    CByteBuffer &dst, size_t &dstSize)
{
  dst.Free();
  dstSize = 0;

  COodle &oodle = GetOodle();
  if (!oodle.CanEncode())
    return E_NOTIMPL;

  if ((UInt64)src.Size() > kOodleMaxSize)
    return E_OUTOFMEMORY;

  const Int64 srcSize = (Int64)src.Size();
  const Int64 bound = oodle.GetCompressedBufferSizeNeeded(0, srcSize);
  if (bound <= 0)
    return E_FAIL;
  if ((UInt64)bound > (UInt64)(size_t)-1)
    return E_OUTOFMEMORY;

  dst.Alloc((size_t)bound);

  const COodleLZ_CompressOptions *defaultOptions = oodle.CompressOptionsGetDefault();
  if (!defaultOptions)
    return E_FAIL;

  COodleLZ_CompressOptions options = *defaultOptions;
  options.seekChunkReset = 1;
  options.seekChunkLen = kSeekChunkLen;

  const Int64 result = oodle.Compress(
      compressor,
      src.ConstData(),
      srcSize,
      dst.NonConstData(),
      DecodeStoredLevel(level),
      &options,
      NULL,
      NULL,
      NULL,
      0);

  if (result < 0 || result > bound || (srcSize != 0 && result == 0))
    return E_FAIL;

  dstSize = (size_t)result;
  return S_OK;
}

static HRESULT Decompress(const CByteBuffer &src, UInt64 outSize,
    CByteBuffer &dst, size_t &dstSize)
{
  dst.Free();
  dstSize = 0;

  COodle &oodle = GetOodle();
  if (!oodle.CanDecode())
    return E_NOTIMPL;

  if (outSize > (UInt64)(size_t)-1 || outSize > kOodleMaxSize)
    return E_OUTOFMEMORY;
  if ((UInt64)src.Size() > kOodleMaxSize)
    return E_OUTOFMEMORY;

  const Int64 expectedSize = (Int64)outSize;
  Int64 decodeBufferSize = oodle.GetDecodeBufferSize(0, expectedSize, 1);
  if (decodeBufferSize < 0)
    return E_FAIL;
  if (decodeBufferSize < expectedSize)
    decodeBufferSize = expectedSize;
  if ((UInt64)decodeBufferSize > (UInt64)(size_t)-1)
    return E_OUTOFMEMORY;

  dst.Alloc((size_t)decodeBufferSize);

  const Int64 result = oodle.Decompress(
      src.ConstData(),
      (Int64)src.Size(),
      dst.NonConstData(),
      expectedSize,
      kFuzzSafeYes,
      kCheckCrcNo,
      kVerbosityNone,
      NULL,
      0,
      NULL,
      NULL,
      NULL,
      0,
      kThreadPhaseAll);

  if (result != expectedSize)
    return E_FAIL;

  dstSize = (size_t)result;
  return S_OK;
}

class CDecoder Z7_final:
  public ICompressCoder,
  public ICompressSetDecoderProperties2,
  public ICompressSetCoderMt,
  public CMyUnknownImp
{
  CProps _props;
  UInt64 _processedIn;
  UInt64 _processedOut;

public:
  CDecoder():
      _processedIn(0),
      _processedOut(0)
  {
  }

  Z7_COM_QI_BEGIN2(ICompressCoder)
  Z7_COM_QI_ENTRY(ICompressSetDecoderProperties2)
  Z7_COM_QI_ENTRY(ICompressSetCoderMt)
  Z7_COM_QI_END
  Z7_COM_ADDREF_RELEASE

  Z7_IFACE_COM7_IMP(ICompressCoder)
  Z7_IFACE_COM7_IMP(ICompressSetDecoderProperties2)
  Z7_IFACE_COM7_IMP(ICompressSetCoderMt)
};

Z7_COM7F_IMF(CDecoder::SetDecoderProperties2(const Byte *props, UInt32 size))
{
  _props.Clear();
  if (size == 0)
    return S_OK;
  if (size != 3 && size != 5)
    return E_NOTIMPL;

  _props.VersionMajor = props[0];
  _props.VersionMinor = props[1];
  _props.Level = props[2];
  if (size == 5)
  {
    _props.Reserved[0] = props[3];
    _props.Reserved[1] = props[4];
  }
  return S_OK;
}

Z7_COM7F_IMF(CDecoder::SetNumberOfThreads(UInt32 /* numThreads */))
{
  return S_OK;
}

Z7_COM7F_IMF(CDecoder::Code(ISequentialInStream *inStream,
    ISequentialOutStream *outStream, const UInt64 *inSize,
    const UInt64 *outSize, ICompressProgressInfo *progress))
{
  if (!outSize)
    return E_INVALIDARG;

  _processedIn = 0;
  _processedOut = 0;

  CByteBuffer src;
  RINOK(ReadInput(inStream, inSize, src, _processedIn))

  CByteBuffer dst;
  size_t dstSize = 0;
  RINOK(Decompress(src, *outSize, dst, dstSize))

  RINOK(WriteStream(outStream, dst.ConstData(), dstSize))
  _processedOut = (UInt64)dstSize;

  if (progress)
    RINOK(progress->SetRatioInfo(&_processedIn, &_processedOut))

  return S_OK;
}

#ifndef Z7_EXTRACT_ONLY

class CEncoder Z7_final:
  public ICompressCoder,
  public ICompressSetCoderMt,
  public ICompressSetCoderProperties,
  public ICompressWriteCoderProperties,
  public CMyUnknownImp
{
  Int32 _compressor;
  CProps _props;
  UInt64 _processedIn;
  UInt64 _processedOut;

public:
  CEncoder(Int32 compressor):
      _compressor(compressor),
      _processedIn(0),
      _processedOut(0)
  {
  }

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

Z7_COM7F_IMF(CEncoder::SetNumberOfThreads(UInt32 /* numThreads */))
{
  return S_OK;
}

Z7_COM7F_IMF(CEncoder::SetCoderProperties(const PROPID *propIDs,
    const PROPVARIANT *props, UInt32 numProps))
{
  _props.Clear();

  for (UInt32 i = 0; i < numProps; i++)
  {
    const PROPVARIANT &prop = props[i];
    switch (propIDs[i])
    {
      case NCoderPropID::kLevel:
        if (prop.vt == VT_UI4)
          _props.Level = EncodeStoredLevel(DecodeStoredLevel((UInt32)prop.ulVal));
        else if (prop.vt == VT_I4)
          _props.Level = EncodeStoredLevel((Int32)prop.lVal);
        else
          return E_INVALIDARG;
        break;

      case NCoderPropID::kNumThreads:
        if (prop.vt != VT_UI4)
          return E_INVALIDARG;
        break;

      default:
        break;
    }
  }

  return S_OK;
}

Z7_COM7F_IMF(CEncoder::WriteCoderProperties(ISequentialOutStream *outStream))
{
  return WriteStream(outStream, &_props, sizeof(_props));
}

Z7_COM7F_IMF(CEncoder::Code(ISequentialInStream *inStream,
    ISequentialOutStream *outStream, const UInt64 *inSize,
    const UInt64 * /* outSize */, ICompressProgressInfo *progress))
{
  _processedIn = 0;
  _processedOut = 0;

  CByteBuffer src;
  RINOK(ReadInput(inStream, inSize, src, _processedIn))

  CByteBuffer dst;
  size_t dstSize = 0;
  RINOK(Compress(_compressor, _props.Level, src, dst, dstSize))

  RINOK(WriteStream(outStream, dst.ConstData(), dstSize))
  _processedOut = (UInt64)dstSize;

  if (progress)
    RINOK(progress->SetRatioInfo(&_processedIn, &_processedOut))

  return S_OK;
}

#endif

REGISTER_CODEC_CREATE(KRAK_CreateDec, CDecoder())
REGISTER_CODEC_CREATE(MRMD_CreateDec, CDecoder())
REGISTER_CODEC_CREATE(SLKE_CreateDec, CDecoder())
REGISTER_CODEC_CREATE(LEVI_CreateDec, CDecoder())

#ifdef Z7_EXTRACT_ONLY

REGISTER_CODEC_2(KRAK, KRAK_CreateDec, NULL, kMethodKrak, "KRAK")
REGISTER_CODEC_2(MRMD, MRMD_CreateDec, NULL, kMethodMrmd, "MRMD")
REGISTER_CODEC_2(SLKE, SLKE_CreateDec, NULL, kMethodSlke, "SLKE")
REGISTER_CODEC_2(LEVI, LEVI_CreateDec, NULL, kMethodLevi, "LEVI")

#else

REGISTER_CODEC_CREATE(KRAK_CreateEnc, CEncoder(kCompressorKraken))
REGISTER_CODEC_CREATE(MRMD_CreateEnc, CEncoder(kCompressorMermaid))
REGISTER_CODEC_CREATE(SLKE_CreateEnc, CEncoder(kCompressorSelkie))
REGISTER_CODEC_CREATE(LEVI_CreateEnc, CEncoder(kCompressorLeviathan))

REGISTER_CODEC_2(KRAK, KRAK_CreateDec, KRAK_CreateEnc, kMethodKrak, "KRAK")
REGISTER_CODEC_2(MRMD, MRMD_CreateDec, MRMD_CreateEnc, kMethodMrmd, "MRMD")
REGISTER_CODEC_2(SLKE, SLKE_CreateDec, SLKE_CreateEnc, kMethodSlke, "SLKE")
REGISTER_CODEC_2(LEVI, LEVI_CreateDec, LEVI_CreateEnc, kMethodLevi, "LEVI")

#endif

}}}

#endif
