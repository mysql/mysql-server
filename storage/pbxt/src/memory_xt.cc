/* Copyright (c) 2005 PrimeBase Technologies GmbH
 *
 * PrimeBase XT
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * 2005-01-04	Paul McCullagh
 *
 * H&G2JCtL
 */

#include "xt_config.h"

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "pthread_xt.h"
#include "thread_xt.h"
#include "strutil_xt.h"
#include "trace_xt.h"

#ifdef DEBUG
#define RECORD_MM
#endif

#ifdef DEBUG

#undef	xt_malloc
#undef	xt_calloc
#undef	xt_realloc
#undef	xt_free
#undef	xt_pfree

#undef	xt_malloc_ns
#undef	xt_calloc_ns
#undef	xt_realloc_ns
#undef	xt_free_ns

void	*xt_malloc(XTThreadPtr self, size_t size);
void	*xt_calloc(XTThreadPtr self, size_t size);
xtBool	xt_realloc(XTThreadPtr self, void **ptr, size_t size);
void	xt_free(XTThreadPtr self, void *ptr);
void	xt_pfree(XTThreadPtr self, void **ptr);

void	*xt_malloc_ns(size_t size);
void	*xt_calloc_ns(size_t size);
xtBool	xt_realloc_ns(void **ptr, size_t size);
void	xt_free_ns(void *ptr);

#define ADD_TOTAL_ALLOCS			4000

#define SHIFT_RIGHT(ptr, n)			memmove(((char *) (ptr)) + sizeof(MissingMemoryRec), (ptr), (long) (n) * sizeof(MissingMemoryRec))
#define SHIFT_LEFT(ptr, n)			memmove((ptr), ((char *) (ptr)) + sizeof(MissingMemoryRec), (long) (n) * sizeof(MissingMemoryRec))

#define STACK_TRACE_DEPTH			4

typedef struct MissingMemory {
	void			*mm_ptr;
	xtWord4			id;
	xtWord2			line_nr;
	xtWord2			trace_count;
	c_char			*mm_file;
	c_char			*mm_func[STACK_TRACE_DEPTH];
} MissingMemoryRec, *MissingMemoryPtr;

static MissingMemoryRec	*mm_addresses = NULL;
static long				mm_nr_in_use = 0L;
static long				mm_total_allocated = 0L;
static xtWord4			mm_alloc_count = 0;
static xt_mutex_type	mm_mutex;

#ifdef RECORD_MM
static long mm_find_pointer(void *ptr);
#endif

#endif

/*
 * -----------------------------------------------------------------------
 * STANDARD SYSTEM BASED MEMORY ALLOCATION
 */

xtPublic void *xt_malloc(XTThreadPtr self, size_t size)
{
	void *ptr;

	if (!(ptr = malloc(size))) {
		xt_throw_errno(XT_CONTEXT, XT_ENOMEM);
		return NULL;
	}
	return ptr;
}

xtPublic xtBool	xt_realloc(XTThreadPtr self, void **ptr, size_t size)
{
	void *new_ptr;

	if (!(new_ptr = realloc(*ptr, size))) {
		xt_throw_errno(XT_CONTEXT, XT_ENOMEM);
		return FAILED;
	}
	*ptr = new_ptr;
	return OK;
}

xtPublic void xt_free(XTThreadPtr XT_UNUSED(self), void *ptr)
{
	free(ptr);
}

xtPublic void *xt_calloc(XTThreadPtr self, size_t size)
{
	void *ptr;

	if ((ptr = xt_malloc(self, size)))
		memset(ptr, 0, size);
	return ptr;
}

#undef	xt_pfree

xtPublic void xt_pfree(XTThreadPtr self, void **ptr)
{
	if (*ptr) {
		void *p = *ptr;

		*ptr = NULL;
		xt_free(self, p);
	}
}

/*
 * -----------------------------------------------------------------------
 * SYSTEM MEMORY ALLOCATION WITH A THREAD
 */

xtPublic void *xt_malloc_ns(size_t size)
{
	void *ptr;

	if (!(ptr = malloc(size))) {
		xt_register_errno(XT_REG_CONTEXT, XT_ENOMEM);
		return NULL;
	}
	return ptr;
}

xtPublic void *xt_calloc_ns(size_t size)
{
	void *ptr;

	if (!(ptr = malloc(size))) {
		xt_register_errno(XT_REG_CONTEXT, XT_ENOMEM);
		return NULL;
	}
	memset(ptr, 0, size);
	return ptr;
}

xtPublic xtBool	xt_realloc_ns(void **ptr, size_t size)
{
	void *new_ptr;

	if (!(new_ptr = realloc(*ptr, size)))
		return xt_register_errno(XT_REG_CONTEXT, XT_ENOMEM);
	*ptr = new_ptr;
	return OK;
}

xtPublic void xt_free_ns(void *ptr)
{
	free(ptr);
}

#ifdef DEBUG_MEMORY

