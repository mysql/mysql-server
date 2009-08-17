/* Copyright (c) 2007 PrimeBase Technologies GmbH
 *
 * PrimeBase Media Stream for MySQL
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
 * Paul McCullagh
 * H&G2JCtL
 *
 * 2007-06-01
 *
 * This file contains the BLOB streaming interface engines that
 * are streaming enabled.
 *
 */
#ifndef __streaming_unx_h__
#define __streaming_unx_h__

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <signal.h>
#include <ctype.h>

#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class implementation */
#endif

#define MS_SHARED_MEMORY_MAGIC			0x7E9A120C
#define MS_ENGINE_VERSION				1
#define MS_CALLBACK_VERSION				1
#define MS_SHARED_MEMORY_VERSION		1
#define MS_ENGINE_LIST_SIZE				80
#define MS_TEMP_FILE_PREFIX				"pbms_temp_"
#define MS_TEMP_FILE_PREFIX				"pbms_temp_"

#define MS_RESULT_MESSAGE_SIZE			300
#define MS_RESULT_STACK_SIZE			200

#define MS_BLOB_HANDLE_SIZE				300

#define SH_MASK							((S_IRUSR | S_IWUSR) | (S_IRGRP | S_IWGRP) | (S_IROTH))

#define MS_OK							0
#define MS_ERR_ENGINE					1							/* Internal engine error. */
#define MS_ERR_UNKNOWN_TABLE			2							/* Returned if the engine cannot open the given table. */
#define MS_ERR_NOT_FOUND				3							/* The BLOB cannot be found. */
#define MS_ERR_TABLE_LOCKED				4							/* Table is currently locked. */
#define MS_ERR_INCORRECT_URL			5
#define MS_ERR_AUTH_FAILED				6
#define MS_ERR_NOT_IMPLEMENTED			7
#define MS_ERR_UNKNOWN_DB				8
#define MS_ERR_REMOVING_REPO			9
#define MS_ERR_DATABASE_DELETED			10

#define MS_LOCK_NONE					0
#define MS_LOCK_READONLY				1
#define MS_LOCK_READ_WRITE				2

#define MS_XACT_NONE					0
#define MS_XACT_BEGIN					1
#define MS_XACT_COMMIT					2
#define MS_XACT_ROLLBACK				3

#define PBMS_ENGINE_REF_LEN				8
#define PBMS_BLOB_URL_SIZE				200

#define PBMS_FIELD_COL_SIZE				128
#define PBMS_FIELD_COND_SIZE			300


typedef struct PBMSBlobID {
	u_int64_t				bi_blob_size;	
	u_int64_t				bi_blob_id;				// or repo file offset if type = REPO
	u_int32_t				bi_tab_id;				// or repo ID if type = REPO
	u_int32_t				bi_auth_code;
	u_int32_t				bi_blob_type;
} PBMSBlobIDRec, *PBMSBlobIDPtr;

typedef struct PBMSResultRec {
	int						mr_code;								/* Engine specific error code. */ 
	char					mr_message[MS_RESULT_MESSAGE_SIZE];		/* Error message, required if non-zero return code. */
	char					mr_stack[MS_RESULT_STACK_SIZE];			/* Trace information about where the error occurred. */
} PBMSResultRec, *PBMSResultPtr;

typedef struct PBMSEngineRefRec {
	unsigned char			er_data[PBMS_ENGINE_REF_LEN];
} PBMSEngineRefRec, *PBMSEngineRefPtr;

typedef struct PBMSBlobURL {
	char					bu_data[PBMS_BLOB_URL_SIZE];
} PBMSBlobURLRec, *PBMSBlobURLPtr;

typedef struct PBMSFieldRef {
	char					fr_column[PBMS_FIELD_COL_SIZE];
	char					fr_cond[PBMS_FIELD_COND_SIZE];
} PBMSFieldRefRec, *PBMSFieldRefPtr;
/*
 * The engine must free its resources for the given thread.
 */
typedef void (*MSCloseConnFunc)(void *thd);

/* Before access BLOBs of a table, the streaming engine will open the table.
 * Open tables are managed as a pool by the streaming engine.
 * When a request is received, the streaming engine will ask all
 * registered engine to open the table. The engine must return a NULL
 * open_table pointer if it does not handle the table.
 * A callback allows an engine to request all open tables to be
 * closed by the streaming engine.
 */
