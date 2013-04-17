/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
///////////////////////////////////////////////////////////////////////////////
//
/// \file       subblock_encoder.c
/// \brief      Encoder of the Subblock filter
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#include "subblock_encoder.h"
#include "filter_encoder.h"


/// Maximum number of repeats that a single Repeating Data can indicate.
/// This is directly from the file format specification.
#define REPEAT_COUNT_MAX (1U << 28)

/// Number of bytes the data chunk (not including the header part) must be
/// before we care about alignment. This is somewhat arbitrary. It just
/// doesn't make sense to waste bytes for alignment when the data chunk
/// is very small.
#define MIN_CHUNK_SIZE_FOR_ALIGN 4

/// Number of bytes of the header part of Subblock Type `Data'. This is
/// used as the `skew' argument for subblock_align().
#define ALIGN_SKEW_DATA 4

/// Like above but for Repeating Data.
#define ALIGN_SKEW_REPEATING_DATA 5

/// Writes one byte to output buffer and updates the alignment counter.
#define write_byte(b) \
do { \
	assert(*out_pos < out_size); \
	out[*out_pos] = b; \
	++*out_pos; \
	++coder->alignment.out_pos; \
} while (0)


struct lzma_coder_s {
	lzma_next_coder next;
	bool next_finished;

	enum {
		SEQ_FILL,
		SEQ_FLUSH,
		SEQ_RLE_COUNT_0,
		SEQ_RLE_COUNT_1,
		SEQ_RLE_COUNT_2,
		SEQ_RLE_COUNT_3,
		SEQ_RLE_SIZE,
		SEQ_RLE_DATA,
		SEQ_DATA_SIZE_0,
		SEQ_DATA_SIZE_1,
		SEQ_DATA_SIZE_2,
		SEQ_DATA_SIZE_3,
		SEQ_DATA,
		SEQ_SUBFILTER_INIT,
		SEQ_SUBFILTER_FLAGS,
	} sequence;

	/// Pointer to the options given by the application. This is used
	/// for two-way communication with the application.
	lzma_options_subblock *options;

	/// Position in various arrays.
	size_t pos;

	/// Holds subblock.size - 1 or rle.size - 1 when encoding size
	/// of Data or Repeat Count.
	uint32_t tmp;

	struct {
		/// This is a copy of options->alignment, or
		/// LZMA_SUBBLOCK_ALIGNMENT_DEFAULT if options is NULL.
		uint32_t multiple;

		/// Number of input bytes which we have processed and started
		/// writing out. 32-bit integer is enough since we care only
		/// about the lowest bits when fixing alignment.
		uint32_t in_pos;

		/// Number of bytes written out.
		uint32_t out_pos;
	} alignment;

	struct {
		/// Pointer to allocated buffer holding the Data field
		/// of Subblock Type "Data".
		uint8_t *data;

		/// Number of bytes in the buffer.
		size_t size;

		/// Allocated size of the buffer.
		size_t limit;

		/// Number of input bytes that we have already read but
		/// not yet started writing out. This can be different
		/// to `size' when using Subfilter. That's why we track
		/// in_pending separately for RLE (see below).
		uint32_t in_pending;
	} subblock;

	struct {
		/// Buffer to hold the data that may be coded with
		/// Subblock Type `Repeating Data'.
		uint8_t buffer[LZMA_SUBBLOCK_RLE_MAX];

		/// Number of bytes in buffer[].
		size_t size;

		/// Number of times the first `size' bytes of buffer[]
		/// will be repeated.
		uint64_t count;

		/// Like subblock.in_pending above, but for RLE.
		uint32_t in_pending;
	} rle;

	struct {
		enum {
			SUB_NONE,
			SUB_SET,
			SUB_RUN,
			SUB_FLUSH,
			SUB_FINISH,
			SUB_END_MARKER,
		} mode;

		/// This is a copy of options->allow_subfilters. We use
		/// this to verify that the application doesn't change
		/// the value of allow_subfilters.
		bool allow;

