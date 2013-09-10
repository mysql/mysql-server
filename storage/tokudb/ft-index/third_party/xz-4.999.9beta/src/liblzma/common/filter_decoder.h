/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
///////////////////////////////////////////////////////////////////////////////
//
/// \file       filter_decoder.c
/// \brief      Filter ID mapping to filter-specific functions
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LZMA_FILTER_DECODER_H
#define LZMA_FILTER_DECODER_H

#include "common.h"


extern lzma_ret lzma_raw_decoder_init(
		lzma_next_coder *next, lzma_allocator *allocator,
		const lzma_filter *options);

#endif