typedef int (*MSOpenTableFunc)(void *thd, const char *table_url, void **open_table, PBMSResultPtr result);
typedef void (*MSCloseTableFunc)(void *thd, void *open_table);

/*
 * When the streaming engine wants to use an open table handle from the
 * pool, it calls the lock table function.
 */ 
typedef int (*MSLockTableFunc)(void *thd, int *xact, void *open_table, int lock_type, PBMSResultPtr result);
typedef int (*MSUnlockTableFunc)(void *thd, int xact, void *open_table, PBMSResultPtr result);

/* This function is used to locate and send a BLOB on the given stream.
 */
typedef int (*MSSendBLOBFunc)(void *thd, void *open_table, const char *blob_column, const char *blob_url, void *stream, PBMSResultPtr result);

/*
 * Lookup and engine reference, and return readable text.
 */
typedef int (*MSLookupRefFunc)(void *thd, void *open_table, unsigned short col_index, PBMSEngineRefPtr eng_ref, PBMSFieldRefPtr feild_ref, PBMSResultPtr result);

typedef struct PBMSEngineRec {
	int						ms_version;							/* MS_ENGINE_VERSION */
	int						ms_index;							/* The index into the engine list. */
	int						ms_removing;						/* TRUE (1) if the engine is being removed. */
	const char				*ms_engine_name;
	void					*ms_engine_info;
	MSCloseConnFunc			ms_close_conn;
	MSOpenTableFunc			ms_open_table;
	MSCloseTableFunc		ms_close_table;
	MSLockTableFunc			ms_lock_table;
	MSUnlockTableFunc		ms_unlock_table;
	MSSendBLOBFunc			ms_send_blob;
	MSLookupRefFunc			ms_lookup_ref;
} PBMSEngineRec, *PBMSEnginePtr;

/*
 * This function should never be called directly, it is called
 * by deregisterEngine() below.
 */
typedef void (*ECDeregisterdFunc)(PBMSEnginePtr engine);

typedef void (*ECTableCloseAllFunc)(const char *table_url);

typedef int (*ECSetContentLenFunc)(void *stream, off_t len, PBMSResultPtr result);

typedef int (*ECWriteHeadFunc)(void *stream, PBMSResultPtr result);

typedef int (*ECWriteStreamFunc)(void *stream, void *buffer, size_t len, PBMSResultPtr result);

/*
 * The engine should call this function from
 * its own close connection function!
 */
typedef int (*ECCloseConnFunc)(void *thd, PBMSResultPtr result);

/*
 * Call this function before retaining or releasing BLOBs in a row.
 */
typedef int (*ECOpenTableFunc)(void **open_table, char *table_path, PBMSResultPtr result);

/*
 * Call this function when the operation is complete.
 */
typedef void (*ECCloseTableFunc)(void *open_table);

/*
 * Call this function for each BLOB to be retained. When a BLOB is used, the 
 * URL may be changed. The returned URL is valid as long as the the
 * table is open.
 *
 * The returned URL must be inserted into the row in place of the given
 * URL.
 */
typedef int (*ECUseBlobFunc)(void *open_table, char **ret_blob_url, char *blob_url, unsigned short col_index, PBMSResultPtr result);

/*
 * Reference Blobs that has been uploaded to the streaming engine.
 *
 * All BLOBs specified by the use blob function are retained by
 * this function.
 *
 * The engine reference is a (unaligned) 8 byte value which
 * identifies the row that the BLOBs are in.
 */
typedef int (*ECRetainBlobsFunc)(void *open_table, PBMSEngineRefPtr eng_ref, PBMSResultPtr result);

/*
 * If a row containing a BLOB is deleted, then the BLOBs in the
 * row must be released.
 *
 * Note: if a table is dropped, all the BLOBs referenced by the
 * table are automatically released.
 */
typedef int (*ECReleaseBlobFunc)(void *open_table, char *blob_url, unsigned short col_index, PBMSEngineRefPtr eng_ref, PBMSResultPtr result);

typedef int (*ECDropTable)(const char *table_path, PBMSResultPtr result);

