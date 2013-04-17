/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
///////////////////////////////////////////////////////////////////////////////
//
/// \file       lzma_encoder_presets.c
/// \brief      Encoder presets
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#include "common.h"


extern LZMA_API(lzma_bool)
lzma_lzma_preset(lzma_options_lzma *options, uint32_t preset)
{
	const uint32_t level = preset & LZMA_PRESET_LEVEL_MASK;
	const uint32_t flags = preset & ~LZMA_PRESET_LEVEL_MASK;
	const uint32_t supported_flags = LZMA_PRESET_EXTREME;

	if (level > 9 || (flags & ~supported_flags))
		return true;

	const uint32_t dict_shift = level <= 1 ? 16 : level + 17;
	options->dict_size = UINT32_C(1) << dict_shift;

	options->preset_dict = NULL;
	options->preset_dict_size = 0;

	options->lc = LZMA_LC_DEFAULT;
	options->lp = LZMA_LP_DEFAULT;
	options->pb = LZMA_PB_DEFAULT;

	options->persistent = false;
	options->mode = level <= 2 ? LZMA_MODE_FAST : LZMA_MODE_NORMAL;

	options->nice_len = level == 0 ? 8 : level <= 5 ? 32 : 64;
	options->mf = level <= 1 ? LZMA_MF_HC3 : level <= 2 ? LZMA_MF_HC4
			: LZMA_MF_BT4;
	options->depth = 0;

	if (flags & LZMA_PRESET_EXTREME) {
		options->lc = 4; // FIXME?
		options->mode = LZMA_MODE_NORMAL;
		options->mf = LZMA_MF_BT4;
		options->nice_len = 273;
		options->depth = 512;
	}

	return false;
}