/*
 * -----------------------------------------------------------------------
 * MEMORY SEARCHING CODE
 */

#define MM_THROW_ASSERTION(str) mm_throw_assertion(self, __FUNC__, __FILE__, __LINE__, str)

static void mm_throw_assertion(XTThreadPtr self, c_char *func, c_char *file, u_int line, c_char *str)
{
	printf("***** MM:FATAL %s\n", str);
	xt_throw_assertion(self, func, file, line, str);
}

/*
 * -----------------------------------------------------------------------
 * MEMORY SEARCHING CODE
 */

static int mm_debug_ik_inc;
static int mm_debug_ik_dec;
static int mm_debug_ik_no;

/*
 * Call this function where the missing memory
 * is referenced.
 */
xtPublic void mm_trace_inc(XTThreadPtr self, XTMMTraceRefPtr tr)
{
	int i;

#ifdef RECORD_MM
	if (xt_lock_mutex(self, &mm_mutex)) {
		long mm;

		mm = mm_find_pointer(tr);
		if (mm >= 0)
			mm_addresses[mm].trace_count = 1;
		xt_unlock_mutex(self, &mm_mutex);
	}
#endif
	mm_debug_ik_inc++;
	if (tr->mm_pos < XT_MM_STACK_TRACE-1) {
		tr->mm_trace[tr->mm_pos++] = self->t_name[0] == 'S' ? XT_MM_TRACE_SW_INC : XT_MM_TRACE_INC;
		for (i=1; i<=XT_MM_TRACE_DEPTH; i++) {
			if (self->t_call_top-i < 0)
				break;
			if (tr->mm_pos < XT_MM_STACK_TRACE-1) {
				tr->mm_line[tr->mm_pos] = self->t_call_stack[self->t_call_top-i].cs_line;
				tr->mm_trace[tr->mm_pos++] = self->t_call_stack[self->t_call_top-i].cs_func;
			}
			else if (tr->mm_pos < XT_MM_STACK_TRACE)
				tr->mm_trace[tr->mm_pos++] = XT_MM_TRACE_ERROR;
		}
	}
	else if (tr->mm_pos < XT_MM_STACK_TRACE)
		tr->mm_trace[tr->mm_pos++] = XT_MM_TRACE_ERROR;
}

xtPublic void mm_trace_dec(XTThreadPtr self, XTMMTraceRefPtr tr)
{
	int i;

#ifdef RECORD_MM
	if (xt_lock_mutex(self, &mm_mutex)) {
		long mm;

		mm = mm_find_pointer(tr);
		if (mm >= 0)
			mm_addresses[mm].trace_count = 1;
		xt_unlock_mutex(self, &mm_mutex);
	}
#endif
	mm_debug_ik_dec++;
	if (tr->mm_pos < XT_MM_STACK_TRACE-1) {
		tr->mm_trace[tr->mm_pos++] = self->t_name[0] == 'S' ? XT_MM_TRACE_SW_DEC : XT_MM_TRACE_DEC;
		for (i=1; i<=XT_MM_TRACE_DEPTH; i++) {
			if (self->t_call_top-i < 0)
				break;
			if (tr->mm_pos < XT_MM_STACK_TRACE-1) {
				tr->mm_line[tr->mm_pos] = self->t_call_stack[self->t_call_top-i].cs_line;
				tr->mm_trace[tr->mm_pos++] = self->t_call_stack[self->t_call_top-i].cs_func;
			}
			else if (tr->mm_pos < XT_MM_STACK_TRACE)
				tr->mm_trace[tr->mm_pos++] = XT_MM_TRACE_ERROR;
		}
	}
	else if (tr->mm_pos < XT_MM_STACK_TRACE)
		tr->mm_trace[tr->mm_pos++] = XT_MM_TRACE_ERROR;
}

xtPublic void mm_trace_init(XTThreadPtr self, XTMMTraceRefPtr tr)
{
	mm_debug_ik_no++;
	tr->mm_id = (u_int) mm_debug_ik_no;
	tr->mm_pos = 0;
	mm_trace_inc(self, tr);
}

xtPublic void mm_trace_print(XTMMTraceRefPtr tr)
{
	int i, cnt = 0;

	for (i=0; i<tr->mm_pos; i++) {
		if (tr->mm_trace[i] == XT_MM_TRACE_INC) {
			if (i > 0)
				printf("\n");
			cnt++;
			printf("INC (%d) ", cnt);
		}
		else if (tr->mm_trace[i] == XT_MM_TRACE_SW_INC) {
			if (i > 0)
				printf("\n");
			printf("SW-DEC (%d) ", cnt);
			cnt--;
		}
		else if (tr->mm_trace[i] == XT_MM_TRACE_DEC) {
			if (i > 0)
				printf("\n");
			printf("DEC (%d) ", cnt);
			cnt--;
		}
		else if (tr->mm_trace[i] == XT_MM_TRACE_SW_DEC) {
			if (i > 0)
				printf("\n");
			printf("SW-DEC (%d) ", cnt);
			cnt--;
		}
		else if (tr->mm_trace[i] == XT_MM_TRACE_ERROR) {
			if (i > 0)
				printf("\n");
			printf("ERROR: Space out");
		}
		else
			printf("%s(%d) ", tr->mm_trace[i], (int) tr->mm_line[i]);
	}
	printf("\n");
}

