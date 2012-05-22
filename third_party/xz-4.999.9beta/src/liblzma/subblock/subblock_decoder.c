/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
///////////////////////////////////////////////////////////////////////////////
//
/// \file       subblock_decoder.c
/// \brief      Decoder of the Subblock filter
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#include "subblock_decoder.h"
#include "subblock_decoder_helper.h"
#include "filter_decoder.h"


/// Maximum number of consecutive Subblocks with Subblock Type Padding
#define PADDING_MAX 31


struct lzma_coder_s {
	lzma_next_coder next;

	enum {
		// These require that there is at least one input
		// byte available.
		SEQ_FLAGS,
		SEQ_FILTER_FLAGS,
		SEQ_FILTER_END,
		SEQ_REPEAT_COUNT_1,
		SEQ_REPEAT_COUNT_2,
		SEQ_REPEAT_COUNT_3,
		SEQ_REPEAT_SIZE,
		SEQ_REPEAT_READ_DATA,
		SEQ_SIZE_1,
		SEQ_SIZE_2,
		SEQ_SIZE_3, // This must be right before SEQ_DATA.

		// These don't require any input to be available.
		SEQ_DATA,
		SEQ_REPEAT_FAST,
		SEQ_REPEAT_NORMAL,
	} sequence;

	/// Number of bytes left in the current Subblock Data field.
	size_t size;

	/// Number of consecutive Subblocks with Subblock Type Padding
	uint32_t padding;

	/// True when .next.code() has returned LZMA_STREAM_END.
	bool next_finished;

	/// True when the Subblock decoder has detected End of Payload Marker.
	/// This may become true before next_finished becomes true.
	bool this_finished;

	/// True if Subfilters are allowed.
	bool allow_subfilters;

	/// Indicates if at least one byte of decoded output has been
	/// produced after enabling Subfilter.
	bool got_output_with_subfilter;

	/// Possible subfilter
	lzma_next_coder subfilter;

	/// Filter Flags decoder is needed to parse the ID and Properties
	/// of the subfilter.
	lzma_next_coder filter_flags_decoder;

	/// The filter_flags_decoder stores its results here.
	lzma_filter filter_flags;

	/// Options for the Subblock decoder helper. This is used to tell
	/// the helper when it should return LZMA_STREAM_END to the subfilter.
	lzma_options_subblock_helper helper;

	struct {
		/// How many times buffer should be repeated
		size_t count;

		/// Size of the buffer
		size_t size;

		/// Position in the buffer
		size_t pos;

		/// Buffer to hold the data to be repeated
		uint8_t buffer[LZMA_SUBBLOCK_RLE_MAX];
	} repeat;

	/// Temporary buffer needed when the Subblock filter is not the last
	/// filter in the chain. The output of the next filter is first
	/// decoded into buffer[], which is then used as input for the actual
	/// Subblock decoder.
	struct {
		size_t pos;
		size_t size;
		uint8_t buffer[LZMA_BUFFER_SIZE];
	} temp;
};


/// Values of valid Subblock Flags
enum {
	FLAG_PADDING,
	FLAG_EOPM,
	FLAG_DATA,
	FLAG_REPEAT,
	FLAG_SET_SUBFILTER,
	FLAG_END_SUBFILTER,
};


/// Calls the subfilter and updates coder->uncompressed_size.
static lzma_ret
subfilter_decode(lzma_coder *coder, lzma_allocator *allocator,
		const uint8_t *in, size_t *in_pos,
		size_t in_size, uint8_t *restrict out,
		size_t *restrict out_pos, size_t out_size, lzma_action action)
{
	assert(coder->subfilter.code != NULL);

	// Call the subfilter.
	const lzma_ret ret = coder->subfilter.code(
			coder->subfilter.coder, allocator,
			in, in_pos, in_size, out, out_pos, out_size, action);

	return ret;
}


