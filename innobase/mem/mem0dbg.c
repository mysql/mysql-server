/************************************************************************
The memory management: the debug code. This is not a compilation module,
but is included in mem0mem.* !

(c) 1994, 1995 Innobase Oy

Created 6/9/1994 Heikki Tuuri
*************************************************************************/

#ifdef UNIV_MEM_DEBUG
mutex_t	mem_hash_mutex;	 /* The mutex which protects in the
			debug version the hash table containing
			the list of live memory heaps, and
			also the global variables below. */

/* The following variables contain information about the
extent of memory allocations. Only used in the debug version.
Protected by mem_hash_mutex above. */

static ulint	mem_n_created_heaps 		= 0;
static ulint	mem_n_allocations	  	= 0;
static ulint	mem_total_allocated_memory	= 0;
ulint		mem_current_allocated_memory	= 0;
static ulint	mem_max_allocated_memory	= 0;
static ulint	mem_last_print_info		= 0;

/* Size of the hash table for memory management tracking */
#define	MEM_HASH_SIZE	997

/* The node of the list containing currently allocated memory heaps */

typedef struct mem_hash_node_struct mem_hash_node_t;
struct mem_hash_node_struct {
	UT_LIST_NODE_T(mem_hash_node_t)
				list;	/* hash list node */
	mem_heap_t*		heap;	/* memory heap */
	const char*		file_name;/* file where heap was created*/
	ulint			line;	/* file line of creation */
	ulint			nth_heap;/* this is the nth heap created */
	UT_LIST_NODE_T(mem_hash_node_t)
				all_list;/* list of all created heaps */
};

typedef UT_LIST_BASE_NODE_T(mem_hash_node_t) mem_hash_cell_t;

/* The hash table of allocated heaps */
static mem_hash_cell_t		mem_hash_table[MEM_HASH_SIZE];

/* The base node of the list of all allocated heaps */
static mem_hash_cell_t		mem_all_list_base;

static ibool	mem_hash_initialized	= FALSE;


UNIV_INLINE
mem_hash_cell_t*
mem_hash_get_nth_cell(ulint i);

/* Accessor function for the hash table. Returns a pointer to the
table cell. */
UNIV_INLINE
mem_hash_cell_t*
mem_hash_get_nth_cell(ulint i)
{
	ut_a(i < MEM_HASH_SIZE);

	return(&(mem_hash_table[i]));
}
#endif /* UNIV_MEM_DEBUG */

/* Accessor functions for a memory field in the debug version */

void
mem_field_header_set_len(byte* field, ulint len)
{
	mach_write_to_4(field - 2 * sizeof(ulint), len);
}

ulint
mem_field_header_get_len(byte* field)
{
	return(mach_read_from_4(field - 2 * sizeof(ulint)));
}

void
mem_field_header_set_check(byte* field, ulint check)
{
	mach_write_to_4(field - sizeof(ulint), check);
}

ulint
mem_field_header_get_check(byte* field)
{
	return(mach_read_from_4(field - sizeof(ulint)));
}

void
mem_field_trailer_set_check(byte* field, ulint check)
{
	mach_write_to_4(field + mem_field_header_get_len(field), check);
}

ulint
mem_field_trailer_get_check(byte* field)
{
	return(mach_read_from_4(field +
			mem_field_header_get_len(field)));
}

/**********************************************************************
Initializes the memory system. */

void
mem_init(
/*=====*/
	ulint	size)	/* in: common pool size in bytes */
{
#ifdef UNIV_MEM_DEBUG

	ulint	i;

	/* Initialize the hash table */
	ut_a(FALSE == mem_hash_initialized);

	mutex_create(&mem_hash_mutex);
	mutex_set_level(&mem_hash_mutex, SYNC_MEM_HASH);

	for (i = 0; i < MEM_HASH_SIZE; i++) {
		UT_LIST_INIT(*mem_hash_get_nth_cell(i));
	}

	UT_LIST_INIT(mem_all_list_base);
	
	mem_hash_initialized = TRUE;
#endif

	mem_comm_pool = mem_pool_create(size);
}

/**********************************************************************
Initializes an allocated memory field in the debug version. */

