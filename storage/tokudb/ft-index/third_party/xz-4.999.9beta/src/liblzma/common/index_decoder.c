/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
///////////////////////////////////////////////////////////////////////////////
//
/// \file       index_decoder.c
/// \brief      Decodes the Index field
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#include "index.h"
#include "check.h"


struct lzma_coder_s {
	enum {
		SEQ_INDICATOR,
		SEQ_COUNT,
		SEQ_MEMUSAGE,
		SEQ_UNPADDED,
		SEQ_UNCOMPRESSED,
		SEQ_PADDING_INIT,
		SEQ_PADDING,
		SEQ_CRC32,
	} sequence;

	/// Memory usage limit
	uint64_t memlimit;

	/// Target Index
	lzma_index *index;

	/// Number of Records left to decode.
	lzma_vli count;

	/// The most recent Unpadded Size field
	lzma_vli unpadded_size;

	/// The most recent Uncompressed Size field
	lzma_vli uncompressed_size;

	/// Position in integers
	size_t pos;

	/// CRC32 of the List of Records field
	uint32_t crc32;
};


static lzma_ret
index_decode(lzma_coder *coder, lzma_allocator *allocator,
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size, uint8_t *restrict out lzma_attribute((unused)),
		size_t *restrict out_pos lzma_attribute((unused)),
		size_t out_size lzma_attribute((unused)),
		lzma_action action lzma_attribute((unused)))
{
	// Similar optimization as in index_encoder.c
	const size_t in_start = *in_pos;
	lzma_ret ret = LZMA_OK;

	while (*in_pos < in_size)
	switch (coder->sequence) {
	case SEQ_INDICATOR:
		// Return LZMA_DATA_ERROR instead of e.g. LZMA_PROG_ERROR or
		// LZMA_FORMAT_ERROR, because a typical usage case for Index
		// decoder is when parsing the Stream backwards. If seeking
		// backward from the Stream Footer gives us something that
		// doesn't begin with Index Indicator, the file is considered
		// corrupt, not "programming error" or "unrecognized file
		// format". One could argue that the application should
		// verify the Index Indicator before trying to decode the
		// Index, but well, I suppose it is simpler this way.
		if (in[(*in_pos)++] != 0x00)
			return LZMA_DATA_ERROR;

		coder->sequence = SEQ_COUNT;
		break;

	case SEQ_COUNT:
		ret = lzma_vli_decode(&coder->count, &coder->pos,
				in, in_pos, in_size);
		if (ret != LZMA_STREAM_END)
			goto out;

		coder->pos = 0;
		coder->sequence = SEQ_MEMUSAGE;

	// Fall through

	case SEQ_MEMUSAGE:
		if (lzma_index_memusage(coder->count) > coder->memlimit) {
			ret = LZMA_MEMLIMIT_ERROR;
			goto out;
		}

		ret = LZMA_OK;
		coder->sequence = coder->count == 0
				? SEQ_PADDING_INIT : SEQ_UNPADDED;
		break;

	case SEQ_UNPADDED:
	case SEQ_UNCOMPRESSED: {
		lzma_vli *size = coder->sequence == SEQ_UNPADDED
				? &coder->unpadded_size
				: &coder->uncompressed_size;

		ret = lzma_vli_decode(size, &coder->pos,
				in, in_pos, in_size);
		if (ret != LZMA_STREAM_END)
			goto out;

		ret = LZMA_OK;
		coder->pos = 0;

		if (coder->sequence == SEQ_UNPADDED) {
			// Validate that encoded Unpadded Size isn't too small
			// or too big.
			if (coder->unpadded_size < UNPADDED_SIZE_MIN
					|| coder->unpadded_size
						> UNPADDED_SIZE_MAX)
				return LZMA_DATA_ERROR;

			coder->sequence = SEQ_UNCOMPRESSED;
		} else {
			// Add the decoded Record to the Index.
			return_if_error(lzma_index_append(
					coder->index, allocator,
					coder->unpadded_size,
					coder->uncompressed_size));

			// Check if this was the last Record.
			coder->sequence = --coder->count == 0
					? SEQ_PADDING_INIT
					: SEQ_UNPADDED;
		}

		break;
	}

	case SEQ_PADDING_INIT:
		coder->pos = lzma_index_padding_size(coder->index);
		coder->sequence = SEQ_PADDING;

	// Fall through

	case SEQ_PADDING:
		if (coder->pos > 0) {
			--coder->pos;
			if (in[(*in_pos)++] != 0x00)
				return LZMA_DATA_ERROR;

			break;
		}

		// Finish the CRC32 calculation.
		coder->crc32 = lzma_crc32(in + in_start,
				*in_pos - in_start, coder->crc32);

		coder->sequence = SEQ_CRC32;

	// Fall through

	case SEQ_CRC32:
		do {
			if (*in_pos == in_size)
				return LZMA_OK;

			if (((coder->crc32 >> (coder->pos * 8)) & 0xFF)
					!= in[(*in_pos)++])
				return LZMA_DATA_ERROR;

		} while (++coder->pos < 4);

		// Make index NULL so we don't free it unintentionally.
		coder->index = NULL;

		return LZMA_STREAM_END;

	default:
		assert(0);
		return LZMA_PROG_ERROR;
	}

out:
	// Update the CRC32,
	coder->crc32 = lzma_crc32(in + in_start,
			*in_pos - in_start, coder->crc32);

	return ret;
}