		/// When this is true, application is not allowed to modify
		/// options->subblock_mode. We may still modify it here.
		bool mode_locked;

		/// True if we have encoded at least one byte of data with
		/// the Subfilter.
		bool got_input;

		/// Track the amount of input available once
		/// LZMA_SUBFILTER_FINISH has been enabled.
		/// This is needed for sanity checking (kind
		/// of duplicating what common/code.c does).
		size_t in_avail;

		/// Buffer for the Filter Flags field written after
		/// the `Set Subfilter' indicator.
		uint8_t *flags;

		/// Size of Filter Flags field.
		uint32_t flags_size;

		/// Pointers to Subfilter.
		lzma_next_coder subcoder;

	} subfilter;

	/// Temporary buffer used when we are not the last filter in the chain.
	struct {
		size_t pos;
		size_t size;
		uint8_t buffer[LZMA_BUFFER_SIZE];
	} temp;
};


/// \brief      Aligns the output buffer
///
/// Aligns the output buffer so that after skew bytes the output position is
/// a multiple of coder->alignment.multiple.
static bool
subblock_align(lzma_coder *coder, uint8_t *restrict out,
		size_t *restrict out_pos, size_t out_size,
		size_t chunk_size, uint32_t skew)
{
	assert(*out_pos < out_size);

	// Fix the alignment only if it makes sense at least a little.
	if (chunk_size >= MIN_CHUNK_SIZE_FOR_ALIGN) {
		const uint32_t target = coder->alignment.in_pos
				% coder->alignment.multiple;

		while ((coder->alignment.out_pos + skew)
				% coder->alignment.multiple != target) {
			// Zero indicates padding.
			write_byte(0x00);

			// Check if output buffer got full and indicate it to
			// the caller.
			if (*out_pos == out_size)
				return true;
		}
	}

	// Output buffer is not full.
	return false;
}


/// \brief      Checks if buffer contains repeated data
///
/// \param      needle      Buffer containing a single repeat chunk
/// \param      needle_size Size of needle in bytes
/// \param      buf         Buffer to search for repeated needles
/// \param      buf_chunks  Buffer size is buf_chunks * needle_size.
///
/// \return     True if the whole buf is filled with repeated needles.
///
static bool
is_repeating(const uint8_t *restrict needle, size_t needle_size,
		const uint8_t *restrict buf, size_t buf_chunks)
{
	while (buf_chunks-- != 0) {
		if (memcmp(buf, needle, needle_size) != 0)
			return false;

		buf += needle_size;
	}

	return true;
}


/// \brief      Optimizes the repeating style and updates coder->sequence
static void
subblock_rle_flush(lzma_coder *coder)
{
	// The Subblock decoder can use memset() when the size of the data
	// being repeated is one byte, so we check if the RLE buffer is
	// filled with a single repeating byte.
	if (coder->rle.size > 1) {
		const uint8_t b = coder->rle.buffer[0];
		size_t i = 0;
		while (true) {
			if (coder->rle.buffer[i] != b)
				break;

			if (++i == coder->rle.size) {
				// TODO Integer overflow check maybe,
				// although this needs at least 2**63 bytes
				// of input until it gets triggered...
				coder->rle.count *= coder->rle.size;
				coder->rle.size = 1;
				break;
			}
		}
	}

	if (coder->rle.count == 1) {
		// The buffer should be repeated only once. It is
		// waste of space to use Repeating Data. Instead,
		// write a regular Data Subblock. See SEQ_RLE_COUNT_0
		// in subblock_buffer() for more info.
		coder->tmp = coder->rle.size - 1;
	} else if (coder->rle.count > REPEAT_COUNT_MAX) {
		// There's so much to repeat that it doesn't fit into
		// 28-bit integer. We will write two or more Subblocks
		// of type Repeating Data.
		coder->tmp = REPEAT_COUNT_MAX - 1;
	} else {
		coder->tmp = coder->rle.count - 1;
	}

	coder->sequence = SEQ_RLE_COUNT_0;

	return;
}


