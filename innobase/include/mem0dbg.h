/******************************************************
The memory management: the debug code. This is not a compilation module,
but is included in mem0mem.* !

(c) 1994, 1995 Innobase Oy

Created 6/9/1994 Heikki Tuuri
*******************************************************/

/* In the debug version each allocated field is surrounded with
check fields whose sizes are given below */

#define MEM_FIELD_HEADER_SIZE   ut_calc_align(2 * sizeof(ulint),\
						UNIV_MEM_ALIGNMENT)
#define MEM_FIELD_TRAILER_SIZE  sizeof(ulint)

#define MEM_BLOCK_MAGIC_N	764741

/* Space needed when allocating for a user a field of
length N. The space is allocated only in multiples of
UNIV_MEM_ALIGNMENT. In the debug version there are also
check fields at the both ends of the field. */
#ifdef UNIV_MEM_DEBUG
#define MEM_SPACE_NEEDED(N) ut_calc_align((N) + MEM_FIELD_HEADER_SIZE\
			       	              + MEM_FIELD_TRAILER_SIZE,\
				          UNIV_MEM_ALIGNMENT)
#else
#define MEM_SPACE_NEEDED(N) ut_calc_align((N), UNIV_MEM_ALIGNMENT)
#endif

/*******************************************************************
Checks a memory heap for consistency and prints the contents if requested.
Outputs the sum of sizes of buffers given to the user (only in
the debug version), the physical size of the heap and the number of
blocks in the heap. In case of error returns 0 as sizes and number
of blocks. */

void
mem_heap_validate_or_print(
/*=======================*/
	mem_heap_t*   	heap, 	/* in: memory heap */
	byte*		top,	/* in: calculate and validate only until
				this top pointer in the heap is reached,
				if this pointer is NULL, ignored */
	ibool            print,  /* in: if TRUE, prints the contents
				of the heap; works only in
				the debug version */
	ibool*           error,  /* out: TRUE if error */
	ulint*          us_size,/* out: allocated memory 
				(for the user) in the heap,
				if a NULL pointer is passed as this
				argument, it is ignored; in the
				non-debug version this is always -1 */
	ulint*          ph_size,/* out: physical size of the heap,
				if a NULL pointer is passed as this
				argument, it is ignored */
	ulint*          n_blocks); /* out: number of blocks in the heap,
				if a NULL pointer is passed as this
				argument, it is ignored */
/******************************************************************
Prints the contents of a memory heap. */

void
mem_heap_print(
/*===========*/
	mem_heap_t*   heap);	/* in: memory heap */
/******************************************************************
Checks that an object is a memory heap (or a block of it) */

ibool
mem_heap_check(
/*===========*/
				/* out: TRUE if ok */
	mem_heap_t*   heap);	/* in: memory heap */
/******************************************************************
Validates the contents of a memory heap. */

ibool
mem_heap_validate(
/*==============*/
				/* out: TRUE if ok */
	mem_heap_t*   heap);	/* in: memory heap */
/*********************************************************************
Prints information of dynamic memory usage and currently live
memory heaps or buffers. Can only be used in the debug version. */

void
mem_print_info(void);
/*=================*/
/*********************************************************************
Prints information of dynamic memory usage and currently allocated memory
heaps or buffers since the last ..._print_info or..._print_new_info. */

void
mem_print_new_info(void);
/*====================*/
/*********************************************************************
TRUE if no memory is currently allocated. */

ibool
mem_all_freed(void);
/*===============*/
			/* out: TRUE if no heaps exist */
/*********************************************************************
Validates the dynamic memory */

ibool
mem_validate_no_assert(void);
/*=========================*/
			/* out: TRUE if error */
/****************************************************************
Validates the dynamic memory */

ibool
mem_validate(void);
/*===============*/
			/* out: TRUE if ok */