void
mem_field_init(
/*===========*/
	byte*	buf,	/* in: memory field */
	ulint	n)	/* in: how many bytes the user requested */
{
	ulint	rnd;
	byte*	usr_buf;

	usr_buf = buf + MEM_FIELD_HEADER_SIZE;
	
	/* In the debug version write the length field and the 
	check fields to the start and the end of the allocated storage.
	The field header consists of a length field and
	a random number field, in this order. The field trailer contains
	the same random number as a check field. */

	mem_field_header_set_len(usr_buf, n);
	
	rnd = ut_rnd_gen_ulint();
	
	mem_field_header_set_check(usr_buf, rnd);
	mem_field_trailer_set_check(usr_buf, rnd);

#ifdef UNIV_MEM_DEBUG
	/* Update the memory allocation information */

	mutex_enter(&mem_hash_mutex);

	mem_total_allocated_memory += n;
	mem_current_allocated_memory += n;
	mem_n_allocations++;

	if (mem_current_allocated_memory > mem_max_allocated_memory) {
		mem_max_allocated_memory = mem_current_allocated_memory;
	}

	mutex_exit(&mem_hash_mutex);

	/* In the debug version set the buffer to a random
	combination of 0xBA and 0xBE */

	mem_init_buf(usr_buf, n);
#endif /* UNIV_MEM_DEBUG */
}

/**********************************************************************
Erases an allocated memory field in the debug version. */

void
mem_field_erase(
/*============*/
	byte*	buf,	/* in: memory field */
	ulint	n __attribute__((unused)))
			/* in: how many bytes the user requested */
{
	byte*	usr_buf;

	usr_buf = buf + MEM_FIELD_HEADER_SIZE;

#ifdef UNIV_MEM_DEBUG
	mutex_enter(&mem_hash_mutex);
	mem_current_allocated_memory    -= n;
	mutex_exit(&mem_hash_mutex);

	/* Check that the field lengths agree */
	ut_ad(n == (ulint)mem_field_header_get_len(usr_buf));

	/* In the debug version, set the freed space to a random
	combination of 0xDE and 0xAD */

	mem_erase_buf(buf, MEM_SPACE_NEEDED(n));
#endif /* UNIV_MEM_DEBUG */
}

#ifdef UNIV_MEM_DEBUG
/*******************************************************************
Initializes a buffer to a random combination of hex BA and BE.
Used to initialize allocated memory. */

void
mem_init_buf(
/*=========*/
	byte*   buf,    /* in: pointer to buffer */
	ulint    n)     /* in: length of buffer */
{
	byte*   ptr;

	for (ptr = buf; ptr < buf + n; ptr++) {

		if (ut_rnd_gen_ibool()) {
			*ptr = 0xBA;
		} else {
			*ptr = 0xBE;
		}
	}
}

/*******************************************************************
Initializes a buffer to a random combination of hex DE and AD.
Used to erase freed memory.*/

void
mem_erase_buf(
/*==========*/
	byte*   buf,    /* in: pointer to buffer */
	ulint    n)      /* in: length of buffer */
{
	byte*   ptr;

	for (ptr = buf; ptr < buf + n; ptr++) {
		if (ut_rnd_gen_ibool()) {
			*ptr = 0xDE;
		} else {
			*ptr = 0xAD;
		}
	}
}

/*******************************************************************
Inserts a created memory heap to the hash table of current allocated
memory heaps. */

void
mem_hash_insert(
/*============*/
	mem_heap_t*	heap,	   /* in: the created heap */
	const char*	file_name, /* in: file name of creation */
	ulint		line)	   /* in: line where created */
{
	mem_hash_node_t*	new_node;
	ulint			cell_no	;

	ut_ad(mem_heap_check(heap));

	mutex_enter(&mem_hash_mutex);
	
	cell_no = ut_hash_ulint((ulint)heap, MEM_HASH_SIZE);

	/* Allocate a new node to the list */
	new_node = ut_malloc(sizeof(mem_hash_node_t));

	new_node->heap = heap;
	new_node->file_name = file_name;
	new_node->line = line;
	new_node->nth_heap = mem_n_created_heaps;

	/* Insert into lists */
	UT_LIST_ADD_FIRST(list, *mem_hash_get_nth_cell(cell_no), new_node);

	UT_LIST_ADD_LAST(all_list, mem_all_list_base, new_node);

	mem_n_created_heaps++;	

	mutex_exit(&mem_hash_mutex);
}

/*******************************************************************
Removes a memory heap (which is going to be freed by the caller)
from the list of live memory heaps. Returns the size of the heap
in terms of how much memory in bytes was allocated for the user of
the heap (not the total space occupied by the heap).
Also validates the heap.
NOTE: This function does not free the storage occupied by the
heap itself, only the node in the list of heaps. */

