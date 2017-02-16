/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
///////////////////////////////////////////////////////////////////////////////
//
/// \file       easy_encoder.c
/// \brief      Easy .xz Stream encoder initialization
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#include "easy_preset.h"
#include "stream_encoder.h"


struct lzma_coder_s {
	lzma_next_coder stream_encoder;
	lzma_options_easy opt_easy;
};


static lzma_ret
easy_encode(lzma_coder *coder, lzma_allocator *allocator,
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size, uint8_t *restrict out,
		size_t *restrict out_pos, size_t out_size, lzma_action action)
{
	return coder->stream_encoder.code(
			coder->stream_encoder.coder, allocator,
			in, in_pos, in_size, out, out_pos, out_size, action);
}


static void
easy_encoder_end(lzma_coder *coder, lzma_allocator *allocator)
{
	lzma_next_end(&coder->stream_encoder, allocator);
	lzma_free(coder, allocator);
	return;
}


static lzma_ret
easy_encoder_init(lzma_next_coder *next, lzma_allocator *allocator,
		uint32_t preset, lzma_check check)
{
	lzma_next_coder_init(&easy_encoder_init, next, allocator);

	if (next->coder == NULL) {
		next->coder = lzma_alloc(sizeof(lzma_coder), allocator);
		if (next->coder == NULL)
			return LZMA_MEM_ERROR;

		next->code = &easy_encode;
		next->end = &easy_encoder_end;

		next->coder->stream_encoder = LZMA_NEXT_CODER_INIT;
	}

	if (lzma_easy_preset(&next->coder->opt_easy, preset))
		return LZMA_OPTIONS_ERROR;

	return lzma_stream_encoder_init(&next->coder->stream_encoder,
			allocator, next->coder->opt_easy.filters, check);
}


extern LZMA_API(lzma_ret)
lzma_easy_encoder(lzma_stream *strm, uint32_t preset, lzma_check check)
{
	lzma_next_strm_init(easy_encoder_init, strm, preset, check);

	strm->internal->supported_actions[LZMA_RUN] = true;
	strm->internal->supported_actions[LZMA_SYNC_FLUSH] = true;
	strm->internal->supported_actions[LZMA_FULL_FLUSH] = true;
	strm->internal->supported_actions[LZMA_FINISH] = true;

	return LZMA_OK;
}
