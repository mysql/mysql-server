/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
///////////////////////////////////////////////////////////////////////////////
//
/// \file       stream_encoder.h
/// \brief      Encodes .xz Streams
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LZMA_STREAM_ENCODER_H
#define LZMA_STREAM_ENCODER_H

#include "common.h"


extern lzma_ret lzma_stream_encoder_init(
		lzma_next_coder *next, lzma_allocator *allocator,
		const lzma_filter *filters, lzma_check check);

#endif
