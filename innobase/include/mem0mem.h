/******************************************************
The memory management

(c) 1994, 1995 Innobase Oy

Created 6/9/1994 Heikki Tuuri
*******************************************************/

#ifndef mem0mem_h
#define mem0mem_h

#include "univ.i"
#include "ut0mem.h"
#include "ut0byte.h"
#include "ut0ut.h"
#include "ut0rnd.h"
#include "sync0sync.h"
#include "ut0lst.h"
#include "mach0data.h"

/* -------------------- MEMORY HEAPS ----------------------------- */

/* The info structure stored at the beginning of a heap block */
typedef struct mem_block_info_struct mem_block_info_t;

/* A block of a memory heap consists of the info structure
followed by an area of memory */
typedef mem_block_info_t	mem_block_t;

/* A memory heap is a nonempty linear list of memory blocks */
typedef mem_block_t	mem_heap_t;

/* Types of allocation for memory heaps: DYNAMIC means allocation from the
dynamic memory pool of the C compiler, BUFFER means allocation from the index
page buffer pool; the latter method is used for very big heaps */

#define MEM_HEAP_DYNAMIC	0	/* the most common type */
#define MEM_HEAP_BUFFER		1
#define MEM_HEAP_BTR_SEARCH	2	/* this flag can be ORed to the
					previous */

/* The following start size is used for the first block in the memory heap if
the size is not specified, i.e., 0 is given as the parameter in the call of
create. The standard size is the maximum (payload) size of the blocks used for
allocations of small buffers. */

#define MEM_BLOCK_START_SIZE            64
#define MEM_BLOCK_STANDARD_SIZE         8000

/* If a memory heap is allowed to grow into the buffer pool, the following
is the maximum size for a single allocated buffer: */
#define MEM_MAX_ALLOC_IN_BUF		(UNIV_PAGE_SIZE - 200)

/**********************************************************************
Initializes the memory system. */

void
mem_init(
/*=====*/
	ulint	size);	/* in: common pool size in bytes */
/******************************************************************
Use this macro instead of the corresponding function! Macro for memory
heap creation. */

#define mem_heap_create(N)    mem_heap_create_func(\
						(N), NULL, MEM_HEAP_DYNAMIC,\
						__FILE__, __LINE__)
/******************************************************************
Use this macro instead of the corresponding function! Macro for memory
heap creation. */

#define mem_heap_create_in_buffer(N)	mem_heap_create_func(\
						(N), NULL, MEM_HEAP_BUFFER,\
						__FILE__, __LINE__)
/******************************************************************
Use this macro instead of the corresponding function! Macro for memory
heap creation. */

#define mem_heap_create_in_btr_search(N) mem_heap_create_func(\
					(N), NULL, MEM_HEAP_BTR_SEARCH |\
						MEM_HEAP_BUFFER,\
						__FILE__, __LINE__)
/******************************************************************
Use this macro instead of the corresponding function! Macro for fast
memory heap creation. An initial block of memory B is given by the
caller, N is its size, and this memory block is not freed by
mem_heap_free. See the parameter comment in mem_heap_create_func below. */

#define mem_heap_fast_create(N, B)	mem_heap_create_func(\
						(N), (B), MEM_HEAP_DYNAMIC,\
						__FILE__, __LINE__)

/******************************************************************
Use this macro instead of the corresponding function! Macro for memory
heap freeing. */

#define mem_heap_free(heap) mem_heap_free_func(\
					  (heap), __FILE__, __LINE__)
/*********************************************************************
NOTE: Use the corresponding macros instead of this function. Creates a
memory heap which allocates memory from dynamic space. For debugging
purposes, takes also the file name and line as argument. */
UNIV_INLINE
mem_heap_t*
mem_heap_create_func(
/*=================*/
					/* out, own: memory heap */
	ulint		n,		/* in: desired start block size,
					this means that a single user buffer
					of size n will fit in the block, 
					0 creates a default size block;
					if init_block is not NULL, n tells
					its size in bytes */
	void*		init_block,	/* in: if very fast creation is
					wanted, the caller can reserve some
					memory from its stack, for example,
					and pass it as the the initial block
					to the heap: then no OS call of malloc
					is needed at the creation. CAUTION:
					the caller must make sure the initial
					block is not unintentionally erased
					(if allocated in the stack), before
					the memory heap is explicitly freed. */
	ulint		type,		/* in: MEM_HEAP_DYNAMIC
					or MEM_HEAP_BUFFER */ 
	const char*	file_name,	/* in: file name where created */
	ulint		line		/* in: line where created */
	);
