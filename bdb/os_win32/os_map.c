/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: os_map.c,v 11.22 2000/10/26 14:18:08 bostic Exp $";
#endif /* not lint */

#include "db_int.h"
#include "os_jump.h"

static int __os_map
  __P((DB_ENV *, char *, REGINFO *, DB_FH *, size_t, int, int, int, void **));
static int __os_unique_name __P((char *, int, char *));

/*
 * __os_r_sysattach --
 *	Create/join a shared memory region.
 */
int
__os_r_sysattach(dbenv, infop, rp)
	DB_ENV *dbenv;
	REGINFO *infop;
	REGION *rp;
{
	DB_FH fh;
	int is_system, ret;

	/*
	 * Try to open/create the file.  We DO NOT need to ensure that multiple
	 * threads/processes attempting to simultaneously create the region are
	 * properly ordered, our caller has already taken care of that.
	 */
	if ((ret = __os_open(dbenv, infop->name,
	    F_ISSET(infop, REGION_CREATE_OK) ? DB_OSO_CREATE: 0,
	    infop->mode, &fh)) != 0) {
		__db_err(dbenv, "%s: %s", infop->name, db_strerror(ret));
		return (ret);
	}

	/*
	 * On Windows/9X, files that are opened by multiple processes do not
	 * share data correctly.  For this reason, the DB_SYSTEM_MEM flag is
	 * implied for any application that does not specify the DB_PRIVATE
	 * flag.
	 */
	is_system = F_ISSET(dbenv, DB_ENV_SYSTEM_MEM) ||
	    (!F_ISSET(dbenv, DB_ENV_PRIVATE) && __os_is_winnt() == 0);

	/*
	 * Map the file in.  If we're creating an in-system-memory region,
	 * specify a segment ID (which is never used again) so that the
	 * calling code writes out the REGENV_REF structure to the primary
	 * environment file.
	 */
	ret = __os_map(dbenv, infop->name, infop, &fh, rp->size,
	   1, is_system, 0, &infop->addr);
	if (ret == 0 && is_system == 1)
		rp->segid = 1;

	(void)__os_closehandle(&fh);

	return (ret);
}

/*
 * __os_r_sysdetach --
 *	Detach from a shared memory region.
 */
int
__os_r_sysdetach(dbenv, infop, destroy)
	DB_ENV *dbenv;
	REGINFO *infop;
	int destroy;
{
	int ret, t_ret;

	if (infop->wnt_handle != NULL) {
		(void)CloseHandle(*((HANDLE*)(infop->wnt_handle)));
		__os_free(infop->wnt_handle, sizeof(HANDLE));
	}

	__os_set_errno(0);
	ret = !UnmapViewOfFile(infop->addr) ? __os_win32_errno() : 0;
	if (ret != 0)
		__db_err(dbenv, "UnmapViewOfFile: %s", strerror(ret));

	if (F_ISSET(dbenv, DB_ENV_SYSTEM_MEM) && destroy &&
	    (t_ret = __os_unlink(dbenv, infop->name)) != 0 && ret == 0)
		ret = t_ret;

	return (ret);
}

/*
 * __os_mapfile --
 *	Map in a shared memory file.
 */
int
__os_mapfile(dbenv, path, fhp, len, is_rdonly, addr)
	DB_ENV *dbenv;
	char *path;
	DB_FH *fhp;
	int is_rdonly;
	size_t len;
	void **addr;
{
	/* If the user replaced the map call, call through their interface. */
	if (__db_jump.j_map != NULL)
		return (__db_jump.j_map(path, len, 0, is_rdonly, addr));

	return (__os_map(dbenv, path, NULL, fhp, len, 0, 0, is_rdonly, addr));
}

/*
 * __os_unmapfile --
 *	Unmap the shared memory file.
 */
int
__os_unmapfile(dbenv, addr, len)
	DB_ENV *dbenv;
	void *addr;
	size_t len;
{
	/* If the user replaced the map call, call through their interface. */
	if (__db_jump.j_unmap != NULL)
		return (__db_jump.j_unmap(addr, len));

	__os_set_errno(0);
	return (!UnmapViewOfFile(addr) ? __os_win32_errno() : 0);
}

/*
 * __os_unique_name --
 *	Create a unique identifying name from a pathname (may be absolute or
 *	relative) and/or a file descriptor.
 *
 *	The name returned must be unique (different files map to different
 *	names), and repeatable (same files, map to same names).  It's not
 *	so easy to do by name.  Should handle not only:
 *
 *		foo.bar  ==  ./foo.bar  ==  c:/whatever_path/foo.bar
 *
 *	but also understand that:
 *
 *		foo.bar  ==  Foo.Bar	(FAT file system)
 *		foo.bar  !=  Foo.Bar	(NTFS)
 *
 *	The best solution is to use the identifying number in the file
 *	information structure (similar to UNIX inode #).
 */
static int
__os_unique_name(orig_path, fd, result_path)
	char *orig_path, *result_path;
	int fd;
{
	BY_HANDLE_FILE_INFORMATION fileinfo;

	__os_set_errno(0);
	if (!GetFileInformationByHandle(
	    (HANDLE)_get_osfhandle(fd), &fileinfo))
		return (__os_win32_errno());
	(void)sprintf(result_path, "%ld.%ld.%ld",
	    fileinfo.dwVolumeSerialNumber,
	    fileinfo.nFileIndexHigh, fileinfo.nFileIndexLow);
	return (0);
}