typedef int (*ECRenameTable)(const char *from_table, const char *to_table, PBMSResultPtr result);

typedef struct PBMSCallbacksRec {
	int						cb_version;							/* MS_CALLBACK_VERSION */
	ECDeregisterdFunc		cb_deregister;
	ECTableCloseAllFunc		cb_table_close_all;
	ECSetContentLenFunc		cb_set_cont_len;
	ECWriteHeadFunc			cb_write_head;
	ECWriteStreamFunc		cb_write_stream;
	ECCloseConnFunc			cb_close_conn;
	ECOpenTableFunc			cb_open_table;
	ECCloseTableFunc		cb_close_table;
	ECUseBlobFunc			cb_use_blob;
	ECRetainBlobsFunc		cb_retain_blobs;
	ECReleaseBlobFunc		cb_release_blob;
	ECDropTable				cb_drop_table;
	ECRenameTable			cb_rename_table;
} PBMSCallbacksRec, *PBMSCallbacksPtr;

typedef struct PBMSSharedMemoryRec {
	int						sm_magic;							/* MS_SHARED_MEMORY_MAGIC */
	int						sm_version;							/* MS_SHARED_MEMORY_VERSION */
	volatile int			sm_shutdown_lock;					/* "Cheap" lock for shutdown! */
	PBMSCallbacksPtr		sm_callbacks;
	int						sm_reserved1[20];
	void					*sm_reserved2[20];
	int						sm_list_size;
	int						sm_list_len;
	PBMSEnginePtr			sm_engine_list[MS_ENGINE_LIST_SIZE];
} PBMSSharedMemoryRec, *PBMSSharedMemoryPtr;

#ifndef PBMS_API
#ifndef PBMS_CLIENT_API
Please define he value of PBMS_API
#endif
#else

class PBMS_API
{
private:
	const char *temp_prefix[3];

public:
	PBMS_API(): sharedMemory(NULL) { 
		int i = 0;
		temp_prefix[i++] = MS_TEMP_FILE_PREFIX;
#ifdef MS_TEMP_FILE_PREFIX
		temp_prefix[i++] = MS_TEMP_FILE_PREFIX;
#endif
		temp_prefix[i++] = NULL;
		
	}

	~PBMS_API() { }

	/*
	 * Register the engine with the Stream Engine.
	 */
	int registerEngine(PBMSEnginePtr engine, PBMSResultPtr result) {
		int err;

		deleteTempFiles();

		if ((err = getSharedMemory(true, result)))
			return err;

		for (int i=0; i<sharedMemory->sm_list_size; i++) {
			if (!sharedMemory->sm_engine_list[i]) {
				sharedMemory->sm_engine_list[i] = engine;
				engine->ms_index = i;
				if (i >= sharedMemory->sm_list_len)
					sharedMemory->sm_list_len = i+1;
				return MS_OK;
			}
		}
		
		result->mr_code = 15010;
		strcpy(MS_RESULT_MESSAGE_SIZE, result->mr_message, "Too many BLOB streaming engines already registered");
		*result->mr_stack = 0;
		return MS_ERR_ENGINE;
	}

	void lock() {
		while (sharedMemory->sm_shutdown_lock)
			usleep(10000);
		sharedMemory->sm_shutdown_lock++;
		while (sharedMemory->sm_shutdown_lock != 1) {
			usleep(random() % 10000);
			sharedMemory->sm_shutdown_lock--;
			usleep(10000);
			sharedMemory->sm_shutdown_lock++;
		}
	}

	void unlock() {
		sharedMemory->sm_shutdown_lock--;
	}

	void deregisterEngine(PBMSEnginePtr engine) {
		PBMSResultRec result;
		int err;

		if ((err = getSharedMemory(true, &result)))
			return;

		lock();

		bool empty = true;
		for (int i=0; i<sharedMemory->sm_list_len; i++) {
			if (sharedMemory->sm_engine_list[i]) {
				if (sharedMemory->sm_engine_list[i] == engine) {
					if (sharedMemory->sm_callbacks)
						sharedMemory->sm_callbacks->cb_deregister(engine);
					sharedMemory->sm_engine_list[i] = NULL;
				}
				else
					empty = false;
			}
		}

		unlock();

		if (empty) {
			char	temp_file[100];

			sharedMemory->sm_magic = 0;
			free(sharedMemory);
			sharedMemory = NULL;
			const char **prefix = temp_prefix;
			while (*prefix) {
				getTempFileName(temp_file, *prefix, getpid());
				unlink(temp_file);
				prefix++;
			}
		}
	}

