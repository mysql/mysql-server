/* ==== malloc.c ============================================================
 * Copyright (c) 1983 Regents of the University of California.
 * Copyright (c) 1993, 1994 by Chris Provenzano, proven@mit.edu
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * 	Description : Malloc functions.
 * 	This is a very fast storage allocator.  It allocates blocks of a small 
 * 	number of different sizes, and keeps free lists of each size.  Blocks that
 * 	don't exactly fit are passed up to the next larger size.  In this 
 * 	implementation, the available sizes are 2^n-4 (or 2^n-10) bytes long.
 * 	This is designed for use in a virtual memory environment.
 *
 * 	0.00 82/02/21 Chris Kingsley kingsley@cit-20 
 *
 *  1.00 93/11/06 proven
 *      -Modified BSD libc malloc to be threadsafe.
 *
 */

#ifndef lint
static const char rcsid[] = "$Id$";
#endif

#include <pthread.h>
#include <sys/types.h>
#include <string.h>
#include <pthread/posix.h>

/*
 * The overhead on a block is at least 4 bytes.  When free, this space
 * contains a pointer to the next free block, and the bottom two bits must
 * be zero.  When in use, the first byte is set to MAGIC, and the second
 * byte is the size index.  The remaining bytes are for alignment.
 * If range checking is enabled then a second word holds the size of the
 * requested block, less 1, rounded up to a multiple of sizeof(RMAGIC).
 * The order of elements is critical: ov_magic must overlay the low order
 * bits of ov_next, and ov_magic can not be a valid ov_next bit pattern.
 */
#ifdef __alpha
#define _MOST_RESTRICTIVE_ALIGNMENT_TYPE char*
#else
#define _MOST_RESTRICTIVE_ALIGNMENT_TYPE double
#endif /* __alpha */
union	overhead {
	_MOST_RESTRICTIVE_ALIGNMENT_TYPE __alignment_pad0;
	union	overhead *ov_next;	/* when free */
	struct {
		u_char	ovu_magic;	/* magic number */
		u_char	ovu_index;	/* bucket # */
#ifdef RCHECK
		u_short	ovu_rmagic;	/* range magic number */
		size_t	ovu_size;	/* actual block size */
#endif
	} ovu;
#define	ov_magic	ovu.ovu_magic
#define	ov_index	ovu.ovu_index
#define	ov_rmagic	ovu.ovu_rmagic
#define	ov_size		ovu.ovu_size
};

#define	MAGIC		0xef		/* magic # on accounting info */
#define RMAGIC		0x5555		/* magic # on range info */

#ifdef RCHECK
#define	RSLOP		sizeof (u_short)
#else
#define	RSLOP		0
#endif

/*
 * nextf[i] is the pointer to the next free block of size 2^(i+3).  The
 * smallest allocatable block is 8 bytes.  The overhead information
 * precedes the data area returned to the user.
 */
#define	NBUCKETS 30
static	union overhead *nextf[NBUCKETS];
#ifndef hpux
extern	char *sbrk();
#endif

static	size_t pagesz;				/* page size */
static	int pagebucket;			/* page size bucket */
static 	pthread_mutex_t malloc_mutex = PTHREAD_MUTEX_INITIALIZER;

#if defined(DEBUG) || defined(RCHECK)
#define	ASSERT(p)   if (!(p)) botch("p")
#include <stdio.h>
static
botch(s)
	char *s;
{
	fprintf(stderr, "\r\nassertion botched: %s\r\n", s);
 	(void) fflush(stderr);		/* just in case user buffered it */
	abort();
}
#else
#define	ASSERT(p)
#endif

/* ==========================================================================
 * morecore()
 *
 * Allocate more memory to the indicated bucket
 */
static inline void morecore(int bucket)
{
  	register union overhead *op;
	register size_t sz;		/* size of desired block */
  	size_t amt;			/* amount to allocate */
  	size_t nblks;			/* how many blocks we get */

	/*
	 * sbrk_size <= 0 only for big, FLUFFY, requests (about
	 * 2^30 bytes on a VAX, I think) or for a negative arg.
	 */
	sz = 1L << (bucket + 3);
#ifdef DEBUG
	ASSERT(sz > 0);
#else
	if (sz <= 0)
		return;
#endif
	if (sz < pagesz) {
		amt = pagesz;
  		nblks = amt / sz;
	} else {
		amt = sz + pagesz;
		nblks = 1;
	}
	op = (union overhead *)sbrk(amt);
	/* no more room! */
	if (op == (union overhead *) -1)
		return;
	/*
	 * Add new memory allocated to that on
	 * free list for this hash bucket.
	 */
  	nextf[bucket] = op;
  	while (--nblks > 0) {
		op->ov_next = (union overhead *)((caddr_t)op + sz);
		op = (union overhead *)((caddr_t)op + sz);
  	}
}

/* ==========================================================================
 * malloc()
 */