/* Call this function on exit, when you know the memory is missing. */
static void mm_debug_trace_count(XTMMTraceRefPtr tr)
{
	printf("MM Trace ID: %d\n", tr->mm_id);
	mm_trace_print(tr);
}

/* The give the sum of allocations, etc. */
static void mm_debug_trace_sum(void)
{
	if (mm_debug_ik_no) {
		printf("MM Trace INC: %d\n", mm_debug_ik_inc);
		printf("MM Trace DEC: %d\n", mm_debug_ik_dec);
		printf("MM Trace ALL: %d\n", mm_debug_ik_no);
	}
}

/*
 * -----------------------------------------------------------------------
 * DEBUG MEMORY ALLOCATION AND HEAP CHECKING
 */

#ifdef RECORD_MM
static long mm_find_pointer(void *ptr)
{
	register long	i, n, guess;

	i = 0;
	n = mm_nr_in_use;
	while (i < n) {
		guess = (i + n - 1) >> 1;
		if (ptr == mm_addresses[guess].mm_ptr)
			return(guess);
		if (ptr < mm_addresses[guess].mm_ptr)
			n = guess;
		else
			i = guess + 1;
	}
	return(-1);
}

static long mm_add_pointer(void *ptr, u_int id)
{
#pragma unused(id)
	register int	i, n, guess;

	if (mm_nr_in_use == mm_total_allocated) {
		/* Not enough space, add more: */
		MissingMemoryRec *new_addresses;

		new_addresses = (MissingMemoryRec *) xt_calloc_ns(sizeof(MissingMemoryRec) * (mm_total_allocated + ADD_TOTAL_ALLOCS));
		if (!new_addresses)
			return(-1);

		if (mm_addresses) {
			memcpy(new_addresses, mm_addresses, sizeof(MissingMemoryRec) * mm_total_allocated);
			free(mm_addresses);
		}

		mm_addresses = new_addresses;
		mm_total_allocated += ADD_TOTAL_ALLOCS;
	}

	i = 0;
	n = mm_nr_in_use;
	while (i < n) {
		guess = (i + n - 1) >> 1;
		if (ptr < mm_addresses[guess].mm_ptr)
			n = guess;
		else
			i = guess + 1;
	}

	SHIFT_RIGHT(&mm_addresses[i], mm_nr_in_use - i);
	mm_nr_in_use++;
	mm_addresses[i].mm_ptr = ptr;
	return(i);
}

xtPublic char *mm_watch_point = 0;

static long mm_remove_pointer(void *ptr)
{
	register int	i, n, guess;

	if (mm_watch_point == ptr)
		printf("Hit watch point!\n");

	i = 0;
	n = mm_nr_in_use;
	while (i < n) {
		guess = (i + n - 1) >> 1;
		if (ptr == mm_addresses[guess].mm_ptr)
			goto remove;
		if (ptr < mm_addresses[guess].mm_ptr)
			n = guess;
		else
			i = guess + 1;
	}
	return(-1);

	remove:
	/* Decrease the number of sets, and shift left: */
	mm_nr_in_use--;
	SHIFT_LEFT(&mm_addresses[guess], mm_nr_in_use - guess);	
	return(guess);
}

static void mm_add_core_ptr(XTThreadPtr self, void *ptr, u_int id, u_int line, c_char *file_name)
{
	long mm;

	mm = mm_add_pointer(ptr, id);
	if (mm < 0) {
		MM_THROW_ASSERTION("MM ERROR: Cannot allocate table big enough!");
		return;
	}

	/* Record the pointer: */
	if (mm_alloc_count >= 4115 && mm_alloc_count <= 4130) {
		if (id)
			mm_addresses[mm].id = id;
		else
			mm_addresses[mm].id = mm_alloc_count++;
	}
	else {
		if (id)
			mm_addresses[mm].id = id;
		else
			mm_addresses[mm].id = mm_alloc_count++;
	}
	mm_addresses[mm].mm_ptr = ptr;
	mm_addresses[mm].line_nr = (ushort) line;
	if (file_name)
		mm_addresses[mm].mm_file = file_name;
	else
		mm_addresses[mm].mm_file = "?";
	if (self) {
		for (int i=1; i<=STACK_TRACE_DEPTH; i++) {
			if (self->t_call_top-i >= 0)
				mm_addresses[mm].mm_func[i-1] = self->t_call_stack[self->t_call_top-i].cs_func;
			else
				mm_addresses[mm].mm_func[i-1] = NULL;
		}
	}
	else {
		for (int i=0; i<STACK_TRACE_DEPTH; i++)
			mm_addresses[mm].mm_func[i] = NULL;
	}
}