static void
index_decoder_end(lzma_coder *coder, lzma_allocator *allocator)
{
	lzma_index_end(coder->index, allocator);
	lzma_free(coder, allocator);
	return;
}


static lzma_ret
index_decoder_memconfig(lzma_coder *coder, uint64_t *memusage,
		uint64_t *old_memlimit, uint64_t new_memlimit)
{
	*memusage = lzma_index_memusage(coder->count);

	if (new_memlimit != 0 && new_memlimit < *memusage)
		return LZMA_MEMLIMIT_ERROR;

	*old_memlimit = coder->memlimit;
	coder->memlimit = new_memlimit;

	return LZMA_OK;
}


static lzma_ret
index_decoder_reset(lzma_coder *coder, lzma_allocator *allocator,
		lzma_index **i, uint64_t memlimit)
{
	// We always allocate a new lzma_index.
	*i = lzma_index_init(NULL, allocator);
	if (*i == NULL)
		return LZMA_MEM_ERROR;

	// Initialize the rest.
	coder->sequence = SEQ_INDICATOR;
	coder->memlimit = memlimit;
	coder->index = *i;
	coder->count = 0; // Needs to be initialized due to _memconfig().
	coder->pos = 0;
	coder->crc32 = 0;

	return LZMA_OK;
}


static lzma_ret
index_decoder_init(lzma_next_coder *next, lzma_allocator *allocator,
		lzma_index **i, uint64_t memlimit)
{
	lzma_next_coder_init(&index_decoder_init, next, allocator);

	if (i == NULL || memlimit == 0)
		return LZMA_PROG_ERROR;

	if (next->coder == NULL) {
		next->coder = lzma_alloc(sizeof(lzma_coder), allocator);
		if (next->coder == NULL)
			return LZMA_MEM_ERROR;

		next->code = &index_decode;
		next->end = &index_decoder_end;
		next->memconfig = &index_decoder_memconfig;
		next->coder->index = NULL;
	} else {
		lzma_index_end(next->coder->index, allocator);
	}

	return index_decoder_reset(next->coder, allocator, i, memlimit);
}


extern LZMA_API(lzma_ret)
lzma_index_decoder(lzma_stream *strm, lzma_index **i, uint64_t memlimit)
{
	lzma_next_strm_init(index_decoder_init, strm, i, memlimit);

	strm->internal->supported_actions[LZMA_RUN] = true;

	return LZMA_OK;
}


extern LZMA_API(lzma_ret)
lzma_index_buffer_decode(
		lzma_index **i, uint64_t *memlimit, lzma_allocator *allocator,
		const uint8_t *in, size_t *in_pos, size_t in_size)
{
	// Sanity checks
	if (i == NULL || in == NULL || in_pos == NULL || *in_pos > in_size)
		return LZMA_PROG_ERROR;

	// Initialize the decoder.
	lzma_coder coder;
	return_if_error(index_decoder_reset(&coder, allocator, i, *memlimit));

	// Store the input start position so that we can restore it in case
	// of an error.
	const size_t in_start = *in_pos;

	// Do the actual decoding.
	lzma_ret ret = index_decode(&coder, allocator, in, in_pos, in_size,
			NULL, NULL, 0, LZMA_RUN);

	if (ret == LZMA_STREAM_END) {
		ret = LZMA_OK;
	} else {
		// Something went wrong, free the Index structure and restore
		// the input position.
		lzma_index_end(*i, allocator);
		*i = NULL;
		*in_pos = in_start;

		if (ret == LZMA_OK) {
			// The input is truncated or otherwise corrupt.
			// Use LZMA_DATA_ERROR instead of LZMA_BUF_ERROR
			// like lzma_vli_decode() does in single-call mode.
			ret = LZMA_DATA_ERROR;

		} else if (ret == LZMA_MEMLIMIT_ERROR) {
			// Tell the caller how much memory would have
			// been needed.
			*memlimit = lzma_index_memusage(coder.count);
		}
	}

	return ret;
}
