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

#include "xt_config.h"

#ifdef DRIZZLED
#include <bitset>
#endif

#ifndef XT_WIN
#include <unistd.h>
#include <dirent.h>
#include <sys/mman.h>
#endif
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>

#include "strutil_xt.h"
#include "pthread_xt.h"
#include "thread_xt.h"
#include "filesys_xt.h"
#include "memory_xt.h"
#include "cache_xt.h"
#include "sortedlist_xt.h"
#include "trace_xt.h"

#ifdef DEBUG
//#define DEBUG_PRINT_IO
//#define DEBUG_TRACE_IO
//#define DEBUG_TRACE_MAP_IO
//#define DEBUG_TRACE_FILES
#endif

#ifdef DEBUG_TRACE_FILES
//#define PRINTF		xt_ftracef
#define PRINTF		xt_trace
#endif

/* ----------------------------------------------------------------------
 * Globals
 */

typedef struct FsGlobals {
	xt_mutex_type		*fsg_lock;						/* The xtPublic cache lock. */
	u_int				fsg_current_id;
	XTSortedListPtr		fsg_open_files;
} FsGlobalsRec;

static FsGlobalsRec	fs_globals;

#ifdef XT_WIN
static int fs_get_win_error()
{
	return (int) GetLastError();
}

xtPublic void xt_get_win_message(char *buffer, size_t size, int err)
{
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        buffer,
        size, NULL);
}
#endif

/* ----------------------------------------------------------------------
 * Open file list
 */

static XTFilePtr fs_new_file(XTThreadPtr self, char *file)
{
	XTFilePtr file_ptr;

	pushsr_(file_ptr, xt_free, (XTFilePtr) xt_calloc(self, sizeof(XTFileRec)));

	file_ptr->fil_path = xt_dup_string(self, file);
	file_ptr->fil_id = fs_globals.fsg_current_id++;
#ifdef DEBUG_TRACE_FILES
	PRINTF("%s: allocated file: (%d) %s\n", self->t_name, (int) file_ptr->fil_id, xt_last_2_names_of_path(file_ptr->fil_path));
#endif
	if (!fs_globals.fsg_current_id)
		fs_globals.fsg_current_id++;
	file_ptr->fil_filedes = XT_NULL_FD;
	file_ptr->fil_handle_count = 0;

	popr_(); // Discard xt_free(file_ptr)
	return file_ptr;
}

static void fs_close_fmap(XTThreadPtr self, XTFileMemMapPtr mm)
{
#ifdef XT_WIN
	if (mm->mm_start) {
		FlushViewOfFile(mm->mm_start, 0);
		UnmapViewOfFile(mm->mm_start);
		mm->mm_start = NULL;
	}
	if (mm->mm_mapdes != NULL) {
		CloseHandle(mm->mm_mapdes);
		mm->mm_mapdes = NULL;
	}
#else
	if (mm->mm_start) {
		msync( (char *)mm->mm_start, (size_t) mm->mm_length, MS_SYNC);
		munmap((caddr_t) mm->mm_start, (size_t) mm->mm_length);
		mm->mm_start = NULL;
	}
#endif
	FILE_MAP_FREE_LOCK(self, &mm->mm_lock);
	xt_free(self, mm);
}

static void fs_free_file(XTThreadPtr self, void *XT_UNUSED(thunk), void *item)
{
	XTFilePtr	file_ptr = *((XTFilePtr *) item);

	if (file_ptr->fil_filedes != XT_NULL_FD) {
#ifdef DEBUG_TRACE_FILES
		PRINTF("%s: close file: (%d) %s\n", self->t_name, (int) file_ptr->fil_id, xt_last_2_names_of_path(file_ptr->fil_path));
#endif
#ifdef XT_WIN
		CloseHandle(file_ptr->fil_filedes);
#else
		close(file_ptr->fil_filedes);
#endif
		//PRINTF("close (FILE) %d %s\n", file_ptr->fil_filedes, file_ptr->fil_path);
		file_ptr->fil_filedes = XT_NULL_FD;
	}

#ifdef DEBUG_TRACE_FILES
	PRINTF("%s: free file: (%d) %s\n", self->t_name, (int) file_ptr->fil_id, 
		file_ptr->fil_path ? xt_last_2_names_of_path(file_ptr->fil_path) : "?");
#endif

	if (!file_ptr->fil_ref_count) {
		ASSERT_NS(!file_ptr->fil_handle_count);
		/* Flush any cache before this file is invalid: */
		if (file_ptr->fil_path) {
			xt_free(self, file_ptr->fil_path);
			file_ptr->fil_path = NULL;
		}

		xt_free(self, file_ptr);
	}
}

static int fs_comp_file(XTThreadPtr XT_UNUSED(self), register const void *XT_UNUSED(thunk), register const void *a, register const void *b)
{
	char		*file_name = (char *) a;
	XTFilePtr	file_ptr = *((XTFilePtr *) b);

	return strcmp(file_name, file_ptr->fil_path);
}

static int fs_comp_file_ci(XTThreadPtr XT_UNUSED(self), register const void *XT_UNUSED(thunk), register const void *a, register const void *b)
{
	char		*file_name = (char *) a;
	XTFilePtr	file_ptr = *((XTFilePtr *) b);

	return strcasecmp(file_name, file_ptr->fil_path);
}

/* ----------------------------------------------------------------------
 * init & exit
 */

xtPublic void xt_fs_init(XTThreadPtr self)
{
	fs_globals.fsg_open_files = xt_new_sortedlist(self,
		sizeof(XTFilePtr), 20, 20,
		pbxt_ignore_case ? fs_comp_file_ci : fs_comp_file,
		NULL, fs_free_file, TRUE, FALSE);
	fs_globals.fsg_lock = fs_globals.fsg_open_files->sl_lock;
	fs_globals.fsg_current_id = 1;
}

xtPublic void xt_fs_exit(XTThreadPtr self)
{
	if (fs_globals.fsg_open_files) {
		xt_free_sortedlist(self, fs_globals.fsg_open_files);
		fs_globals.fsg_open_files = NULL;
	}
	fs_globals.fsg_lock = NULL;
	fs_globals.fsg_current_id = 0;
}

/* ----------------------------------------------------------------------
 * File operations
 */

static void fs_set_stats(XTThreadPtr self, char *path)
{
	char		super_path[PATH_MAX];
	struct stat	stats;
	char		*ptr;

	ptr = xt_last_name_of_path(path);
	if (ptr == path) 
		strcpy(super_path, ".");
	else {
		xt_strcpy(PATH_MAX, super_path, path);

		if ((ptr = xt_last_name_of_path(super_path)))
			*ptr = 0;
	}
	if (stat(super_path, &stats) == -1)
		xt_throw_ferrno(XT_CONTEXT, errno, super_path);

	if (chmod(path, stats.st_mode) == -1)
		xt_throw_ferrno(XT_CONTEXT, errno, path);

	/*chown(path, stats.st_uid, stats.st_gid);*/
}

xtPublic char *xt_file_path(struct XTFileRef *of)
{
	return of->fr_file->fil_path;
}

xtBool xt_fs_exists(char *path)
{
	int err;

	err = access(path, F_OK);
	if (err == -1)
		return FALSE;
	return TRUE;
}

/*
 * No error is generated if the file dose not exist.
 */
