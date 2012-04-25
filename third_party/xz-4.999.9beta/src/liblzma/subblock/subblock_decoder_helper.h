///////////////////////////////////////////////////////////////////////////////
//
/// \file       subblock_decoder_helper.h
/// \brief      Helper filter for the Subblock decoder
///
/// This filter is used to indicate End of Input for subfilters needing it.
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LZMA_SUBBLOCK_DECODER_HELPER_H
#define LZMA_SUBBLOCK_DECODER_HELPER_H

#include "common.h"


typedef struct {
	bool end_was_reached;
} lzma_options_subblock_helper;


extern lzma_ret lzma_subblock_decoder_helper_init(lzma_next_coder *next,
		lzma_allocator *allocator, const lzma_filter_info *filters);

#endif