	void closeAllTables(const char *table_url)
	{
		PBMSResultRec	result;
		int					err;

		if ((err = getSharedMemory(true, &result)))
			return;

		if (sharedMemory->sm_callbacks)
			sharedMemory->sm_callbacks->cb_table_close_all(table_url);
	}

	int setContentLength(void *stream, off_t len, PBMSResultPtr result)
	{
		int err;

		if ((err = getSharedMemory(true, result)))
			return err;

		return sharedMemory->sm_callbacks->cb_set_cont_len(stream, len, result);
	}

	int writeHead(void *stream, PBMSResultPtr result)
	{
		int err;

		if ((err = getSharedMemory(true, result)))
			return err;

		return sharedMemory->sm_callbacks->cb_write_head(stream, result);
	}

	int writeStream(void *stream, void *buffer, size_t len, PBMSResultPtr result)
	{
		int err;

		if ((err = getSharedMemory(true, result)))
			return err;

		return sharedMemory->sm_callbacks->cb_write_stream(stream, buffer, len, result);
	}

	int closeConn(void *thd, PBMSResultPtr result)
	{
		int err;

		if ((err = getSharedMemory(true, result)))
			return err;

		if (!sharedMemory->sm_callbacks)
			return MS_OK;

		return sharedMemory->sm_callbacks->cb_close_conn(thd, result);
	}

	int openTable(void **open_table, char *table_path, PBMSResultPtr result)
	{
		int err;

		if ((err = getSharedMemory(true, result)))
			return err;

		if (!sharedMemory->sm_callbacks) {
			*open_table = NULL;
			return MS_OK;
		}

		return sharedMemory->sm_callbacks->cb_open_table(open_table, table_path, result);
	}

	int closeTable(void *open_table, PBMSResultPtr result)
	{
		int err;

		if ((err = getSharedMemory(true, result)))
			return err;

		if (sharedMemory->sm_callbacks && open_table)
			sharedMemory->sm_callbacks->cb_close_table(open_table);
		return MS_OK;
	}

	int couldBeURL(char *blob_url)
	/* ~*test/~1-150-2b5e0a7-0[*<blob size>][.ext] */
	/* ~*test/_1-150-2b5e0a7-0[*<blob size>][.ext] */
	{
		char	*ptr;
		size_t	len;
		bool have_blob_size = false;

		if (blob_url) {
			if ((len = strlen(blob_url))) {
				/* Too short: */
				if (len <= 10)
					return 0;

				/* Required prefix: */
				/* NOTE: ~> is deprecated v0.5.4+, now use ~* */
				if (*blob_url != '~' || (*(blob_url + 1) != '>' && *(blob_url + 1) != '*'))
					return 0;

				ptr = blob_url + len - 1;

				/* Allow for an optional extension: */
				if (!isdigit(*ptr)) {
					while (ptr > blob_url && *ptr != '/' && *ptr != '.')
						ptr--;
					if (ptr == blob_url || *ptr != '.')
						return 0;
					if (ptr == blob_url || !isdigit(*ptr))
						return 0;
				}
	
				// field 1: server id OR blob size
				do_again:
				while (ptr > blob_url && isdigit(*ptr))
					ptr--;

				if (ptr != blob_url && *ptr == '*' && !have_blob_size) {
					ptr--;
					have_blob_size = true;
					goto do_again;
				}
				
				if (ptr == blob_url || *ptr != '-')
					return 0;
					
					
				// field 2: Authoration code
				ptr--;
				if (!isxdigit(*ptr))
					return 0;

				while (ptr > blob_url && isxdigit(*ptr))
					ptr--;

				if (ptr == blob_url || *ptr != '-')
					return 0;
					
				// field 3:offset
				ptr--;
				if (!isxdigit(*ptr))
					return 0;
					
				while (ptr > blob_url && isdigit(*ptr))
					ptr--;

				if (ptr == blob_url || *ptr != '-')
					return 0;
					
					
				// field 4:Table id
				ptr--;
				if (!isdigit(*ptr))
					return 0;

				while (ptr > blob_url && isdigit(*ptr))
					ptr--;

				/* NOTE: ^ and : are deprecated v0.5.4+, now use ! and ~ */
				if (ptr == blob_url || (*ptr != '^' && *ptr != ':' && *ptr != '_' && *ptr != '~'))
					return 0;
				ptr--;

				if (ptr == blob_url || *ptr != '/')
					return 0;
				ptr--;
				if (ptr == blob_url)
					return 0;
				return 1;
			}
		}
		return 0;
	}

