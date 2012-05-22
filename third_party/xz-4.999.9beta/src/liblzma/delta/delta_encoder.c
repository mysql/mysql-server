/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
///////////////////////////////////////////////////////////////////////////////
//
/// \file       delta_encoder.c
/// \brief      Delta filter encoder
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#include "delta_encoder.h"
#include "delta_private.h"


/// Copies and encodes the data at the same time. This is used when Delta
/// is the first filter in the chain (and thus the last filter in the
/// encoder's filter stack).
static void
copy_and_encode(lzma_coder *coder,
		const uint8_t *restrict in, uint8_t *restrict out, size_t size)
{
	const size_t distance = coder->distance;

	for (size_t i = 0; i < size; ++i) {
		const uint8_t tmp = coder->history[
				(distance + coder->pos) & 0xFF];
		coder->history[coder->pos-- & 0xFF] = in[i];
		out[i] = in[i] - tmp;
	}
}


/// Encodes the data in place. This is used when we are the last filter
/// in the chain (and thus non-last filter in the encoder's filter stack).
static void
encode_in_place(lzma_coder *coder, uint8_t *buffer, size_t size)
{
	const size_t distance = coder->distance;

	for (size_t i = 0; i < size; ++i) {
		const uint8_t tmp = coder->history[
				(distance + coder->pos) & 0xFF];
		coder->history[coder->pos-- & 0xFF] = buffer[i];
		buffer[i] -= tmp;
	}
}


static lzma_ret
delta_encode(lzma_coder *coder, lzma_allocator *allocator,
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size, uint8_t *restrict out,
		size_t *restrict out_pos, size_t out_size, lzma_action action)
{
	lzma_ret ret;

	if (coder->next.code == NULL) {
		const size_t in_avail = in_size - *in_pos;
		const size_t out_avail = out_size - *out_pos;
		const size_t size = MIN(in_avail, out_avail);

		copy_and_encode(coder, in + *in_pos, out + *out_pos, size);

		*in_pos += size;
		*out_pos += size;

		ret = action != LZMA_RUN && *in_pos == in_size
				? LZMA_STREAM_END : LZMA_OK;

	} else {
		const size_t out_start = *out_pos;

		ret = coder->next.code(coder->next.coder, allocator,
				in, in_pos, in_size, out, out_pos, out_size,
				action);

		encode_in_place(coder, out + out_start, *out_pos - out_start);
	}

	return ret;
}


extern lzma_ret
lzma_delta_encoder_init(lzma_next_coder *next, lzma_allocator *allocator,
		const lzma_filter_info *filters)
{
	return lzma_delta_coder_init(next, allocator, filters, &delta_encode);
}


extern lzma_ret
lzma_delta_props_encode(const void *options, uint8_t *out)
{
	// The caller must have already validated the options, so it's
	// LZMA_PROG_ERROR if they are invalid.
	if (lzma_delta_coder_memusage(options) == UINT64_MAX)
		return LZMA_PROG_ERROR;

	const lzma_options_delta *opt = options;
	out[0] = opt->dist - LZMA_DELTA_DIST_MIN;

	return LZMA_OK;
}