/*
 * __os_map --
 *	The mmap(2) function for Windows.
 */
static int
__os_map(dbenv, path, infop, fhp, len, is_region, is_system, is_rdonly, addr)
	DB_ENV *dbenv;
	REGINFO *infop;
	char *path;
	DB_FH *fhp;
	int is_region, is_system, is_rdonly;
	size_t len;
	void **addr;
{
	HANDLE hMemory;
	REGENV *renv;
	int ret;
	void *pMemory;
	char shmem_name[MAXPATHLEN];
	int use_pagefile;

	ret = 0;
	if (infop != NULL)
		infop->wnt_handle = NULL;

	use_pagefile = is_region && is_system;

	/*
	 * If creating a region in system space, get a matching name in the
	 * paging file namespace.
	 */
	if (use_pagefile) {
		(void)strcpy(shmem_name, "__db_shmem.");
		if ((ret = __os_unique_name(path, fhp->fd,
		    &shmem_name[strlen(shmem_name)])) != 0)
			return (ret);
	}

	/*
	 * XXX
	 * DB: We have not implemented copy-on-write here.
	 *
	 * XXX
	 * DB: This code will fail if the library is ever compiled on a 64-bit
	 * machine.
	 *
	 * XXX
	 * If this is an region in system memory, let's try opening using the
	 * OpenFileMapping() first.  Why, oh why are we doing this?
	 *
	 * Well, we might be asking the OS for a handle to a pre-existing
	 * memory section, or we might be the first to get here and want the
	 * section created. CreateFileMapping() sounds like it will do both
	 * jobs. But, not so. It seems to mess up making the commit charge to
	 * the process. It thinks, incorrectly, that when we want to join a
	 * previously existing section, that it should make a commit charge
	 * for the whole section.  In fact, there is no new committed memory
	 * whatever.  The call can fail if there is insufficient memory free
	 * to handle the erroneous commit charge.  So, we find that the bogus
	 * commit is not made if we call OpenFileMapping().  So we do that
	 * first, and only call CreateFileMapping() if we're really creating
	 * the section.
	 */
	hMemory = NULL;
	__os_set_errno(0);
	if (use_pagefile)
		hMemory = OpenFileMapping(
		    is_rdonly ? FILE_MAP_READ : FILE_MAP_ALL_ACCESS,
		    0,
		    shmem_name);

	if (hMemory == NULL)
		hMemory = CreateFileMapping(
		    use_pagefile ?
		    (HANDLE)0xFFFFFFFF : (HANDLE)_get_osfhandle(fhp->fd),
		    0,
		    is_rdonly ? PAGE_READONLY : PAGE_READWRITE,
		    0, len,
		    use_pagefile ? shmem_name : NULL);
	if (hMemory == NULL) {
		__db_err(dbenv,
		    "OpenFileMapping: %s", strerror(__os_win32_errno()));
		return (__os_win32_errno());
	}

	pMemory = MapViewOfFile(hMemory,
	    (is_rdonly ? FILE_MAP_READ : FILE_MAP_ALL_ACCESS), 0, 0, len);
	if (pMemory == NULL) {
		__db_err(dbenv,
		    "MapViewOfFile: %s", strerror(__os_win32_errno()));
		return (__os_win32_errno());
	}

	/*
	 * XXX
	 * It turns out that the kernel object underlying the named section
	 * is reference counted, but that the call to MapViewOfFile() above
	 * does NOT increment the reference count! So, if we close the handle
	 * here, the kernel deletes the object from the kernel namespace.
	 * When a second process comes along to join the region, the kernel
	 * happily creates a new object with the same name, but completely
	 * different identity. The two processes then have distinct isolated
	 * mapped sections, not at all what was wanted. Not closing the handle
	 * here fixes this problem.  We carry the handle around in the region
	 * structure so we can close it when unmap is called.  Ignore malloc
	 * errors, it just means we leak the memory.
	 */
	if (use_pagefile && infop != NULL) {
		if (__os_malloc(NULL,
		    sizeof(HANDLE), NULL, &infop->wnt_handle) == 0)
			memcpy(infop->wnt_handle, &hMemory, sizeof(HANDLE));
	} else
		CloseHandle(hMemory);

	if (is_region) {
		/*
		 * XXX
		 * Windows/95 zeroes anonymous memory regions at last close.
		 * This means that the backing file can exist and reference
		 * the region, but the region itself is no longer initialized.
		 * If the caller is capable of creating the region, update
		 * the REGINFO structure so that they do so.
		 */
		renv = (REGENV *)pMemory;
		if (renv->magic == 0)
			if (F_ISSET(infop, REGION_CREATE_OK))
				F_SET(infop, REGION_CREATE);
			else {
				(void)UnmapViewOfFile(pMemory);
				pMemory = NULL;
				ret = EAGAIN;
			}
	}

	*addr = pMemory;
	return (ret);
}