void
mem_hash_remove(
/*============*/
	mem_heap_t*	heap,	   /* in: the heap to be freed */
	const char*	file_name, /* in: file name of freeing */
	ulint		line)	   /* in: line where freed */
{
	mem_hash_node_t*	node;
	ulint			cell_no;
	ibool			error;
	ulint			size;
	
	ut_ad(mem_heap_check(heap));

	mutex_enter(&mem_hash_mutex);

	cell_no = ut_hash_ulint((ulint)heap, MEM_HASH_SIZE);

	/* Look for the heap in the hash table list */	
	node = UT_LIST_GET_FIRST(*mem_hash_get_nth_cell(cell_no));

	while (node != NULL) {
		if (node->heap == heap) {

			break;
		}

		node = UT_LIST_GET_NEXT(list, node);
	}
						
	if (node == NULL) {
		fprintf(stderr,
    	    "Memory heap or buffer freed in %s line %lu did not exist.\n",
			file_name, (ulong) line);
		ut_error;
	}

	/* Remove from lists */
	UT_LIST_REMOVE(list, *mem_hash_get_nth_cell(cell_no), node);

	UT_LIST_REMOVE(all_list, mem_all_list_base, node);

	/* Validate the heap which will be freed */
	mem_heap_validate_or_print(node->heap, NULL, FALSE, &error, &size,
								NULL, NULL);
	if (error) {
		fprintf(stderr,
	"Inconsistency in memory heap or buffer n:o %lu created\n"
	"in %s line %lu and tried to free in %s line %lu.\n"
	"Hex dump of 400 bytes around memory heap first block start:\n",
			node->nth_heap, node->file_name, (ulong) node->line,
			file_name, (ulong) line);
		ut_print_buf(stderr, (byte*)node->heap - 200, 400);
		fputs("\nDump of the mem heap:\n", stderr);
		mem_heap_validate_or_print(node->heap, NULL, TRUE, &error,
							 &size, NULL, NULL);
		ut_error;
	}

	/* Free the memory occupied by the node struct */
	ut_free(node);

	mem_current_allocated_memory -= size;

	mutex_exit(&mem_hash_mutex);
}
#endif /* UNIV_MEM_DEBUG */

/*******************************************************************
Checks a memory heap for consistency and prints the contents if requested.
Outputs the sum of sizes of buffers given to the user (only in
the debug version), the physical size of the heap and the number of
blocks in the heap. In case of error returns 0 as sizes and number
of blocks. */

