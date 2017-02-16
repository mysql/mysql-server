/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
///////////////////////////////////////////////////////////////////////////////
//
/// \file       delta_encoder.h
/// \brief      Delta filter encoder
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LZMA_DELTA_ENCODER_H
#define LZMA_DELTA_ENCODER_H

#include "delta_common.h"

extern lzma_ret lzma_delta_encoder_init(lzma_next_coder *next,
		lzma_allocator *allocator, const lzma_filter_info *filters);

extern lzma_ret lzma_delta_props_encode(const void *options, uint8_t *out);

#endif
