/* Copyright (c) 2005 PrimeBase Technologies GmbH
 *
 * PrimeBase XT
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * 2005-01-12	Paul McCullagh
 *
 * H&G2JCtL
 */
#ifndef __xt_filesys_h__
#define __xt_filesys_h__

#ifdef XT_WIN
#include <time.h>
#else
#include <sys/time.h>
#include <dirent.h>
#endif
#include <sys/stat.h>

#include "xt_defs.h"
#include "lock_xt.h"

#ifdef XT_WIN
#define XT_FILE_IN_USE(x)			((x) == ERROR_SHARING_VIOLATION)
#define XT_FILE_ACCESS_DENIED(x)	((x) == ERROR_ACCESS_DENIED || (x) == ERROR_NETWORK_ACCESS_DENIED)
#define XT_FILE_TOO_MANY_OPEN(x)	((x) == ERROR_TOO_MANY_OPEN_FILES)
#define XT_FILE_NOT_FOUND(x)		((x) == ERROR_FILE_NOT_FOUND || (x) == ERROR_PATH_NOT_FOUND)
#else
#define XT_FILE_IN_USE(x)			((x) == ETXTBSY)
#define XT_FILE_ACCESS_DENIED(x)	((x) == EACCES)
#define XT_FILE_TOO_MANY_OPEN(x)	((x) == EMFILE)
#define XT_FILE_NOT_FOUND(x)		((x) == ENOENT)
#endif

struct XTOpenFile;

#define XT_MASK				((S_IRUSR | S_IWUSR) | (S_IRGRP | S_IWGRP) | (S_IROTH))

#define XT_FS_DEFAULT		0		/* Open for read/write, error if does not exist. */
#define XT_FS_READONLY		1		/* Open for read only (otherwize read/write). */
#define XT_FS_CREATE		2		/* Create if the file does not exist. */
#define XT_FS_EXCLUSIVE		4		/* Create, and generate an error if it already exists. */
#define XT_FS_MISSING_OK	8		/* Set this flag if you don't want to throw an error if the file does not exist! */
#define XT_FS_MAKE_PATH		16		/* Create the path if it does not exist. */
#define XT_FS_DIRECT_IO		32		/* Use direct I/O on this file if possible (O_DIRECT). */

xtBool			xt_fs_exists(char *path);
xtBool			xt_fs_delete(struct XTThread *self, char *path);
xtBool			xt_fs_file_not_found(int err);
void			xt_fs_mkdir(struct XTThread *self, char *path);
void			xt_fs_mkpath(struct XTThread *self, char *path);
xtBool			xt_fs_rmdir(struct XTThread *self, char *path);
xtBool			xt_fs_stat(struct XTThread *self, char *path, off_t *size, struct timespec *mod_time);
void			xt_fs_move(struct XTThread *self, char *from_path, char *to_path);
xtBool			xt_fs_rename(struct XTThread *self, char *from_path, char *to_path);

#ifdef XT_WIN
#define XT_FD		HANDLE
#define XT_NULL_FD	INVALID_HANDLE_VALUE
#else
#define XT_FD		int
#define XT_NULL_FD	(-1)
#endif

typedef struct XTFileMemMap {
	xtWord1				*mm_start;			/* The in-memory start of the map. */
#ifdef XT_WIN
	HANDLE				mm_mapdes;
#endif
	off_t				mm_length;			/* The length of the file map. */
	XTRWMutexRec		mm_lock;			/* The file map R/W lock. */
	size_t				mm_grow_size;		/* The amount by which the map file is increased. */
} XTFileMemMapRec, *XTFileMemMapPtr;

typedef struct XTFile {
	u_int				fil_ref_count;		/* The number of open file structure referencing this file. */
	char				*fil_path;
	u_int				fil_id;				/* This is used by the disk cache to identify a file in the hash index. */
	XT_FD				fil_filedes;		/* The shared file descriptor (pread and pwrite allow this), on Windows this is used only for mmapped files */
	u_int				fil_handle_count;	/* Number of references in the case of mmapped fil_filedes, both Windows and Unix */
	XTFileMemMapPtr		fil_memmap;			/* Non-null if this file is memory mapped. */
} XTFileRec, *XTFilePtr;

