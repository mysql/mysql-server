/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
///////////////////////////////////////////////////////////////////////////////
//
/// \file       memusage.c
/// \brief      Calculates memory usage using lzma_memory_usage()
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#include "sysdefs.h"
#include "lzma.h"
#include <stdio.h>

int
main(void)
{
	lzma_options_lzma lzma = {
		.dict_size = (1U << 30) + (1U << 29),
		.lc = 3,
		.lp = 0,
		.pb = 2,
		.preset_dict = NULL,
		.preset_dict_size = 0,
		.mode = LZMA_MODE_NORMAL,
		.nice_len = 48,
		.mf = LZMA_MF_BT4,
		.depth = 0,
	};

/*
	lzma_options_filter filters[] = {
		{ LZMA_FILTER_LZMA1,
			(lzma_options_lzma *)&lzma_preset_lzma[6 - 1] },
		{ UINT64_MAX, NULL }
	};
*/
	lzma_filter filters[] = {
		{ LZMA_FILTER_LZMA1, &lzma },
		{ UINT64_MAX, NULL }
	};

	printf("Encoder: %10" PRIu64 " B\n", lzma_memusage_encoder(filters));
	printf("Decoder: %10" PRIu64 " B\n", lzma_memusage_decoder(filters));

	return 0;
}