/// \brief      Resizes coder->subblock.data for a new size limit
static lzma_ret
subblock_data_size(lzma_coder *coder, lzma_allocator *allocator,
		size_t new_limit)
{
	// Verify that the new limit is valid.
	if (new_limit < LZMA_SUBBLOCK_DATA_SIZE_MIN
			|| new_limit > LZMA_SUBBLOCK_DATA_SIZE_MAX)
		return LZMA_OPTIONS_ERROR;

	// Ff the new limit is different than the previous one, we need
	// to reallocate the data buffer.
	if (new_limit != coder->subblock.limit) {
		lzma_free(coder->subblock.data, allocator);
		coder->subblock.data = lzma_alloc(new_limit, allocator);
		if (coder->subblock.data == NULL)
			return LZMA_MEM_ERROR;
	}

	coder->subblock.limit = new_limit;

	return LZMA_OK;
}


static lzma_ret
subblock_buffer(lzma_coder *coder, lzma_allocator *allocator,
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size, uint8_t *restrict out,
		size_t *restrict out_pos, size_t out_size, lzma_action action)
{
	// Changing allow_subfilter is not allowed.
	if (coder->options != NULL && coder->subfilter.allow
			!= coder->options->allow_subfilters)
		return LZMA_PROG_ERROR;

	// Check if we need to do something special with the Subfilter.
	if (coder->subfilter.allow) {
		assert(coder->options != NULL);

		// See if subfilter_mode has been changed.
		switch (coder->options->subfilter_mode) {
		case LZMA_SUBFILTER_NONE:
			if (coder->subfilter.mode != SUB_NONE)
				return LZMA_PROG_ERROR;
			break;

		case LZMA_SUBFILTER_SET:
			if (coder->subfilter.mode_locked
					|| coder->subfilter.mode != SUB_NONE)
				return LZMA_PROG_ERROR;

			coder->subfilter.mode = SUB_SET;
			coder->subfilter.got_input = false;

			if (coder->sequence == SEQ_FILL)
				coder->sequence = SEQ_FLUSH;

			break;

		case LZMA_SUBFILTER_RUN:
			if (coder->subfilter.mode != SUB_RUN)
				return LZMA_PROG_ERROR;

			break;

		case LZMA_SUBFILTER_FINISH: {
			const size_t in_avail = in_size - *in_pos;

			if (coder->subfilter.mode == SUB_RUN) {
				if (coder->subfilter.mode_locked)
					return LZMA_PROG_ERROR;

				coder->subfilter.mode = SUB_FINISH;
				coder->subfilter.in_avail = in_avail;

			} else if (coder->subfilter.mode != SUB_FINISH
					|| coder->subfilter.in_avail
						!= in_avail) {
				return LZMA_PROG_ERROR;
			}

			break;
		}

		default:
			return LZMA_OPTIONS_ERROR;
		}

		// If we are sync-flushing or finishing, the application may
		// no longer change subfilter_mode. Note that this check is
		// done after checking the new subfilter_mode above; this
		// way the application may e.g. set LZMA_SUBFILTER_SET and
		// LZMA_SYNC_FLUSH at the same time, but it cannot modify
		// subfilter_mode on the later lzma_code() calls before
		// we have returned LZMA_STREAM_END.
		if (action != LZMA_RUN)
			coder->subfilter.mode_locked = true;
	}

	// Main loop
	while (*out_pos < out_size)
	switch (coder->sequence) {
	case SEQ_FILL:
		// Grab the new Subblock Data Size and reallocate the buffer.
		if (coder->subblock.size == 0 && coder->options != NULL
				&& coder->options->subblock_data_size
					!= coder->subblock.limit)
			return_if_error(subblock_data_size(coder,
					allocator, coder->options
						->subblock_data_size));

		if (coder->subfilter.mode == SUB_NONE) {
			assert(coder->subfilter.subcoder.code == NULL);

			// No Subfilter is enabled, just copy the data as is.
			coder->subblock.in_pending += lzma_bufcpy(
					in, in_pos, in_size,
					coder->subblock.data,
					&coder->subblock.size,
					coder->subblock.limit);

			// If we ran out of input before the whole buffer
			// was filled, return to application.
			if (coder->subblock.size < coder->subblock.limit
					&& action == LZMA_RUN)
				return LZMA_OK;

		} else {
			assert(coder->options->subfilter_mode
					!= LZMA_SUBFILTER_SET);

			// Using LZMA_FINISH automatically toggles
			// LZMA_SUBFILTER_FINISH.
			//
			// NOTE: It is possible that application had set
			// LZMA_SUBFILTER_SET and LZMA_FINISH at the same
			// time. In that case it is possible that we will
			// cycle to LZMA_SUBFILTER_RUN, LZMA_SUBFILTER_FINISH,
			// and back to LZMA_SUBFILTER_NONE in a single
			// Subblock encoder function call.
			if (action == LZMA_FINISH) {
				coder->options->subfilter_mode
						= LZMA_SUBFILTER_FINISH;
				coder->subfilter.mode = SUB_FINISH;
			}

			const size_t in_start = *in_pos;

			const lzma_ret ret = coder->subfilter.subcoder.code(
					coder->subfilter.subcoder.coder,
					allocator, in, in_pos, in_size,
					coder->subblock.data,
					&coder->subblock.size,
					coder->subblock.limit,
					coder->subfilter.mode == SUB_FINISH
						? LZMA_FINISH : action);

			const size_t in_used = *in_pos - in_start;
			coder->subblock.in_pending += in_used;
			if (in_used > 0)
				coder->subfilter.got_input = true;

			coder->subfilter.in_avail = in_size - *in_pos;

			if (ret == LZMA_STREAM_END) {
				// All currently available input must have
				// been processed.
				assert(*in_pos == in_size);

				// Flush now. Even if coder->subblock.size
				// happened to be zero, we still need to go
				// to SEQ_FLUSH to possibly finish RLE or
				// write the Subfilter Unset indicator.
				coder->sequence = SEQ_FLUSH;

				if (coder->subfilter.mode == SUB_RUN) {
					// Flushing with Subfilter enabled.
					assert(action == LZMA_SYNC_FLUSH);
					coder->subfilter.mode = SUB_FLUSH;
					break;
				}

				// Subfilter finished its job.
				assert(coder->subfilter.mode == SUB_FINISH
						|| action == LZMA_FINISH);

				// At least one byte of input must have been
				// encoded with the Subfilter. This is
				// required by the file format specification.
				if (!coder->subfilter.got_input)
					return LZMA_PROG_ERROR;

				// We don't strictly need to do this, but
				// doing it sounds like a good idea, because
				// otherwise the Subfilter's memory could be
				// left allocated for long time, and would
				// just waste memory.
				lzma_next_end(&coder->subfilter.subcoder,
						allocator);

				// We need to flush the currently buffered
				// data and write Unset Subfilter marker.
				// Note that we cannot set
				// coder->options->subfilter_mode to
				// LZMA_SUBFILTER_NONE yet, because we
				// haven't written the Unset Subfilter
				// marker yet.
				coder->subfilter.mode = SUB_END_MARKER;
				coder->sequence = SEQ_FLUSH;
				break;
			}

			// Return if we couldn't fill the buffer or
			// if an error occurred.
			if (coder->subblock.size < coder->subblock.limit
					|| ret != LZMA_OK)
				return ret;
		}

		coder->sequence = SEQ_FLUSH;

		// SEQ_FILL doesn't produce any output so falling through
		// to SEQ_FLUSH is safe.
		assert(*out_pos < out_size);

	// Fall through

	case SEQ_FLUSH:
		if (coder->options != NULL) {
			// Update the alignment variable.
			coder->alignment.multiple = coder->options->alignment;
			if (coder->alignment.multiple
					< LZMA_SUBBLOCK_ALIGNMENT_MIN
					|| coder->alignment.multiple
					> LZMA_SUBBLOCK_ALIGNMENT_MAX)
				return LZMA_OPTIONS_ERROR;

			// Run-length encoder
			//
			// First check if there is some data pending and we
			// have an obvious need to flush it immediatelly.
			if (coder->rle.count > 0
					&& (coder->rle.size
							!= coder->options->rle
						|| coder->subblock.size
							% coder->rle.size)) {
				subblock_rle_flush(coder);
				break;
			}

			// Grab the (possibly new) RLE chunk size and
			// validate it.
			coder->rle.size = coder->options->rle;
			if (coder->rle.size > LZMA_SUBBLOCK_RLE_MAX)
				return LZMA_OPTIONS_ERROR;

			if (coder->subblock.size != 0
					&& coder->rle.size
						!= LZMA_SUBBLOCK_RLE_OFF
					&& coder->subblock.size
						% coder->rle.size == 0) {

				// Initialize coder->rle.buffer if we don't
				// have RLE already running.
				if (coder->rle.count == 0)
					memcpy(coder->rle.buffer,
							coder->subblock.data,
							coder->rle.size);

				// Test if coder->subblock.data is repeating.
				// If coder->rle.count would overflow, we
				// force flushing. Forced flushing shouldn't
				// really happen in real-world situations.
				const size_t count = coder->subblock.size
						/ coder->rle.size;
				if (UINT64_MAX - count > coder->rle.count
						&& is_repeating(
							coder->rle.buffer,
							coder->rle.size,
							coder->subblock.data,
							count)) {
					coder->rle.count += count;
					coder->rle.in_pending += coder
							->subblock.in_pending;
					coder->subblock.in_pending = 0;
					coder->subblock.size = 0;

				} else if (coder->rle.count > 0) {
					// It's not repeating or at least not
					// with the same byte sequence as the
					// earlier Subblock Data buffers. We
					// have some data pending in the RLE
					// buffer already, so do a flush.
					// Once flushed, we will check again
					// if the Subblock Data happens to
					// contain a different repeating
					// sequence.
					subblock_rle_flush(coder);
					break;
				}
			}
		}

		// If we now have some data left in coder->subblock, the RLE
		// buffer is empty and we must write a regular Subblock Data.
		if (coder->subblock.size > 0) {
			assert(coder->rle.count == 0);
			coder->tmp = coder->subblock.size - 1;
			coder->sequence = SEQ_DATA_SIZE_0;
			break;
		}

		// Check if we should enable Subfilter.
		if (coder->subfilter.mode == SUB_SET) {
			if (coder->rle.count > 0)
				subblock_rle_flush(coder);
			else
				coder->sequence = SEQ_SUBFILTER_INIT;
			break;
		}

		// Check if we have just finished Subfiltering.
		if (coder->subfilter.mode == SUB_END_MARKER) {
			if (coder->rle.count > 0) {
				subblock_rle_flush(coder);
				break;
			}

			coder->options->subfilter_mode = LZMA_SUBFILTER_NONE;
			coder->subfilter.mode = SUB_NONE;

			write_byte(0x50);
			if (*out_pos == out_size)
				return LZMA_OK;
		}

		// Check if we have already written everything.
		if (action != LZMA_RUN && *in_pos == in_size
				&& (coder->subfilter.mode == SUB_NONE
				|| coder->subfilter.mode == SUB_FLUSH)) {
			if (coder->rle.count > 0) {
				subblock_rle_flush(coder);
				break;
			}

			if (action == LZMA_SYNC_FLUSH) {
				if (coder->subfilter.mode == SUB_FLUSH)
					coder->subfilter.mode = SUB_RUN;

				coder->subfilter.mode_locked = false;
				coder->sequence = SEQ_FILL;

			} else {
				assert(action == LZMA_FINISH);

				// Write EOPM.
				// NOTE: No need to use write_byte() here
				// since we are finishing.
				out[*out_pos] = 0x10;
				++*out_pos;
			}

			return LZMA_STREAM_END;
		}

		// Otherwise we have more work to do.
		coder->sequence = SEQ_FILL;
		break;

	case SEQ_RLE_COUNT_0:
		assert(coder->rle.count > 0);

		if (coder->rle.count == 1) {
			// The buffer should be repeated only once. Fix
			// the alignment and write the first byte of
			// Subblock Type `Data'.
			if (subblock_align(coder, out, out_pos, out_size,
					coder->rle.size, ALIGN_SKEW_DATA))
				return LZMA_OK;

			write_byte(0x20 | (coder->tmp & 0x0F));

		} else {
			// We have something to actually repeat, which should
			// mean that it takes less space with run-length
			// encoding.
			if (subblock_align(coder, out, out_pos, out_size,
						coder->rle.size,
						ALIGN_SKEW_REPEATING_DATA))
				return LZMA_OK;

			write_byte(0x30 | (coder->tmp & 0x0F));
		}

		// NOTE: If we have to write more than one Repeating Data
		// due to rle.count > REPEAT_COUNT_MAX, the subsequent
		// Repeating Data Subblocks may get wrong alignment, because
		// we add rle.in_pending to alignment.in_pos at once instead
		// of adding only as much as this particular Repeating Data
		// consumed input data. Correct alignment is always restored
		// after all the required Repeating Data Subblocks have been
		// written. This problem occurs in such a weird cases that
		// it's not worth fixing.
		coder->alignment.out_pos += coder->rle.size;
		coder->alignment.in_pos += coder->rle.in_pending;
		coder->rle.in_pending = 0;

		coder->sequence = SEQ_RLE_COUNT_1;
		break;

	case SEQ_RLE_COUNT_1:
		write_byte(coder->tmp >> 4);
		coder->sequence = SEQ_RLE_COUNT_2;
		break;

	case SEQ_RLE_COUNT_2:
		write_byte(coder->tmp >> 12);
		coder->sequence = SEQ_RLE_COUNT_3;
		break;

	case SEQ_RLE_COUNT_3:
		write_byte(coder->tmp >> 20);

		// Again, see if we are writing regular Data or Repeating Data.
		// In the former case, we skip SEQ_RLE_SIZE.
		if (coder->rle.count == 1)
			coder->sequence = SEQ_RLE_DATA;
		else
			coder->sequence = SEQ_RLE_SIZE;

		if (coder->rle.count > REPEAT_COUNT_MAX)
			coder->rle.count -= REPEAT_COUNT_MAX;
		else
			coder->rle.count = 0;

		break;

	case SEQ_RLE_SIZE:
		assert(coder->rle.size >= LZMA_SUBBLOCK_RLE_MIN);
		assert(coder->rle.size <= LZMA_SUBBLOCK_RLE_MAX);
		write_byte(coder->rle.size - 1);
		coder->sequence = SEQ_RLE_DATA;
		break;

	case SEQ_RLE_DATA:
		lzma_bufcpy(coder->rle.buffer, &coder->pos, coder->rle.size,
				out, out_pos, out_size);
		if (coder->pos < coder->rle.size)
			return LZMA_OK;

		coder->pos = 0;
		coder->sequence = SEQ_FLUSH;
		break;

	case SEQ_DATA_SIZE_0:
		// We need four bytes for the Size field.
		if (subblock_align(coder, out, out_pos, out_size,
				coder->subblock.size, ALIGN_SKEW_DATA))
			return LZMA_OK;

		coder->alignment.out_pos += coder->subblock.size;
		coder->alignment.in_pos += coder->subblock.in_pending;
		coder->subblock.in_pending = 0;

		write_byte(0x20 | (coder->tmp & 0x0F));
		coder->sequence = SEQ_DATA_SIZE_1;
		break;

	case SEQ_DATA_SIZE_1:
		write_byte(coder->tmp >> 4);
		coder->sequence = SEQ_DATA_SIZE_2;
		break;

	case SEQ_DATA_SIZE_2:
		write_byte(coder->tmp >> 12);
		coder->sequence = SEQ_DATA_SIZE_3;
		break;

	case SEQ_DATA_SIZE_3:
		write_byte(coder->tmp >> 20);
		coder->sequence = SEQ_DATA;
		break;

	case SEQ_DATA:
		lzma_bufcpy(coder->subblock.data, &coder->pos,
				coder->subblock.size, out, out_pos, out_size);
		if (coder->pos < coder->subblock.size)
			return LZMA_OK;

		coder->subblock.size = 0;
		coder->pos = 0;
		coder->sequence = SEQ_FLUSH;
		break;

	case SEQ_SUBFILTER_INIT: {
		assert(coder->subblock.size == 0);
		assert(coder->subblock.in_pending == 0);
		assert(coder->rle.count == 0);
		assert(coder->rle.in_pending == 0);
		assert(coder->subfilter.mode == SUB_SET);
		assert(coder->options != NULL);

		// There must be a filter specified.
		if (coder->options->subfilter_options.id == LZMA_VLI_UNKNOWN)
			return LZMA_OPTIONS_ERROR;

		// Initialize a raw encoder to work as a Subfilter.
		lzma_filter options[2];
		options[0] = coder->options->subfilter_options;
		options[1].id = LZMA_VLI_UNKNOWN;

		return_if_error(lzma_raw_encoder_init(
				&coder->subfilter.subcoder, allocator,
				options));

		// Encode the Filter Flags field into a buffer. This should
		// never fail since we have already successfully initialized
		// the Subfilter itself. Check it still, and return
		// LZMA_PROG_ERROR instead of whatever the ret would say.
		lzma_ret ret = lzma_filter_flags_size(
				&coder->subfilter.flags_size, options);
		assert(ret == LZMA_OK);
		if (ret != LZMA_OK)
			return LZMA_PROG_ERROR;

		coder->subfilter.flags = lzma_alloc(
				coder->subfilter.flags_size, allocator);
		if (coder->subfilter.flags == NULL)
			return LZMA_MEM_ERROR;

		// Now we have a big-enough buffer. Encode the Filter Flags.
		// Like above, this should never fail.
		size_t dummy = 0;
		ret = lzma_filter_flags_encode(options, coder->subfilter.flags,
				&dummy, coder->subfilter.flags_size);
		assert(ret == LZMA_OK);
		assert(dummy == coder->subfilter.flags_size);
		if (ret != LZMA_OK || dummy != coder->subfilter.flags_size)
			return LZMA_PROG_ERROR;

		// Write a Subblock indicating a new Subfilter.
		write_byte(0x40);

		coder->options->subfilter_mode = LZMA_SUBFILTER_RUN;
		coder->subfilter.mode = SUB_RUN;
		coder->alignment.out_pos += coder->subfilter.flags_size;
		coder->sequence = SEQ_SUBFILTER_FLAGS;

		// It is safe to fall through because SEQ_SUBFILTER_FLAGS
		// uses lzma_bufcpy() which doesn't write unless there is
		// output space.
	}

	// Fall through

	case SEQ_SUBFILTER_FLAGS:
		// Copy the Filter Flags to the output stream.
		lzma_bufcpy(coder->subfilter.flags, &coder->pos,
				coder->subfilter.flags_size,
				out, out_pos, out_size);
		if (coder->pos < coder->subfilter.flags_size)
			return LZMA_OK;

		lzma_free(coder->subfilter.flags, allocator);
		coder->subfilter.flags = NULL;

		coder->pos = 0;
		coder->sequence = SEQ_FILL;
		break;

	default:
		return LZMA_PROG_ERROR;
	}

	return LZMA_OK;
}