/*********************************************************************
NOTE: Use the corresponding macro instead of this function. Frees the space
occupied by a memory heap. In the debug version erases the heap memory
blocks. */
UNIV_INLINE
void
mem_heap_free_func(
/*===============*/
	mem_heap_t*   	heap,  		/* in, own: heap to be freed */
	const char*	file_name, 	/* in: file name where freed */
	ulint    	line);		/* in: line where freed */
/*******************************************************************
Allocates n bytes of memory from a memory heap. */
UNIV_INLINE
void*
mem_heap_alloc(
/*===========*/
				/* out: allocated storage, NULL if
				did not succeed */
	mem_heap_t*   	heap, 	/* in: memory heap */
	ulint           n);	/* in: number of bytes; if the heap is allowed
				to grow into the buffer pool, this must be
				<= MEM_MAX_ALLOC_IN_BUF */
/*********************************************************************
Returns a pointer to the heap top. */
UNIV_INLINE
byte*
mem_heap_get_heap_top(
/*==================*/     
				/* out: pointer to the heap top */
	mem_heap_t*   	heap); 	/* in: memory heap */
/*********************************************************************
Frees the space in a memory heap exceeding the pointer given. The
pointer must have been acquired from mem_heap_get_heap_top. The first
memory block of the heap is not freed. */
UNIV_INLINE
void
mem_heap_free_heap_top(
/*===================*/
	mem_heap_t*   	heap,	/* in: heap from which to free */
	byte*		old_top);/* in: pointer to old top of heap */
/*********************************************************************
Empties a memory heap. The first memory block of the heap is not freed. */
UNIV_INLINE
void
mem_heap_empty(
/*===========*/
	mem_heap_t*   	heap);	/* in: heap to empty */
/*********************************************************************
Returns a pointer to the topmost element in a memory heap.
The size of the element must be given. */
UNIV_INLINE
void*
mem_heap_get_top(
/*=============*/     
				/* out: pointer to the topmost element */
	mem_heap_t*   	heap, 	/* in: memory heap */
	ulint           n);     /* in: size of the topmost element */
/*********************************************************************
Frees the topmost element in a memory heap.
The size of the element must be given. */
UNIV_INLINE
void
mem_heap_free_top(
/*==============*/     
	mem_heap_t*   	heap, 	/* in: memory heap */
	ulint           n);     /* in: size of the topmost element */
/*********************************************************************
Returns the space in bytes occupied by a memory heap. */
UNIV_INLINE
ulint
mem_heap_get_size(
/*==============*/
	mem_heap_t*	heap);  	/* in: heap */
/******************************************************************
Use this macro instead of the corresponding function!
Macro for memory buffer allocation */

#define mem_alloc(N)    mem_alloc_func((N), __FILE__, __LINE__)
/******************************************************************
Use this macro instead of the corresponding function!
Macro for memory buffer allocation */

#define mem_alloc_noninline(N)    mem_alloc_func_noninline(\
					  (N), __FILE__, __LINE__)
/*******************************************************************
NOTE: Use the corresponding macro instead of this function.
Allocates a single buffer of memory from the dynamic memory of
the C compiler. Is like malloc of C. The buffer must be freed 
with mem_free. */
UNIV_INLINE
void*
mem_alloc_func(
/*===========*/
					/* out, own: free storage, NULL
					if did not succeed */
	ulint		n,		/* in: desired number of bytes */
	const char*	file_name,	/* in: file name where created */
	ulint		line		/* in: line where created */
);
/*******************************************************************
NOTE: Use the corresponding macro instead of this function.
Allocates a single buffer of memory from the dynamic memory of
the C compiler. Is like malloc of C. The buffer must be freed 
with mem_free. */

void*
mem_alloc_func_noninline(
/*=====================*/
					/* out, own: free storage,
					NULL if did not succeed */
	ulint		n,		/* in: desired number of bytes */
	const char*	file_name,	/* in: file name where created */
	ulint		line		/* in: line where created */
	);
/******************************************************************
Use this macro instead of the corresponding function!
Macro for memory buffer freeing */

