/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: os_alloc.c,v 11.18 2000/11/30 00:58:42 ubell Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stdlib.h>
#include <string.h>
#endif

#include "db_int.h"
#include "os_jump.h"

#ifdef DIAGNOSTIC
static void __os_guard __P((void));
#endif

/*
 * !!!
 * Correct for systems that return NULL when you allocate 0 bytes of memory.
 * There are several places in DB where we allocate the number of bytes held
 * by the key/data item, and it can be 0.  Correct here so that malloc never
 * returns a NULL for that reason (which behavior is permitted by ANSI).  We
 * could make these calls macros on non-Alpha architectures (that's where we
 * saw the problem), but it's probably not worth the autoconf complexity.
 *
 * !!!
 * Correct for systems that don't set errno when malloc and friends fail.
 *
 *	Out of memory.
 *	We wish to hold the whole sky,
 *	But we never will.
 */

/*
 * __os_strdup --
 *	The strdup(3) function for DB.
 *
 * PUBLIC: int __os_strdup __P((DB_ENV *, const char *, void *));
 */
int
__os_strdup(dbenv, str, storep)
	DB_ENV *dbenv;
	const char *str;
	void *storep;
{
	size_t size;
	int ret;
	void *p;

	*(void **)storep = NULL;

	size = strlen(str) + 1;
	if ((ret = __os_malloc(dbenv, size, NULL, &p)) != 0)
		return (ret);

	memcpy(p, str, size);

	*(void **)storep = p;
	return (0);
}

/*
 * __os_calloc --
 *	The calloc(3) function for DB.
 *
 * PUBLIC: int __os_calloc __P((DB_ENV *, size_t, size_t, void *));
 */
int
__os_calloc(dbenv, num, size, storep)
	DB_ENV *dbenv;
	size_t num, size;
	void *storep;
{
	void *p;
	int ret;

	size *= num;
	if ((ret = __os_malloc(dbenv, size, NULL, &p)) != 0)
		return (ret);

	memset(p, 0, size);

	*(void **)storep = p;
	return (0);
}

/*
 * __os_malloc --
 *	The malloc(3) function for DB.
 *
 * PUBLIC: int __os_malloc __P((DB_ENV *, size_t, void *(*)(size_t), void *));
 */
int
__os_malloc(dbenv, size, db_malloc, storep)
	DB_ENV *dbenv;
	size_t size;
	void *(*db_malloc) __P((size_t)), *storep;
{
	int ret;
	void *p;

	*(void **)storep = NULL;

	/* Never allocate 0 bytes -- some C libraries don't like it. */
	if (size == 0)
		++size;
#ifdef DIAGNOSTIC
	else
		++size;				/* Add room for a guard byte. */
#endif

	/* Some C libraries don't correctly set errno when malloc(3) fails. */
	__os_set_errno(0);
	if (db_malloc != NULL)
		p = db_malloc(size);
	else if (__db_jump.j_malloc != NULL)
		p = __db_jump.j_malloc(size);
	else
		p = malloc(size);
	if (p == NULL) {
		ret = __os_get_errno();
		if (ret == 0) {
			__os_set_errno(ENOMEM);
			ret = ENOMEM;
		}
		__db_err(dbenv,
		    "malloc: %s: %lu", strerror(ret), (u_long)size);
		return (ret);
	}

#ifdef DIAGNOSTIC
	/*
	 * Guard bytes: if #DIAGNOSTIC is defined, we allocate an additional
	 * byte after the memory and set it to a special value that we check
	 * for when the memory is free'd.  This is fine for structures, but
	 * not quite so fine for strings.  There are places in DB where memory
	 * is allocated sufficient to hold the largest possible string that
	 * we'll see, and then only some subset of the memory is used.  To
	 * support this usage, the __os_freestr() function checks the byte
	 * after the string's nul, which may or may not be the last byte in
	 * the originally allocated memory.
	 */
	memset(p, CLEAR_BYTE, size);		/* Initialize guard byte. */
#endif
	*(void **)storep = p;

	return (0);
}

/*
 * __os_realloc --
 *	The realloc(3) function for DB.
 *
 * PUBLIC: int __os_realloc __P((DB_ENV *,
 * PUBLIC:     size_t, void *(*)(void *, size_t), void *));
 */
