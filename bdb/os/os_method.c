/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: os_method.c,v 11.6 2000/11/15 19:25:39 sue Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#endif

#include "db_int.h"
#include "os_jump.h"

struct __db_jumptab __db_jump;

int
db_env_set_func_close(func_close)
	int (*func_close) __P((int));
{
	__db_jump.j_close = func_close;
	return (0);
}

int
db_env_set_func_dirfree(func_dirfree)
	void (*func_dirfree) __P((char **, int));
{
	__db_jump.j_dirfree = func_dirfree;
	return (0);
}

int
db_env_set_func_dirlist(func_dirlist)
	int (*func_dirlist) __P((const char *, char ***, int *));
{
	__db_jump.j_dirlist = func_dirlist;
	return (0);
}

int
db_env_set_func_exists(func_exists)
	int (*func_exists) __P((const char *, int *));
{
	__db_jump.j_exists = func_exists;
	return (0);
}

int
db_env_set_func_free(func_free)
	void (*func_free) __P((void *));
{
	__db_jump.j_free = func_free;
	return (0);
}

int
db_env_set_func_fsync(func_fsync)
	int (*func_fsync) __P((int));
{
	__db_jump.j_fsync = func_fsync;
	return (0);
}

int
db_env_set_func_ioinfo(func_ioinfo)
	int (*func_ioinfo)
	    __P((const char *, int, u_int32_t *, u_int32_t *, u_int32_t *));
{
	__db_jump.j_ioinfo = func_ioinfo;
	return (0);
}

int
db_env_set_func_malloc(func_malloc)
	void *(*func_malloc) __P((size_t));
{
	__db_jump.j_malloc = func_malloc;
	return (0);
}

int
db_env_set_func_map(func_map)
	int (*func_map) __P((char *, size_t, int, int, void **));
{
	__db_jump.j_map = func_map;
	return (0);
}

int
db_env_set_func_open(func_open)
	int (*func_open) __P((const char *, int, ...));
{
	__db_jump.j_open = func_open;
	return (0);
}

int
db_env_set_func_read(func_read)
	ssize_t (*func_read) __P((int, void *, size_t));
{
	__db_jump.j_read = func_read;
	return (0);
}

int
db_env_set_func_realloc(func_realloc)
	void *(*func_realloc) __P((void *, size_t));
{
	__db_jump.j_realloc = func_realloc;
	return (0);
}

int
db_env_set_func_rename(func_rename)
	int (*func_rename) __P((const char *, const char *));
{
	__db_jump.j_rename = func_rename;
	return (0);
}

int
db_env_set_func_seek(func_seek)
	int (*func_seek) __P((int, size_t, db_pgno_t, u_int32_t, int, int));
{
	__db_jump.j_seek = func_seek;
	return (0);
}

int
db_env_set_func_sleep(func_sleep)
	int (*func_sleep) __P((u_long, u_long));
{
	__db_jump.j_sleep = func_sleep;
	return (0);
}

int
db_env_set_func_unlink(func_unlink)
	int (*func_unlink) __P((const char *));
{
	__db_jump.j_unlink = func_unlink;
	return (0);
}

int
db_env_set_func_unmap(func_unmap)
	int (*func_unmap) __P((void *, size_t));
{
	__db_jump.j_unmap = func_unmap;
	return (0);
}

int
db_env_set_func_write(func_write)
	ssize_t (*func_write) __P((int, const void *, size_t));
{
	__db_jump.j_write = func_write;
	return (0);
}

int
db_env_set_func_yield(func_yield)
	int (*func_yield) __P((void));
{
	__db_jump.j_yield = func_yield;
	return (0);
}

int
db_env_set_pageyield(onoff)
	int onoff;
{
	DB_GLOBAL(db_pageyield) = onoff;
	return (0);
}

int
db_env_set_panicstate(onoff)
	int onoff;
{
	DB_GLOBAL(db_panic) = onoff;
	return (0);
}

int
db_env_set_region_init(onoff)
	int onoff;
{
	DB_GLOBAL(db_region_init) = onoff;
	return (0);
}

int
db_env_set_tas_spins(tas_spins)
	u_int32_t tas_spins;
{
	DB_GLOBAL(db_tas_spins) = tas_spins;
	return (0);
}
