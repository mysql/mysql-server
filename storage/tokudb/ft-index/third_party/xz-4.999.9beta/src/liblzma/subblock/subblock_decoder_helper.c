/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
///////////////////////////////////////////////////////////////////////////////
//
/// \file       subblock_decoder_helper.c
/// \brief      Helper filter for the Subblock decoder
///
/// This filter is used to indicate End of Input for subfilters needing it.
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#include "subblock_decoder_helper.h"


struct lzma_coder_s {
	const lzma_options_subblock_helper *options;
};


static lzma_ret
helper_decode(lzma_coder *coder,
		lzma_allocator *allocator lzma_attribute((unused)),
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size, uint8_t *restrict out,
		size_t *restrict out_pos, size_t out_size,
		lzma_action action lzma_attribute((unused)))
{
	// If end_was_reached is true, we cannot have any input.
	assert(!coder->options->end_was_reached || *in_pos == in_size);

	// We can safely copy as much as possible, because we are never
	// given more data than a single Subblock Data field.
	lzma_bufcpy(in, in_pos, in_size, out, out_pos, out_size);

	// Return LZMA_STREAM_END when instructed so by the Subblock decoder.
	return coder->options->end_was_reached ? LZMA_STREAM_END : LZMA_OK;
}


static void
helper_end(lzma_coder *coder, lzma_allocator *allocator)
{
	lzma_free(coder, allocator);
	return;
}


extern lzma_ret
lzma_subblock_decoder_helper_init(lzma_next_coder *next,
		lzma_allocator *allocator, const lzma_filter_info *filters)
{
	// This is always the last filter in the chain.
	assert(filters[1].init == NULL);

	if (next->coder == NULL) {
		next->coder = lzma_alloc(sizeof(lzma_coder), allocator);
		if (next->coder == NULL)
			return LZMA_MEM_ERROR;

		next->code = &helper_decode;
		next->end = &helper_end;
	}

	next->coder->options = filters[0].options;

	return LZMA_OK;
}
