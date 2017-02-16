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

#include "filter_encoder.h"
#include "filter_common.h"
#include "lzma_encoder.h"
#include "lzma2_encoder.h"
#include "subblock_encoder.h"
#include "simple_encoder.h"
#include "delta_encoder.h"


typedef struct {
	/// Filter ID
	lzma_vli id;

	/// Initializes the filter encoder and calls lzma_next_filter_init()
	/// for filters + 1.
	lzma_init_function init;

	/// Calculates memory usage of the encoder. If the options are
	/// invalid, UINT64_MAX is returned.
	uint64_t (*memusage)(const void *options);

	/// Calculates the minimum sane size for Blocks (or other types of
	/// chunks) to which the input data can be splitted to make
	/// multithreaded encoding possible. If this is NULL, it is assumed
	/// that the encoder is fast enough with single thread.
	lzma_vli (*chunk_size)(const void *options);

	/// Tells the size of the Filter Properties field. If options are
	/// invalid, UINT32_MAX is returned. If this is NULL, props_size_fixed
	/// is used.
	lzma_ret (*props_size_get)(uint32_t *size, const void *options);
	uint32_t props_size_fixed;

	/// Encodes Filter Properties.
	///
	/// \return     - LZMA_OK: Properties encoded sucessfully.
	///             - LZMA_OPTIONS_ERROR: Unsupported options
	///             - LZMA_PROG_ERROR: Invalid options or not enough
	///               output space
	lzma_ret (*props_encode)(const void *options, uint8_t *out);

} lzma_filter_encoder;


static const lzma_filter_encoder encoders[] = {
#ifdef HAVE_ENCODER_LZMA1
	{
		.id = LZMA_FILTER_LZMA1,
		.init = &lzma_lzma_encoder_init,
		.memusage = &lzma_lzma_encoder_memusage,
		.chunk_size = NULL, // FIXME
		.props_size_get = NULL,
		.props_size_fixed = 5,
		.props_encode = &lzma_lzma_props_encode,
	},
#endif
#ifdef HAVE_ENCODER_LZMA2
	{
		.id = LZMA_FILTER_LZMA2,
		.init = &lzma_lzma2_encoder_init,
		.memusage = &lzma_lzma2_encoder_memusage,
		.chunk_size = NULL, // FIXME
		.props_size_get = NULL,
		.props_size_fixed = 1,
		.props_encode = &lzma_lzma2_props_encode,
	},
#endif
#ifdef HAVE_ENCODER_SUBBLOCK
	{
		.id = LZMA_FILTER_SUBBLOCK,
		.init = &lzma_subblock_encoder_init,
// 		.memusage = &lzma_subblock_encoder_memusage,
		.chunk_size = NULL,
		.props_size_get = NULL,
		.props_size_fixed = 0,
		.props_encode = NULL,
	},
#endif
#ifdef HAVE_ENCODER_X86
	{
		.id = LZMA_FILTER_X86,
		.init = &lzma_simple_x86_encoder_init,
		.memusage = NULL,
		.chunk_size = NULL,
		.props_size_get = &lzma_simple_props_size,
		.props_encode = &lzma_simple_props_encode,
	},
#endif
#ifdef HAVE_ENCODER_POWERPC
	{
		.id = LZMA_FILTER_POWERPC,
		.init = &lzma_simple_powerpc_encoder_init,
		.memusage = NULL,
		.chunk_size = NULL,
		.props_size_get = &lzma_simple_props_size,
		.props_encode = &lzma_simple_props_encode,
	},
#endif
#ifdef HAVE_ENCODER_IA64
	{
		.id = LZMA_FILTER_IA64,
		.init = &lzma_simple_ia64_encoder_init,
		.memusage = NULL,
		.chunk_size = NULL,
		.props_size_get = &lzma_simple_props_size,
		.props_encode = &lzma_simple_props_encode,
	},
#endif
#ifdef HAVE_ENCODER_ARM
	{
		.id = LZMA_FILTER_ARM,
		.init = &lzma_simple_arm_encoder_init,
		.memusage = NULL,
		.chunk_size = NULL,
		.props_size_get = &lzma_simple_props_size,
		.props_encode = &lzma_simple_props_encode,
	},
#endif
#ifdef HAVE_ENCODER_ARMTHUMB
	{
		.id = LZMA_FILTER_ARMTHUMB,
		.init = &lzma_simple_armthumb_encoder_init,
		.memusage = NULL,
		.chunk_size = NULL,
		.props_size_get = &lzma_simple_props_size,
		.props_encode = &lzma_simple_props_encode,
	},
#endif
#ifdef HAVE_ENCODER_SPARC
	{
		.id = LZMA_FILTER_SPARC,
		.init = &lzma_simple_sparc_encoder_init,
		.memusage = NULL,
		.chunk_size = NULL,
		.props_size_get = &lzma_simple_props_size,
		.props_encode = &lzma_simple_props_encode,
	},
#endif
#ifdef HAVE_ENCODER_DELTA
	{
		.id = LZMA_FILTER_DELTA,
		.init = &lzma_delta_encoder_init,
		.memusage = &lzma_delta_coder_memusage,
		.chunk_size = NULL,
		.props_size_get = NULL,
		.props_size_fixed = 1,
		.props_encode = &lzma_delta_props_encode,
	},
#endif
};


