// (C) 2026

#include "StdAfx.h"

#include "OodleCodec.h"

#include "../../Common/Common.h"
#include "../Common/StreamUtils.h"

namespace NCompress {
namespace NOODLE {

namespace {

const UInt64 k_Oodle_Max_SInt = (UInt64)(Int64)-1;
#if defined(_WIN64)
const Int32 k_Oodle_FuzzSafe_Yes = 1;
const Int32 k_Oodle_CheckCRC_No = 0;
const Int32 k_Oodle_Verbosity_None = 0;
const Int32 k_Oodle_Decode_ThreadPhaseAll = 3;
#endif

struct C_OodleLZ_CompressOptions
{
  UInt32 verbosity;
  Int32 minMatchLen;
  Int32 seekChunkReset; // bool
  Int32 seekChunkLen;
  Int32 profile;
  Int32 dictionarySize;
  Int32 spaceSpeedTradeoffBytes;
  Int32 maxHuffmansPerChunk;
  Int32 sendQuantumCRCs; // bool
  Int32 maxLocalDictionarySize;
  Int32 makeLongRangeMatcher;
  Int32 matchTableSizeLog2;
};

struct COodleApi
{
  bool Available;
  bool CanCompress;
  bool CanDecompress;

#if defined(_WIN64)
  HMODULE Module;

  typedef Int64 (WINAPI *Func_Compress)(
      Int32 compressor,
      const void *rawBuf,
      Int64 rawLen,
      void *compBuf,
      Int32 level,
      const C_OodleLZ_CompressOptions *pOptions,
      const void *dictionaryBase,
      const void *lrm,
      void *scratchMem,
      Int64 scratchSize);