static lzma_ret
decode_buffer(lzma_coder *coder, lzma_allocator *allocator,
		const uint8_t *in, size_t *in_pos,
		size_t in_size, uint8_t *restrict out,
		size_t *restrict out_pos, size_t out_size, lzma_action action)
{
	while (*out_pos < out_size && (*in_pos < in_size
			|| coder->sequence >= SEQ_DATA))
	switch (coder->sequence) {
	case SEQ_FLAGS: {
		// Do the correct action depending on the Subblock Type.
		switch (in[*in_pos] >> 4) {
		case FLAG_PADDING:
			// Only check that reserved bits are zero.
			if (++coder->padding > PADDING_MAX
					|| in[*in_pos] & 0x0F)
				return LZMA_DATA_ERROR;
			++*in_pos;
			break;

		case FLAG_EOPM:
			// There must be no Padding before EOPM.
			if (coder->padding != 0)
				return LZMA_DATA_ERROR;

			// Check that reserved bits are zero.
			if (in[*in_pos] & 0x0F)
				return LZMA_DATA_ERROR;

			// There must be no Subfilter enabled.
			if (coder->subfilter.code != NULL)
				return LZMA_DATA_ERROR;

			++*in_pos;
			return LZMA_STREAM_END;

		case FLAG_DATA:
			// First four bits of the Subblock Data size.
			coder->size = in[*in_pos] & 0x0F;
			++*in_pos;
			coder->got_output_with_subfilter = true;
			coder->sequence = SEQ_SIZE_1;
			break;

		case FLAG_REPEAT:
			// First four bits of the Repeat Count. We use
			// coder->size as a temporary place for it.
			coder->size = in[*in_pos] & 0x0F;
			++*in_pos;
			coder->got_output_with_subfilter = true;
			coder->sequence = SEQ_REPEAT_COUNT_1;
			break;

		case FLAG_SET_SUBFILTER: {
			if (coder->padding != 0 || (in[*in_pos] & 0x0F)
					|| coder->subfilter.code != NULL
					|| !coder->allow_subfilters)
				return LZMA_DATA_ERROR;

			assert(coder->filter_flags.options == NULL);
			abort();
// 			return_if_error(lzma_filter_flags_decoder_init(
// 					&coder->filter_flags_decoder,
// 					allocator, &coder->filter_flags));

			coder->got_output_with_subfilter = false;

			++*in_pos;
			coder->sequence = SEQ_FILTER_FLAGS;
			break;
		}

		case FLAG_END_SUBFILTER: {
			if (coder->padding != 0 || (in[*in_pos] & 0x0F)
					|| coder->subfilter.code == NULL
					|| !coder->got_output_with_subfilter)
				return LZMA_DATA_ERROR;

			// Tell the helper filter to indicate End of Input
			// to our subfilter.
			coder->helper.end_was_reached = true;

			size_t dummy = 0;
			const lzma_ret ret = subfilter_decode(coder, allocator,
					NULL, &dummy, 0, out, out_pos,out_size,
					action);

			// If we didn't reach the end of the subfilter's output
			// yet, return to the application. On the next call we
			// will get to this same switch-case again, because we
			// haven't updated *in_pos yet.
			if (ret != LZMA_STREAM_END)
				return ret;

			// Free Subfilter's memory. This is a bit debatable,
			// since we could avoid some malloc()/free() calls
			// if the same Subfilter gets used soon again. But
			// if Subfilter isn't used again, we could leave
			// a memory-hogging filter dangling until someone
			// frees Subblock filter itself.
			lzma_next_end(&coder->subfilter, allocator);

			// Free memory used for subfilter options. This is
			// safe, because we don't support any Subfilter that
			// would allow pointers in the options structure.
			lzma_free(coder->filter_flags.options, allocator);
			coder->filter_flags.options = NULL;

			++*in_pos;

			break;
		}

		default:
			return LZMA_DATA_ERROR;
		}

		break;
	}

	case SEQ_FILTER_FLAGS: {
		const lzma_ret ret = coder->filter_flags_decoder.code(
				coder->filter_flags_decoder.coder, allocator,
				in, in_pos, in_size, NULL, NULL, 0, LZMA_RUN);
		if (ret != LZMA_STREAM_END)
			return ret == LZMA_OPTIONS_ERROR
					? LZMA_DATA_ERROR : ret;

		// Don't free the filter_flags_decoder. It doesn't take much
		// memory and we may need it again.

		// Initialize the Subfilter. Subblock and Copy filters are
		// not allowed.
		if (coder->filter_flags.id == LZMA_FILTER_SUBBLOCK)
			return LZMA_DATA_ERROR;

		coder->helper.end_was_reached = false;

		lzma_filter filters[3] = {
			{
				.id = coder->filter_flags.id,
				.options = coder->filter_flags.options,
			}, {
				.id = LZMA_FILTER_SUBBLOCK_HELPER,
				.options = &coder->helper,
			}, {
				.id = LZMA_VLI_UNKNOWN,
				.options = NULL,
			}
		};

		// Optimization: We know that LZMA uses End of Payload Marker
		// (not End of Input), so we can omit the helper filter.
		if (filters[0].id == LZMA_FILTER_LZMA1)
			filters[1].id = LZMA_VLI_UNKNOWN;

		return_if_error(lzma_raw_decoder_init(
				&coder->subfilter, allocator, filters));

		coder->sequence = SEQ_FLAGS;
		break;
	}

	case SEQ_FILTER_END:
		// We are in the beginning of a Subblock. The next Subblock
		// whose type is not Padding, must indicate end of Subfilter.
		if (in[*in_pos] == (FLAG_PADDING << 4)) {
			++*in_pos;
			break;
		}

		if (in[*in_pos] != (FLAG_END_SUBFILTER << 4))
			return LZMA_DATA_ERROR;

		coder->sequence = SEQ_FLAGS;
		break;

	case SEQ_REPEAT_COUNT_1:
	case SEQ_SIZE_1:
		// We use the same code to parse
		//  - the Size (28 bits) in Subblocks of type Data; and
		//  - the Repeat count (28 bits) in Subblocks of type
		//    Repeating Data.
		coder->size |= (size_t)(in[*in_pos]) << 4;
		++*in_pos;
		++coder->sequence;
		break;

	case SEQ_REPEAT_COUNT_2:
	case SEQ_SIZE_2:
		coder->size |= (size_t)(in[*in_pos]) << 12;
		++*in_pos;
		++coder->sequence;
		break;

	case SEQ_REPEAT_COUNT_3:
	case SEQ_SIZE_3:
		coder->size |= (size_t)(in[*in_pos]) << 20;
		++*in_pos;

		// The real value is the stored value plus one.
		++coder->size;

		// This moves to SEQ_REPEAT_SIZE or SEQ_DATA. That's why
		// SEQ_DATA must be right after SEQ_SIZE_3 in coder->sequence.
		++coder->sequence;
		break;

	case SEQ_REPEAT_SIZE:
		// Move the Repeat Count to the correct variable and parse
		// the Size of the Data to be repeated.
		coder->repeat.count = coder->size;
		coder->repeat.size = (size_t)(in[*in_pos]) + 1;
		coder->repeat.pos = 0;

		// The size of the Data field must be bigger than the number
		// of Padding bytes before this Subblock.
		if (coder->repeat.size <= coder->padding)
			return LZMA_DATA_ERROR;

		++*in_pos;
		coder->padding = 0;
		coder->sequence = SEQ_REPEAT_READ_DATA;
		break;

	case SEQ_REPEAT_READ_DATA: {
		// Fill coder->repeat.buffer[].
		const size_t in_avail = in_size - *in_pos;
		const size_t out_avail
				= coder->repeat.size - coder->repeat.pos;
		const size_t copy_size = MIN(in_avail, out_avail);

		memcpy(coder->repeat.buffer + coder->repeat.pos,
				in + *in_pos, copy_size);
		*in_pos += copy_size;
		coder->repeat.pos += copy_size;

		if (coder->repeat.pos == coder->repeat.size) {
			coder->repeat.pos = 0;

			if (coder->repeat.size == 1
					&& coder->subfilter.code == NULL)
				coder->sequence = SEQ_REPEAT_FAST;
			else
				coder->sequence = SEQ_REPEAT_NORMAL;
		}

		break;
	}

	case SEQ_DATA: {
		// The size of the Data field must be bigger than the number
		// of Padding bytes before this Subblock.
		assert(coder->size > 0);
		if (coder->size <= coder->padding)
			return LZMA_DATA_ERROR;

		coder->padding = 0;

		// Limit the amount of input to match the available
		// Subblock Data size.
		size_t in_limit;
		if (in_size - *in_pos > coder->size)
			in_limit = *in_pos + coder->size;
		else
			in_limit = in_size;

		if (coder->subfilter.code == NULL) {
			const size_t copy_size = lzma_bufcpy(
					in, in_pos, in_limit,
					out, out_pos, out_size);

			coder->size -= copy_size;
		} else {
			const size_t in_start = *in_pos;
			const lzma_ret ret = subfilter_decode(
					coder, allocator,
					in, in_pos, in_limit,
					out, out_pos, out_size,
					action);

			// Update the number of unprocessed bytes left in
			// this Subblock. This assert() is true because
			// in_limit prevents *in_pos getting too big.
			assert(*in_pos - in_start <= coder->size);
			coder->size -= *in_pos - in_start;

			if (ret == LZMA_STREAM_END) {
				// End of Subfilter can occur only at
				// a Subblock boundary.
				if (coder->size != 0)
					return LZMA_DATA_ERROR;

				// We need a Subblock with Unset
				// Subfilter before more data.
				coder->sequence = SEQ_FILTER_END;
				break;
			}

			if (ret != LZMA_OK)
				return ret;
		}

		// If we couldn't process the whole Subblock Data yet, return.
		if (coder->size > 0)
			return LZMA_OK;

		coder->sequence = SEQ_FLAGS;
		break;
	}

	case SEQ_REPEAT_FAST: {
		// Optimization for cases when there is only one byte to
		// repeat and no Subfilter.
		const size_t out_avail = out_size - *out_pos;
		const size_t copy_size = MIN(coder->repeat.count, out_avail);

		memset(out + *out_pos, coder->repeat.buffer[0], copy_size);

		*out_pos += copy_size;
		coder->repeat.count -= copy_size;

		if (coder->repeat.count != 0)
			return LZMA_OK;

		coder->sequence = SEQ_FLAGS;
		break;
	}

	case SEQ_REPEAT_NORMAL:
		do {
			// Cycle the repeat buffer if needed.
			if (coder->repeat.pos == coder->repeat.size) {
				if (--coder->repeat.count == 0) {
					coder->sequence = SEQ_FLAGS;
					break;
				}

				coder->repeat.pos = 0;
			}

			if (coder->subfilter.code == NULL) {
				lzma_bufcpy(coder->repeat.buffer,
						&coder->repeat.pos,
						coder->repeat.size,
						out, out_pos, out_size);
			} else {
				const lzma_ret ret = subfilter_decode(
						coder, allocator,
						coder->repeat.buffer,
						&coder->repeat.pos,
						coder->repeat.size,
						out, out_pos, out_size,
						action);

				if (ret == LZMA_STREAM_END) {
					// End of Subfilter can occur only at
					// a Subblock boundary.
					if (coder->repeat.pos
							!= coder->repeat.size
							|| --coder->repeat
								.count != 0)
						return LZMA_DATA_ERROR;

					// We need a Subblock with Unset
					// Subfilter before more data.
					coder->sequence = SEQ_FILTER_END;
					break;

				} else if (ret != LZMA_OK) {
					return ret;
				}
			}
		} while (*out_pos < out_size);

		break;

	default:
		return LZMA_PROG_ERROR;
	}

	return LZMA_OK;
}


