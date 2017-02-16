/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
///////////////////////////////////////////////////////////////////////////////
//
/// \file       index.h
/// \brief      Handling of Index
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LZMA_INDEX_H
#define LZMA_INDEX_H

#include "common.h"


/// Minimum Unpadded Size
#define UNPADDED_SIZE_MIN LZMA_VLI_C(5)

/// Maximum Unpadded Size
#define UNPADDED_SIZE_MAX (LZMA_VLI_MAX & ~LZMA_VLI_C(3))


/// Get the size of the Index Padding field. This is needed by Index encoder
/// and decoder, but applications should have no use for this.
extern uint32_t lzma_index_padding_size(const lzma_index *i);


/// Round the variable-length integer to the next multiple of four.
static inline lzma_vli
vli_ceil4(lzma_vli vli)
{
	assert(vli <= LZMA_VLI_MAX);
	return (vli + 3) & ~LZMA_VLI_C(3);
}


/// Calculate the size of the Index field excluding Index Padding
static inline lzma_vli
index_size_unpadded(lzma_vli count, lzma_vli index_list_size)
{
	// Index Indicator + Number of Records + List of Records + CRC32
	return 1 + lzma_vli_size(count) + index_list_size + 4;
}


/// Calculate the size of the Index field including Index Padding
static inline lzma_vli
index_size(lzma_vli count, lzma_vli index_list_size)
{
	return vli_ceil4(index_size_unpadded(count, index_list_size));
}


/// Calculate the total size of the Stream
static inline lzma_vli
index_stream_size(lzma_vli blocks_size,
		lzma_vli count, lzma_vli index_list_size)
{
	return LZMA_STREAM_HEADER_SIZE + blocks_size
			+ index_size(count, index_list_size)
			+ LZMA_STREAM_HEADER_SIZE;
}

#endif