static void mm_remove_core_ptr(void *ptr)
{
	XTThreadPtr	self = NULL;
	long		mm;

	mm = mm_remove_pointer(ptr);
	if (mm < 0) {
		MM_THROW_ASSERTION("Pointer not allocated");
		return;
	}
}

static void mm_throw_assertion(MissingMemoryPtr mm_ptr, void *p, c_char *message);

static long mm_find_core_ptr(void *ptr)
{
	long mm;

	mm = mm_find_pointer(ptr);
	if (mm < 0)
		mm_throw_assertion(NULL, ptr, "Pointer not allocated");
	return(mm);
}

static void mm_replace_core_ptr(long i, void *ptr)
{
	XTThreadPtr			self = NULL;
	MissingMemoryRec	tmp = mm_addresses[i];
	long				mm;

	mm_remove_pointer(mm_addresses[i].mm_ptr);
	mm = mm_add_pointer(ptr, mm_addresses[i].id);
	if (mm < 0) {
		MM_THROW_ASSERTION("Cannot allocate table big enough!");
		return;
	}
	mm_addresses[mm] = tmp;
	mm_addresses[mm].mm_ptr = ptr;
}
#endif

static void mm_throw_assertion(MissingMemoryPtr mm_ptr, void *p, c_char *message)
{
	XTThreadPtr	self = NULL;
	char		str[200];

	if (mm_ptr) {
		sprintf(str, "MM: %08lX (#%ld) %s:%d %s",
					   (unsigned long) mm_ptr->mm_ptr,
					   (long) mm_ptr->id,
					   xt_last_name_of_path(mm_ptr->mm_file),
					   (int) mm_ptr->line_nr,
					   message);
	}
	else
		sprintf(str, "MM: %08lX %s", (unsigned long) p, message);
	MM_THROW_ASSERTION(str);
}

/*
 * -----------------------------------------------------------------------
 * MISSING MEMORY PUBLIC ROUTINES
 */

#define MEM_DEBUG_HDR_SIZE		offsetof(MemoryDebugRec, data)
#define MEM_TRAILER_SIZE		2
#define MEM_HEADER				0x01010101
#define MEM_FREED				0x03030303
#define MEM_TRAILER_BYTE		0x02
#define MEM_FREED_BYTE			0x03

typedef struct MemoryDebug {
	xtWord4		check;
	xtWord4		size;
	char		data[200];
} MemoryDebugRec, *MemoryDebugPtr;

static size_t mm_checkmem(XTThreadPtr self, MissingMemoryPtr mm_ptr, void *p, xtBool freeme)
{
	unsigned char	*ptr	= (unsigned char *) p - MEM_DEBUG_HDR_SIZE;
	MemoryDebugPtr	debug_ptr = (MemoryDebugPtr) ptr;
	size_t			size	= debug_ptr->size;
	long			a_value;  /* Added to simplfy debugging. */

	if (!ASSERT(p)) 
		return(0);
	if (!ASSERT(((long) p & 1L) == 0)) 
		return(0);
	a_value = MEM_FREED;
	if (debug_ptr->check == MEM_FREED) { 
		mm_throw_assertion(mm_ptr, p, "Pointer already freed 'debug_ptr->check != MEM_FREED'");
		return(0);
	}
	a_value = MEM_HEADER;
	if (debug_ptr->check != MEM_HEADER) {
		mm_throw_assertion(mm_ptr, p, "Header not valid 'debug_ptr->check != MEM_HEADER'");
		return(0);
	}
	a_value = MEM_TRAILER_BYTE;
	if (!(*((unsigned char *) ptr + size + MEM_DEBUG_HDR_SIZE) == MEM_TRAILER_BYTE &&
			*((unsigned char *) ptr + size + MEM_DEBUG_HDR_SIZE + 1L) == MEM_TRAILER_BYTE)) { 
		mm_throw_assertion(mm_ptr, p, "Trailer overwritten");
		return(0);
	}

	if (freeme) {
		debug_ptr->check = MEM_FREED;
		*((unsigned char *) ptr + size + MEM_DEBUG_HDR_SIZE) = MEM_FREED_BYTE;
		*((unsigned char *) ptr + size + MEM_DEBUG_HDR_SIZE + 1L) = MEM_FREED_BYTE;

		memset(((unsigned char *) ptr) + MEM_DEBUG_HDR_SIZE, 0xF5, size);
		xt_free(self, ptr);
	}

	return size;
}

xtBool xt_mm_scan_core(void)
{
	long mm;

	if (!mm_addresses)
		return TRUE;

	if (!xt_lock_mutex(NULL, &mm_mutex))
		return TRUE;

	for (mm=0; mm<mm_nr_in_use; mm++)	{
		mm_checkmem(NULL, &mm_addresses[mm], mm_addresses[mm].mm_ptr, FALSE);
	}
	
	xt_unlock_mutex(NULL, &mm_mutex);
	return TRUE;
}