	int useBlob(void *open_table, char **ret_blob_url, char *blob_url, unsigned short col_index, PBMSResultPtr result)
	{
		int err;

		if ((err = getSharedMemory(true, result)))
			return err;

		if (!couldBeURL(blob_url)) {
			*ret_blob_url = NULL;
			return MS_OK;
		}

		if (!sharedMemory->sm_callbacks) {
			result->mr_code = MS_ERR_INCORRECT_URL;
			strcpy(MS_RESULT_MESSAGE_SIZE, result->mr_message, "BLOB streaming engine (PBMS) not installed");
			*result->mr_stack = 0;
			return MS_ERR_INCORRECT_URL;
		}

		return sharedMemory->sm_callbacks->cb_use_blob(open_table, ret_blob_url, blob_url, col_index, result);
	}

	int retainBlobs(void *open_table, PBMSEngineRefPtr eng_ref, PBMSResultPtr result)
	{
		int err;

		if ((err = getSharedMemory(true, result)))
			return err;

		if (!sharedMemory->sm_callbacks)
			return MS_OK;

		return sharedMemory->sm_callbacks->cb_retain_blobs(open_table, eng_ref, result);
	}

	int releaseBlob(void *open_table, char *blob_url, unsigned short col_index, PBMSEngineRefPtr eng_ref, PBMSResultPtr result)
	{
		int err;

		if ((err = getSharedMemory(true, result)))
			return err;

		if (!sharedMemory->sm_callbacks)
			return MS_OK;

		if (!couldBeURL(blob_url))
			return MS_OK;

		return sharedMemory->sm_callbacks->cb_release_blob(open_table, blob_url, col_index, eng_ref, result);
	}

	int dropTable(const char *table_path, PBMSResultPtr result)
	{
		int err;

		if ((err = getSharedMemory(true, result)))
			return err;

		if (!sharedMemory->sm_callbacks)
			return MS_OK;
			
		return sharedMemory->sm_callbacks->cb_drop_table(table_path, result);
	}

	int renameTable(const char *from_table, const char *to_table, PBMSResultPtr result)
	{
		int err;

		if ((err = getSharedMemory(true, result)))
			return err;

		if (!sharedMemory->sm_callbacks)
			return MS_OK;
			
		return sharedMemory->sm_callbacks->cb_rename_table(from_table, to_table, result);
	}

