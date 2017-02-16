/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
///////////////////////////////////////////////////////////////////////////////
//
/// \file       block_decoder.h
/// \brief      Decodes .xz Blocks
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LZMA_BLOCK_DECODER_H
#define LZMA_BLOCK_DECODER_H

#include "common.h"


extern lzma_ret lzma_block_decoder_init(lzma_next_coder *next,
		lzma_allocator *allocator, lzma_block *block);

#endif