static lzma_ret
subblock_encode(lzma_coder *coder, lzma_allocator *allocator,
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size, uint8_t *restrict out,
		size_t *restrict out_pos, size_t out_size, lzma_action action)
{
	if (coder->next.code == NULL)
		return subblock_buffer(coder, allocator, in, in_pos, in_size,
				out, out_pos, out_size, action);

	while (*out_pos < out_size
			&& (*in_pos < in_size || action != LZMA_RUN)) {
		if (!coder->next_finished
				&& coder->temp.pos == coder->temp.size) {
			coder->temp.pos = 0;
			coder->temp.size = 0;

			const lzma_ret ret = coder->next.code(coder->next.coder,
					allocator, in, in_pos, in_size,
					coder->temp.buffer, &coder->temp.size,
					LZMA_BUFFER_SIZE, action);
			if (ret == LZMA_STREAM_END) {
				assert(action != LZMA_RUN);
				coder->next_finished = true;
			} else if (coder->temp.size == 0 || ret != LZMA_OK) {
				return ret;
			}
		}

		const lzma_ret ret = subblock_buffer(coder, allocator,
				coder->temp.buffer, &coder->temp.pos,
				coder->temp.size, out, out_pos, out_size,
				coder->next_finished ? LZMA_FINISH : LZMA_RUN);
		if (ret == LZMA_STREAM_END) {
			assert(action != LZMA_RUN);
			assert(coder->next_finished);
			return LZMA_STREAM_END;
		}

		if (ret != LZMA_OK)
			return ret;
	}

	return LZMA_OK;
}


