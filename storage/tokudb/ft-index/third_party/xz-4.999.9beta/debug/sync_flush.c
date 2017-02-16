/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
///////////////////////////////////////////////////////////////////////////////
//
/// \file       sync_flush.c
/// \brief      Encode files using LZMA_SYNC_FLUSH
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


static lzma_stream strm = LZMA_STREAM_INIT;
static FILE *file_in;


static void
encode(size_t size, lzma_action action)
{
	static const size_t CHUNK = 64;
	uint8_t in[CHUNK];
	uint8_t out[CHUNK];
	lzma_ret ret;

	do {
		if (strm.avail_in == 0 && size > 0) {
			const size_t amount = MIN(size, CHUNK);
			strm.avail_in = fread(in, 1, amount, file_in);
			strm.next_in = in;
			size -= amount; // Intentionally not using avail_in.
		}

		strm.next_out = out;
		strm.avail_out = CHUNK;

		ret = lzma_code(&strm, size == 0 ? action : LZMA_RUN);

		if (ret != LZMA_OK && ret != LZMA_STREAM_END) {
			fprintf(stderr, "%s:%u: %s: ret == %d\n",
					__FILE__, __LINE__, __func__, ret);
			exit(1);
		}

		fwrite(out, 1, CHUNK - strm.avail_out, stdout);

	} while (size > 0 || strm.avail_out == 0);

	if ((action == LZMA_RUN && ret != LZMA_OK)
			|| (action != LZMA_RUN && ret != LZMA_STREAM_END)) {
		fprintf(stderr, "%s:%u: %s: ret == %d\n",
				__FILE__, __LINE__, __func__, ret);
		exit(1);
	}
}


int
main(int argc, char **argv)
{
	file_in = argc > 1 ? fopen(argv[1], "rb") : stdin;

	// Config
	lzma_options_lzma opt_lzma = {
		.dict_size = 1U << 16,
		.lc = LZMA_LC_DEFAULT,
		.lp = LZMA_LP_DEFAULT,
		.pb = LZMA_PB_DEFAULT,
		.preset_dict = NULL,
		.persistent = true,
		.mode = LZMA_MODE_NORMAL,
		.nice_len = 32,
		.mf = LZMA_MF_HC3,
		.depth = 0,
	};

	lzma_options_delta opt_delta = {
		.dist = 16
	};

	lzma_options_subblock opt_subblock = {
		.allow_subfilters = true,
		.alignment = 8, // LZMA_SUBBLOCK_ALIGNMENT_DEFAULT,
		.subblock_data_size = LZMA_SUBBLOCK_DATA_SIZE_DEFAULT,
		.rle = 1, // LZMA_SUBBLOCK_RLE_OFF,
		.subfilter_mode = LZMA_SUBFILTER_SET,
	};
	opt_subblock.subfilter_options.id = LZMA_FILTER_LZMA1;
	opt_subblock.subfilter_options.options = &opt_lzma;
	opt_subblock.subfilter_options.id = LZMA_FILTER_DELTA;
	opt_subblock.subfilter_options.options = &opt_delta;

	lzma_filter filters[LZMA_FILTERS_MAX + 1];
	filters[0].id = LZMA_FILTER_LZMA2;
	filters[0].options = &opt_lzma;
	filters[1].id = LZMA_VLI_UNKNOWN;

	// Init
	if (lzma_stream_encoder(&strm, filters, LZMA_CHECK_CRC32) != LZMA_OK) {
		fprintf(stderr, "init failed\n");
		exit(1);
	}

	// Encoding

	encode(0, LZMA_SYNC_FLUSH);
	encode(6, LZMA_SYNC_FLUSH);
	encode(0, LZMA_SYNC_FLUSH);
	encode(7, LZMA_SYNC_FLUSH);
	encode(0, LZMA_SYNC_FLUSH);
	encode(0, LZMA_FINISH);
/*
	encode(53, LZMA_SYNC_FLUSH);
// 	opt_lzma.literal_context_bits = 2;
// 	opt_lzma.literal_pos_bits = 1;
// 	opt_lzma.pos_bits = 0;
	encode(404, LZMA_FINISH);
*/
	// Clean up
	lzma_end(&strm);

	return 0;

	// Prevent useless warnings so we don't need to have special CFLAGS
	// to disable -Werror.
	(void)opt_lzma;
	(void)opt_subblock;
	(void)opt_delta;
}
