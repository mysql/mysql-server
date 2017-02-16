/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
///////////////////////////////////////////////////////////////////////////////
//
/// \file       lzma_decoder.h
/// \brief      LZMA decoder API
///
//  Authors:    Igor Pavlov
//              Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LZMA_LZMA_DECODER_H
#define LZMA_LZMA_DECODER_H

#include "common.h"


/// Allocates and initializes LZMA decoder
extern lzma_ret lzma_lzma_decoder_init(lzma_next_coder *next,
		lzma_allocator *allocator, const lzma_filter_info *filters);

extern uint64_t lzma_lzma_decoder_memusage(const void *options);

extern lzma_ret lzma_lzma_props_decode(
		void **options, lzma_allocator *allocator,
		const uint8_t *props, size_t props_size);


/// \brief      Decodes the LZMA Properties byte (lc/lp/pb)
///
/// \return     true if error occorred, false on success
///
extern bool lzma_lzma_lclppb_decode(
		lzma_options_lzma *options, uint8_t byte);


#ifdef LZMA_LZ_DECODER_H
/// Allocate and setup function pointers only. This is used by LZMA1 and
/// LZMA2 decoders.
extern lzma_ret lzma_lzma_decoder_create(
		lzma_lz_decoder *lz, lzma_allocator *allocator,
		const void *opt, lzma_lz_options *lz_options);

/// Gets memory usage without validating lc/lp/pb. This is used by LZMA2
/// decoder, because raw LZMA2 decoding doesn't need lc/lp/pb.
extern uint64_t lzma_lzma_decoder_memusage_nocheck(const void *options);

#endif

#endif