static void
subblock_encoder_end(lzma_coder *coder, lzma_allocator *allocator)
{
	lzma_next_end(&coder->next, allocator);
	lzma_next_end(&coder->subfilter.subcoder, allocator);
	lzma_free(coder->subblock.data, allocator);
	lzma_free(coder->subfilter.flags, allocator);
	lzma_free(coder, allocator);
	return;
}


extern lzma_ret
lzma_subblock_encoder_init(lzma_next_coder *next, lzma_allocator *allocator,
		const lzma_filter_info *filters)
{
	if (next->coder == NULL) {
		next->coder = lzma_alloc(sizeof(lzma_coder), allocator);
		if (next->coder == NULL)
			return LZMA_MEM_ERROR;

		next->code = &subblock_encode;
		next->end = &subblock_encoder_end;

		next->coder->next = LZMA_NEXT_CODER_INIT;
		next->coder->subblock.data = NULL;
		next->coder->subblock.limit = 0;
		next->coder->subfilter.subcoder = LZMA_NEXT_CODER_INIT;
	} else {
		lzma_next_end(&next->coder->subfilter.subcoder,
				allocator);
		lzma_free(next->coder->subfilter.flags, allocator);
	}

	next->coder->subfilter.flags = NULL;

	next->coder->next_finished = false;
	next->coder->sequence = SEQ_FILL;
	next->coder->options = filters[0].options;
	next->coder->pos = 0;

	next->coder->alignment.in_pos = 0;
	next->coder->alignment.out_pos = 0;
	next->coder->subblock.size = 0;
	next->coder->subblock.in_pending = 0;
	next->coder->rle.count = 0;
	next->coder->rle.in_pending = 0;
	next->coder->subfilter.mode = SUB_NONE;
	next->coder->subfilter.mode_locked = false;

	next->coder->temp.pos = 0;
	next->coder->temp.size = 0;

	// Grab some values from the options structure if it is available.
	size_t subblock_size_limit;
	if (next->coder->options != NULL) {
		if (next->coder->options->alignment
					< LZMA_SUBBLOCK_ALIGNMENT_MIN
				|| next->coder->options->alignment
					> LZMA_SUBBLOCK_ALIGNMENT_MAX) {
			subblock_encoder_end(next->coder, allocator);
			return LZMA_OPTIONS_ERROR;
		}
		next->coder->alignment.multiple
				= next->coder->options->alignment;
		next->coder->subfilter.allow
				= next->coder->options->allow_subfilters;
		subblock_size_limit = next->coder->options->subblock_data_size;
	} else {
		next->coder->alignment.multiple
				= LZMA_SUBBLOCK_ALIGNMENT_DEFAULT;
		next->coder->subfilter.allow = false;
		subblock_size_limit = LZMA_SUBBLOCK_DATA_SIZE_DEFAULT;
	}

	return_if_error(subblock_data_size(next->coder, allocator,
				subblock_size_limit));

	return lzma_next_filter_init(
			&next->coder->next, allocator, filters + 1);
}
