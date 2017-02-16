/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
///////////////////////////////////////////////////////////////////////////////
//
/// \file       common.h
/// \brief      Common functions needed in many places in liblzma
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#include "common.h"


/////////////
// Version //
/////////////

extern LZMA_API(uint32_t)
lzma_version_number(void)
{
	return LZMA_VERSION;
}


extern LZMA_API(const char *)
lzma_version_string(void)
{
	return LZMA_VERSION_STRING;
}


///////////////////////
// Memory allocation //
///////////////////////

extern void * lzma_attribute((malloc))
lzma_alloc(size_t size, lzma_allocator *allocator)
{
	// Some malloc() variants return NULL if called with size == 0.
	if (size == 0)
		size = 1;

	void *ptr;

	if (allocator != NULL && allocator->alloc != NULL)
		ptr = allocator->alloc(allocator->opaque, 1, size);
	else
		ptr = malloc(size);

	return ptr;
}


extern void
lzma_free(void *ptr, lzma_allocator *allocator)
{
	if (allocator != NULL && allocator->free != NULL)
		allocator->free(allocator->opaque, ptr);
	else
		free(ptr);

	return;
}


//////////
// Misc //
//////////

extern size_t
lzma_bufcpy(const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size, uint8_t *restrict out,
		size_t *restrict out_pos, size_t out_size)
{
	const size_t in_avail = in_size - *in_pos;
	const size_t out_avail = out_size - *out_pos;
	const size_t copy_size = MIN(in_avail, out_avail);

	memcpy(out + *out_pos, in + *in_pos, copy_size);

	*in_pos += copy_size;
	*out_pos += copy_size;

	return copy_size;
}


extern lzma_ret
lzma_next_filter_init(lzma_next_coder *next, lzma_allocator *allocator,
		const lzma_filter_info *filters)
{
	lzma_next_coder_init(filters[0].init, next, allocator);

	return filters[0].init == NULL
			? LZMA_OK : filters[0].init(next, allocator, filters);
}


extern void
lzma_next_end(lzma_next_coder *next, lzma_allocator *allocator)
{
	if (next->init != (uintptr_t)(NULL)) {
		// To avoid tiny end functions that simply call
		// lzma_free(coder, allocator), we allow leaving next->end
		// NULL and call lzma_free() here.
		if (next->end != NULL)
			next->end(next->coder, allocator);
		else
			lzma_free(next->coder, allocator);

		// Reset the variables so the we don't accidentally think
		// that it is an already initialized coder.
		*next = LZMA_NEXT_CODER_INIT;
	}

	return;
}


//////////////////////////////////////
// External to internal API wrapper //
//////////////////////////////////////

extern lzma_ret
lzma_strm_init(lzma_stream *strm)
{
	if (strm == NULL)
		return LZMA_PROG_ERROR;

	if (strm->internal == NULL) {
		strm->internal = lzma_alloc(sizeof(lzma_internal),
				strm->allocator);
		if (strm->internal == NULL)
			return LZMA_MEM_ERROR;

		strm->internal->next = LZMA_NEXT_CODER_INIT;
	}

	strm->internal->supported_actions[LZMA_RUN] = false;
	strm->internal->supported_actions[LZMA_SYNC_FLUSH] = false;
	strm->internal->supported_actions[LZMA_FULL_FLUSH] = false;
	strm->internal->supported_actions[LZMA_FINISH] = false;
	strm->internal->sequence = ISEQ_RUN;

	strm->total_in = 0;
	strm->total_out = 0;

	return LZMA_OK;
}