#define mem_free(PTR)   mem_free_func((PTR), __FILE__, __LINE__)
/*******************************************************************
NOTE: Use the corresponding macro instead of this function.
Frees a single buffer of storage from
the dynamic memory of C compiler. Similar to free of C. */
UNIV_INLINE
void
mem_free_func(
/*==========*/
	void*		ptr,		/* in, own: buffer to be freed */
	const char*	file_name,	/* in: file name where created */
	ulint		line		/* in: line where created */
);

/**************************************************************************
Duplicates a NUL-terminated string. */
UNIV_INLINE
char*
mem_strdup(
/*=======*/
				/* out, own: a copy of the string,
				must be deallocated with mem_free */
	const char*	str);	/* in: string to be copied */
/**************************************************************************
Makes a NUL-terminated copy of a nonterminated string. */
UNIV_INLINE
char*
mem_strdupl(
/*========*/
				/* out, own: a copy of the string,
				must be deallocated with mem_free */
	const char*	str,	/* in: string to be copied */
	ulint		len);	/* in: length of str, in bytes */

/**************************************************************************
Makes a NUL-terminated quoted copy of a NUL-terminated string. */
UNIV_INLINE
char*
mem_strdupq(
/*========*/
				/* out, own: a quoted copy of the string,
				must be deallocated with mem_free */
	const char*	str,	/* in: string to be copied */
	char		q);	/* in: quote character */

/**************************************************************************
Duplicates a NUL-terminated string, allocated from a memory heap. */

char*
mem_heap_strdup(
/*============*/
				/* out, own: a copy of the string */
	mem_heap_t* heap,	/* in: memory heap where string is allocated */
	const char* str);	/* in: string to be copied */
/**************************************************************************
Makes a NUL-terminated copy of a nonterminated string,
allocated from a memory heap. */
UNIV_INLINE
char*
mem_heap_strdupl(
/*=============*/
				/* out, own: a copy of the string */
	mem_heap_t*	heap,	/* in: memory heap where string is allocated */
	const char*	str,	/* in: string to be copied */
	ulint		len);	/* in: length of str, in bytes */

#ifdef MEM_PERIODIC_CHECK
/**********************************************************************
Goes through the list of all allocated mem blocks, checks their magic
numbers, and reports possible corruption. */

void
mem_validate_all_blocks(void);
/*=========================*/
#endif

/*#######################################################################*/
	
/* The info header of a block in a memory heap */

struct mem_block_info_struct {
	ulint   magic_n;/* magic number for debugging */
	char	file_name[8];/* file name where the mem heap was created */
	ulint	line;	/* line number where the mem heap was created */
	UT_LIST_BASE_NODE_T(mem_block_t) base; /* In the first block in the
			the list this is the base node of the list of blocks;
			in subsequent blocks this is undefined */
	UT_LIST_NODE_T(mem_block_t) list; /* This contains pointers to next
			and prev in the list. The first block allocated
			to the heap is also the first block in this list,
			though it also contains the base node of the list. */
	ulint   len;    /* physical length of this block in bytes */
	ulint 	type; 	/* type of heap: MEM_HEAP_DYNAMIC, or
			MEM_HEAP_BUF possibly ORed to MEM_HEAP_BTR_SEARCH */
	ibool	init_block; /* TRUE if this is the first block used in fast
			creation of a heap: the memory will be freed
			by the creator, not by mem_heap_free */
	ulint   free;   /* offset in bytes of the first free position for
			user data in the block */
	ulint   start;  /* the value of the struct field 'free' at the 
			creation of the block */
	byte* 	free_block;
			/* if the MEM_HEAP_BTR_SEARCH bit is set in type,
			and this is the heap root, this can contain an
			allocated buffer frame, which can be appended as a
			free block to the heap, if we need more space;
			otherwise, this is NULL */
#ifdef MEM_PERIODIC_CHECK	
	UT_LIST_NODE_T(mem_block_t) mem_block_list;
			/* List of all mem blocks allocated; protected
			by the mem_comm_pool mutex */
#endif
};

#define MEM_BLOCK_MAGIC_N	764741555
#define MEM_FREED_BLOCK_MAGIC_N	547711122

/* Header size for a memory heap block */
#define MEM_BLOCK_HEADER_SIZE   ut_calc_align(sizeof(mem_block_info_t),\
							UNIV_MEM_ALIGNMENT)
#include "mem0dbg.h"

#ifndef UNIV_NONINL
#include "mem0mem.ic"
#endif

#endif 