void *malloc(size_t nbytes)
{
	pthread_mutex_t *mutex;
  	union overhead *op;
	size_t amt;
	size_t bucket, n;

	mutex = &malloc_mutex;
	pthread_mutex_lock(mutex);
	/*
	 * First time malloc is called, setup page size and
	 * align break pointer so all data will be page aligned.
	 */
	if (pagesz == 0) {
		size_t x;
		pagesz = n = getpagesize();
		op = (union overhead *)sbrk(0);
		x = sizeof (*op) - ((long)op & (n - 1));
		if (n < x)
			n = n + pagesz - x;
		else
			n = n - x;
  		if (n) {
		  if (sbrk(n) == (char *)-1) {
		    /* Unlock before returning (mevans) */
		    pthread_mutex_unlock(mutex);
		    return (NULL);
		  }
		}
		bucket = 0;
		amt = 8;
		while (pagesz > amt) {
			amt <<= 1;
			bucket++;
		}
		pagebucket = bucket;
	}
	/*
	 * Convert amount of memory requested into closest block size
	 * stored in hash buckets which satisfies request.
	 * Account for space used per block for accounting.
	 */
	if (nbytes <= (n = pagesz - sizeof (*op) - RSLOP)) {
#ifndef RCHECK
		amt = 8;	/* size of first bucket */
		bucket = 0;
#else
		amt = 16;	/* size of first bucket */
		bucket = 1;
#endif
		n = -(sizeof (*op) + RSLOP);
	} else {
		amt = pagesz;
		bucket = pagebucket;
	}
	while (nbytes > amt + n) {
		amt <<= 1;
		if (amt == 0) {
			pthread_mutex_unlock(mutex);
			return (NULL);
		}
		bucket++;
	}
	ASSERT (bucket < NBUCKETS);
	/*
	 * If nothing in hash bucket right now,
	 * request more memory from the system.
	 */
  	if ((op = nextf[bucket]) == NULL) {
  		morecore(bucket);
  		if ((op = nextf[bucket]) == NULL) {
			pthread_mutex_unlock(mutex);
  			return (NULL);
		}
	}
	/* remove from linked list */
  	nextf[bucket] = op->ov_next;
	op->ov_magic = MAGIC;
	op->ov_index = bucket;
#ifdef RCHECK
	/*
	 * Record allocated size of block and
	 * bound space with magic numbers.
	 */
	op->ov_size = (nbytes + RSLOP - 1) & ~(RSLOP - 1);
	op->ov_rmagic = RMAGIC;
  	*(u_short *)((caddr_t)(op + 1) + op->ov_size) = RMAGIC;
#endif
	pthread_mutex_unlock(mutex);
  	return ((char *)(op + 1));
}

/* ==========================================================================
 * free()
 */
void free(void *cp)
{   
	pthread_mutex_t *mutex;
	union overhead *op;
	int size;

	mutex = &malloc_mutex;
	pthread_mutex_lock(mutex);
  	if (cp == NULL) {
		pthread_mutex_unlock(mutex);
  		return;
	}
	op = (union overhead *)((caddr_t)cp - sizeof (union overhead));
#ifdef DEBUG
  	ASSERT(op->ov_magic == MAGIC);		/* make sure it was in use */
#else
	if (op->ov_magic != MAGIC) {
		pthread_mutex_unlock(mutex);
		return;				/* sanity */
	}
#endif
#ifdef RCHECK
  	ASSERT(op->ov_rmagic == RMAGIC);
	ASSERT(*(u_short *)((caddr_t)(op + 1) + op->ov_size) == RMAGIC);
#endif
  	size = op->ov_index;
  	ASSERT(size < NBUCKETS);
	op->ov_next = nextf[size];	/* also clobbers ov_magic */
  	nextf[size] = op;

	pthread_mutex_unlock(mutex);
}

/* ==========================================================================
 * realloc()
 *
 * Storage compaction is no longer supported, fix program and try again.
 */
void *realloc(void *cp, size_t nbytes)
{   
	pthread_mutex_t *mutex;
  	size_t onb;
	size_t i;
	union overhead *op;
  	char *res;

  	if (cp == NULL)
  		return (malloc(nbytes));
	op = (union overhead *)((caddr_t)cp - sizeof (union overhead));

	if (op->ov_magic == MAGIC) {
		i = op->ov_index;
	} else {
		/*
		 * This will cause old programs using storage compaction feature of
		 * realloc to break in a pseudo resonable way that is easy to debug.
		 * Returning a malloced buffer without the copy may cause
		 * indeterministic behavior.
		 */
	    return(NULL);
	}

	mutex = &malloc_mutex;
	pthread_mutex_lock(mutex);
	onb = 1L << (i + 3);
	if (onb < pagesz)
		onb -= sizeof (*op) + RSLOP;
	else
		onb += pagesz - sizeof (*op) - RSLOP;

	/* avoid the copy if same size block */
	if (i) {
		i = 1L << (i + 2);
		if (i < pagesz)
			i -= sizeof (*op) + RSLOP;
		else
			i += pagesz - sizeof (*op) - RSLOP;
	}

	if (nbytes <= onb && nbytes > i) {
#ifdef RCHECK
		op->ov_size = (nbytes + RSLOP - 1) & ~(RSLOP - 1);
		*(u_short *)((caddr_t)(op + 1) + op->ov_size) = RMAGIC;
#endif
		pthread_mutex_unlock(mutex);
		return(cp);
	}
	pthread_mutex_unlock(mutex);

  	if ((res = malloc(nbytes)) == NULL) {
		free(cp);
  		return (NULL);
	}

	memcpy(res, cp, (nbytes < onb) ? nbytes : onb);
	free(cp);

  	return (res);
}

/* ==========================================================================
 * calloc()
 *
 * Added to ensure pthread's allocation is used (mevans).
 */
void *calloc(size_t nmemb, size_t size)
{
  void *p;
  size *= nmemb;
  p = malloc(size);
  if (p) memset(p, 0, size);
  return (p);
}