xtPublic xtBool xt_fs_delete(XTThreadPtr self, char *name)
{
#ifdef DEBUG_TRACE_FILES
	PRINTF("%s: DELETE FILE: %s\n", xt_get_self()->t_name, xt_last_2_names_of_path(name));
#endif
#ifdef XT_WIN
	//PRINTF("delete %s\n", name);
	if (!DeleteFile(name)) {
		int err = fs_get_win_error();

		if (!XT_FILE_NOT_FOUND(err)) {
			xt_throw_ferrno(XT_CONTEXT, err, name);
			return FAILED;
		}
	}
#else
	if (unlink(name) == -1) {
		int err = errno;

		if (err != ENOENT) {
			xt_throw_ferrno(XT_CONTEXT, err, name);
			return FAILED;
		}
	}
#endif
	return OK;
}

xtPublic xtBool xt_fs_file_not_found(int err)
{
#ifdef XT_WIN
	return XT_FILE_NOT_FOUND(err);
#else
	return err == ENOENT;
#endif
}

xtPublic void xt_fs_move(struct XTThread *self, char *from_path, char *to_path)
{
#ifdef DEBUG_TRACE_FILES
	PRINTF("%s: MOVE FILE: %s --> %s\n", xt_get_self()->t_name, xt_last_2_names_of_path(from_path), xt_last_2_names_of_path(to_path));
#endif
#ifdef XT_WIN
	if (!MoveFile(from_path, to_path))
		xt_throw_ferrno(XT_CONTEXT, fs_get_win_error(), from_path);
#else
	int err;

	if (link(from_path, to_path) == -1) {
		err = errno;
		xt_throw_ferrno(XT_CONTEXT, err, from_path);
	}

	if (unlink(from_path) == -1) {
		err = errno;
		unlink(to_path);
		xt_throw_ferrno(XT_CONTEXT, err, from_path);
	}
#endif
}

xtPublic xtBool xt_fs_rename(struct XTThread *self, char *from_path, char *to_path)
{
	int err;

#ifdef DEBUG_TRACE_FILES
	PRINTF("%s: RENAME FILE: %s --> %s\n", xt_get_self()->t_name, xt_last_2_names_of_path(from_path), xt_last_2_names_of_path(to_path));
#endif
	if (rename(from_path, to_path) == -1) {
		err = errno;
		xt_throw_ferrno(XT_CONTEXT, err, from_path);
		return FAILED;
	}
	return OK;
}