static const lzma_filter_encoder *
encoder_find(lzma_vli id)
{
	for (size_t i = 0; i < ARRAY_SIZE(encoders); ++i)
		if (encoders[i].id == id)
			return encoders + i;

	return NULL;
}


extern LZMA_API(lzma_bool)
lzma_filter_encoder_is_supported(lzma_vli id)
{
	return encoder_find(id) != NULL;
}


extern lzma_ret
lzma_raw_encoder_init(lzma_next_coder *next, lzma_allocator *allocator,
		const lzma_filter *options)
{
	return lzma_raw_coder_init(next, allocator,
			options, (lzma_filter_find)(&encoder_find), true);
}


extern LZMA_API(lzma_ret)
lzma_raw_encoder(lzma_stream *strm, const lzma_filter *options)
{
	lzma_next_strm_init(lzma_raw_coder_init, strm, options,
			(lzma_filter_find)(&encoder_find), true);

	strm->internal->supported_actions[LZMA_RUN] = true;
	strm->internal->supported_actions[LZMA_SYNC_FLUSH] = true;
	strm->internal->supported_actions[LZMA_FINISH] = true;

	return LZMA_OK;
}


extern LZMA_API(uint64_t)
lzma_raw_encoder_memusage(const lzma_filter *filters)
{
	return lzma_raw_coder_memusage(
			(lzma_filter_find)(&encoder_find), filters);
}


extern LZMA_API(lzma_vli)
lzma_chunk_size(const lzma_filter *filters)
{
	lzma_vli max = 0;

	for (size_t i = 0; filters[i].id != LZMA_VLI_UNKNOWN; ++i) {
		const lzma_filter_encoder *const fe
				= encoder_find(filters[i].id);
		if (fe->chunk_size != NULL) {
			const lzma_vli size
					= fe->chunk_size(filters[i].options);
			if (size == LZMA_VLI_UNKNOWN)
				return LZMA_VLI_UNKNOWN;

			if (size > max)
				max = size;
		}
	}

	return max;
}


extern LZMA_API(lzma_ret)
lzma_properties_size(uint32_t *size, const lzma_filter *filter)
{
	const lzma_filter_encoder *const fe = encoder_find(filter->id);
	if (fe == NULL) {
		// Unknown filter - if the Filter ID is a proper VLI,
		// return LZMA_OPTIONS_ERROR instead of LZMA_PROG_ERROR,
		// because it's possible that we just don't have support
		// compiled in for the requested filter.
		return filter->id <= LZMA_VLI_MAX
				? LZMA_OPTIONS_ERROR : LZMA_PROG_ERROR;
	}

	if (fe->props_size_get == NULL) {
		// No props_size_get() function, use props_size_fixed.
		*size = fe->props_size_fixed;
		return LZMA_OK;
	}

	return fe->props_size_get(size, filter->options);
}


extern LZMA_API(lzma_ret)
lzma_properties_encode(const lzma_filter *filter, uint8_t *props)
{
	const lzma_filter_encoder *const fe = encoder_find(filter->id);
	if (fe == NULL)
		return LZMA_PROG_ERROR;

	if (fe->props_encode == NULL)
		return LZMA_OK;

	return fe->props_encode(filter->options, props);
}
