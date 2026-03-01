// (C) 2026

#include "StdAfx.h"

#include "../Common/RegisterCodec.h"

#include "OodleDecoder.h"
#ifndef Z7_EXTRACT_ONLY
#include "OodleEncoder.h"
#endif

namespace NCompress {
namespace NOODLE {

REGISTER_CODEC_CREATE(KRAK_CreateDec, NCompress::NOODLE::CDecoderKrak())
REGISTER_CODEC_CREATE(MRMD_CreateDec, NCompress::NOODLE::CDecoderMrmd())
REGISTER_CODEC_CREATE(SLKE_CreateDec, NCompress::NOODLE::CDecoderSlke())
REGISTER_CODEC_CREATE(LEVI_CreateDec, NCompress::NOODLE::CDecoderLevi())

#ifdef Z7_EXTRACT_ONLY

REGISTER_CODEC_2(
  KRAK,
  KRAK_CreateDec,
  NULL,
  0x4F71107, "KRAK")

REGISTER_CODEC_2(
  MRMD,
  MRMD_CreateDec,
  NULL,
  0x4F71108, "MRMD")

REGISTER_CODEC_2(
  SLKE,
  SLKE_CreateDec,
  NULL,
  0x4F71109, "SLKE")

REGISTER_CODEC_2(
  LEVI,
  LEVI_CreateDec,
  NULL,
  0x4F7110A, "LEVI")

#else

REGISTER_CODEC_CREATE(KRAK_CreateEnc, NCompress::NOODLE::CEncoderKrak())
REGISTER_CODEC_CREATE(MRMD_CreateEnc, NCompress::NOODLE::CEncoderMrmd())
REGISTER_CODEC_CREATE(SLKE_CreateEnc, NCompress::NOODLE::CEncoderSlke())
REGISTER_CODEC_CREATE(LEVI_CreateEnc, NCompress::NOODLE::CEncoderLevi())

REGISTER_CODEC_2(
  KRAK,
  KRAK_CreateDec,
  KRAK_CreateEnc,
  0x4F71107, "KRAK")

REGISTER_CODEC_2(
  MRMD,
  MRMD_CreateDec,
  MRMD_CreateEnc,
  0x4F71108, "MRMD")

REGISTER_CODEC_2(
  SLKE,
  SLKE_CreateDec,
  SLKE_CreateEnc,
  0x4F71109, "SLKE")

REGISTER_CODEC_2(
  LEVI,
  LEVI_CreateDec,
  LEVI_CreateEnc,
  0x4F7110A, "LEVI")

#endif

}}
