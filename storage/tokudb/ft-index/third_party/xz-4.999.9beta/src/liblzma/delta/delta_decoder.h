/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
///////////////////////////////////////////////////////////////////////////////
//
/// \file       delta_decoder.h
/// \brief      Delta filter decoder
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LZMA_DELTA_DECODER_H
#define LZMA_DELTA_DECODER_H

#include "delta_common.h"

extern lzma_ret lzma_delta_decoder_init(lzma_next_coder *next,
		lzma_allocator *allocator, const lzma_filter_info *filters);

extern lzma_ret lzma_delta_props_decode(
		void **options, lzma_allocator *allocator,
		const uint8_t *props, size_t props_size);

#endif
