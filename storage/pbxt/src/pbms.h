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
 * Original author: Paul McCullagh
 * Continued development: Barry Leslie
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
#include <errno.h>


#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class implementation */
#endif

/*			2	10		1			10			20			10				10			20				20
 * Format: "~*"<db_id><'~' || '_'><tab_id>"-"<blob_id>"-"<auth_code>"-"<server_id>"-"<blob_ref_id>"-"<blob_size>
 */
//If URL_FMT changes do not forget to update couldBeURL() in this file.
 
#define URL_FMT "~*%lu%c%lu-%llu-%lx-%lu-%llu-%llu"

#define MS_SHARED_MEMORY_MAGIC			0x7E9A120C
#define MS_ENGINE_VERSION				1
#define MS_CALLBACK_VERSION				4
#define MS_SHARED_MEMORY_VERSION		2
#define MS_ENGINE_LIST_SIZE				10
#define MS_TEMP_FILE_PREFIX				"pbms_temp_"

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
#define MS_ERR_DUPLICATE				11						/* Attempt to insert a duplicate key into a system table. */
#define MS_ERR_INVALID_RECORD			12
#define MS_ERR_RECOVERY_IN_PROGRESS		13
#define MS_ERR_DUPLICATE_DB				14
#define MS_ERR_DUPLICATE_DB_ID			15
#define MS_ERR_INVALID_OPERATION		16

#define MS_LOCK_NONE					0
#define MS_LOCK_READONLY				1
#define MS_LOCK_READ_WRITE				2

#define PBMS_BLOB_URL_SIZE				120

#define PBMS_FIELD_COL_SIZE				128
#define PBMS_FIELD_COND_SIZE			300

#define MS_RESULT_MESSAGE_SIZE			300
#define MS_RESULT_STACK_SIZE			200

typedef struct PBMSResultRec {
	int						mr_code;								/* Engine specific error code. */ 
	char					mr_message[MS_RESULT_MESSAGE_SIZE];		/* Error message, required if non-zero return code. */
	char					mr_stack[MS_RESULT_STACK_SIZE];			/* Trace information about where the error occurred. */
} PBMSResultRec, *PBMSResultPtr;



typedef struct PBMSBlobID {
	u_int32_t				bi_db_id;	
	u_int64_t				bi_blob_size;	
	u_int64_t				bi_blob_id;				// or repo file offset if type = REPO
	u_int64_t				bi_blob_ref_id;			
	u_int32_t				bi_tab_id;				// or repo ID if type = REPO
	u_int32_t				bi_auth_code;
	u_int32_t				bi_blob_type;
} PBMSBlobIDRec, *PBMSBlobIDPtr;

typedef struct PBMSBlobURL {
	char					bu_data[PBMS_BLOB_URL_SIZE];
} PBMSBlobURLRec, *PBMSBlobURLPtr;

typedef struct PBMSEngineRec {
	int						ms_version;							/* MS_ENGINE_VERSION */
	int						ms_index;							/* The index into the engine list. */
	int						ms_removing;						/* TRUE (1) if the engine is being removed. */
	int						ms_internal;						/* TRUE (1) if the engine is supported directly in the mysq/drizzle handler code . */
	char					ms_engine_name[32];
} PBMSEngineRec, *PBMSEnginePtr;

/*
 * This function should never be called directly, it is called
 * by deregisterEngine() below.
 */
typedef void (*ECRegisterdFunc)(PBMSEnginePtr engine);

typedef void (*ECDeregisterdFunc)(PBMSEnginePtr engine);

/*
 * Call this function to store a BLOB in the repository the BLOB's
 * URL will be returned. The returned URL buffer is expected to be atleast 
 * PBMS_BLOB_URL_SIZE long.
 *
 * The BLOB URL must still be retained or it will automaticly be deleted after a timeout expires.
 */
typedef int (*ECCreateBlobsFunc)(bool built_in, const char *db_name, const char *tab_name, char *blob, size_t blob_len, char *blob_url, unsigned short col_index, PBMSResultPtr result);

/*
 * Call this function for each BLOB to be retained. When a BLOB is used, the 
 * URL may be changed. The returned URL buffer is expected to be atleast 
 * PBMS_BLOB_URL_SIZE long.
 *
 * The returned URL must be inserted into the row in place of the given
 * URL.
 */
typedef int (*ECRetainBlobsFunc)(bool built_in, const char *db_name, const char *tab_name, char *ret_blob_url, char *blob_url, unsigned short col_index, PBMSResultPtr result);

/*
 * If a row containing a BLOB is deleted, then the BLOBs in the
 * row must be released.
 *
 * Note: if a table is dropped, all the BLOBs referenced by the
 * table are automatically released.
 */
typedef int (*ECReleaseBlobFunc)(bool built_in, const char *db_name, const char *tab_name, char *blob_url, PBMSResultPtr result);

typedef int (*ECDropTable)(bool built_in, const char *db_name, const char *tab_name, PBMSResultPtr result);