void xt_mm_memmove(void *block, void *dest, void *source, size_t size)
{
	if (block) {
		MemoryDebugPtr	debug_ptr = (MemoryDebugPtr) ((char *) block - MEM_DEBUG_HDR_SIZE);

#ifdef RECORD_MM
		if (xt_lock_mutex(NULL, &mm_mutex)) {
			mm_find_core_ptr(block);
			xt_unlock_mutex(NULL, &mm_mutex);
		}
#endif
		mm_checkmem(NULL, NULL, block, FALSE);

		if (dest < block || (char *) dest > (char *) block + debug_ptr->size)
			mm_throw_assertion(NULL, block, "Destination not in block");
		if ((char *) dest + size > (char *) block + debug_ptr->size)
			mm_throw_assertion(NULL, block, "Copy will overwrite memory");
	}

	memmove(dest, source, size);
}

void xt_mm_memcpy(void *block, void *dest, void *source, size_t size)
{
	if (block) {
		MemoryDebugPtr	debug_ptr = (MemoryDebugPtr) ((char *) block - MEM_DEBUG_HDR_SIZE);

#ifdef RECORD_MM
		if (xt_lock_mutex(NULL, &mm_mutex)) {
			mm_find_core_ptr(block);
			xt_unlock_mutex(NULL, &mm_mutex);
		}
#endif
		mm_checkmem(NULL, NULL, block, FALSE);

		if (dest < block || (char *) dest > (char *) block + debug_ptr->size)
			mm_throw_assertion(NULL, block, "Destination not in block");
		if ((char *) dest + size > (char *) block + debug_ptr->size)
			mm_throw_assertion(NULL, block, "Copy will overwrite memory");
	}

	memcpy(dest, source, size);
}

void xt_mm_memset(void *block, void *dest, int value, size_t size)
{
	if (block) {
		MemoryDebugPtr	debug_ptr = (MemoryDebugPtr) ((char *) block - MEM_DEBUG_HDR_SIZE);

#ifdef RECORD_MM
		if (xt_lock_mutex(NULL, &mm_mutex)) {
			mm_find_core_ptr(block);
			xt_unlock_mutex(NULL, &mm_mutex);
		}
#endif
		mm_checkmem(NULL, NULL, block, FALSE);

		if (dest < block || (char *) dest > (char *) block + debug_ptr->size)
			mm_throw_assertion(NULL, block, "Destination not in block");
		if ((char *) dest + size > (char *) block + debug_ptr->size)
			mm_throw_assertion(NULL, block, "Copy will overwrite memory");
	}

	memset(dest, value, size);
}

void *xt_mm_malloc(XTThreadPtr self, size_t size, u_int line, c_char *file)
{
	unsigned char *p;

	if (size > (600*1024*1024))
		mm_throw_assertion(NULL, NULL, "Very large block allocated - meaybe error");
	p = (unsigned char *) xt_malloc(self, size + MEM_DEBUG_HDR_SIZE + MEM_TRAILER_SIZE);
	if (!p)
		return NULL;

	memset(p, 0x55, size + MEM_DEBUG_HDR_SIZE + MEM_TRAILER_SIZE);

	((MemoryDebugPtr) p)->check = MEM_HEADER;
	((MemoryDebugPtr) p)->size  = size;
	*(p + size + MEM_DEBUG_HDR_SIZE) = MEM_TRAILER_BYTE;
	*(p + size + MEM_DEBUG_HDR_SIZE + 1L) = MEM_TRAILER_BYTE;

	(void) line;
	(void) file;
#ifdef RECORD_MM
	xt_lock_mutex(self, &mm_mutex);
	mm_add_core_ptr(self, p + MEM_DEBUG_HDR_SIZE, 0, line, file);
	xt_unlock_mutex(self, &mm_mutex);
#endif

	return p + MEM_DEBUG_HDR_SIZE;
}

void *xt_mm_calloc(XTThreadPtr self, size_t size, u_int line, c_char *file)
{
	unsigned char *p;
	
	if (size > (500*1024*1024))
		mm_throw_assertion(NULL, NULL, "Very large block allocated - meaybe error");
	p = (unsigned char *) xt_calloc(self, size + MEM_DEBUG_HDR_SIZE + MEM_TRAILER_SIZE);
	if (!p) 
		return NULL;

	((MemoryDebugPtr) p)->check = MEM_HEADER;
	((MemoryDebugPtr) p)->size  = size;
	*(p + size + MEM_DEBUG_HDR_SIZE) = MEM_TRAILER_BYTE;
	*(p + size + MEM_DEBUG_HDR_SIZE + 1L) = MEM_TRAILER_BYTE;

	(void) line;
	(void) file;
#ifdef RECORD_MM
	xt_lock_mutex(self, &mm_mutex);
	mm_add_core_ptr(self, p + MEM_DEBUG_HDR_SIZE, 0, line, file);
	xt_unlock_mutex(self, &mm_mutex);
#endif

	return p + MEM_DEBUG_HDR_SIZE;
}