int
__os_realloc(dbenv, size, db_realloc, storep)
	DB_ENV *dbenv;
	size_t size;
	void *(*db_realloc) __P((void *, size_t)), *storep;
{
	int ret;
	void *p, *ptr;

	ptr = *(void **)storep;

	/* If we haven't yet allocated anything yet, simply call malloc. */
	if (ptr == NULL && db_realloc == NULL)
		return (__os_malloc(dbenv, size, NULL, storep));

	/* Never allocate 0 bytes -- some C libraries don't like it. */
	if (size == 0)
		++size;
#ifdef DIAGNOSTIC
	else
		++size;				/* Add room for a guard byte. */
#endif

	/*
	 * Some C libraries don't correctly set errno when realloc(3) fails.
	 *
	 * Don't overwrite the original pointer, there are places in DB we
	 * try to continue after realloc fails.
	 */
	__os_set_errno(0);
	if (db_realloc != NULL)
		p = db_realloc(ptr, size);
	else if (__db_jump.j_realloc != NULL)
		p = __db_jump.j_realloc(ptr, size);
	else
		p = realloc(ptr, size);
	if (p == NULL) {
		if ((ret = __os_get_errno()) == 0) {
			ret = ENOMEM;
			__os_set_errno(ENOMEM);
		}
		__db_err(dbenv,
		    "realloc: %s: %lu", strerror(ret), (u_long)size);
		return (ret);
	}
#ifdef DIAGNOSTIC
	((u_int8_t *)p)[size - 1] = CLEAR_BYTE;	/* Initialize guard byte. */
#endif

	*(void **)storep = p;

	return (0);
}

/*
 * __os_free --
 *	The free(3) function for DB.
 *
 * PUBLIC: void __os_free __P((void *, size_t));
 */
void
__os_free(ptr, size)
	void *ptr;
	size_t size;
{
#ifdef DIAGNOSTIC
	if (size != 0) {
		/*
		 * Check that the guard byte (one past the end of the memory) is
		 * still CLEAR_BYTE.
		 */
		if (((u_int8_t *)ptr)[size] != CLEAR_BYTE)
			 __os_guard();

		/* Clear memory. */
		if (size != 0)
			memset(ptr, CLEAR_BYTE, size);
	}
#else
	COMPQUIET(size, 0);
#endif

	if (__db_jump.j_free != NULL)
		__db_jump.j_free(ptr);
	else
		free(ptr);
}

/*
 * __os_freestr --
 *	The free(3) function for DB, freeing a string.
 *
 * PUBLIC: void __os_freestr __P((void *));
 */
void
__os_freestr(ptr)
	void *ptr;
{
#ifdef DIAGNOSTIC
	size_t size;

	size = strlen(ptr) + 1;

	/*
	 * Check that the guard byte (one past the end of the memory) is
	 * still CLEAR_BYTE.
	 */
	if (((u_int8_t *)ptr)[size] != CLEAR_BYTE)
		 __os_guard();

	/* Clear memory. */
	memset(ptr, CLEAR_BYTE, size);
#endif

	if (__db_jump.j_free != NULL)
		__db_jump.j_free(ptr);
	else
		free(ptr);
}

#ifdef DIAGNOSTIC
/*
 * __os_guard --
 *	Complain and abort.
 */
static void
__os_guard()
{
	/*
	 * Eventually, once we push a DB_ENV handle down to these
	 * routines, we should use the standard output channels.
	 */
	fprintf(stderr, "Guard byte incorrect during free.\n");
	abort();
	/* NOTREACHED */
}
#endif

/*
 * __ua_memcpy --
 *	Copy memory to memory without relying on any kind of alignment.
 *
 *	There are places in DB that we have unaligned data, for example,
 *	when we've stored a structure in a log record as a DBT, and now
 *	we want to look at it.  Unfortunately, if you have code like:
 *
 *		struct a {
 *			int x;
 *		} *p;
 *
 *		void *func_argument;
 *		int local;
 *
 *		p = (struct a *)func_argument;
 *		memcpy(&local, p->x, sizeof(local));
 *
 *	compilers optimize to use inline instructions requiring alignment,
 *	and records in the log don't have any particular alignment.  (This
 *	isn't a compiler bug, because it's a structure they're allowed to
 *	assume alignment.)
 *
 *	Casting the memcpy arguments to (u_int8_t *) appears to work most
 *	of the time, but we've seen examples where it wasn't sufficient
 *	and there's nothing in ANSI C that requires that work.
 *
 * PUBLIC: void *__ua_memcpy __P((void *, const void *, size_t));
 */
void *
__ua_memcpy(dst, src, len)
	void *dst;
	const void *src;
	size_t len;
{
	return ((void *)memcpy(dst, src, len));
}