xtPublic xtBool xt_fs_stat(XTThreadPtr self, char *path, off_t *size, struct timespec *mod_time)
{
#ifdef XT_WIN
	HANDLE						fh;
	BY_HANDLE_FILE_INFORMATION	info;
	SECURITY_ATTRIBUTES			sa = { sizeof(SECURITY_ATTRIBUTES), 0, 0 };

	fh = CreateFile(
		path,
		GENERIC_READ,
		FILE_SHARE_READ,
		&sa,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	if (fh == INVALID_HANDLE_VALUE) {
		xt_throw_ferrno(XT_CONTEXT, fs_get_win_error(), path);
		return FAILED;
	}

	if (!GetFileInformationByHandle(fh, &info)) {
		CloseHandle(fh);
		xt_throw_ferrno(XT_CONTEXT, fs_get_win_error(), path);
		return FAILED;
	}

	CloseHandle(fh);
	if (size)
		*size = (off_t) info.nFileSizeLow | (((off_t) info.nFileSizeHigh) << 32);
	if (mod_time)
		mod_time->tv.ft = info.ftLastWriteTime;
#else
	struct stat sb;

	if (stat(path, &sb) == -1) {
		xt_throw_ferrno(XT_CONTEXT, errno, path);
		return FAILED;
	}
	if (size)
		*size = sb.st_size;
	if (mod_time) {
		mod_time->tv_sec = sb.st_mtime;
#ifdef XT_MAC
		/* This is the Mac OS X version: */
		mod_time->tv_nsec = sb.st_mtimespec.tv_nsec;
#else
#ifdef __USE_MISC
		/* This is the Linux version: */
		mod_time->tv_nsec = sb.st_mtim.tv_nsec;
#else
		/* Not supported? */
		mod_time->tv_nsec = 0;
#endif
#endif
	}
#endif
	return OK;
}

void xt_fs_mkdir(XTThreadPtr self, char *name)
{
	char path[PATH_MAX];

	xt_strcpy(PATH_MAX, path, name);
	xt_remove_dir_char(path);

#ifdef XT_WIN
	{
		SECURITY_ATTRIBUTES	sa = { sizeof(SECURITY_ATTRIBUTES), 0, 0 };

		if (!CreateDirectory(path, &sa))
			xt_throw_ferrno(XT_CONTEXT, fs_get_win_error(), path);
	}
#else
	if (mkdir(path, S_IRWXU | S_IRWXG | S_IRWXO) == -1)
		xt_throw_ferrno(XT_CONTEXT, errno, path);

	try_(a) {
		fs_set_stats(self, path);
	}
	catch_(a) {
		xt_fs_rmdir(NULL, name);
		throw_();
	}
	cont_(a);
#endif
}

void xt_fs_mkpath(XTThreadPtr self, char *path)
{
	char *ptr;

	if (xt_fs_exists(path))
		return;

	if (!(ptr = (char *) xt_last_directory_of_path((c_char *) path)))
		return;
	if (ptr == path)
		return;
	ptr--;
	if (XT_IS_DIR_CHAR(*ptr)) {
		*ptr = 0;
		xt_fs_mkpath(self, path);
		*ptr = XT_DIR_CHAR;
		xt_fs_mkdir(self, path);
	}
}

xtBool xt_fs_rmdir(XTThreadPtr self, char *name)
{
	char path[PATH_MAX];

	xt_strcpy(PATH_MAX, path, name);
	xt_remove_dir_char(path);

#ifdef XT_WIN
	if (!RemoveDirectory(path)) {
		int err = fs_get_win_error();

		if (!XT_FILE_NOT_FOUND(err)) {
			xt_throw_ferrno(XT_CONTEXT, err, path);
			return FAILED;
		}
	}
#else
	if (rmdir(path) == -1) {
		int err = errno;

		if (err != ENOENT) {
			xt_throw_ferrno(XT_CONTEXT, err, path);
			return FAILED;
		}
	}
#endif
	return OK;
}

/* ----------------------------------------------------------------------
 * Open & Close operations
 */

xtPublic XTFilePtr xt_fs_get_file(XTThreadPtr self, char *file_name)
{
	XTFilePtr	file_ptr, *file_pptr;

	xt_sl_lock(self, fs_globals.fsg_open_files);
	pushr_(xt_sl_unlock, fs_globals.fsg_open_files);

	if ((file_pptr = (XTFilePtr *) xt_sl_find(self, fs_globals.fsg_open_files, file_name)))
		file_ptr = *file_pptr;
	else {
		file_ptr = fs_new_file(self, file_name);
		xt_sl_insert(self, fs_globals.fsg_open_files, file_name, &file_ptr);
	}
	file_ptr->fil_ref_count++;
	freer_(); // xt_sl_unlock(fs_globals.fsg_open_files)
	return file_ptr;
}

xtPublic void xt_fs_release_file(XTThreadPtr self, XTFilePtr file_ptr)
{
	xt_sl_lock(self, fs_globals.fsg_open_files);
	pushr_(xt_sl_unlock, fs_globals.fsg_open_files);

	file_ptr->fil_ref_count--;
	if (!file_ptr->fil_ref_count) {
		xt_sl_delete(self, fs_globals.fsg_open_files, file_ptr->fil_path);
	}

	freer_(); // xt_ht_unlock(fs_globals.fsg_open_files)
}

static xtBool fs_open_file(XTThreadPtr self, XT_FD *fd, XTFilePtr file, int mode)
{
	int retried = FALSE;

#ifdef DEBUG_TRACE_FILES
	PRINTF("%s: OPEN FILE: (%d) %s\n", self->t_name, (int) file->fil_id, xt_last_2_names_of_path(file->fil_path));
#endif
	retry:
#ifdef XT_WIN
	SECURITY_ATTRIBUTES	sa = { sizeof(SECURITY_ATTRIBUTES), 0, 0 };
	DWORD				flags;

	if (mode & XT_FS_EXCLUSIVE)
		flags = CREATE_NEW;
	else if (mode & XT_FS_CREATE)
		flags = OPEN_ALWAYS;
	else
		flags = OPEN_EXISTING;

	*fd = CreateFile(
		file->fil_path,
		mode & XT_FS_READONLY ? GENERIC_READ : (GENERIC_READ | GENERIC_WRITE),
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		&sa,
		flags,
		FILE_FLAG_RANDOM_ACCESS,
		NULL);
	if (*fd == INVALID_HANDLE_VALUE) {
		int err = fs_get_win_error();

		if (!(mode & XT_FS_MISSING_OK) || !XT_FILE_NOT_FOUND(err)) {
			if (!retried && (mode & XT_FS_MAKE_PATH) && XT_FILE_NOT_FOUND(err)) {
				char path[PATH_MAX];

				xt_strcpy(PATH_MAX, path, file->fil_path);
				xt_remove_last_name_of_path(path);
				xt_fs_mkpath(self, path);
				retried = TRUE;
				goto retry;
			}

			xt_throw_ferrno(XT_CONTEXT, err, file->fil_path);
		}

		/* File is missing, but don't throw an error. */
		return FAILED;
	}
	//PRINTF("open %d %s\n", *fd, file->fil_path);
	return OK;
#else
	int flags = 0;

	if (mode & XT_FS_READONLY)
		flags = O_RDONLY;
	else
		flags = O_RDWR;
	if (mode & XT_FS_CREATE)
		flags |= O_CREAT;
	if (mode & XT_FS_EXCLUSIVE)
		flags |= O_EXCL;
#ifdef O_DIRECT
	if (mode & XT_FS_DIRECT_IO)
		flags |= O_DIRECT;
#endif

	*fd = open(file->fil_path, flags, XT_MASK);
	if (*fd == -1) {
		int err = errno;

		if (!(mode & XT_FS_MISSING_OK) || err != ENOENT) {
			if (!retried && (mode & XT_FS_MAKE_PATH) && err == ENOENT) {
				char path[PATH_MAX];

				xt_strcpy(PATH_MAX, path, file->fil_path);
				xt_remove_last_name_of_path(path);
				xt_fs_mkpath(self, path);
				retried = TRUE;
				goto retry;
			}

			xt_throw_ferrno(XT_CONTEXT, err, file->fil_path);
		}

		/* File is missing, but don't throw an error. */
		return FAILED;
	}
	///PRINTF("open %d %s\n", *fd, file->fil_path);
	return OK;
#endif
}

xtPublic XTOpenFilePtr xt_open_file(XTThreadPtr self, char *file, int mode)
{
	XTOpenFilePtr	of;

	pushsr_(of, xt_close_file, (XTOpenFilePtr) xt_calloc(self, sizeof(XTOpenFileRec)));
	of->fr_file = xt_fs_get_file(self, file);
	of->fr_id = of->fr_file->fil_id;
	of->of_filedes = XT_NULL_FD;

#ifdef XT_WIN
	if (!fs_open_file(self, &of->of_filedes, of->fr_file, mode)) {
		xt_close_file(self, of);
		of = NULL;
	}
#else
	xtBool failed = FALSE;

	if (of->fr_file->fil_filedes == -1) {
		xt_sl_lock(self, fs_globals.fsg_open_files);
		pushr_(xt_sl_unlock, fs_globals.fsg_open_files);
		if (of->fr_file->fil_filedes == -1) {
			if (!fs_open_file(self, &of->fr_file->fil_filedes, of->fr_file, mode))
				failed = TRUE;
		}
		freer_(); // xt_ht_unlock(fs_globals.fsg_open_files)
	}

	if (failed) {
		/* Close, but after we have release the fsg_open_files lock! */
		xt_close_file(self, of);
		of = NULL;
	}
	else
		of->of_filedes = of->fr_file->fil_filedes;
#endif

	popr_(); // Discard xt_close_file(of)
	return of;
}

xtPublic XTOpenFilePtr xt_open_file_ns(char *file, int mode)
{
	XTThreadPtr		self = xt_get_self();
	XTOpenFilePtr	of;

	try_(a) {
		of = xt_open_file(self, file, mode);
	}
	catch_(a) {
		of = NULL;
	}
	cont_(a);
	return of;
}

xtPublic xtBool xt_open_file_ns(XTOpenFilePtr *fh, char *file, int mode)
{
	XTThreadPtr		self = xt_get_self();
	xtBool			ok = TRUE;

	try_(a) {
		*fh = xt_open_file(self, file, mode);
	}
	catch_(a) {
		ok = FALSE;
	}
	cont_(a);
	return ok;
}

xtPublic void xt_close_file(XTThreadPtr self, XTOpenFilePtr of)
{
	if (of->of_filedes != XT_NULL_FD) {
#ifdef XT_WIN
		CloseHandle(of->of_filedes);
#ifdef DEBUG_TRACE_FILES
		PRINTF("%s: close file: (%d) %s\n", self->t_name, (int) of->fr_file->fil_id, xt_last_2_names_of_path(of->fr_file->fil_path));
#endif
#else
		if (!of->fr_file || of->of_filedes != of->fr_file->fil_filedes) {
			close(of->of_filedes);
#ifdef DEBUG_TRACE_FILES
			PRINTF("%s: close file: (%d) %s\n", self->t_name, (int) of->fr_file->fil_id, xt_last_2_names_of_path(of->fr_file->fil_path));
#endif
		}
#endif

		of->of_filedes = XT_NULL_FD;
	}

	if (of->fr_file) {
		xt_fs_release_file(self, of->fr_file);
		of->fr_file = NULL;
	}
	xt_free(self, of);
}

xtPublic xtBool xt_close_file_ns(XTOpenFilePtr of)
{
	XTThreadPtr self = xt_get_self();
	xtBool		failed = FALSE;

	try_(a) {
		xt_close_file(self, of);
	}
	catch_(a) {
		failed = TRUE;
	}
	cont_(a);
	return failed;
}

/* ----------------------------------------------------------------------
 * I/O operations
 */

xtPublic xtBool xt_lock_file(struct XTThread *self, XTOpenFilePtr of)
{
#ifdef XT_WIN
	if (!LockFile(of->of_filedes, 0, 0, 512, 0)) {
		int err = fs_get_win_error();
		
		if (err == ERROR_LOCK_VIOLATION ||
			err == ERROR_LOCK_FAILED)
			return FAILED;
		
		xt_throw_ferrno(XT_CONTEXT, err, xt_file_path(of));
		return FAILED;
	}
	return OK;
#else
	if (lockf(of->of_filedes, F_TLOCK, 0) == 0)
		return OK;
	if (errno == EAGAIN)
		return FAILED;
	xt_throw_ferrno(XT_CONTEXT, errno, xt_file_path(of));
	return FAILED;
#endif
}

xtPublic void xt_unlock_file(struct XTThread *self, XTOpenFilePtr of)
{
#ifdef XT_WIN
	if (!UnlockFile(of->of_filedes, 0, 0, 512, 0)) {
		int err = fs_get_win_error();
		
		if (err != ERROR_NOT_LOCKED)
			xt_throw_ferrno(XT_CONTEXT, err, xt_file_path(of));
	}
#else
	if (lockf(of->of_filedes, F_ULOCK, 0) == -1)
		xt_throw_ferrno(XT_CONTEXT, errno, xt_file_path(of));
#endif
}

static off_t fs_seek_eof(XTThreadPtr self, XT_FD fd, XTFilePtr file)
{
#ifdef XT_WIN
	DWORD			result;
	LARGE_INTEGER	lpFileSize;

	result = SetFilePointer(fd, 0, NULL, FILE_END);
	if (result == 0xFFFFFFFF) {
		xt_throw_ferrno(XT_CONTEXT, fs_get_win_error(), file->fil_path);
		return (off_t) -1;
	}

	if (!GetFileSizeEx(fd, &lpFileSize)) {
		xt_throw_ferrno(XT_CONTEXT, fs_get_win_error(), file->fil_path);
		return (off_t) -1;
	}

	return lpFileSize.QuadPart;
#else
	off_t off;

	off = lseek(fd, 0, SEEK_END);
	if (off == -1) {
		xt_throw_ferrno(XT_CONTEXT, errno, file->fil_path);
		return -1;
	}

     return off;
#endif
}

xtPublic off_t xt_seek_eof_file(XTThreadPtr self, XTOpenFilePtr of)
{
	return fs_seek_eof(self, of->of_filedes, of->fr_file);
}

xtPublic xtBool xt_set_eof_file(XTThreadPtr self, XTOpenFilePtr of, off_t offset)
{
#ifdef XT_WIN
	LARGE_INTEGER liDistanceToMove;
	
	liDistanceToMove.QuadPart = offset;
	if (!SetFilePointerEx(of->of_filedes, liDistanceToMove, NULL, FILE_BEGIN)) {
		xt_throw_ferrno(XT_CONTEXT, fs_get_win_error(), xt_file_path(of));
		return FAILED;
	}

	if (!SetEndOfFile(of->of_filedes)) {
		xt_throw_ferrno(XT_CONTEXT, fs_get_win_error(), xt_file_path(of));
		return FAILED;
	}
#else
	if (ftruncate(of->of_filedes, offset) == -1) {
		xt_throw_ferrno(XT_CONTEXT, errno, xt_file_path(of));
		return FAILED;
	}
#endif
	return OK;
}

xtPublic xtBool xt_pwrite_file(XTOpenFilePtr of, off_t offset, size_t size, void *data, XTIOStatsPtr stat, XTThreadPtr XT_UNUSED(thread))
{
#ifdef DEBUG_PRINT_IO
	PRINTF("PBXT WRITE %s offs=%d size=%d\n", of->fr_file->fil_path, (int) offset, (int) size);
#endif
#ifdef DEBUG_TRACE_IO
	char	timef[50];
	xtWord8	start = xt_trace_clock();
#endif
#ifdef XT_WIN
	LARGE_INTEGER	liDistanceToMove;
	DWORD			result;
	
	liDistanceToMove.QuadPart = offset;
	if (!SetFilePointerEx(of->of_filedes, liDistanceToMove, NULL, FILE_BEGIN))
		return xt_register_ferrno(XT_REG_CONTEXT, fs_get_win_error(), xt_file_path(of));

	if (!WriteFile(of->of_filedes, data, size, &result, NULL))
		return xt_register_ferrno(XT_REG_CONTEXT, fs_get_win_error(), xt_file_path(of));

	if (result != size)
		return xt_register_ferrno(XT_REG_CONTEXT, ERROR_HANDLE_EOF, xt_file_path(of));
#else
	ssize_t write_size;

	write_size = pwrite(of->of_filedes, data, size, offset);
	if (write_size == -1)
		return xt_register_ferrno(XT_REG_CONTEXT, errno, xt_file_path(of));

	if ((size_t) write_size != size)
		return xt_register_ferrno(XT_REG_CONTEXT, ESPIPE, xt_file_path(of));

#endif
	stat->ts_write += (u_int) size;

#ifdef DEBUG_TRACE_IO
	xt_trace("/* %s */ pbxt_file_writ(\"%s\", %lu, %lu);\n", xt_trace_clock_diff(timef, start), of->fr_file->fil_path, (u_long) offset, (u_long) size);
#endif
	return OK;
}

xtPublic xtBool xt_flush_file(XTOpenFilePtr of, XTIOStatsPtr stat, XTThreadPtr XT_UNUSED(thread))
{
	xtWord8 s;

#ifdef DEBUG_PRINT_IO
	PRINTF("PBXT FLUSH %s\n", of->fr_file->fil_path);
#endif
#ifdef DEBUG_TRACE_IO
	char	timef[50];
	xtWord8	start = xt_trace_clock();
#endif
	stat->ts_flush_start = xt_trace_clock();
#ifdef XT_WIN
	if (!FlushFileBuffers(of->of_filedes)) {
		xt_register_ferrno(XT_REG_CONTEXT, fs_get_win_error(), xt_file_path(of));
		goto failed;
	}
#else
	if (fsync(of->of_filedes) == -1) {
		xt_register_ferrno(XT_REG_CONTEXT, errno, xt_file_path(of));
		goto failed;
	}
#endif
#ifdef DEBUG_TRACE_IO
	xt_trace("/* %s */ pbxt_file_sync(\"%s\");\n", xt_trace_clock_diff(timef, start), of->fr_file->fil_path);
#endif
	s = stat->ts_flush_start;
	stat->ts_flush_start = 0;
	stat->ts_flush_time += xt_trace_clock() - s;
	stat->ts_flush++;
	return OK;

	failed:
	s = stat->ts_flush_start;
	stat->ts_flush_start = 0;
	stat->ts_flush_time += xt_trace_clock() - s;
	return FAILED;
}

xtBool xt_pread_file(XTOpenFilePtr of, off_t offset, size_t size, size_t min_size, void *data, size_t *red_size, XTIOStatsPtr stat, XTThreadPtr XT_UNUSED(thread))
{
#ifdef DEBUG_PRINT_IO
	PRINTF("PBXT READ %s offset=%d size=%d\n", of->fr_file->fil_path, (int) offset, (int) size);
#endif
#ifdef DEBUG_TRACE_IO
	char	timef[50];
	xtWord8	start = xt_trace_clock();
#endif
#ifdef XT_WIN
	LARGE_INTEGER	liDistanceToMove;
	DWORD			result;

	liDistanceToMove.QuadPart = offset;
	if (!SetFilePointerEx(of->of_filedes, liDistanceToMove, NULL, FILE_BEGIN))
		return xt_register_ferrno(XT_REG_CONTEXT, fs_get_win_error(), xt_file_path(of));

	if (!ReadFile(of->of_filedes, data, size, &result, NULL))
		return xt_register_ferrno(XT_REG_CONTEXT, fs_get_win_error(), xt_file_path(of));

	if ((size_t) result < min_size)
		return xt_register_ferrno(XT_REG_CONTEXT, ERROR_HANDLE_EOF, xt_file_path(of));

	if (red_size)
		*red_size = (size_t) result;
	stat->ts_read += (u_int) result;
#else
	ssize_t read_size;

	read_size = pread(of->of_filedes, data, size, offset);
	if (read_size == -1)
		return xt_register_ferrno(XT_REG_CONTEXT, errno, xt_file_path(of));

	/* Throw an error if read less than the minimum: */
	if ((size_t) read_size < min_size) {
//PRINTF("PMC PBXT <-- offset:%llu, count:%lu \n", (u_llong) offset, (u_long) size);
		return xt_register_ferrno(XT_REG_CONTEXT, ESPIPE, xt_file_path(of));
	}

	if (red_size)
		*red_size = (size_t) read_size;
	stat->ts_read += (u_int) read_size;
#endif
#ifdef DEBUG_TRACE_IO
	xt_trace("/* %s */ pbxt_file_read(\"%s\", %lu, %lu);\n", xt_trace_clock_diff(timef, start), of->fr_file->fil_path, (u_long) offset, (u_long) size);
#endif
	return OK;
}

xtPublic xtBool xt_lock_file_ptr(XTOpenFilePtr of, xtWord1 **data, off_t offset, size_t size, XTIOStatsPtr stat, XTThreadPtr thread)
{
	size_t red_size;

	if (!*data) {
		if (!(*data = (xtWord1 *) xt_malloc_ns(size)))
			return FAILED;
	}

	if (!xt_pread_file(of, offset, size, 0, *data, &red_size, stat, thread))
		return FAILED;
	
	//if (red_size < size)
	//	memset();
	return OK;
}

xtPublic void xt_unlock_file_ptr(XTOpenFilePtr XT_UNUSED(of), xtWord1 *data, XTThreadPtr XT_UNUSED(thread))
{
	if (data)
		xt_free_ns(data);
}

/* ----------------------------------------------------------------------
 * Directory operations
 */

/*
 * The filter may contain one '*' as wildcard.
 */
XTOpenDirPtr xt_dir_open(XTThreadPtr self, c_char *path, c_char *filter)
{
	XTOpenDirPtr	od;

#ifdef XT_SOLARIS
	/* see the comment in filesys_xt.h */
	size_t sz = pathconf(path, _PC_NAME_MAX) + sizeof(XTOpenDirRec) + 1;
#else
	size_t sz = sizeof(XTOpenDirRec);
#endif
	pushsr_(od, xt_dir_close, (XTOpenDirPtr) xt_calloc(self, sz));

#ifdef XT_WIN
	size_t			len;

	od->od_handle = XT_NULL_FD;

	// path = path\(filter | *)
	len = strlen(path) + 1 + (filter ? strlen(filter) : 1) + 1;
	od->od_path = (char *) xt_malloc(self, len);

	strcpy(od->od_path, path);
	xt_add_dir_char(len, od->od_path);
	if (filter)
		strcat(od->od_path, filter);
	else
		strcat(od->od_path, "*");
#else
	od->od_path = xt_dup_string(self, path);

	if (filter)
		od->od_filter = xt_dup_string(self, filter);

	od->od_dir = opendir(path);
	if (!od->od_dir)
		xt_throw_ferrno(XT_CONTEXT, errno, path);
#endif
	popr_(); // Discard xt_dir_close(od)
	return od;
}

void xt_dir_close(XTThreadPtr self, XTOpenDirPtr od)
{
	if (od) {
#ifdef XT_WIN
		if (od->od_handle != XT_NULL_FD) {
			FindClose(od->od_handle);
			od->od_handle = XT_NULL_FD;
		}
#else
		if (od->od_dir) {
			closedir(od->od_dir);
			od->od_dir = NULL;
		}
		if (od->od_filter) {
			xt_free(self, od->od_filter);
			od->od_filter = NULL;
		}
#endif
		if (od->od_path) {
			xt_free(self, od->od_path);
			od->od_path = NULL;
		}
		xt_free(self, od);
	}
}

#ifdef XT_WIN
xtBool xt_dir_next(XTThreadPtr self, XTOpenDirPtr od)
{
	int err = 0;

	if (od->od_handle == INVALID_HANDLE_VALUE) {
		od->od_handle = FindFirstFile(od->od_path, &od->od_data);
		if (od->od_handle == INVALID_HANDLE_VALUE)
			err = fs_get_win_error();
	}
	else {
		if (!FindNextFile(od->od_handle, &od->od_data))
			err = fs_get_win_error();
	}

	if (err) {
		if (err != ERROR_NO_MORE_FILES) {
			if (err == ERROR_FILE_NOT_FOUND) {
				char path[PATH_MAX];

				xt_strcpy(PATH_MAX, path, od->od_path);
				xt_remove_last_name_of_path(path);
				if (!xt_fs_exists(path))
					xt_throw_ferrno(XT_CONTEXT, err, path);
			}
			else
				xt_throw_ferrno(XT_CONTEXT, err, od->od_path);
		}
		return FAILED;
	}

	return OK;
}
#else
static xtBool fs_match_filter(c_char *name, c_char *filter)
{
	while (*name && *filter) {
		if (*filter == '*') {
			if (filter[1] == *name)
				filter++;
			else
				name++;
		}
		else {
			if (*name != *filter)
				return FALSE;
			name++;
			filter++;
		}
	}
	if (!*name) {
		if (!*filter || (*filter == '*' && !filter[1]))
			return TRUE;
	}
	return FALSE;
}

xtBool xt_dir_next(XTThreadPtr self, XTOpenDirPtr od)
{
	int				err;
	struct dirent	*result;

	for (;;) {
		err = readdir_r(od->od_dir, &od->od_entry, &result);
		if (err) {
			xt_throw_ferrno(XT_CONTEXT, err, od->od_path);
			return FAILED;
		}
		if (!result)
			break;
		/* Filter out '.' and '..': */
		if (od->od_entry.d_name[0] == '.') {
			if (od->od_entry.d_name[1] == '.') {
				if (od->od_entry.d_name[2] == '\0')
					continue;
			}
			else {
				if (od->od_entry.d_name[1] == '\0')
					continue;
			}
		}
		if (!od->od_filter)
			break;
		if (fs_match_filter(od->od_entry.d_name, od->od_filter))
			break;
	}
	return result ? TRUE : FALSE;
}
#endif

char *xt_dir_name(XTThreadPtr XT_UNUSED(self), XTOpenDirPtr od)
{
#ifdef XT_WIN
	return od->od_data.cFileName;
#else
	return od->od_entry.d_name;
#endif
}

xtBool xt_dir_is_file(XTThreadPtr self, XTOpenDirPtr od)
{
	(void) self;
#ifdef XT_WIN
	if (od->od_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		return FALSE;
#elif defined(XT_SOLARIS)
        char path[PATH_MAX];
	struct stat sb;

	xt_strcpy(PATH_MAX, path, od->od_path);
	xt_add_dir_char(PATH_MAX, path);
	xt_strcat(PATH_MAX, path, od->od_entry.d_name);

	if (stat(path, &sb) == -1) {
		xt_throw_ferrno(XT_CONTEXT, errno, path);
		return FAILED;
	}

	if ( sb.st_mode & S_IFDIR )
		return FALSE;
#else
	if (od->od_entry.d_type & DT_DIR)
		return FALSE;
#endif
	return TRUE;
}

off_t xt_dir_file_size(XTThreadPtr self, XTOpenDirPtr od)
{
#ifdef XT_WIN
	return (off_t) od->od_data.nFileSizeLow | (((off_t) od->od_data.nFileSizeHigh) << 32);
#else
	char	path[PATH_MAX];
	off_t	size;

	xt_strcpy(PATH_MAX, path, od->od_path);
	xt_add_dir_char(PATH_MAX, path);
	xt_strcat(PATH_MAX, path, od->od_entry.d_name);
	if (!xt_fs_stat(self, path, &size, NULL))
		return -1;
	return size;
#endif
}

/* ----------------------------------------------------------------------
 * File mapping operations
 */

static xtBool fs_map_file(XTFileMemMapPtr mm, XTFilePtr file, xtBool grow)
{
	ASSERT_NS(!mm->mm_start);
#ifdef XT_WIN
	/* This will grow the file to the given size: */
	mm->mm_mapdes = CreateFileMapping(file->fil_filedes, NULL, PAGE_READWRITE, (DWORD) (mm->mm_length >> 32), (DWORD) mm->mm_length, NULL);
	if (mm->mm_mapdes == NULL) {
		xt_register_ferrno(XT_REG_CONTEXT, fs_get_win_error(), file->fil_path);
		return FAILED;
	}

	mm->mm_start = (xtWord1 *) MapViewOfFile(mm->mm_mapdes, FILE_MAP_WRITE, 0, 0, 0);
	if (!mm->mm_start) {
		CloseHandle(mm->mm_mapdes);
		mm->mm_mapdes = NULL;
		xt_register_ferrno(XT_REG_CONTEXT, fs_get_win_error(), file->fil_path);
		return FAILED;
	}
#else
	if (grow) {
		char data[2];

		if (pwrite(file->fil_filedes, data, 1, mm->mm_length - 1) == -1) {
			xt_register_ferrno(XT_REG_CONTEXT, errno, file->fil_path);
			return FAILED;
		}
	}

	/* Remap: */
	mm->mm_start = (xtWord1 *) mmap(0, (size_t) mm->mm_length, PROT_READ | PROT_WRITE, MAP_SHARED, file->fil_filedes, 0);
	if (mm->mm_start == MAP_FAILED) {
		mm->mm_start = NULL;
		xt_register_ferrno(XT_REG_CONTEXT, errno, file->fil_path);
		return FAILED;
	}
#endif
	return OK;
}

xtPublic XTMapFilePtr xt_open_fmap(XTThreadPtr self, char *file, size_t grow_size)
{
	XTMapFilePtr	map;

	pushsr_(map, xt_close_fmap, (XTMapFilePtr) xt_calloc(self, sizeof(XTMapFileRec)));
	map->fr_file = xt_fs_get_file(self, file);
	map->fr_id = map->fr_file->fil_id;

	xt_sl_lock(self, fs_globals.fsg_open_files);
	pushr_(xt_sl_unlock, fs_globals.fsg_open_files);

	if (map->fr_file->fil_filedes == XT_NULL_FD) {
		if (!fs_open_file(self, &map->fr_file->fil_filedes, map->fr_file, XT_FS_DEFAULT)) {
			xt_close_fmap(self, map);
			map = NULL;
		}
	}

	map->fr_file->fil_handle_count++;

	freer_(); // xt_ht_unlock(fs_globals.fsg_open_files)

	if (!map->fr_file->fil_memmap) {
		xt_sl_lock(self, fs_globals.fsg_open_files);
		pushr_(xt_sl_unlock, fs_globals.fsg_open_files);
		if (!map->fr_file->fil_memmap) {
			XTFileMemMapPtr mm;

			mm = (XTFileMemMapPtr) xt_calloc(self, sizeof(XTFileMemMapRec));
			pushr_(fs_close_fmap, mm);

#ifdef XT_WIN
			/* NULL is the value returned on error! */
			mm->mm_mapdes = NULL;
#endif
			FILE_MAP_INIT_LOCK(self, &mm->mm_lock);
			mm->mm_length = fs_seek_eof(self, map->fr_file->fil_filedes, map->fr_file);
			if (sizeof(size_t) == 4 && mm->mm_length >= (off_t) 0xFFFFFFFF)
				xt_throw_ixterr(XT_CONTEXT, XT_ERR_FILE_TOO_LONG, map->fr_file->fil_path);
			mm->mm_grow_size = grow_size;

			if (mm->mm_length < (off_t) grow_size) {
				mm->mm_length = (off_t) grow_size;
				if (!fs_map_file(mm, map->fr_file, TRUE))
					xt_throw(self);
			}
			else {
				if (!fs_map_file(mm, map->fr_file, FALSE))
					xt_throw(self);
			}

			popr_(); // Discard fs_close_fmap(mm)
			map->fr_file->fil_memmap = mm;
		}
		freer_(); // xt_ht_unlock(fs_globals.fsg_open_files)
	}
	map->mf_memmap = map->fr_file->fil_memmap;

	popr_(); // Discard xt_close_fmap(map)
	return map;
}

xtPublic void xt_close_fmap(XTThreadPtr self, XTMapFilePtr map)
{
	ASSERT_NS(!map->mf_slock_count);
	if (map->fr_file) {
		xt_sl_lock(self, fs_globals.fsg_open_files);
		pushr_(xt_sl_unlock, fs_globals.fsg_open_files);		
		map->fr_file->fil_handle_count--;
		if (!map->fr_file->fil_handle_count) {
			fs_close_fmap(self, map->fr_file->fil_memmap);
			map->fr_file->fil_memmap = NULL;
		}
		freer_();
		
		xt_fs_release_file(self, map->fr_file);
		map->fr_file = NULL;
	}
	map->mf_memmap = NULL;
	xt_free(self, map);
}

xtPublic xtBool xt_close_fmap_ns(XTMapFilePtr map)
{
	XTThreadPtr self = xt_get_self();
	xtBool		failed = FALSE;

	try_(a) {
		xt_close_fmap(self, map);
	}
	catch_(a) {
		failed = TRUE;
	}
	cont_(a);
	return failed;
}

static xtBool fs_remap_file(XTMapFilePtr map, off_t offset, size_t size, XTIOStatsPtr stat)
{
	off_t			new_size = 0;
	XTFileMemMapPtr	mm = map->mf_memmap;
	xtWord8			s;

	if (offset + (off_t) size > mm->mm_length) {
		/* Expand the file: */
		new_size = (mm->mm_length + (off_t) mm->mm_grow_size) / (off_t) mm->mm_grow_size;
		new_size *= mm->mm_grow_size;
		while (new_size < offset + (off_t) size)
			new_size += mm->mm_grow_size;

		if (sizeof(size_t) == 4 && new_size >= (off_t) 0xFFFFFFFF) {
			xt_register_ixterr(XT_REG_CONTEXT, XT_ERR_FILE_TOO_LONG, xt_file_path(map));
			return FAILED;
		}
	}
	else if (!mm->mm_start)
		new_size = mm->mm_length;

	if (new_size) {
		if (mm->mm_start) {
			/* Flush & unmap: */
			stat->ts_flush_start = xt_trace_clock();
#ifdef XT_WIN
			if (!FlushViewOfFile(mm->mm_start, 0)) {
				xt_register_ferrno(XT_REG_CONTEXT, fs_get_win_error(), xt_file_path(map));
				goto failed;
			}

			if (!UnmapViewOfFile(mm->mm_start)) {
				xt_register_ferrno(XT_REG_CONTEXT, fs_get_win_error(), xt_file_path(map));
				goto failed;
			}
#else
			if (msync( (char *)mm->mm_start, (size_t) mm->mm_length, MS_SYNC) == -1) {
				xt_register_ferrno(XT_REG_CONTEXT, errno, xt_file_path(map));
				goto failed;
			}

			/* Unmap: */
			if (munmap((caddr_t) mm->mm_start, (size_t) mm->mm_length) == -1) {
				xt_register_ferrno(XT_REG_CONTEXT, errno, xt_file_path(map));
				goto failed;
			}
#endif
			s = stat->ts_flush_start;
			stat->ts_flush_start = 0;
			stat->ts_flush_time += xt_trace_clock() - s;
			stat->ts_flush++;
		}
		mm->mm_start = NULL;
#ifdef XT_WIN
		if (!CloseHandle(mm->mm_mapdes))
			return xt_register_ferrno(XT_REG_CONTEXT, fs_get_win_error(), xt_file_path(map));
		mm->mm_mapdes = NULL;
#endif
		mm->mm_length = new_size;

		if (!fs_map_file(mm, map->fr_file, TRUE))
			return FAILED;
	}
	return OK;
	
	failed:
	s = stat->ts_flush_start;
	stat->ts_flush_start = 0;
	stat->ts_flush_time += xt_trace_clock() - s;
	return FAILED;
}

xtPublic xtBool xt_pwrite_fmap(XTMapFilePtr map, off_t offset, size_t size, void *data, XTIOStatsPtr stat, XTThreadPtr thread)
{
	XTFileMemMapPtr mm = map->mf_memmap;
#ifndef FILE_MAP_USE_PTHREAD_RW
	xtThreadID		thd_id = thread->t_id;
#endif

#ifdef DEBUG_TRACE_MAP_IO
	xt_trace("/* %s */ pbxt_fmap_writ(\"%s\", %lu, %lu);\n", xt_trace_clock_diff(NULL), map->fr_file->fil_path, (u_long) offset, (u_long) size);
#endif
	ASSERT_NS(!map->mf_slock_count);
	FILE_MAP_READ_LOCK(&mm->mm_lock, thd_id);
	if (!mm->mm_start || offset + (off_t) size > mm->mm_length) {
		FILE_MAP_UNLOCK(&mm->mm_lock, thd_id);

		FILE_MAP_WRITE_LOCK(&mm->mm_lock, thd_id);
		if (!fs_remap_file(map, offset, size, stat))
			goto failed;
	}

#ifdef XT_WIN
	__try
	{
		memcpy(mm->mm_start + offset, data, size);
	}
	// GetExceptionCode()== EXCEPTION_IN_PAGE_ERROR ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		xt_register_ferrno(XT_REG_CONTEXT, GetExceptionCode(), xt_file_path(map));
		goto failed;
	}
#else
	memcpy(mm->mm_start + offset, data, size);
#endif

	FILE_MAP_UNLOCK(&mm->mm_lock, thd_id);
	stat->ts_write += size;
	return OK;

	failed:
	FILE_MAP_UNLOCK(&mm->mm_lock, thd_id);
	return FAILED;
}

xtPublic xtBool xt_pread_fmap_4(XTMapFilePtr map, off_t offset, xtWord4 *value, XTIOStatsPtr stat, XTThreadPtr thread)
{
	XTFileMemMapPtr	mm = map->mf_memmap;
#ifndef FILE_MAP_USE_PTHREAD_RW
	xtThreadID		thd_id = thread->t_id;
#endif

#ifdef DEBUG_TRACE_MAP_IO
	xt_trace("/* %s */ pbxt_fmap_read_4(\"%s\", %lu, 4);\n", xt_trace_clock_diff(NULL), map->fr_file->fil_path, (u_long) offset);
#endif
	if (!map->mf_slock_count)
		FILE_MAP_READ_LOCK(&mm->mm_lock, thd_id);
	if (!mm->mm_start) {
		FILE_MAP_UNLOCK(&mm->mm_lock, thd_id);
		FILE_MAP_WRITE_LOCK(&mm->mm_lock, thd_id);
		if (!fs_remap_file(map, 0, 0, stat)) {
			FILE_MAP_UNLOCK(&mm->mm_lock, thd_id);
			return FAILED;
		}
	}
	if (offset >= mm->mm_length)
		*value = 0;
	else {
		xtWord1 *data;

		data = mm->mm_start + offset;
#ifdef XT_WIN
		__try
		{
			*value = XT_GET_DISK_4(data);
			// GetExceptionCode()== EXCEPTION_IN_PAGE_ERROR ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH
		}
		__except(EXCEPTION_EXECUTE_HANDLER)
		{
			FILE_MAP_UNLOCK(&mm->mm_lock, thd_id);
			return xt_register_ferrno(XT_REG_CONTEXT, GetExceptionCode(), xt_file_path(map));
		}
#else
		*value = XT_GET_DISK_4(data);
#endif
	}

	if (!map->mf_slock_count)
		FILE_MAP_UNLOCK(&mm->mm_lock, thd_id);
	stat->ts_read += 4;
	return OK;
}

xtPublic xtBool xt_pread_fmap(XTMapFilePtr map, off_t offset, size_t size, size_t min_size, void *data, size_t *red_size, XTIOStatsPtr stat, XTThreadPtr thread)
{
	XTFileMemMapPtr	mm = map->mf_memmap;
#ifndef FILE_MAP_USE_PTHREAD_RW
	xtThreadID		thd_id = thread->t_id;
#endif
	size_t			tfer;

#ifdef DEBUG_TRACE_MAP_IO
	xt_trace("/* %s */ pbxt_fmap_read(\"%s\", %lu, %lu);\n", xt_trace_clock_diff(NULL), map->fr_file->fil_path, (u_long) offset, (u_long) size);
#endif
	/* NOTE!! The file map may already be locked,
	 * by a call to xt_lock_fmap_ptr()!
	 *
	 * 20.05.2009: This problem should be fixed now with mf_slock_count!
	 *
	 * This can occur during a sequential scan:
	 * xt_pread_fmap()  Line 1330
	 * XTTabCache::tc_read_direct()  Line 361
	 * XTTabCache::xt_tc_read()  Line 220
	 * xt_tab_get_rec_data()
	 * tab_visible()  Line 2412
	 * xt_tab_seq_next()  Line 4068
	 *
	 * And occurs during the following test:
	 * create table t1 ( a int not null, b int not null) ;
	 * --disable_query_log
	 * insert into t1 values (1,1),(2,2),(3,3),(4,4);
	 * let $1=19;
	 * set @d=4;
	 * while ($1)
	 * {
	 *   eval insert into t1 select a+@d,b+@d from t1;
	 *   eval set @d=@d*2;
	 *   dec $1;
	 * }
	 * 
	 * --enable_query_log
	 * alter table t1 add index i1(a);
	 * delete from t1 where a > 2000000;
	 * create table t2 like t1;
	 * insert into t2 select * from t1;
	 *
	 * As a result, the slock must be able to handle
	 * nested calls to lock/unlock.
	 */
	if (!map->mf_slock_count)
		FILE_MAP_READ_LOCK(&mm->mm_lock, thd_id);
	tfer = size;
	if (!mm->mm_start) {
		FILE_MAP_UNLOCK(&mm->mm_lock, thd_id);
		ASSERT_NS(!map->mf_slock_count);
		FILE_MAP_WRITE_LOCK(&mm->mm_lock, thd_id);
		if (!fs_remap_file(map, 0, 0, stat)) {
			if (!map->mf_slock_count)
				FILE_MAP_UNLOCK(&mm->mm_lock, thd_id);
			return FAILED;
		}
	}
	if (offset >= mm->mm_length)
		tfer = 0;
	else {
		if (mm->mm_length - offset < (off_t) tfer)
			tfer = (size_t) (mm->mm_length - offset);
#ifdef XT_WIN
		__try
		{
			memcpy(data, mm->mm_start + offset, tfer);
			// GetExceptionCode()== EXCEPTION_IN_PAGE_ERROR ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH
		}
		__except(EXCEPTION_EXECUTE_HANDLER)
		{
			if (!map->mf_slock_count)
				FILE_MAP_UNLOCK(&mm->mm_lock, thd_id);
			return xt_register_ferrno(XT_REG_CONTEXT, GetExceptionCode(), xt_file_path(map));
		}
#else
		memcpy(data, mm->mm_start + offset, tfer);
#endif
	}

	if (!map->mf_slock_count)
		FILE_MAP_UNLOCK(&mm->mm_lock, thd_id);
	if (tfer < min_size)
		return xt_register_ferrno(XT_REG_CONTEXT, ESPIPE, xt_file_path(map));

	if (red_size)
		*red_size = tfer;
	stat->ts_read += tfer;
	return OK;
}

xtPublic xtBool xt_flush_fmap(XTMapFilePtr map, XTIOStatsPtr stat, XTThreadPtr thread)
{
	XTFileMemMapPtr	mm = map->mf_memmap;
#ifndef FILE_MAP_USE_PTHREAD_RW
	xtThreadID		thd_id = thread->t_id;
#endif
	xtWord8			s;

#ifdef DEBUG_TRACE_MAP_IO
	xt_trace("/* %s */ pbxt_fmap_sync(\"%s\");\n", xt_trace_clock_diff(NULL), map->fr_file->fil_path);
#endif
	if (!map->mf_slock_count)
		FILE_MAP_READ_LOCK(&mm->mm_lock, thd_id);
	if (!mm->mm_start) {
		FILE_MAP_UNLOCK(&mm->mm_lock, thd_id);
		ASSERT_NS(!map->mf_slock_count);
		FILE_MAP_WRITE_LOCK(&mm->mm_lock, thd_id);
		if (!fs_remap_file(map, 0, 0, stat)) {
			if (!map->mf_slock_count)
				FILE_MAP_UNLOCK(&mm->mm_lock, thd_id);
			return FAILED;
		}
	}
	stat->ts_flush_start = xt_trace_clock();
#ifdef XT_WIN
	if (!FlushViewOfFile(mm->mm_start, 0)) {
		xt_register_ferrno(XT_REG_CONTEXT, fs_get_win_error(), xt_file_path(map));
		goto failed;
	}
#else
	if (msync( (char *)mm->mm_start, (size_t) mm->mm_length, MS_SYNC) == -1) {
		xt_register_ferrno(XT_REG_CONTEXT, errno, xt_file_path(map));
		goto failed;
	}
#endif
	if (!map->mf_slock_count)
		FILE_MAP_UNLOCK(&mm->mm_lock, thd_id);
	s = stat->ts_flush_start;
	stat->ts_flush_start = 0;
	stat->ts_flush_time += xt_trace_clock() - s;
	stat->ts_flush++;
	return OK;

	failed:
	if (!map->mf_slock_count)
		FILE_MAP_UNLOCK(&mm->mm_lock, thd_id);
	s = stat->ts_flush_start;
	stat->ts_flush_start = 0;
	stat->ts_flush_time += xt_trace_clock() - s;
	return FAILED;
}

xtPublic xtWord1 *xt_lock_fmap_ptr(XTMapFilePtr map, off_t offset, size_t size, XTIOStatsPtr stat, XTThreadPtr thread)
{
	XTFileMemMapPtr	mm = map->mf_memmap;
#ifndef FILE_MAP_USE_PTHREAD_RW
	xtThreadID		thd_id = thread->t_id;
#endif

	if (!map->mf_slock_count)
		FILE_MAP_READ_LOCK(&mm->mm_lock, thd_id);
	map->mf_slock_count++;
	if (!mm->mm_start) {
		FILE_MAP_UNLOCK(&mm->mm_lock, thd_id);
		FILE_MAP_WRITE_LOCK(&mm->mm_lock, thd_id);
		if (!fs_remap_file(map, 0, 0, stat))
			goto failed;
	}
	if (offset >= mm->mm_length)
		goto failed;
	
	if (offset + (off_t) size > mm->mm_length)
		stat->ts_read += (u_int) (offset + (off_t) size - mm->mm_length);
	else
		stat->ts_read += size;
	return mm->mm_start + offset;

	failed:
	map->mf_slock_count--;
	if (!map->mf_slock_count)
		FILE_MAP_UNLOCK(&mm->mm_lock, thd_id);
	return NULL;
}

xtPublic void xt_unlock_fmap_ptr(XTMapFilePtr map, XTThreadPtr thread)
{
	map->mf_slock_count--;
	if (!map->mf_slock_count)
		FILE_MAP_UNLOCK(&map->mf_memmap->mm_lock, thread->t_id);
}

/* ----------------------------------------------------------------------
 * Copy files/directories
 */

static void fs_copy_file(XTThreadPtr self, char *from_path, char *to_path, void *copy_buf)
{
	XTOpenFilePtr	from;
	XTOpenFilePtr	to;
	off_t			offset = 0;
	size_t			read_size= 0;

	from = xt_open_file(self, from_path, XT_FS_READONLY);
	pushr_(xt_close_file, from);
	to = xt_open_file(self, to_path, XT_FS_CREATE | XT_FS_MAKE_PATH);
	pushr_(xt_close_file, to);

	for (;;) {
		if (!xt_pread_file(from, offset, 16*1024, 0, copy_buf, &read_size, &self->st_statistics.st_x, self))
			xt_throw(self);
		if (!read_size)
			break;
		if (!xt_pwrite_file(to, offset, read_size, copy_buf, &self->st_statistics.st_x, self))
			xt_throw(self);
		offset += (off_t) read_size;
	}

	freer_();
	freer_();
}

xtPublic void xt_fs_copy_file(XTThreadPtr self, char *from_path, char *to_path)
{
	void *buffer;

	buffer = xt_malloc(self, 16*1024);
	pushr_(xt_free, buffer);
	fs_copy_file(self, from_path, to_path, buffer);
	freer_();
}

static void fs_copy_dir(XTThreadPtr self, char *from_path, char *to_path, void *copy_buf)
{
	XTOpenDirPtr	od;
	char			*file;
	
	xt_add_dir_char(PATH_MAX, from_path);
	xt_add_dir_char(PATH_MAX, to_path);

	pushsr_(od, xt_dir_close, xt_dir_open(self, from_path, NULL));
	while (xt_dir_next(self, od)) {
		file = xt_dir_name(self, od);
		if (*file == '.')
			continue;
#ifdef XT_WIN
		if (strcmp(file, "pbxt-lock") == 0)
			continue;
#endif
		xt_strcat(PATH_MAX, from_path, file);
		xt_strcat(PATH_MAX, to_path, file);
		if (xt_dir_is_file(self, od))
			fs_copy_file(self, from_path, to_path, copy_buf);
		else
			fs_copy_dir(self, from_path, to_path, copy_buf);
		xt_remove_last_name_of_path(from_path);
		xt_remove_last_name_of_path(to_path);
	}
	freer_();

	xt_remove_dir_char(from_path);
	xt_remove_dir_char(to_path);
}

xtPublic void xt_fs_copy_dir(XTThreadPtr self, const char *from, const char *to)
{
	void	*buffer;
	char	from_path[PATH_MAX];
	char	to_path[PATH_MAX];

	xt_strcpy(PATH_MAX, from_path, from);
	xt_strcpy(PATH_MAX, to_path, to);

	buffer = xt_malloc(self, 16*1024);
	pushr_(xt_free, buffer);
	fs_copy_dir(self, from_path, to_path, buffer);
	freer_();
}