static lzma_ret
subblock_decode(lzma_coder *coder, lzma_allocator *allocator,
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size, uint8_t *restrict out,
		size_t *restrict out_pos, size_t out_size, lzma_action action)
{
	if (coder->next.code == NULL)
		return decode_buffer(coder, allocator, in, in_pos, in_size,
				out, out_pos, out_size, action);

	while (*out_pos < out_size) {
		if (!coder->next_finished
				&& coder->temp.pos == coder->temp.size) {
			coder->temp.pos = 0;
			coder->temp.size = 0;

			const lzma_ret ret = coder->next.code(
					coder->next.coder,
					allocator, in, in_pos, in_size,
					coder->temp.buffer, &coder->temp.size,
					LZMA_BUFFER_SIZE, action);

			if (ret == LZMA_STREAM_END)
				coder->next_finished = true;
			else if (coder->temp.size == 0 || ret != LZMA_OK)
				return ret;
		}

		if (coder->this_finished) {
			if (coder->temp.pos != coder->temp.size)
				return LZMA_DATA_ERROR;

			if (coder->next_finished)
				return LZMA_STREAM_END;

			return LZMA_OK;
		}

		const lzma_ret ret = decode_buffer(coder, allocator,
				coder->temp.buffer, &coder->temp.pos,
				coder->temp.size,
				out, out_pos, out_size, action);

		if (ret == LZMA_STREAM_END)
			// The next coder in the chain hasn't finished
			// yet. If the input data is valid, there
			// must be no more output coming, but the
			// next coder may still need a litle more
			// input to detect End of Payload Marker.
			coder->this_finished = true;
		else if (ret != LZMA_OK)
			return ret;
		else if (coder->next_finished && *out_pos < out_size)
			return LZMA_DATA_ERROR;
	}

	return LZMA_OK;
}