xtBool xt_mm_sys_realloc(XTThreadPtr self, void **ptr, size_t newsize, u_int line, c_char *file)
{
	return xt_mm_realloc(self, ptr, newsize, line, file);
}

xtBool xt_mm_realloc(XTThreadPtr self, void **ptr, size_t newsize, u_int line, c_char *file)
{
	unsigned char	*oldptr = (unsigned char *) *ptr;
	size_t			size;
#ifdef RECORD_MM
	long			mm;
#endif
	unsigned char	*pnew;

	if (!oldptr) {
		*ptr = xt_mm_malloc(self, newsize, line, file);
		return *ptr ? TRUE : FALSE;
	}

#ifdef RECORD_MM
	xt_lock_mutex(self, &mm_mutex);
	if ((mm = mm_find_core_ptr(oldptr)) < 0) {
		xt_unlock_mutex(self, &mm_mutex);
		xt_throw_errno(XT_CONTEXT, XT_ENOMEM);
		return FAILED;
	}
	xt_unlock_mutex(self, &mm_mutex);
#endif

	oldptr = oldptr - MEM_DEBUG_HDR_SIZE;
	size = ((MemoryDebugPtr) oldptr)->size;

	ASSERT(((MemoryDebugPtr) oldptr)->check == MEM_HEADER);
	ASSERT(*((unsigned char *) oldptr + size + MEM_DEBUG_HDR_SIZE) == MEM_TRAILER_BYTE && 
			*((unsigned char *) oldptr + size + MEM_DEBUG_HDR_SIZE + 1L) == MEM_TRAILER_BYTE);

	/* Realloc allways moves! */
	pnew = (unsigned char *) xt_malloc(self, newsize + MEM_DEBUG_HDR_SIZE + MEM_TRAILER_SIZE);
	if (!pnew) {
		xt_throw_errno(XT_CONTEXT, XT_ENOMEM);
		return FAILED;
	}

	if (newsize > size) {
		memcpy(((MemoryDebugPtr) pnew)->data, ((MemoryDebugPtr) oldptr)->data, size);
		memset(((MemoryDebugPtr) pnew)->data + size, 0x55, newsize - size);
	}
	else
		memcpy(((MemoryDebugPtr) pnew)->data, ((MemoryDebugPtr) oldptr)->data, newsize);

	((MemoryDebugPtr) pnew)->check = MEM_HEADER;
	((MemoryDebugPtr) pnew)->size = newsize;
	*(pnew + newsize + MEM_DEBUG_HDR_SIZE) = MEM_TRAILER_BYTE;
	*(pnew + newsize + MEM_DEBUG_HDR_SIZE + 1L)	= MEM_TRAILER_BYTE;

#ifdef RECORD_MM
	xt_lock_mutex(self, &mm_mutex);
	if ((mm = mm_find_core_ptr(oldptr + MEM_DEBUG_HDR_SIZE)) < 0) {
		xt_unlock_mutex(self, &mm_mutex);
		xt_throw_errno(XT_CONTEXT, XT_ENOMEM);
		return FAILED;
	}
	mm_replace_core_ptr(mm, pnew + MEM_DEBUG_HDR_SIZE);
	xt_unlock_mutex(self, &mm_mutex);
#endif

	memset(oldptr, 0x55, size + MEM_DEBUG_HDR_SIZE + MEM_TRAILER_SIZE);
	xt_free(self, oldptr);

	*ptr = pnew + MEM_DEBUG_HDR_SIZE;
	return OK;
}

void xt_mm_free(XTThreadPtr self, void *ptr)
{
#ifdef RECORD_MM
	if (xt_lock_mutex(self, &mm_mutex)) {
		mm_remove_core_ptr(ptr);
		xt_unlock_mutex(self, &mm_mutex);
	}
#endif
	mm_checkmem(self, NULL, ptr, TRUE);
}

void xt_mm_pfree(XTThreadPtr self, void **ptr)
{
	if (*ptr) {
		void *p = *ptr;

		*ptr = NULL;
		xt_mm_free(self, p);
	}
}

size_t xt_mm_malloc_size(XTThreadPtr self, void *ptr)
{
	size_t size = 0;

#ifdef RECORD_MM
	if (xt_lock_mutex(self, &mm_mutex)) {
		mm_find_core_ptr(ptr);
		xt_unlock_mutex(self, &mm_mutex);
	}
#endif
	size = mm_checkmem(self, NULL, ptr, FALSE);
	return size;
}

void xt_mm_check_ptr(XTThreadPtr self, void *ptr)
{
	mm_checkmem(self, NULL, ptr, FALSE);
}
#endif

/*
 * -----------------------------------------------------------------------
 * INIT/EXIT MEMORY
 */

