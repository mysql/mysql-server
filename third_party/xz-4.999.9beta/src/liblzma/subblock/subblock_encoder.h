///////////////////////////////////////////////////////////////////////////////
//
/// \file       subblock_encoder.h
/// \brief      Encoder of the Subblock filter
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LZMA_SUBBLOCK_ENCODER_H
#define LZMA_SUBBLOCK_ENCODER_H

#include "common.h"

extern lzma_ret lzma_subblock_encoder_init(lzma_next_coder *next,
		lzma_allocator *allocator, const lzma_filter_info *filters);

#endif
