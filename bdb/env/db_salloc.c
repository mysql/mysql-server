/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: db_salloc.c,v 11.10 2000/12/06 19:55:44 ubell Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stdlib.h>
#include <string.h>
#endif

#include "db_int.h"

/*
 * Implement shared memory region allocation, using simple first-fit algorithm.
 * The model is that we take a "chunk" of shared memory store and begin carving
 * it up into areas, similarly to how malloc works.  We do coalescing on free.
 *
 * The "len" field in the __data struct contains the length of the free region
 * (less the size_t bytes that holds the length).  We use the address provided
 * by the caller to find this length, which allows us to free a chunk without
 * requiring that the caller pass in the length of the chunk they're freeing.
 */
SH_LIST_HEAD(__head);
struct __data {
	size_t len;
	SH_LIST_ENTRY links;
};

/*
 * __db_shalloc_init --
 *	Initialize the area as one large chunk.
 *
 * PUBLIC: void __db_shalloc_init __P((void *, size_t));
 */
void
__db_shalloc_init(area, size)
	void *area;
	size_t size;
{
	struct __data *elp;
	struct __head *hp;

	hp = area;
	SH_LIST_INIT(hp);

	elp = (struct __data *)(hp + 1);
	elp->len = size - sizeof(struct __head) - sizeof(elp->len);
	SH_LIST_INSERT_HEAD(hp, elp, links, __data);
}

/*
 * __db_shalloc --
 *	Allocate some space from the shared region.
 *
 * PUBLIC: int __db_shalloc_size __P((size_t, size_t));
 */
int
__db_shalloc_size(len, align)
	size_t len, align;
{
	/* Never allocate less than the size of a struct __data. */
	if (len < sizeof(struct __data))
		len = sizeof(struct __data);

#ifdef DIAGNOSTIC
	/* Add room for a guard byte. */
	++len;
#endif

	/* Never align to less than a db_align_t boundary. */
	if (align <= sizeof(db_align_t))
		align = sizeof(db_align_t);

	return (ALIGN(len, align) + sizeof (struct __data));
}

/*
 * __db_shalloc --
 *	Allocate some space from the shared region.
 *
 * PUBLIC: int __db_shalloc __P((void *, size_t, size_t, void *));
 */
int
__db_shalloc(p, len, align, retp)
	void *p, *retp;
	size_t len, align;
{
	struct __data *elp;
	size_t *sp;
	void *rp;

	/* Never allocate less than the size of a struct __data. */
	if (len < sizeof(struct __data))
		len = sizeof(struct __data);

#ifdef DIAGNOSTIC
	/* Add room for a guard byte. */
	++len;
#endif

	/* Never align to less than a db_align_t boundary. */
	if (align <= sizeof(db_align_t))
		align = sizeof(db_align_t);

	/* Walk the list, looking for a slot. */
	for (elp = SH_LIST_FIRST((struct __head *)p, __data);
	    elp != NULL;
	    elp = SH_LIST_NEXT(elp, links, __data)) {
		/*
		 * Calculate the value of the returned pointer if we were to
		 * use this chunk.
		 *	+ Find the end of the chunk.
		 *	+ Subtract the memory the user wants.
		 *	+ Find the closest previous correctly-aligned address.
		 */
		rp = (u_int8_t *)elp + sizeof(size_t) + elp->len;
		rp = (u_int8_t *)rp - len;
		rp = (u_int8_t *)((db_alignp_t)rp & ~(align - 1));

		/*
		 * Rp may now point before elp->links, in which case the chunk
		 * was too small, and we have to try again.
		 */
		if ((u_int8_t *)rp < (u_int8_t *)&elp->links)
			continue;

		*(void **)retp = rp;
#ifdef DIAGNOSTIC
		/*
		 * At this point, whether or not we still need to split up a
		 * chunk, retp is the address of the region we are returning,
		 * and (u_int8_t *)elp + sizeof(size_t) + elp->len gives us
		 * the address of the first byte after the end of the chunk.
		 * Make the byte immediately before that the guard byte.
		 */
		*((u_int8_t *)elp + sizeof(size_t) + elp->len - 1) = GUARD_BYTE;
#endif

#define	SHALLOC_FRAGMENT	32
		/*
		 * If there are at least SHALLOC_FRAGMENT additional bytes of
		 * memory, divide the chunk into two chunks.
		 */
		if ((u_int8_t *)rp >=
		    (u_int8_t *)&elp->links + SHALLOC_FRAGMENT) {
			sp = rp;
			*--sp = elp->len -
			    ((u_int8_t *)rp - (u_int8_t *)&elp->links);
			elp->len -= *sp + sizeof(size_t);
			return (0);
		}

		/*
		 * Otherwise, we return the entire chunk, wasting some amount
		 * of space to keep the list compact.  However, because the
		 * address we're returning to the user may not be the address
		 * of the start of the region for alignment reasons, set the
		 * size_t length fields back to the "real" length field to a
		 * flag value, so that we can find the real length during free.
		 */
#define	ILLEGAL_SIZE	1
		SH_LIST_REMOVE(elp, links, __data);
		for (sp = rp; (u_int8_t *)--sp >= (u_int8_t *)&elp->links;)
			*sp = ILLEGAL_SIZE;
		return (0);
	}

	return (ENOMEM);
}