xtPublic xtBool xt_init_memory(void)
{
#ifdef DEBUG_MEMORY
	XTThreadPtr	self = NULL;

	if (!xt_init_mutex_with_autoname(NULL, &mm_mutex))
		return FALSE;

	mm_addresses = (MissingMemoryRec *) malloc(sizeof(MissingMemoryRec) * ADD_TOTAL_ALLOCS);
	if (!mm_addresses) {
		MM_THROW_ASSERTION("MM ERROR: Insuffient memory to allocate MM table");
		xt_free_mutex(&mm_mutex);
		return FALSE;
	}

	memset(mm_addresses, 0, sizeof(MissingMemoryRec) * ADD_TOTAL_ALLOCS);
	mm_total_allocated = ADD_TOTAL_ALLOCS;
	mm_nr_in_use = 0L;
	mm_alloc_count = 0L;
#endif
	return TRUE;
}

xtPublic void debug_ik_count(void *value);
xtPublic void debug_ik_sum(void);

xtPublic void xt_exit_memory(void)
{
#ifdef DEBUG_MEMORY
	long	mm;
	int		i;

	if (!mm_addresses)
		return;

	xt_lock_mutex(NULL, &mm_mutex);
	for (mm=0; mm<mm_nr_in_use; mm++) {
		MissingMemoryPtr mm_ptr = &mm_addresses[mm];

		xt_logf(XT_NS_CONTEXT, XT_LOG_FATAL, "MM: %p (#%ld) %s:%d Not freed\n",
			mm_ptr->mm_ptr,
			(long) mm_ptr->id,
			xt_last_name_of_path(mm_ptr->mm_file),
			(int) mm_ptr->line_nr);
		for (i=0; i<STACK_TRACE_DEPTH; i++) {
			if (mm_ptr->mm_func[i])
				xt_logf(XT_NS_CONTEXT, XT_LOG_FATAL, "MM: %s\n", mm_ptr->mm_func[i]);
		}
		/*
		 * Assumes we place out tracing function in the first
		 * position!!
		 */
		if (mm_ptr->trace_count)
			mm_debug_trace_count((XTMMTraceRefPtr) mm_ptr->mm_ptr);
	}
	mm_debug_trace_sum();
	free(mm_addresses);
	mm_addresses = NULL;
	mm_nr_in_use = 0L;
	mm_total_allocated = 0L;
	mm_alloc_count = 0L;
	xt_unlock_mutex(NULL, &mm_mutex);

	xt_free_mutex(&mm_mutex);
#endif
}

/*
 * -----------------------------------------------------------------------
 * MEMORY ALLOCATION UTILITIES
 */

#ifdef DEBUG_MEMORY
char	*xt_mm_dup_string(XTThreadPtr self, c_char *str, u_int line, c_char *file)
#else
char	*xt_dup_string(XTThreadPtr self, c_char *str)
#endif
{
	size_t	len;
	char	*new_str;

	if (!str)
		return NULL;
	len = strlen(str);
#ifdef DEBUG_MEMORY
	new_str = (char *) xt_mm_malloc(self, len + 1, line, file);
#else
	new_str = (char *) xt_malloc(self, len + 1);
#endif
	if (new_str)
		strcpy(new_str, str);
	return new_str;
}

xtPublic char *xt_long_to_str(XTThreadPtr self, long v)
{
	char str[50];

	sprintf(str, "%lu", v);
	return xt_dup_string(self, str);
}

char *xt_dup_nstr(XTThreadPtr self, c_char *str, int start, size_t len)
{
	char *new_str = (char *) xt_malloc(self, len + 1);
	
	if (new_str) {
		memcpy(new_str, str + start, len);
		new_str[len] = 0;
	}
	return new_str;
}

/*
 * -----------------------------------------------------------------------
 * LIGHT WEIGHT CHECK FUNCTIONS
 * Timing related memory management problems my not like the memset
 * or other heavy checking. Try this...
 */
 
#ifdef LIGHT_WEIGHT_CHECKS
xtPublic void *xt_malloc(XTThreadPtr self, size_t size)
{
	char *ptr;

	if (!(ptr = (char *) malloc(size+8))) {
		xt_throw_errno(XT_CONTEXT, XT_ENOMEM);
		return NULL;
	}
	*((xtWord4 *) ptr) = size;
	*((xtWord4 *) (ptr + size + 4)) = 0x7E7EFEFE;
	return ptr+4;
}

xtPublic void xt_check_ptr(void *ptr)
{
	char *old_ptr;
	xtWord4 size;

	old_ptr = (char *) ptr;
	old_ptr -= 4;
	size = *((xtWord4 *) old_ptr);
	if (size == 0xDEADBEAF || *((xtWord4 *) (old_ptr + size + 4)) != 0x7E7EFEFE) {
		char *dummy = NULL;
		
		xt_dump_trace();
		*dummy = 40;
	}
}