extern LZMA_API(lzma_ret)
lzma_code(lzma_stream *strm, lzma_action action)
{
	// Sanity checks
	if ((strm->next_in == NULL && strm->avail_in != 0)
			|| (strm->next_out == NULL && strm->avail_out != 0)
			|| strm->internal == NULL
			|| strm->internal->next.code == NULL
			|| (unsigned int)(action) > LZMA_FINISH
			|| !strm->internal->supported_actions[action])
		return LZMA_PROG_ERROR;

	switch (strm->internal->sequence) {
	case ISEQ_RUN:
		switch (action) {
		case LZMA_RUN:
			break;

		case LZMA_SYNC_FLUSH:
			strm->internal->sequence = ISEQ_SYNC_FLUSH;
			break;

		case LZMA_FULL_FLUSH:
			strm->internal->sequence = ISEQ_FULL_FLUSH;
			break;

		case LZMA_FINISH:
			strm->internal->sequence = ISEQ_FINISH;
			break;
		}

		break;

	case ISEQ_SYNC_FLUSH:
		// The same action must be used until we return
		// LZMA_STREAM_END, and the amount of input must not change.
		if (action != LZMA_SYNC_FLUSH
				|| strm->internal->avail_in != strm->avail_in)
			return LZMA_PROG_ERROR;

		break;

	case ISEQ_FULL_FLUSH:
		if (action != LZMA_FULL_FLUSH
				|| strm->internal->avail_in != strm->avail_in)
			return LZMA_PROG_ERROR;

		break;

	case ISEQ_FINISH:
		if (action != LZMA_FINISH
				|| strm->internal->avail_in != strm->avail_in)
			return LZMA_PROG_ERROR;

		break;

	case ISEQ_END:
		return LZMA_STREAM_END;

	case ISEQ_ERROR:
	default:
		return LZMA_PROG_ERROR;
	}

	size_t in_pos = 0;
	size_t out_pos = 0;
	lzma_ret ret = strm->internal->next.code(
			strm->internal->next.coder, strm->allocator,
			strm->next_in, &in_pos, strm->avail_in,
			strm->next_out, &out_pos, strm->avail_out, action);

	strm->next_in += in_pos;
	strm->avail_in -= in_pos;
	strm->total_in += in_pos;

	strm->next_out += out_pos;
	strm->avail_out -= out_pos;
	strm->total_out += out_pos;

	strm->internal->avail_in = strm->avail_in;

	switch (ret) {
	case LZMA_OK:
		// Don't return LZMA_BUF_ERROR when it happens the first time.
		// This is to avoid returning LZMA_BUF_ERROR when avail_out
		// was zero but still there was no more data left to written
		// to next_out.
		if (out_pos == 0 && in_pos == 0) {
			if (strm->internal->allow_buf_error)
				ret = LZMA_BUF_ERROR;
			else
				strm->internal->allow_buf_error = true;
		} else {
			strm->internal->allow_buf_error = false;
		}
		break;

	case LZMA_STREAM_END:
		if (strm->internal->sequence == ISEQ_SYNC_FLUSH
				|| strm->internal->sequence == ISEQ_FULL_FLUSH)
			strm->internal->sequence = ISEQ_RUN;
		else
			strm->internal->sequence = ISEQ_END;

	// Fall through

	case LZMA_NO_CHECK:
	case LZMA_UNSUPPORTED_CHECK:
	case LZMA_GET_CHECK:
	case LZMA_MEMLIMIT_ERROR:
		// Something else than LZMA_OK, but not a fatal error,
		// that is, coding may be continued (except if ISEQ_END).
		strm->internal->allow_buf_error = false;
		break;

	default:
		// All the other errors are fatal; coding cannot be continued.
		assert(ret != LZMA_BUF_ERROR);
		strm->internal->sequence = ISEQ_ERROR;
		break;
	}

	return ret;
}


extern LZMA_API(void)
lzma_end(lzma_stream *strm)
{
	if (strm != NULL && strm->internal != NULL) {
		lzma_next_end(&strm->internal->next, strm->allocator);
		lzma_free(strm->internal, strm->allocator);
		strm->internal = NULL;
	}

	return;
}


extern LZMA_API(lzma_check)
lzma_get_check(const lzma_stream *strm)
{
	// Return LZMA_CHECK_NONE if we cannot know the check type.
	// It's a bug in the application if this happens.
	if (strm->internal->next.get_check == NULL)
		return LZMA_CHECK_NONE;

	return strm->internal->next.get_check(strm->internal->next.coder);
}


extern LZMA_API(uint64_t)
lzma_memusage(const lzma_stream *strm)
{
	uint64_t memusage;
	uint64_t old_memlimit;

	if (strm == NULL || strm->internal == NULL
			|| strm->internal->next.memconfig == NULL
			|| strm->internal->next.memconfig(
				strm->internal->next.coder,
				&memusage, &old_memlimit, 0) != LZMA_OK)
		return 0;

	return memusage;
}


extern LZMA_API(uint64_t)
lzma_memlimit_get(const lzma_stream *strm)
{
	uint64_t old_memlimit;
	uint64_t memusage;

	if (strm == NULL || strm->internal == NULL
			|| strm->internal->next.memconfig == NULL
			|| strm->internal->next.memconfig(
				strm->internal->next.coder,
				&memusage, &old_memlimit, 0) != LZMA_OK)
		return 0;

	return old_memlimit;
}


extern LZMA_API(lzma_ret)
lzma_memlimit_set(lzma_stream *strm, uint64_t new_memlimit)
{
	// Dummy variables to simplify memconfig functions
	uint64_t old_memlimit;
	uint64_t memusage;

	if (strm == NULL || strm->internal == NULL
			|| strm->internal->next.memconfig == NULL)
		return LZMA_PROG_ERROR;

	if (new_memlimit != 0 && new_memlimit < LZMA_MEMUSAGE_BASE)
		return LZMA_MEMLIMIT_ERROR;

	return strm->internal->next.memconfig(strm->internal->next.coder,
			&memusage, &old_memlimit, new_memlimit);
}
