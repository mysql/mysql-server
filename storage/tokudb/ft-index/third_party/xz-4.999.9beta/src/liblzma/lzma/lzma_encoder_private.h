/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
///////////////////////////////////////////////////////////////////////////////
//
/// \file       lzma_encoder_private.h
/// \brief      Private definitions for LZMA encoder
///
//  Authors:    Igor Pavlov
//              Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LZMA_LZMA_ENCODER_PRIVATE_H
#define LZMA_LZMA_ENCODER_PRIVATE_H

#include "lz_encoder.h"
#include "range_encoder.h"
#include "lzma_common.h"
#include "lzma_encoder.h"


// Macro to compare if the first two bytes in two buffers differ. This is
// needed in lzma_lzma_optimum_*() to test if the match is at least
// MATCH_LEN_MIN bytes. Unaligned access gives tiny gain so there's no
// reason to not use it when it is supported.
#ifdef HAVE_FAST_UNALIGNED_ACCESS
#	define not_equal_16(a, b) \
		(*(const uint16_t *)(a) != *(const uint16_t *)(b))
#else
#	define not_equal_16(a, b) \
		((a)[0] != (b)[0] || (a)[1] != (b)[1])
#endif


// Optimal - Number of entries in the optimum array.
#define OPTS (1 << 12)


typedef struct {
	probability choice;
	probability choice2;
	probability low[POS_STATES_MAX][LEN_LOW_SYMBOLS];
	probability mid[POS_STATES_MAX][LEN_MID_SYMBOLS];
	probability high[LEN_HIGH_SYMBOLS];

	uint32_t prices[POS_STATES_MAX][LEN_SYMBOLS];
	uint32_t table_size;
	uint32_t counters[POS_STATES_MAX];

} lzma_length_encoder;


typedef struct {
	lzma_lzma_state state;

	bool prev_1_is_literal;
	bool prev_2;

	uint32_t pos_prev_2;
	uint32_t back_prev_2;

	uint32_t price;
	uint32_t pos_prev;  // pos_next;
	uint32_t back_prev;

	uint32_t backs[REP_DISTANCES];

} lzma_optimal;


struct lzma_coder_s {
	/// Range encoder
	lzma_range_encoder rc;

	/// State
	lzma_lzma_state state;

	/// The four most recent match distances
	uint32_t reps[REP_DISTANCES];

	/// Array of match candidates
	lzma_match matches[MATCH_LEN_MAX + 1];

	/// Number of match candidates in matches[]
	uint32_t matches_count;

	/// Varibale to hold the length of the longest match between calls
	/// to lzma_lzma_optimum_*().
	uint32_t longest_match_length;

	/// True if using getoptimumfast
	bool fast_mode;

	/// True if the encoder has been initialized by encoding the first
	/// byte as a literal.
	bool is_initialized;

	/// True if the range encoder has been flushed, but not all bytes
	/// have been written to the output buffer yet.
	bool is_flushed;

	uint32_t pos_mask;         ///< (1 << pos_bits) - 1
	uint32_t literal_context_bits;
	uint32_t literal_pos_mask;

	// These are the same as in lzma_decoder.c. See comments there.
	probability literal[LITERAL_CODERS_MAX][LITERAL_CODER_SIZE];
	probability is_match[STATES][POS_STATES_MAX];
	probability is_rep[STATES];
	probability is_rep0[STATES];
	probability is_rep1[STATES];
	probability is_rep2[STATES];
	probability is_rep0_long[STATES][POS_STATES_MAX];
	probability pos_slot[LEN_TO_POS_STATES][POS_SLOTS];
	probability pos_special[FULL_DISTANCES - END_POS_MODEL_INDEX];
	probability pos_align[ALIGN_TABLE_SIZE];

	// These are the same as in lzma_decoder.c except that the encoders
	// include also price tables.
	lzma_length_encoder match_len_encoder;
	lzma_length_encoder rep_len_encoder;

	// Price tables
	uint32_t pos_slot_prices[LEN_TO_POS_STATES][POS_SLOTS];
	uint32_t distances_prices[LEN_TO_POS_STATES][FULL_DISTANCES];
	uint32_t dist_table_size;
	uint32_t match_price_count;

	uint32_t align_prices[ALIGN_TABLE_SIZE];
	uint32_t align_price_count;

	// Optimal
	uint32_t opts_end_index;
	uint32_t opts_current_index;
	lzma_optimal opts[OPTS];
};


extern void lzma_lzma_optimum_fast(
		lzma_coder *restrict coder, lzma_mf *restrict mf,
		uint32_t *restrict back_res, uint32_t *restrict len_res);

extern void lzma_lzma_optimum_normal(lzma_coder *restrict coder,
		lzma_mf *restrict mf, uint32_t *restrict back_res,
		uint32_t *restrict len_res, uint32_t position);

#endif