void
mem_heap_validate_or_print(
/*=======================*/
	mem_heap_t* 	heap, 	/* in: memory heap */
	byte*		top __attribute__((unused)),	
	                        /* in: calculate and validate only until
				this top pointer in the heap is reached,
				if this pointer is NULL, ignored */
	ibool		print,	/* in: if TRUE, prints the contents
				of the heap; works only in
				the debug version */
	ibool*		error,	/* out: TRUE if error */
	ulint*		us_size,/* out: allocated memory 
				(for the user) in the heap,
				if a NULL pointer is passed as this
				argument, it is ignored; in the
				non-debug version this is always -1 */
	ulint*		ph_size,/* out: physical size of the heap,
				if a NULL pointer is passed as this
				argument, it is ignored */
	ulint*		n_blocks) /* out: number of blocks in the heap,
				if a NULL pointer is passed as this
				argument, it is ignored */
{
	mem_block_t*	block;
	ulint		total_len 	= 0;
	ulint		block_count	= 0;
	ulint		phys_len	= 0;
#ifdef UNIV_MEM_DEBUG
	ulint		len;
	byte*		field;
	byte*		user_field;
	ulint		check_field;
#endif

	/* Pessimistically, we set the parameters to error values */
 	if (us_size != NULL) {
		*us_size = 0;
	}
 	if (ph_size != NULL) {
		*ph_size = 0;
	}
	if (n_blocks != NULL) {
		*n_blocks = 0;
	}	
	*error = TRUE;

	block = heap;
	
	if (block->magic_n != MEM_BLOCK_MAGIC_N) {
		return;
	}

	if (print) {
		fputs("Memory heap:", stderr);
	}

	while (block != NULL) {	
		phys_len += mem_block_get_len(block);

		if ((block->type == MEM_HEAP_BUFFER)
		    && (mem_block_get_len(block) > UNIV_PAGE_SIZE)) {

			fprintf(stderr,
"InnoDB: Error: mem block %p length %lu > UNIV_PAGE_SIZE\n", block,
				(ulong) mem_block_get_len(block));
		    	/* error */

		    	return;
		}

#ifdef UNIV_MEM_DEBUG
		/* We can trace the fields of the block only in the debug
		version */
		if (print) {
			fprintf(stderr, " Block %ld:", block_count);
		}

		field = (byte*)block + mem_block_get_start(block);

		if (top && (field == top)) {

			goto completed;
		}

		while (field < (byte*)block + mem_block_get_free(block)) {

			/* Calculate the pointer to the storage
			which was given to the user */
			
			user_field = field + MEM_FIELD_HEADER_SIZE;
						
			len = mem_field_header_get_len(user_field); 
						   
			if (print) {
				ut_print_buf(stderr, user_field, len);
			}
				
			total_len += len;
			check_field = mem_field_header_get_check(user_field);
			
			if (check_field != 
			    mem_field_trailer_get_check(user_field)) {
				/* error */

				fprintf(stderr,
"InnoDB: Error: block %lx mem field %lx len %lu\n"
"InnoDB: header check field is %lx but trailer %lx\n", (ulint)block,
				   (ulint)field, len, check_field,
				   mem_field_trailer_get_check(user_field));

			     	return;
			}

			/* Move to next field */
			field = field + MEM_SPACE_NEEDED(len);

			if (top && (field == top)) {

				goto completed;
			}

		}

		/* At the end check that we have arrived to the first free
		position */

		if (field != (byte*)block + mem_block_get_free(block)) {
			/* error */

			fprintf(stderr,
"InnoDB: Error: block %lx end of mem fields %lx\n"
"InnoDB: but block free at %lx\n", (ulint)block, (ulint)field,
			(ulint)((byte*)block + mem_block_get_free(block)));

			return;
		}

#endif

		block = UT_LIST_GET_NEXT(list, block);
		block_count++;
	}
#ifdef UNIV_MEM_DEBUG
completed:
#endif	
 	if (us_size != NULL) {
		*us_size = total_len;
	}
 	if (ph_size != NULL) {
		*ph_size = phys_len;
	}
	if (n_blocks != NULL) {
		*n_blocks = block_count;
	}	
	*error = FALSE;
}

/******************************************************************
Prints the contents of a memory heap. */

void
mem_heap_print(
/*===========*/
	mem_heap_t*	heap)	/* in: memory heap */
{
	ibool	error;	
	ulint	us_size;
	ulint	phys_size;
	ulint	n_blocks;

	ut_ad(mem_heap_check(heap));

	mem_heap_validate_or_print(heap, NULL, TRUE, &error, 
				&us_size, &phys_size, &n_blocks);
	fprintf(stderr,
  "\nheap type: %lu; size: user size %lu; physical size %lu; blocks %lu.\n",
			(ulong) heap->type, (ulong) us_size,
			(ulong) phys_size, (ulong) n_blocks);
	ut_a(!error);
}

/******************************************************************
Checks that an object is a memory heap (or a block of it). */

ibool
mem_heap_check(
/*===========*/
				/* out: TRUE if ok */
	mem_heap_t*	heap)	/* in: memory heap */
{
	ut_a(heap->magic_n == MEM_BLOCK_MAGIC_N);

	return(TRUE);
}

/******************************************************************
Validates the contents of a memory heap. */

ibool
mem_heap_validate(
/*==============*/
				/* out: TRUE if ok */
	mem_heap_t*	heap)	/* in: memory heap */
{
	ibool	error;	
	ulint	us_size;
	ulint	phys_size;
	ulint	n_blocks;

	ut_ad(mem_heap_check(heap));

	mem_heap_validate_or_print(heap, NULL, FALSE, &error, &us_size,
						&phys_size, &n_blocks);
	if (error) {
		mem_heap_print(heap);
	}

	ut_a(!error);

	return(TRUE);
}

#ifdef UNIV_MEM_DEBUG
/*********************************************************************
TRUE if no memory is currently allocated. */