static void
subblock_decoder_end(lzma_coder *coder, lzma_allocator *allocator)
{
	lzma_next_end(&coder->next, allocator);
	lzma_next_end(&coder->subfilter, allocator);
	lzma_next_end(&coder->filter_flags_decoder, allocator);
	lzma_free(coder->filter_flags.options, allocator);
	lzma_free(coder, allocator);
	return;
}


extern lzma_ret
lzma_subblock_decoder_init(lzma_next_coder *next, lzma_allocator *allocator,
		const lzma_filter_info *filters)
{
	if (next->coder == NULL) {
		next->coder = lzma_alloc(sizeof(lzma_coder), allocator);
		if (next->coder == NULL)
			return LZMA_MEM_ERROR;

		next->code = &subblock_decode;
		next->end = &subblock_decoder_end;

		next->coder->next = LZMA_NEXT_CODER_INIT;
		next->coder->subfilter = LZMA_NEXT_CODER_INIT;
		next->coder->filter_flags_decoder = LZMA_NEXT_CODER_INIT;

	} else {
		lzma_next_end(&next->coder->subfilter, allocator);
		lzma_free(next->coder->filter_flags.options, allocator);
	}

	next->coder->filter_flags.options = NULL;

	next->coder->sequence = SEQ_FLAGS;
	next->coder->padding = 0;
	next->coder->next_finished = false;
	next->coder->this_finished = false;
	next->coder->temp.pos = 0;
	next->coder->temp.size = 0;

	if (filters[0].options != NULL)
		next->coder->allow_subfilters = ((lzma_options_subblock *)(
				filters[0].options))->allow_subfilters;
	else
		next->coder->allow_subfilters = false;

	return lzma_next_filter_init(
			&next->coder->next, allocator, filters + 1);
}
