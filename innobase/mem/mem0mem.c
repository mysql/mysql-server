/************************************************************************
The memory management

(c) 1994, 1995 Innobase Oy

Created 6/9/1994 Heikki Tuuri
*************************************************************************/


#include "mem0mem.h"
#ifdef UNIV_NONINL
#include "mem0mem.ic"
#endif

#include "mach0data.h"
#include "buf0buf.h"
#include "mem0dbg.c"
#include "btr0sea.h"

/*
			THE MEMORY MANAGEMENT
			=====================

The basic element of the memory management is called a memory
heap. A memory heap is conceptually a
stack from which memory can be allocated. The stack may grow infinitely.
The top element of the stack may be freed, or
the whole stack can be freed at one time. The advantage of the
memory heap concept is that we can avoid using the malloc and free
functions of C which are quite expensive, for example, on the Solaris + GCC
system (50 MHz Sparc, 1993) the pair takes 3 microseconds,
on Win NT + 100MHz Pentium, 2.5 microseconds.
When we use a memory heap,
we can allocate larger blocks of memory at a time and thus
reduce overhead. Slightly more efficient the method is when we
allocate the memory from the index page buffer pool, as we can
claim a new page fast. This is called buffer allocation. 
When we allocate the memory from the dynamic memory of the
C environment, that is called dynamic allocation.

The default way of operation of the memory heap is the following.
First, when the heap is created, an initial block of memory is
allocated. In dynamic allocation this may be about 50 bytes.
If more space is needed, additional blocks are allocated
and they are put into a linked list.
After the initial block, each allocated block is twice the size of the 
previous, until a threshold is attained, after which the sizes
of the blocks stay the same. An exception is, of course, the case
where the caller requests a memory buffer whose size is
bigger than the threshold. In that case a block big enough must
be allocated.

The heap is physically arranged so that if the current block
becomes full, a new block is allocated and always inserted in the
chain of blocks as the last block.

In the debug version of the memory management, all the allocated
heaps are kept in a list (which is implemented as a hash table).
Thus we can notice if the caller tries to free an already freed
heap. In addition, each buffer given to the caller contains
start field at the start and a trailer field at the end of the buffer.

The start field has the following content:
A. sizeof(ulint) bytes of field length (in the standard byte order)
B. sizeof(ulint) bytes of check field (a random number)

The trailer field contains:
A. sizeof(ulint) bytes of check field (the same random number as at the start)

Thus we can notice if something has been copied over the
borders of the buffer, which is illegal.
The memory in the buffers is initialized to a random byte sequence.
After freeing, all the blocks in the heap are set to random bytes
to help us discover errors which result from the use of
buffers in an already freed heap. */

/*******************************************************************
NOTE: Use the corresponding macro instead of this function.
Allocates a single buffer of memory from the dynamic memory of
the C compiler. Is like malloc of C. The buffer must be freed 
with mem_free. */

void*
mem_alloc_func_noninline(
/*=====================*/
				/* out, own: free storage, NULL if did not
				succeed */
	ulint   n              	/* in: desired number of bytes */
	#ifdef UNIV_MEM_DEBUG
	,char*  file_name,	/* in: file name where created */
	ulint   line		/* in: line where created */
	#endif
	)
{
	return(mem_alloc_func(n
#ifdef UNIV_MEM_DEBUG
				, file_name, line
#endif
	));	
}

/*******************************************************************
Creates a memory heap block where data can be allocated. */