	volatile PBMSSharedMemoryPtr sharedMemory;

private:
	int getSharedMemory(bool create, PBMSResultPtr result)
	{
		int		tmp_f;
		int		r;
		char	temp_file[100];
		const char	**prefix = temp_prefix;
		void		*tmp_p = NULL;

		if (sharedMemory)
			return MS_OK;

		while (*prefix) {
			getTempFileName(temp_file, *prefix, getpid());
			tmp_f = open(temp_file, O_RDWR | (create ? O_CREAT : 0), SH_MASK);
			if (tmp_f == -1)
				return setOSResult(errno, "open", temp_file, result);

			r = lseek(tmp_f, 0, SEEK_SET);
			if (r == -1) {
				close(tmp_f);
				return setOSResult(errno, "lseek", temp_file, result);
			}
			ssize_t tfer;
			char buffer[100];
			
			tfer = read(tmp_f, buffer, 100);
			if (tfer == -1) {
				close(tmp_f);
				return setOSResult(errno, "read", temp_file, result);
			}

			buffer[tfer] = 0;
			sscanf(buffer, "%p", &tmp_p);
			sharedMemory = (PBMSSharedMemoryPtr) tmp_p;
			if (!sharedMemory || sharedMemory->sm_magic != MS_SHARED_MEMORY_MAGIC) {
				if (!create)
					return MS_OK;

				sharedMemory = (PBMSSharedMemoryPtr) calloc(1, sizeof(PBMSSharedMemoryRec));
				sharedMemory->sm_magic = MS_SHARED_MEMORY_MAGIC;
				sharedMemory->sm_version = MS_SHARED_MEMORY_VERSION;
				sharedMemory->sm_list_size = MS_ENGINE_LIST_SIZE;

				r = lseek(tmp_f, 0, SEEK_SET);
				if (r == -1) {
					close(tmp_f);
					return setOSResult(errno, "fseek", temp_file, result);
				}

				sprintf(buffer, "%p", (void *) sharedMemory);
				tfer = write(tmp_f, buffer, strlen(buffer));
				if (tfer != (ssize_t) strlen(buffer)) {
					close(tmp_f);
					return setOSResult(errno, "write", temp_file, result);
				}
				r = fsync(tmp_f);
				if (r == -1) {
					close(tmp_f);
					return setOSResult(errno, "fsync", temp_file, result);
				}
			}
			else if (sharedMemory->sm_version != MS_SHARED_MEMORY_VERSION) {
				close(tmp_f);
				result->mr_code = -1000;
				*result->mr_stack = 0;
				strcpy(MS_RESULT_MESSAGE_SIZE, result->mr_message, "Shared memory version: ");		
				strcat(MS_RESULT_MESSAGE_SIZE, result->mr_message, sharedMemory->sm_version);		
				strcat(MS_RESULT_MESSAGE_SIZE, result->mr_message, ", does not match engine shared memory version: ");		
				strcat(MS_RESULT_MESSAGE_SIZE, result->mr_message, MS_SHARED_MEMORY_VERSION);		
				strcat(MS_RESULT_MESSAGE_SIZE, result->mr_message, ".");		
				return MS_ERR_ENGINE;
			}
			close(tmp_f);
			
			// For backward compatability we need to create the old versions but we only need to read the current version.
			if (create)
				prefix++;
			else
				break;
		}
		return MS_OK;
	}

	void strcpy(size_t size, char *to, const char *from)
	{
		if (size > 0) {
			size--;
			while (*from && size--)
				*to++ = *from++;
			*to = 0;
		}
	}

	void strcat(size_t size, char *to, const char *from)
	{
		while (*to && size--) to++;
		strcpy(size, to, from);
	}

	void strcat(size_t size, char *to, int val)
	{
		char buffer[100];

		sprintf(buffer, "%d", val);
		strcat(size, to, buffer);
	}

	int setOSResult(int err, const char *func, char *file, PBMSResultPtr result) {
		char *msg;

		result->mr_code = err;
		*result->mr_stack = 0;
		strcpy(MS_RESULT_MESSAGE_SIZE, result->mr_message, "System call ");		
		strcat(MS_RESULT_MESSAGE_SIZE, result->mr_message, func);		
		strcat(MS_RESULT_MESSAGE_SIZE, result->mr_message, "() failed on ");		
		strcat(MS_RESULT_MESSAGE_SIZE, result->mr_message, file);		
		strcat(MS_RESULT_MESSAGE_SIZE, result->mr_message, ": ");		

#ifdef XT_WIN
		if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err, 0, iMessage + strlen(iMessage), MS_RESULT_MESSAGE_SIZE - strlen(iMessage), NULL)) {
			char *ptr;

			ptr = &iMessage[strlen(iMessage)];
			while (ptr-1 > err_msg) {
				if (*(ptr-1) != '\n' && *(ptr-1) != '\r' && *(ptr-1) != '.')
					break;
				ptr--;
			}
			*ptr = 0;

			strcat(MS_RESULT_MESSAGE_SIZE, result->mr_message, " (");
			strcat(MS_RESULT_MESSAGE_SIZE, result->mr_message, err);
			strcat(MS_RESULT_MESSAGE_SIZE, result->mr_message, ")");
			return MS_ERR_ENGINE;
		}
