#ifndef TOKU_LOADER_H
#define TOKU_LOADER_H

#if defined(__cplusplus)
extern "C" {
#endif

/* Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved.
 *
 * The technology is licensed by the Massachusetts Institute of Technology, 
 * Rutgers State University of New Jersey, and the Research Foundation of 
 * State University of New York at Stony Brook under United States of America 
 * Serial No. 11/760379 and to the patents and/or patent applications resulting from it.
 */

/*
Create and set up a loader.
 - The loader will operate in environment env, and the load will happen within transaction txn.
 - You must remember to close (or abort) the loader eventually (otherwise the resulting DBs will 
   not be valid, and you will have a memory leak).
 - The number of databases to be loaded is N.
 - The databases must already be open, and their handles are passed in in the array dbs.   
   In particular dbs[i] is the ith database.
 - The loader will work right whether the DBs are empty or full.  However if any of the DBs are not empty, 
   it may not be fast (e.g., the loader may simply perform DB->put() operations).   
 - For each row that is put into the loader, for i over each of the N DBs, generate_row is invoked on the 
   row to generate a secondary row.
 - The DBTs passed to generate_row() will have the DB_DBT_REALLOC flag set, and the extract 
   function should realloc the memory passed in.  The ulen field indicates how large the realloc'd 
   storage is, and if the extract function does perform a realloc it should update the ulen field.
 - We require that the extract function always return 0.  
 - The generate_row function must be thread safe.
 - Whenever two rows in dbs[i] need to be compared we use that db's comparison function.  The 
   comparison function must be thread safe.
 - DBs must have been set up with descriptors and comparison functions before calling any extract 
   or compare functions.
 - loader_flags is used to specify loader specific behavior.  For instance, LOADER_USE_PUTS tells the 
   loader to use traditional puts to save disk space while loading (at the cost of performance)
 - The new loader is returned in *blp.

 Modifies: :: env, txn, blp, and dbs.
*/
int toku_loader_create_loader(DB_ENV *env, DB_TXN *txn, DB_LOADER **blp, DB *src_db, int N, DB *dbs[N], uint32_t db_flags[N], uint32_t dbt_flags[N], uint32_t loader_flags);


/*
Set a error callback. 
 - If at any point during the load the system notices that an error has occurred, error information is recorded. 
 - The callback function may be called during DB_LOADER->close() or DB_LOADER->abort(), at which time the error 
   information is returned. 
 - A key-val pair for one of the errors is returned along with the db, and the index i indicating which db 
   had the problem. 
 - This function will be called at most once (so even if there are many problems, only one call will be made.) 
 - If a duplicate is discovered, the error is DB_KEYEXIST. 
 - The error_extra passed at the time of set_error_callback is the value passed as the error_extra when an error occurs.
*/
int toku_loader_set_error_callback(DB_LOADER *loader, void (*error_cb)(DB *db, int i, int err, DBT *key, DBT *val, void *error_extra), void *error_extra);


/*
Set the polling function. 
 - During the DB_LOADER->close operation, the poll function is called periodically. 
 - If it ever returns nonzero, then the loader stops as soon as possible. 
 - The poll function is called with the extra passed into the loader create function. 
 - A floating point number is also returned, which ranges from 0.0 to 1.0, indicating progress. Progress of 0.0 means 
   no progress so far. Progress of 0.5 means that the job is about half done. Progress of 1.0 means the job is done. 
   The progress is just an estimate.
*/
int toku_loader_set_poll_function(DB_LOADER *loader, int (*poll_func)(void *poll_extra, float progress), void *poll_extra);


/*
Give a row to the loader.
 - Returns zero if no error, non-zero if error.
 - When the application sees a non-zero return from put(), it must abort(), which would then call the error callback. 
 - Once put() returns a non-zero value, any loader calls other than abort() are unsupported and will result in undefined behavior.
*/
int toku_loader_put(DB_LOADER *loader, DBT *key, DBT *val);


/*
Finish the load, 
 - Take all the rows and put them into dictionaries which are returned as open handlers through the original dbs array. 
 - Frees all the memory allocated by the loader. 
 - You may not use the loader handle again after calling close.
 - The system will return an DB_KEYEXIST if in any of the resulting databases, there are two different rows with keys 
   that compare to be equal (and the duplicate callback function, if set, is called first).
 - If the polling function has been set, the loader will periodically call the polling function. If the polling function 
   ever returns a nonzero value, then the loader will return immediately, possibly with the dictionaries in some 
   inconsistent state. (To get them to a consistent state, the enclosing transaction should abort.)
 - To free the resources used by a loader, either DB_LOADER->close or DB_LOADER->abort must be called. After calling either 
   of those functions, no further loader operations can be performed with that loader.
 - The DBs remain open after the loader is closed.
*/
int toku_loader_close(DB_LOADER *loader);


/*
Abort the load, 
 - Possibly leave none, some, or all of the puts in effect. You may need to abort the enclosing transaction to get 
   back to a sane state.
 - To free the resources used by a loader, either DB_LOADER->close or DB_LOADER->abort must be called. After calling either 
   of those functions, no further loader operations can be performed with that loader.
 - The DBs remain open after the loader is aborted.
 */
int toku_loader_abort(DB_LOADER *loader);

// Remove any loader temp files that may have been left from a crashed system
int toku_loader_cleanup_temp_files(DB_ENV *env);

typedef struct loader_status {
  uint64_t create;          // number of loaders succefully created
  uint64_t create_fail;     // number of calls to toku_loader_create_loader() that failed
  uint64_t put;             // number of calls to toku_loader_put() that succeeded
  uint64_t put_fail;        // number of calls to toku_loader_put() that failed
  uint64_t close;           // number of calls to toku_loader_close()
  uint64_t close_fail;      // number of calls to toku_loader_close() that failed
  uint64_t abort;           // number of calls to toku_loader_abort()
  uint32_t current;         // number of loaders currently in existence
  uint32_t max;             // max number of loaders that ever existed simultaneously
} LOADER_STATUS_S, *LOADER_STATUS;

void toku_loader_get_status(LOADER_STATUS s);


#if defined(__cplusplus)
}

#endif

#endif