  typedef Int64 (WINAPI *Func_Decompress)(
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

  typedef Int64 (WINAPI *Func_GetCompressedBufferSizeNeeded)(
      Byte unkCompressorArg,
      Int64 rawSize);

  typedef Int64 (WINAPI *Func_GetDecodeBufferSize)(
      Byte unkCompressorArg,
      Int64 rawSize,
      Int32 corruptionPossible);

  typedef const C_OodleLZ_CompressOptions * (WINAPI *Func_CompressOptions_GetDefault)();

  Func_Compress Compress;
  Func_Decompress Decompress;
  Func_GetCompressedBufferSizeNeeded GetCompressedBufferSizeNeeded;
  Func_GetDecodeBufferSize GetDecodeBufferSize;
  Func_CompressOptions_GetDefault CompressOptions_GetDefault;
#endif

  COodleApi():
      Available(false)
      , CanCompress(false)
      , CanDecompress(false)
#if defined(_WIN64)
      , Module(NULL)
      , Compress(NULL)
      , Decompress(NULL)
      , GetCompressedBufferSizeNeeded(NULL)
      , GetDecodeBufferSize(NULL)
      , CompressOptions_GetDefault(NULL)
#endif
  {
  }
};

#if defined(_WIN64)
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
#endif

#if defined(_WIN64)
static COodleApi InitOodleApi()
{
  COodleApi api;

  api.Module = LoadOodleLibrary();
  if (!api.Module)
    return api;

  api.Compress = (COodleApi::Func_Compress)::GetProcAddress(api.Module, "OodleLZ_Compress");
  api.Decompress = (COodleApi::Func_Decompress)::GetProcAddress(api.Module, "OodleLZ_Decompress");
  api.GetCompressedBufferSizeNeeded =
      (COodleApi::Func_GetCompressedBufferSizeNeeded)::GetProcAddress(api.Module, "OodleLZ_GetCompressedBufferSizeNeeded");
  api.GetDecodeBufferSize =
      (COodleApi::Func_GetDecodeBufferSize)::GetProcAddress(api.Module, "OodleLZ_GetDecodeBufferSize");
  api.CompressOptions_GetDefault =
      (COodleApi::Func_CompressOptions_GetDefault)::GetProcAddress(api.Module, "OodleLZ_CompressOptions_GetDefault");

  api.CanCompress = (api.Compress
      && api.GetCompressedBufferSizeNeeded
      && api.CompressOptions_GetDefault);
  api.CanDecompress = (api.Decompress
      && api.GetDecodeBufferSize);
  api.Available = (api.CanCompress || api.CanDecompress);

  return api;
}

static COodleApi &GetOodleApi()
{
  static COodleApi api = InitOodleApi();
  return api;
}
#endif

} // namespace

Int32 DecodeLevel(UInt32 level)
{
  if (level > Z7_ZSTD_FAST_LEV_INC && level <= Z7_ZSTD_FAST_LEV_INC + k_Oodle_FastLevel_Max)
    return -(Int32)(level - Z7_ZSTD_FAST_LEV_INC);
  return (Int32)level;
}

Int32 NormalizeLevel(Int32 level)
{
  if (level < k_Oodle_Level_Min)
    level = k_Oodle_Level_Min;
  else if (level > k_Oodle_Level_Max)
    level = k_Oodle_Level_Max;
  return level;
}

Byte EncodeLevel(Int32 level)
{
  level = NormalizeLevel(level);
  if (level < 0)
    return (Byte)(Z7_ZSTD_FAST_LEV_INC + (UInt32)(-level));
  return (Byte)level;
}

HRESULT ReadAllInput(ISequentialInStream *inStream, const UInt64 *inSize, CByteBuffer &data, UInt64 &processedIn)
{
  processedIn = 0;
  data.Free();

  if (inSize && *inSize != (UInt64)(Int64)-1)
  {
    if (*inSize > (UInt64)(size_t)-1 || *inSize > k_Oodle_Max_SInt)
      return E_OUTOFMEMORY;

    const size_t size = (size_t)*inSize;
    data.Alloc(size);
    size_t processedSize = size;
    RINOK(ReadStream(inStream, data, &processedSize))
    processedIn = (UInt64)processedSize;
    if (processedSize != size)
      return S_FALSE;
    return S_OK;
  }

  size_t curSize = 0;
  size_t capacity = 0;
  Byte temp[1 << 15];
  for (;;)
  {
    size_t cur = sizeof(temp);
    RINOK(ReadStream(inStream, temp, &cur))
    if (cur == 0)
      break;

    if (curSize > (size_t)-1 - cur)
      return E_OUTOFMEMORY;
    const size_t newSize = curSize + cur;
    if (newSize > capacity)
    {
      size_t newCapacity = capacity;
      if (newCapacity == 0)
        newCapacity = 1 << 20;
      while (newCapacity < newSize)
      {
        if (newCapacity > ((size_t)-1 >> 1))
        {
          newCapacity = newSize;
          break;
        }
        newCapacity <<= 1;
      }
      data.ChangeSize_KeepData(newCapacity, curSize);
      capacity = newCapacity;
    }

    memcpy(data.NonConstData() + curSize, temp, cur);
    curSize = newSize;
    processedIn += cur;
  }

  if (curSize != capacity)
    data.ChangeSize_KeepData(curSize, curSize);

  return S_OK;
}

HRESULT OodleCompress(
    Int32 compressor,
    UInt32 level,
    const CByteBuffer &rawData,
    CByteBuffer &compressedData,
    size_t &compressedSize)
{
  compressedData.Free();
  compressedSize = 0;

#if !defined(_WIN64)
  (void)compressor;
  (void)level;
  (void)rawData;
  return E_NOTIMPL;
#else
  COodleApi &api = GetOodleApi();
  if (!api.CanCompress)
    return E_NOTIMPL;

  if ((UInt64)rawData.Size() > k_Oodle_Max_SInt)
    return E_OUTOFMEMORY;

  const Int64 rawLen = (Int64)rawData.Size();
  const Int64 need = api.GetCompressedBufferSizeNeeded(0, rawLen);
  if (need <= 0)
    return E_FAIL;
  if ((UInt64)need > (UInt64)(size_t)-1)
    return E_OUTOFMEMORY;

  compressedData.Alloc((size_t)need);

  const C_OodleLZ_CompressOptions *defaultOptions = api.CompressOptions_GetDefault();
  if (!defaultOptions)
    return E_FAIL;

  C_OodleLZ_CompressOptions options = *defaultOptions;
  options.seekChunkReset = 1;
  options.seekChunkLen = (Int32)k_Oodle_SeekChunkLen;

  const Int32 normalizedLevel = NormalizeLevel(DecodeLevel(level));

  const Int64 compLen = api.Compress(
      compressor,
      rawData.ConstData(),
      rawLen,
      compressedData.NonConstData(),
      normalizedLevel,
      &options,
      NULL,
      NULL,
      NULL,
      0);

  if (compLen < 0 || compLen > need)
    return E_FAIL;

  compressedSize = (size_t)compLen;
  return S_OK;
#endif
}

HRESULT OodleDecompress(
    const CByteBuffer &compressedData,
    UInt64 outSize,
    CByteBuffer &rawData,
    size_t &rawSize)
{
  rawData.Free();
  rawSize = 0;

#if !defined(_WIN64)
  (void)compressedData;
  (void)outSize;
  return E_NOTIMPL;
#else
  COodleApi &api = GetOodleApi();
  if (!api.CanDecompress)
    return E_NOTIMPL;

  if (outSize > (UInt64)(size_t)-1 || outSize > k_Oodle_Max_SInt)
    return E_OUTOFMEMORY;
  if ((UInt64)compressedData.Size() > k_Oodle_Max_SInt)
    return E_OUTOFMEMORY;

  const Int64 expectedOutSize = (Int64)outSize;
  Int64 decodeBufferSize = api.GetDecodeBufferSize(0, expectedOutSize, 1);
  if (decodeBufferSize < expectedOutSize)
    decodeBufferSize = expectedOutSize;
  if (decodeBufferSize < 0)
    return E_FAIL;
  if ((UInt64)decodeBufferSize > (UInt64)(size_t)-1)
    return E_OUTOFMEMORY;

  rawData.Alloc((size_t)decodeBufferSize);

  const Int64 decoded = api.Decompress(
      compressedData.ConstData(),
      (Int64)compressedData.Size(),
      rawData.NonConstData(),
      expectedOutSize,
      k_Oodle_FuzzSafe_Yes,
      k_Oodle_CheckCRC_No,
      k_Oodle_Verbosity_None,
      NULL,
      0,
      NULL,
      NULL,
      NULL,
      0,
      k_Oodle_Decode_ThreadPhaseAll);

  if (decoded < 0 || decoded != expectedOutSize)
    return E_FAIL;

  rawSize = (size_t)decoded;
  return S_OK;
#endif
}

}}