typedef int (*ECRenameTable)(bool built_in, const char *db_name, const char *from_table, const char *to_table, PBMSResultPtr result);

typedef void (*ECCallCompleted)(bool built_in, bool ok);

typedef struct PBMSCallbacksRec {
	int						cb_version;							/* MS_CALLBACK_VERSION */
	ECRegisterdFunc			cb_register;
	ECDeregisterdFunc		cb_deregister;
	ECCreateBlobsFunc		cb_create_blob;
	ECRetainBlobsFunc		cb_retain_blob;
	ECReleaseBlobFunc		cb_release_blob;
	ECDropTable				cb_drop_table;
	ECRenameTable			cb_rename_table;
	ECCallCompleted			cb_completed;
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

#ifdef PBMS_API

class PBMS_API
{
private:
	const char *temp_prefix[3];
	bool built_in;

public:
	PBMS_API(): sharedMemory(NULL) { 
		int i = 0;
		temp_prefix[i++] = MS_TEMP_FILE_PREFIX;
		temp_prefix[i++] = NULL;
		
	}

	~PBMS_API() { }

	/*
	 * This method is called by the PBMS engine during startup.
	 */
	int PBMSStartup(PBMSCallbacksPtr callbacks, PBMSResultPtr result) {
		int err;
		
		deleteTempFiles();
		err = getSharedMemory(true, result);
		if (!err)
			sharedMemory->sm_callbacks = callbacks;
			
		return err;
	}

	/*
	 * This method is called by the PBMS engine during startup.
	 */
	void PBMSShutdown() {
		
		if (!sharedMemory)
			return;
			
		lock();
		sharedMemory->sm_callbacks = NULL;

		bool empty = true;
		for (int i=0; i<sharedMemory->sm_list_len && empty; i++) {
			if (sharedMemory->sm_engine_list[i]) 
				empty = false;
		}

		unlock();
		
		if (empty) 
			removeSharedMemory();
	}

	/*
	 * Register the engine with the Stream Engine.
	 */
	int registerEngine(PBMSEnginePtr engine, PBMSResultPtr result) {
		int err;

		deleteTempFiles();

		// The first engine to register creates the shared memory.
		if ((err = getSharedMemory(true, result)))
			return err;

		for (int i=0; i<sharedMemory->sm_list_size; i++) {
			if (!sharedMemory->sm_engine_list[i]) {
				sharedMemory->sm_engine_list[i] = engine;
				engine->ms_index = i;
				if (i >= sharedMemory->sm_list_len)
					sharedMemory->sm_list_len = i+1;
				if (sharedMemory->sm_callbacks)
					sharedMemory->sm_callbacks->cb_register(engine);
					
				built_in = (engine->ms_internal == 1);
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

		if ((err = getSharedMemory(false, &result)))
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

		if (empty) 
			removeSharedMemory();
	}

	void removeSharedMemory() 
	{
		const char **prefix = temp_prefix;
		char	temp_file[100];

		// Do not remove the sharfed memory until after
		// the PBMS engine has shutdown.
		if (sharedMemory->sm_callbacks)
			return;
			
		sharedMemory->sm_magic = 0;
		free(sharedMemory);
		sharedMemory = NULL;
		
		while (*prefix) {
			getTempFileName(temp_file, *prefix, getpid());
			unlink(temp_file);
			prefix++;
		}
	}
	
	int couldBeURL(char *blob_url, int size)
	{
		if (blob_url && (size < PBMS_BLOB_URL_SIZE)) {
			char			buffer[PBMS_BLOB_URL_SIZE+1];
			u_int32_t		db_id = 0;
			u_int32_t		tab_id = 0;
			u_int64_t		blob_id = 0;
			u_int64_t		blob_ref_id = 0;
			u_int64_t		blob_size = 0;
			u_int32_t		auth_code = 0;
			u_int32_t		server_id = 0;
			char		type, junk[5];
			int			scanned;

			junk[0] = 0;
			if (blob_url[size]) { // There is no guarantee that the URL will be null terminated.
				memcpy(buffer, blob_url, size);
				buffer[size] = 0;
				blob_url = buffer;
			}
			
			scanned = sscanf(blob_url, URL_FMT"%4s", &db_id, &type, &tab_id, &blob_id, &auth_code, &server_id, &blob_ref_id, &blob_size, junk);
			if (scanned != 8) {// If junk is found at the end this will also result in an invalid URL. 
		printf("Bad URL \"%s\": scanned = %d, junk: %d, %d, %d, %d\n", blob_url, scanned, junk[0], junk[1], junk[2], junk[3]); 
				return 0;
			}
			
			if (junk[0] || (type != '~' && type != '_')) {
		printf("Bad URL \"%s\": scanned = %d, junk: %d, %d, %d, %d\n", blob_url, scanned, junk[0], junk[1], junk[2], junk[3]); 
				return 0;
			}
		
			return 1;
		}
		
		return 0;
	}
	
	int  retainBlob(const char *db_name, const char *tab_name, char *ret_blob_url, char *blob_url, size_t blob_size, unsigned short col_index, PBMSResultPtr result)
	{
		int err;
		char safe_url[PBMS_BLOB_URL_SIZE+1];


		if ((err = getSharedMemory(false, result)))
			return err;

		if (!couldBeURL(blob_url, blob_size)) {
		
			if (!sharedMemory->sm_callbacks)  {
				*ret_blob_url = 0;
				return MS_OK;
			}
			err = sharedMemory->sm_callbacks->cb_create_blob(built_in, db_name, tab_name, blob_url, blob_size, ret_blob_url, col_index, result);
			if (err)
				return err;
				
			blob_url = ret_blob_url;
		} else {
			// Make sure the url is a C string:
			if (blob_url[blob_size]) {
				memcpy(safe_url, blob_url, blob_size);
				safe_url[blob_size] = 0;
				blob_url = safe_url;
			}
		}
		

		if (!sharedMemory->sm_callbacks) {
			result->mr_code = MS_ERR_INCORRECT_URL;
			strcpy(MS_RESULT_MESSAGE_SIZE, result->mr_message, "BLOB streaming engine (PBMS) not installed");
			*result->mr_stack = 0;
			return MS_ERR_INCORRECT_URL;
		}

		return sharedMemory->sm_callbacks->cb_retain_blob(built_in, db_name, tab_name, ret_blob_url, blob_url, col_index, result);
	}

	int releaseBlob(const char *db_name, const char *tab_name, char *blob_url, size_t blob_size, PBMSResultPtr result)
	{
		int err;
		char safe_url[PBMS_BLOB_URL_SIZE+1];

		if ((err = getSharedMemory(false, result)))
			return err;

		if (!sharedMemory->sm_callbacks)
			return MS_OK;

		if (!couldBeURL(blob_url, blob_size))
			return MS_OK;

		if (blob_url[blob_size]) {
			memcpy(safe_url, blob_url, blob_size);
			safe_url[blob_size] = 0;
			blob_url = safe_url;
		}
		
		return sharedMemory->sm_callbacks->cb_release_blob(built_in, db_name, tab_name, blob_url, result);
	}

	int dropTable(const char *db_name, const char *tab_name, PBMSResultPtr result)
	{
		int err;

		if ((err = getSharedMemory(false, result)))
			return err;

		if (!sharedMemory->sm_callbacks)
			return MS_OK;
			
		return sharedMemory->sm_callbacks->cb_drop_table(built_in, db_name, tab_name, result);
	}

	int renameTable(const char *db_name, const char *from_table, const char *to_table, PBMSResultPtr result)
	{
		int err;

		if ((err = getSharedMemory(false, result)))
			return err;

		if (!sharedMemory->sm_callbacks)
			return MS_OK;
			
		return sharedMemory->sm_callbacks->cb_rename_table(built_in, db_name, from_table, to_table, result);
	}

	void completed(int ok)
	{
		PBMSResultRec result;

		if (getSharedMemory(false, &result))
			return;

		if (!sharedMemory->sm_callbacks)
			return;
			
		sharedMemory->sm_callbacks->cb_completed(built_in, ok);
	}
	
	volatile PBMSSharedMemoryPtr sharedMemory;

private:
	int getSharedMemory(bool create, PBMSResultPtr result)
	{
		int		tmp_f;
		int		r;
		char	temp_file[100];
		const char	**prefix = temp_prefix;

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
			sscanf(buffer, "%p", &sharedMemory);
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

				sprintf(buffer, "%p", sharedMemory);
				tfer = write(tmp_f, buffer, strlen(buffer));
				if (tfer != strlen(buffer)) {
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
* PBMSCreateBlob():Creates a new blob in the database of the given size.
*/
extern bool PBMSCreateBlob(PBMSBlobIDPtr blob_id, char *database_name, u_int64_t size);

/* 
* PBMSWriteBlob():Write the data to the blob in one or more chunks. The total size of all the chuncks of 
* data written to the blob must match the size specified when the blob was created.
*/
extern bool PBMSWriteBlob(PBMSBlobIDPtr blob_id, char *data, size_t size, size_t offset);

/* 
* PBMSReadBlob():Read the blob data out of the blob in one or more chunks.
*/
extern bool PBMSReadBlob(PBMSBlobIDPtr blob_id, char *buffer, size_t *size, size_t offset);

/*
* PBMSIDToURL():Convert a blob id to a blob URL. The 'url' buffer must be atleast  PBMS_BLOB_URL_SIZE bytes in size.
*/
extern bool PBMSIDToURL(PBMSBlobIDPtr blob_id, char *url);

/*
* PBMSIDToURL():Convert a blob URL to a blob ID.
*/
extern bool PBMSURLToID(char *url, PBMSBlobIDPtr blob_id);

#endif