#endif

		msg = strerror(err);
		if (msg) {
			strcat(MS_RESULT_MESSAGE_SIZE, result->mr_message, msg);
			strcat(MS_RESULT_MESSAGE_SIZE, result->mr_message, " (");
			strcat(MS_RESULT_MESSAGE_SIZE, result->mr_message, err);
			strcat(MS_RESULT_MESSAGE_SIZE, result->mr_message, ")");
		}
		else {
			strcat(MS_RESULT_MESSAGE_SIZE, result->mr_message, "Unknown OS error code ");
			strcat(MS_RESULT_MESSAGE_SIZE, result->mr_message, err);
		}

		return MS_ERR_ENGINE;
	}

	void getTempFileName(char *temp_file, const char * prefix, int pid)
	{
		sprintf(temp_file, "/tmp/%s%d", prefix,  pid);
	}

	bool startsWith(const char *cstr, const char *w_cstr)
	{
		while (*cstr && *w_cstr) {
			if (*cstr != *w_cstr)
				return false;
			cstr++;
			w_cstr++;
		}
		return *cstr || !*w_cstr;
	}

	void deleteTempFiles()
	{
		struct dirent	*entry;
		struct dirent	*result;
		DIR				*odir;
		int				err;
		size_t			sz;
		char			temp_file[100];

#ifdef __sun
		sz = sizeof(struct dirent) + pathconf("/tmp/", _PC_NAME_MAX); // Solaris, see readdir(3C)
#else
		sz = sizeof(struct dirent);
#endif
		if (!(entry = (struct dirent *) malloc(sz)))
			return;
		if (!(odir = opendir("/tmp/")))
			return;
		err = readdir_r(odir, entry, &result);
		while (!err && result) {
			const char **prefix = temp_prefix;
			
			while (*prefix) {
				if (startsWith(entry->d_name, *prefix)) {
					int pid = atoi(entry->d_name + strlen(*prefix));
					
					/* If the process does not exist: */
					if (kill(pid, 0) == -1 && errno == ESRCH) {
						getTempFileName(temp_file, *prefix, pid);
						unlink(temp_file);
					}
				}
				prefix++;
			}
			
			err = readdir_r(odir, entry, &result);
		}
		closedir(odir);
		free(entry);
	}
};
#endif // PBMS_API

/*
 * The following is a low level API for accessing blobs directly.
 */
 

/*
 * Any threads using the direct blob access API must first register them selves with the
 * blob streaming engine before using the blob access functions. This is done by calling
 * PBMSInitBlobStreamingThread(). Call PBMSDeinitBlobStreamingThread() after the thread is
 * done using the direct blob access API
 */
 
/* 
* PBMSInitBlobStreamingThread(): Returns a pointer to a blob streaming thread.
*/
extern void *PBMSInitBlobStreamingThread(char *thread_name, PBMSResultPtr result);
extern void PBMSDeinitBlobStreamingThread(void *v_bs_thread);

/* 
* PBMSGetError():Gets the last error reported by a blob streaming thread.
*/
extern void PBMSGetError(void *v_bs_thread, PBMSResultPtr result);

/* 
* PBMSCreateBlob():Creates a new blob in the database of the given size. cont_type can be NULL.
*/
extern bool PBMSCreateBlob(PBMSBlobIDPtr blob_id, char *database_name, char *cont_type, u_int64_t size);

/* 
* PBMSWriteBlob():Write the data to the blob in one or more chunks. The total size of all the chuncks of 
* data written to the blob must match the size specified when the blob was created.
*/
extern bool PBMSWriteBlob(PBMSBlobIDPtr blob_id, char *database_name, char *data, size_t size, size_t offset);

/* 
* PBMSReadBlob():Read the blob data out of the blob in one or more chunks.
*/
extern bool PBMSReadBlob(PBMSBlobIDPtr blob_id, char *database_name, char *buffer, size_t *size, size_t offset);

/*
* PBMSIDToURL():Convert a blob id to a blob URL. The 'url' buffer must be atleast  PBMS_BLOB_URL_SIZE bytes in size.
*/
extern bool PBMSIDToURL(PBMSBlobIDPtr blob_id, char *database_name, char *url);

/*
* PBMSIDToURL():Convert a blob URL to a blob ID.
*/
extern bool PBMSURLToID(char *url, PBMSBlobIDPtr blob_id);

#endif