/*
 * __db_shalloc_free --
 *	Free a shared memory allocation.
 *
 * PUBLIC: void __db_shalloc_free __P((void *, void *));
 */
void
__db_shalloc_free(regionp, ptr)
	void *regionp, *ptr;
{
	struct __data *elp, *lastp, *newp;
	struct __head *hp;
	size_t free_size, *sp;
	int merged;

	/*
	 * Step back over flagged length fields to find the beginning of
	 * the object and its real size.
	 */
	for (sp = (size_t *)ptr; sp[-1] == ILLEGAL_SIZE; --sp)
		;
	ptr = sp;

	newp = (struct __data *)((u_int8_t *)ptr - sizeof(size_t));
	free_size = newp->len;

#ifdef DIAGNOSTIC
	/*
	 * The "real size" includes the guard byte;  it's just the last
	 * byte in the chunk, and the caller never knew it existed.
	 *
	 * Check it to make sure it hasn't been stomped.
	 */
	if (*((u_int8_t *)ptr + free_size - 1) != GUARD_BYTE) {
		/*
		 * Eventually, once we push a DB_ENV handle down to these
		 * routines, we should use the standard output channels.
		 */
		fprintf(stderr,
		    "Guard byte incorrect during shared memory free.\n");
		abort();
		/* NOTREACHED */
	}

	/* Trash the returned memory (including guard byte). */
	memset(ptr, CLEAR_BYTE, free_size);
#endif

	/*
	 * Walk the list, looking for where this entry goes.
	 *
	 * We keep the free list sorted by address so that coalescing is
	 * trivial.
	 *
	 * XXX
	 * Probably worth profiling this to see how expensive it is.
	 */
	hp = (struct __head *)regionp;
	for (elp = SH_LIST_FIRST(hp, __data), lastp = NULL;
	    elp != NULL && (void *)elp < (void *)ptr;
	    lastp = elp, elp = SH_LIST_NEXT(elp, links, __data))
		;

	/*
	 * Elp is either NULL (we reached the end of the list), or the slot
	 * after the one that's being returned.  Lastp is either NULL (we're
	 * returning the first element of the list) or the element before the
	 * one being returned.
	 *
	 * Check for coalescing with the next element.
	 */
	merged = 0;
	if ((u_int8_t *)ptr + free_size == (u_int8_t *)elp) {
		newp->len += elp->len + sizeof(size_t);
		SH_LIST_REMOVE(elp, links, __data);
		if (lastp != NULL)
			SH_LIST_INSERT_AFTER(lastp, newp, links, __data);
		else
			SH_LIST_INSERT_HEAD(hp, newp, links, __data);
		merged = 1;
	}

	/* Check for coalescing with the previous element. */
	if (lastp != NULL && (u_int8_t *)lastp +
	    lastp->len + sizeof(size_t) == (u_int8_t *)newp) {
		lastp->len += newp->len + sizeof(size_t);

		/*
		 * If we have already put the new element into the list take
		 * it back off again because it's just been merged with the
		 * previous element.
		 */
		if (merged)
			SH_LIST_REMOVE(newp, links, __data);
		merged = 1;
	}

	if (!merged) {
		if (lastp == NULL)
			SH_LIST_INSERT_HEAD(hp, newp, links, __data);
		else
			SH_LIST_INSERT_AFTER(lastp, newp, links, __data);
	}
}

/*
 * __db_shalloc_count --
 *	Return the amount of memory on the free list.
 *
 * PUBLIC: size_t __db_shalloc_count __P((void *));
 */
size_t
__db_shalloc_count(addr)
	void *addr;
{
	struct __data *elp;
	size_t count;

	count = 0;
	for (elp = SH_LIST_FIRST((struct __head *)addr, __data);
	    elp != NULL;
	    elp = SH_LIST_NEXT(elp, links, __data))
		count += elp->len;

	return (count);
}

/*
 * __db_shsizeof --
 *	Return the size of a shalloc'd piece of memory.
 *
 * !!!
 * Note that this is from an internal standpoint -- it includes not only
 * the size of the memory being used, but also the extra alignment bytes
 * in front and, #ifdef DIAGNOSTIC, the guard byte at the end.
 *
 * PUBLIC: size_t __db_shsizeof __P((void *));
 */
size_t
__db_shsizeof(ptr)
	void *ptr;
{
	struct __data *elp;
	size_t *sp;

	/*
	 * Step back over flagged length fields to find the beginning of
	 * the object and its real size.
	 */
	for (sp = (size_t *)ptr; sp[-1] == ILLEGAL_SIZE; --sp)
		;

	elp = (struct __data *)((u_int8_t *)sp - sizeof(size_t));
	return (elp->len);
}

/*
 * __db_shalloc_dump --
 *
 * PUBLIC: void __db_shalloc_dump __P((void *, FILE *));
 */
void
__db_shalloc_dump(addr, fp)
	void *addr;
	FILE *fp;
{
	struct __data *elp;

	/* Make it easy to call from the debugger. */
	if (fp == NULL)
		fp = stderr;

	fprintf(fp, "%s\nMemory free list\n", DB_LINE);

	for (elp = SH_LIST_FIRST((struct __head *)addr, __data);
	    elp != NULL;
	    elp = SH_LIST_NEXT(elp, links, __data))
		fprintf(fp, "%#lx: %lu\t", (u_long)elp, (u_long)elp->len);
	fprintf(fp, "\n");
}