ibool
mem_all_freed(void)
/*===============*/
			/* out: TRUE if no heaps exist */
{
	mem_hash_node_t*	node;
	ulint			heap_count	= 0;
	ulint			i;

	mem_validate();

	mutex_enter(&mem_hash_mutex);

	for (i = 0; i < MEM_HASH_SIZE; i++) {

		node = UT_LIST_GET_FIRST(*mem_hash_get_nth_cell(i));
		while (node != NULL) {
			heap_count++;
			node = UT_LIST_GET_NEXT(list, node);
		}
	}

	mutex_exit(&mem_hash_mutex);

	if (heap_count == 0) {

		ut_a(mem_pool_get_reserved(mem_comm_pool) == 0);

		return(TRUE);
	} else {
		return(FALSE);
	}
}

/*********************************************************************
Validates the dynamic memory allocation system. */

ibool
mem_validate_no_assert(void)
/*========================*/
			/* out: TRUE if error */
{
	mem_hash_node_t*	node;
	ulint			n_heaps 		= 0;
	ulint			allocated_mem;
	ulint			ph_size;
	ulint			total_allocated_mem 	= 0;
	ibool			error			= FALSE;
	ulint			n_blocks;
	ulint			i;

	mem_pool_validate(mem_comm_pool);

	mutex_enter(&mem_hash_mutex);

	for (i = 0; i < MEM_HASH_SIZE; i++) {

		node = UT_LIST_GET_FIRST(*mem_hash_get_nth_cell(i));

		while (node != NULL) {
			n_heaps++;

			mem_heap_validate_or_print(node->heap, NULL,
				FALSE, &error, &allocated_mem, 
				&ph_size, &n_blocks);

			if (error) {
				fprintf(stderr,
		"\nERROR!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n\n"
		"Inconsistency in memory heap or buffer created\n"
		"in %s line %lu.\n",
					node->file_name, node->line);

				mutex_exit(&mem_hash_mutex);

				return(TRUE);
			}

			total_allocated_mem += allocated_mem;
			node = UT_LIST_GET_NEXT(list, node);
		}
	}
	
	if ((n_heaps == 0) && (mem_current_allocated_memory != 0)) {
		error = TRUE;
	}
	
	if (mem_total_allocated_memory < mem_current_allocated_memory) {
		error = TRUE;
	}

	if (mem_max_allocated_memory > mem_total_allocated_memory) {
		error = TRUE;
	}
	
	if (mem_n_created_heaps < n_heaps) {
		error = TRUE;
	}
	
	mutex_exit(&mem_hash_mutex);
 
	return(error);
}

/****************************************************************
Validates the dynamic memory */

ibool
mem_validate(void)
/*==============*/
			/* out: TRUE if ok */
{
	ut_a(!mem_validate_no_assert());

	return(TRUE);
}
#endif /* UNIV_MEM_DEBUG */

/****************************************************************
Tries to find neigboring memory allocation blocks and dumps to stderr
the neighborhood of a given pointer. */

void
mem_analyze_corruption(
/*===================*/
	byte*	ptr)	/* in: pointer to place of possible corruption */
{
	byte*	p;
	ulint	i;
	ulint	dist;

	fputs("InnoDB: Apparent memory corruption: mem dump ", stderr);
	ut_print_buf(stderr, ptr - 250, 500);

	fputs("\nInnoDB: Scanning backward trying to find previous allocated mem blocks\n", stderr);

	p = ptr;
	dist = 0;
  
	for (i = 0; i < 10; i++) {
		for (;;) {
			if (((ulint)p) % 4 == 0) {

			    if (*((ulint*)p) == MEM_BLOCK_MAGIC_N) {
				fprintf(stderr,
			"Mem block at - %lu, file %s, line %lu\n",
				(ulong) dist, (p + sizeof(ulint)),
				(ulong) (*(ulint*)(p + 8 + sizeof(ulint))));

				break;
			    }

			    if (*((ulint*)p) == MEM_FREED_BLOCK_MAGIC_N) {
				fprintf(stderr,
			"Freed mem block at - %lu, file %s, line %lu\n",
				(ulong) dist, (p + sizeof(ulint)),
				(ulong) (*(ulint*)(p + 8 + sizeof(ulint))));

				break;
			    }
			}

			p--;
			dist++;
		}

		p--;
		dist++;
	}

	fprintf(stderr,
  "InnoDB: Scanning forward trying to find next allocated mem blocks\n");

	p = ptr;
	dist = 0;
  
	for (i = 0; i < 10; i++) {
		for (;;) {
			if (((ulint)p) % 4 == 0) {

			    if (*((ulint*)p) == MEM_BLOCK_MAGIC_N) {
				fprintf(stderr,
			"Mem block at + %lu, file %s, line %lu\n",
				(ulong) dist, (p + sizeof(ulint)),
				(ulong) (*(ulint*)(p + 8 + sizeof(ulint))));

				break;
			    }

			    if (*((ulint*)p) == MEM_FREED_BLOCK_MAGIC_N) {
				fprintf(stderr,
			"Freed mem block at + %lu, file %s, line %lu\n",
				(ulong) dist, (p + sizeof(ulint)),
				(ulong) (*(ulint*)(p + 8 + sizeof(ulint))));

				break;
			    }
			}

			p++;
			dist++;
		}

		p++;
		dist++;
	}
}