xtPublic xtBool	xt_realloc(XTThreadPtr self, void **ptr, size_t size)
{
	char *old_ptr;
	char *new_ptr;

	if ((old_ptr = (char *) *ptr)) {
		void check_for_file(char *my_ptr, xtWord4 len);

		xt_check_ptr(old_ptr);
		check_for_file((char *) old_ptr, *((xtWord4 *) (old_ptr - 4)));
		if (!(new_ptr = (char *) realloc(old_ptr - 4, size+8))) {
			xt_throw_errno(XT_CONTEXT, XT_ENOMEM);
			return FAILED;
		}
		*((xtWord4 *) new_ptr) = size;
		*((xtWord4 *) (new_ptr + size + 4)) = 0x7E7EFEFE;
		*ptr = new_ptr+4;
		return OK;
	}
	*ptr = xt_malloc(self, size);
	return *ptr != NULL;
}

xtPublic void xt_free(XTThreadPtr XT_UNUSED(self), void *ptr)
{
	char	*old_ptr;
	xtWord4 size;
	void	check_for_file(char *my_ptr, xtWord4 len);

	old_ptr = (char *) ptr;
	old_ptr -= 4;
	size = *((xtWord4 *) old_ptr);
	if (size == 0xDEADBEAF || *((xtWord4 *) (old_ptr + size + 4)) != 0x7E7EFEFE) {
		char *dummy = NULL;
		
		xt_dump_trace();
		*dummy = 41;
	}
	check_for_file((char *) ptr, size);
	*((xtWord4 *) old_ptr) = 0xDEADBEAF;
	*((xtWord4 *) (old_ptr + size)) = 0xEFEFDFDF;
	*((xtWord4 *) (old_ptr + size + 4)) = 0x1F1F1F1F;
	//memset(old_ptr, 0xEF, size+4);
	free(old_ptr);
}

xtPublic void *xt_calloc(XTThreadPtr self, size_t size)
{
	void *ptr;

	if ((ptr = xt_malloc(self, size)))
		memset(ptr, 0, size);
	return ptr;
}

#undef	xt_pfree

xtPublic void xt_pfree(XTThreadPtr self, void **ptr)
{
	if (*ptr) {
		void *p = *ptr;

		*ptr = NULL;
		xt_free(self, p);
	}
}

xtPublic void *xt_malloc_ns(size_t size)
{
	char *ptr;

	if (!(ptr = (char *) malloc(size+8))) {
		xt_register_errno(XT_REG_CONTEXT, XT_ENOMEM);
		return NULL;
	}
	*((xtWord4 *) ptr) = size;
	*((xtWord4 *) (ptr + size + 4)) = 0x7E7EFEFE;
	return ptr+4;
}

xtPublic void *xt_calloc_ns(size_t size)
{
	char *ptr;

	if (!(ptr = (char *) malloc(size+8))) {
		xt_register_errno(XT_REG_CONTEXT, XT_ENOMEM);
		return NULL;
	}
	*((xtWord4 *) ptr) = size;
	*((xtWord4 *) (ptr + size + 4)) = 0x7E7EFEFE;
	memset(ptr+4, 0, size);
	return ptr+4;
}

xtPublic xtBool	xt_realloc_ns(void **ptr, size_t size)
{
	char *old_ptr;
	char *new_ptr;

	if ((old_ptr = (char *) *ptr)) {
		void check_for_file(char *my_ptr, xtWord4 len);
		
		xt_check_ptr(old_ptr);
		check_for_file((char *) old_ptr, *((xtWord4 *) (old_ptr - 4)));
		if (!(new_ptr = (char *) realloc(old_ptr - 4, size+8)))
			return xt_register_errno(XT_REG_CONTEXT, XT_ENOMEM);
		*((xtWord4 *) new_ptr) = size;
		*((xtWord4 *) (new_ptr + size + 4)) = 0x7E7EFEFE;
		*ptr = new_ptr+4;
		return OK;
	}
	*ptr = xt_malloc_ns(size);
	return *ptr != NULL;
}

xtPublic void xt_free_ns(void *ptr)
{
	char	*old_ptr;
	xtWord4	size;
	void	check_for_file(char *my_ptr, xtWord4 len);

	old_ptr = (char *) ptr;
	old_ptr -= 4;
	size = *((xtWord4 *) old_ptr);
	if (size == 0xDEADBEAF || *((xtWord4 *) (old_ptr + size + 4)) != 0x7E7EFEFE) {
		char *dummy = NULL;
		
		xt_dump_trace();
		*dummy = 42;
	}
	check_for_file((char *) ptr, size);
	*((xtWord4 *) old_ptr) = 0xDEADBEAF;
	*((xtWord4 *) (old_ptr + size)) = 0xEFEFDFDF;
	*((xtWord4 *) (old_ptr + size + 4)) = 0x1F1F1F1F;
	//memset(old_ptr, 0xEE, size+4);
	free(old_ptr);
}
#endif

