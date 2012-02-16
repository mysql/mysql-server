/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#ident "Copyright (c) 2007-2009 Tokutek Inc.  All rights reserved."
 
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <toku_portability.h>
#include <db.h>
#include "brt-internal.h"
#include "brt-flusher.h"
#include "dlmalloc.h"
#include "checkpoint.h"
#include "brtloader.h"
#include "ydb_env_func.h"

// For test purposes only.
// These callbacks are never used in production code, only as a way to test the system
// (for example, by causing crashes at predictable times).
void (*checkpoint_callback_f)(void*) = NULL;
void * checkpoint_callback_extra     = NULL;
void (*checkpoint_callback2_f)(void*) = NULL;
void * checkpoint_callback2_extra     = NULL;

uint32_t  engine_status_enable = 1;   // if zero, suppress engine status output on failed assert, for test programs only

int 
db_env_set_func_fsync (int (*fsync_function)(int)) {
    return toku_set_func_fsync(fsync_function);
}

int 
db_env_set_func_pwrite (ssize_t (*pwrite_function)(int, const void *, size_t, toku_off_t)) {
    return toku_set_func_pwrite(pwrite_function);
}

int 
db_env_set_func_full_pwrite (ssize_t (*pwrite_function)(int, const void *, size_t, toku_off_t)) {
    return toku_set_func_full_pwrite(pwrite_function);
}

int 
db_env_set_func_write (ssize_t (*write_function)(int, const void *, size_t)) {
    return toku_set_func_write(write_function);
}

int 
db_env_set_func_full_write (ssize_t (*write_function)(int, const void *, size_t)) {
    return toku_set_func_full_write(write_function);
}

int 
db_env_set_func_fdopen (FILE * (*fdopen_function)(int, const char *)) {
    return toku_set_func_fdopen(fdopen_function);
}

int 
db_env_set_func_fopen (FILE * (*fopen_function)(const char *, const char *)) {
    return toku_set_func_fopen(fopen_function);
}

int 
db_env_set_func_open (int (*open_function)(const char *, int, int)) {
    return toku_set_func_open(open_function);
}

int 
db_env_set_func_fclose (int (*fclose_function)(FILE*)) {
    return toku_set_func_fclose(fclose_function);
}

int
db_env_set_func_pread (ssize_t (*fun)(int, void *, size_t, off_t)) {
    return toku_set_func_pread(fun);
}

void 
db_env_set_func_loader_fwrite (size_t (*fwrite_fun)(const void*,size_t,size_t,FILE*)) {
    brtloader_set_os_fwrite(fwrite_fun);
}

int 
db_env_set_func_malloc (void *(*f)(size_t)) {
    toku_set_func_malloc(f);
    return 0;
}

int 
db_env_set_func_realloc (void *(*f)(void*, size_t)) {
    toku_set_func_realloc(f);
    return 0;
}

int 
db_env_set_func_free (void (*f)(void*)) {
    toku_set_func_free(f);
    return 0;
}


// Got to call dlmalloc, or else it won't get included.
void 
setup_dlmalloc (void) {
    db_env_set_func_malloc(dlmalloc);
    db_env_set_func_realloc(dlrealloc);
    db_env_set_func_free(dlfree);
}

// For test purposes only.
// With this interface, all checkpoint users get the same callbacks and the same extras.
void 
db_env_set_checkpoint_callback (void (*callback_f)(void*), void* extra) {
    toku_checkpoint_safe_client_lock();
    checkpoint_callback_f = callback_f;
    checkpoint_callback_extra = extra;
    toku_checkpoint_safe_client_unlock();
    //printf("set callback = %p, extra = %p\n", callback_f, extra);
}

void 
db_env_set_checkpoint_callback2 (void (*callback_f)(void*), void* extra) {
    toku_checkpoint_safe_client_lock();
    checkpoint_callback2_f = callback_f;
    checkpoint_callback2_extra = extra;
    toku_checkpoint_safe_client_unlock();
    //printf("set callback2 = %p, extra2 = %p\n", callback2_f, extra2);
}

void 
db_env_set_recover_callback (void (*callback_f)(void*), void* extra) {
    toku_recover_set_callback(callback_f, extra);
}

void 
db_env_set_recover_callback2 (void (*callback_f)(void*), void* extra) {
    toku_recover_set_callback2(callback_f, extra);
}

void 
db_env_set_flusher_thread_callback(void (*callback_f)(int, void*), void* extra) {
    toku_flusher_thread_set_callback(callback_f, extra);
}

void 
db_env_set_loader_size_factor (uint32_t factor) {
    toku_brtloader_set_size_factor(factor);
}

void 
db_env_set_mvcc_garbage_collection_verification(u_int32_t verification_mode) {
    garbage_collection_debug = (verification_mode != 0);
}

// Purpose: allow test programs that expect to fail to suppress engine status output on failed assert.
void
db_env_enable_engine_status(uint32_t enable) {
    engine_status_enable = enable;
}