/*********************************************************************
Prints information of dynamic memory usage and currently allocated
memory heaps or buffers. Can only be used in the debug version. */
static
void
mem_print_info_low(
/*===============*/
	ibool	print_all)      /* in: if TRUE, all heaps are printed,
				else only the heaps allocated after the
				previous call of this function */	
{
#ifdef UNIV_MEM_DEBUG
	mem_hash_node_t*	node;
	ulint			n_heaps 		= 0;
	ulint			allocated_mem;
	ulint			ph_size;
	ulint			total_allocated_mem 	= 0;
	ibool			error;
	ulint			n_blocks;
#endif
	FILE*			outfile;
	
	/* outfile = fopen("ibdebug", "a"); */

	outfile = stdout;
	
	fprintf(outfile, "\n");	
	fprintf(outfile,
		"________________________________________________________\n");
	fprintf(outfile, "MEMORY ALLOCATION INFORMATION\n\n");

#ifndef UNIV_MEM_DEBUG

	UT_NOT_USED(print_all);

	mem_pool_print_info(outfile, mem_comm_pool);
	
	fprintf(outfile,
		"Sorry, non-debug version cannot give more memory info\n");

	/* fclose(outfile); */
	
	return;
#else
	mutex_enter(&mem_hash_mutex);
	
	fprintf(outfile, "LIST OF CREATED HEAPS AND ALLOCATED BUFFERS: \n\n");

	if (!print_all) {
		fprintf(outfile, "AFTER THE LAST PRINT INFO\n");
	}

	node = UT_LIST_GET_FIRST(mem_all_list_base);

	while (node != NULL) {
		n_heaps++;
		
		if (!print_all && node->nth_heap < mem_last_print_info) {

			goto next_heap;
		}	

		mem_heap_validate_or_print(node->heap, NULL, 
				FALSE, &error, &allocated_mem, 
				&ph_size, &n_blocks);
		total_allocated_mem += allocated_mem;

		fprintf(outfile,
 "%lu: file %s line %lu of size %lu phys.size %lu with %lu blocks, type %lu\n",
				node->nth_heap, node->file_name, node->line, 
				allocated_mem, ph_size, n_blocks,
				(node->heap)->type);
	next_heap:
		node = UT_LIST_GET_NEXT(all_list, node);
	}
	
	fprintf(outfile, "\n");

	fprintf(outfile, "Current allocated memory	  	: %lu\n", 
			mem_current_allocated_memory);
	fprintf(outfile, "Current allocated heaps and buffers	: %lu\n", 
			n_heaps);
	fprintf(outfile, "Cumulative allocated memory	  	: %lu\n", 
			mem_total_allocated_memory);
	fprintf(outfile, "Maximum allocated memory	  	: %lu\n",
			mem_max_allocated_memory);
	fprintf(outfile, "Cumulative created heaps and buffers	: %lu\n", 
			mem_n_created_heaps);
	fprintf(outfile, "Cumulative number of allocations	: %lu\n", 
			mem_n_allocations);

	mem_last_print_info = mem_n_created_heaps;

	mutex_exit(&mem_hash_mutex);

	mem_pool_print_info(outfile, mem_comm_pool);
	
/*	mem_validate(); */

/* 	fclose(outfile); */
#endif
}

/*********************************************************************
Prints information of dynamic memory usage and currently allocated memory
heaps or buffers. Can only be used in the debug version. */

void
mem_print_info(void)
/*================*/
{
	mem_print_info_low(TRUE);
}

/*********************************************************************
Prints information of dynamic memory usage and currently allocated memory
heaps or buffers since the last ..._print_info or..._print_new_info. */

void
mem_print_new_info(void)
/*====================*/
{
	mem_print_info_low(FALSE);
}