typedef struct XTFileRef {
	XTFilePtr			fr_file;
	u_int				fr_id;				/* Copied from above (small optimisation). */
} XTFileRefRec, *XTFileRefPtr;

typedef struct XTOpenFile : public XTFileRef {
	XT_FD				of_filedes;
} XTOpenFileRec, *XTOpenFilePtr;

void			xt_fs_init(struct XTThread *self);
void			xt_fs_exit(struct XTThread *self);

XTFilePtr		xt_fs_get_file(struct XTThread *self, char *file_name);
void			xt_fs_release_file(struct XTThread *self, XTFilePtr file_ptr);

XTOpenFilePtr	xt_open_file(struct XTThread *self, char *file, int mode);
XTOpenFilePtr	xt_open_file_ns(char *file, int mode);
xtBool			xt_open_file_ns(XTOpenFilePtr *fh, char *file, int mode);
void			xt_close_file(struct XTThread *self, XTOpenFilePtr f);
xtBool			xt_close_file_ns(XTOpenFilePtr f);
char			*xt_file_path(struct XTFileRef *of);

xtBool			xt_lock_file(struct XTThread *self, XTOpenFilePtr of);
void			xt_unlock_file(struct XTThread *self, XTOpenFilePtr of);

off_t			xt_seek_eof_file(struct XTThread *self, XTOpenFilePtr of);
xtBool			xt_set_eof_file(struct XTThread *self, XTOpenFilePtr of, off_t offset);

xtBool			xt_pwrite_file(XTOpenFilePtr of, off_t offset, size_t size, void *data, struct XTIOStats *timer, struct XTThread *thread);
xtBool			xt_pread_file(XTOpenFilePtr of, off_t offset, size_t size, size_t min_size, void *data, size_t *red_size, struct XTIOStats *timer, struct XTThread *thread);
xtBool			xt_flush_file(XTOpenFilePtr of, struct XTIOStats *timer, struct XTThread *thread);

typedef struct XTOpenDir {
	char				*od_path;
#ifdef XT_WIN
	HANDLE				od_handle;
	WIN32_FIND_DATA		od_data;
#else
	char				*od_filter;
	struct dirent		od_entry;
	DIR					*od_dir;
#endif
} XTOpenDirRec, *XTOpenDirPtr;

XTOpenDirPtr	xt_dir_open(struct XTThread *self, c_char *path, c_char *filter);
void			xt_dir_close(struct XTThread *self, XTOpenDirPtr od);
xtBool			xt_dir_next(struct XTThread *self, XTOpenDirPtr od);
char			*xt_dir_name(struct XTThread *self, XTOpenDirPtr od);
xtBool			xt_dir_is_file(struct XTThread *self, XTOpenDirPtr od);
off_t			xt_dir_file_size(struct XTThread *self, XTOpenDirPtr od);

typedef struct XTMapFile : public XTFileRef {
	XTFileMemMapPtr		mf_memmap;
} XTMapFileRec, *XTMapFilePtr;

XTMapFilePtr	xt_open_fmap(struct XTThread *self, char *file, size_t grow_size);
void			xt_close_fmap(struct XTThread *self, XTMapFilePtr map);
xtBool			xt_close_fmap_ns(XTMapFilePtr map);
xtBool			xt_pwrite_fmap(XTMapFilePtr map, off_t offset, size_t size, void *data, struct XTIOStats *timer, struct XTThread *thread);
xtBool			xt_pread_fmap(XTMapFilePtr map, off_t offset, size_t size, size_t min_size, void *data, size_t *red_size, struct XTIOStats *timer, struct XTThread *thread);
xtBool			xt_pread_fmap_4(XTMapFilePtr map, off_t offset, xtWord4 *value, struct XTIOStats *timer, struct XTThread *thread);
xtBool			xt_flush_fmap(XTMapFilePtr map, struct XTIOStats *stat, struct XTThread *thread);
xtWord1			*xt_lock_fmap_ptr(XTMapFilePtr map, off_t offset, size_t size, struct XTIOStats *timer, struct XTThread *thread);
void			xt_unlock_fmap_ptr(XTMapFilePtr map, struct XTThread *thread);

void			xt_fs_copy_file(struct XTThread *self, char *from_path, char *to_path);
void			xt_fs_copy_dir(struct XTThread *self, const char *from, const char *to);

#endif

