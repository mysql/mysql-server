/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: os_jump.h,v 11.3 2000/02/14 02:59:55 bostic Exp $
 */

/* Calls which can be replaced by the application. */
struct __db_jumptab {
	int	(*j_close) __P((int));
	void	(*j_dirfree) __P((char **, int));
	int	(*j_dirlist) __P((const char *, char ***, int *));
	int	(*j_exists) __P((const char *, int *));
	void	(*j_free) __P((void *));
	int	(*j_fsync) __P((int));
	int	(*j_ioinfo) __P((const char *,
		    int, u_int32_t *, u_int32_t *, u_int32_t *));
	void   *(*j_malloc) __P((size_t));
	int	(*j_map) __P((char *, size_t, int, int, void **));
	int	(*j_open) __P((const char *, int, ...));
	ssize_t	(*j_read) __P((int, void *, size_t));
	void   *(*j_realloc) __P((void *, size_t));
	int	(*j_rename) __P((const char *, const char *));
	int	(*j_seek) __P((int, size_t, db_pgno_t, u_int32_t, int, int));
	int	(*j_sleep) __P((u_long, u_long));
	int	(*j_unlink) __P((const char *));
	int	(*j_unmap) __P((void *, size_t));
	ssize_t	(*j_write) __P((int, const void *, size_t));
	int	(*j_yield) __P((void));
};

extern struct __db_jumptab __db_jump;