mem_block_t*
mem_heap_create_block(
/*==================*/
			/* out, own: memory heap block, NULL if did not
			succeed */
	mem_heap_t* heap,/* in: memory heap or NULL if first block should
			be created */
	ulint	n,	/* in: number of bytes needed for user data, or
			if init_block is not NULL, its size in bytes */
	void*	init_block, /* in: init block in fast create, type must be
			MEM_HEAP_DYNAMIC */
	ulint 	type)	/* in: type of heap: MEM_HEAP_DYNAMIC, or
			MEM_HEAP_BUFFER possibly ORed to MEM_HEAP_BTR_SEARCH */
{
	mem_block_t*	block;
	ulint		len;
	
	ut_ad((type == MEM_HEAP_DYNAMIC) || (type == MEM_HEAP_BUFFER)
		|| (type == MEM_HEAP_BUFFER + MEM_HEAP_BTR_SEARCH));

	/* In dynamic allocation, calculate the size: block header + data. */

	if (init_block != NULL) {
		ut_ad(type == MEM_HEAP_DYNAMIC);
		ut_ad(n > MEM_BLOCK_START_SIZE + MEM_BLOCK_HEADER_SIZE);
		len = n;
		block = init_block;

	} else if (type == MEM_HEAP_DYNAMIC) {

		len = MEM_BLOCK_HEADER_SIZE + MEM_SPACE_NEEDED(n);
		block = mem_area_alloc(len, mem_comm_pool);
	} else {
		ut_ad(n <= MEM_MAX_ALLOC_IN_BUF);

		len = MEM_BLOCK_HEADER_SIZE + MEM_SPACE_NEEDED(n);

		if (len < UNIV_PAGE_SIZE / 2) {

			block = mem_area_alloc(len, mem_comm_pool);
		} else {
			len = UNIV_PAGE_SIZE;

			if ((type & MEM_HEAP_BTR_SEARCH) && heap) {
				/* We cannot allocate the block from the
				buffer pool, but must get the free block from
				the heap header free block field */

				block = (mem_block_t*)heap->free_block;
				heap->free_block = NULL;
			} else {
				block = (mem_block_t*)buf_frame_alloc();
			}
		}
	}

	if (block == NULL) {

		return(NULL);
	}

	block->magic_n = MEM_BLOCK_MAGIC_N;

	mem_block_set_len(block, len);
	mem_block_set_type(block, type);
	mem_block_set_free(block, MEM_BLOCK_HEADER_SIZE);
	mem_block_set_start(block, MEM_BLOCK_HEADER_SIZE);

	block->free_block = NULL;

	if (init_block != NULL) {
		block->init_block = TRUE;
	} else {
		block->init_block = FALSE;
	}

	ut_ad((ulint)MEM_BLOCK_HEADER_SIZE < len);

	return(block);
}

/*******************************************************************
Adds a new block to a memory heap. */

mem_block_t*
mem_heap_add_block(
/*===============*/
				/* out: created block, NULL if did not
				succeed */
	mem_heap_t* 	heap,	/* in: memory heap */
	ulint		n)	/* in: number of bytes user needs */
{
	mem_block_t*	block; 
	mem_block_t*	new_block; 
	ulint		new_size;

	ut_ad(mem_heap_check(heap));

	block = UT_LIST_GET_LAST(heap->base);

	/* We have to allocate a new block. The size is always at least
	doubled until the standard size is reached. After that the size
	stays the same, except in cases where the caller needs more space. */
		
	new_size = 2 * mem_block_get_len(block);

	if (heap->type != MEM_HEAP_DYNAMIC) {
		ut_ad(n <= MEM_MAX_ALLOC_IN_BUF);

		if (new_size > MEM_MAX_ALLOC_IN_BUF) {
			new_size = MEM_MAX_ALLOC_IN_BUF;
		}
	} else if (new_size > MEM_BLOCK_STANDARD_SIZE) {

		new_size = MEM_BLOCK_STANDARD_SIZE;
	}

	if (new_size < n) {
		new_size = n;
	}
	
	new_block = mem_heap_create_block(heap, new_size, NULL, heap->type);

	if (new_block == NULL) {

		return(NULL);
	}

	/* Add the new block as the last block */

	UT_LIST_INSERT_AFTER(list, heap->base, block, new_block);

	return(new_block);
}

/**********************************************************************
Frees a block from a memory heap. */

void
mem_heap_block_free(
/*================*/
	mem_heap_t*	heap,	/* in: heap */
	mem_block_t*	block)	/* in: block to free */
{
	ulint	type;
	ulint	len;
	ibool	init_block;	

	UT_LIST_REMOVE(list, heap->base, block);
		
	type = heap->type;
	len = block->len;
	init_block = block->init_block;

	#ifdef UNIV_MEM_DEBUG
	/* In the debug version we set the memory to a random combination
	of hex 0xDE and 0xAD. */

	mem_erase_buf((byte*)block, len);

	#endif

	if (init_block) {
		/* Do not have to free: do nothing */

	} else if (type == MEM_HEAP_DYNAMIC) {

		mem_area_free(block, mem_comm_pool);
	} else {
		ut_ad(type & MEM_HEAP_BUFFER);

		if (len >= UNIV_PAGE_SIZE / 2) {
			buf_frame_free((byte*)block);
		} else {
			mem_area_free(block, mem_comm_pool);
		}
	}
}

/**********************************************************************
Frees the free_block field from a memory heap. */

void
mem_heap_free_block_free(
/*=====================*/
	mem_heap_t*	heap)	/* in: heap */
{
	if (heap->free_block) {

		buf_frame_free(heap->free_block);

		heap->free_block = NULL;
	}
}
