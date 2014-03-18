/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuDB, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#ident "$Id$"

#include <toku_portability.h>

#include <memory.h>
#include <db.h>

#include <ft/ft.h>
#include <ft/ft-ops.h>
#include <ft/ft-flusher.h>
#include <ft/checkpoint.h>
#include <ft/recover.h>
#include <ft/ftloader.h>

#include "ydb_env_func.h"

// For test purposes only.
// These callbacks are never used in production code, only as a way to test the system
// (for example, by causing crashes at predictable times).
void (*checkpoint_callback_f)(void*) = NULL;
void * checkpoint_callback_extra     = NULL;
void (*checkpoint_callback2_f)(void*) = NULL;
void * checkpoint_callback2_extra     = NULL;

bool engine_status_enable = true; // if false, suppress engine status output on failed assert, for test programs only

void db_env_set_direct_io (bool direct_io_on) {
    toku_ft_set_direct_io(direct_io_on);
}

void db_env_set_compress_buffers_before_eviction (bool compress_buffers) {
    toku_ft_set_compress_buffers_before_eviction(compress_buffers);
}

void db_env_set_func_fsync (int (*fsync_function)(int)) {
    toku_set_func_fsync(fsync_function);
}

void db_env_set_func_pwrite (ssize_t (*pwrite_function)(int, const void *, size_t, toku_off_t)) {
    toku_set_func_pwrite(pwrite_function);
}

void db_env_set_func_full_pwrite (ssize_t (*pwrite_function)(int, const void *, size_t, toku_off_t)) {
    toku_set_func_full_pwrite(pwrite_function);
}

void db_env_set_func_write (ssize_t (*write_function)(int, const void *, size_t)) {
    toku_set_func_write(write_function);
}

void db_env_set_func_full_write (ssize_t (*write_function)(int, const void *, size_t)) {
    toku_set_func_full_write(write_function);
}

void db_env_set_func_fdopen (FILE * (*fdopen_function)(int, const char *)) {
    toku_set_func_fdopen(fdopen_function);
}

void db_env_set_func_fopen (FILE * (*fopen_function)(const char *, const char *)) {
    toku_set_func_fopen(fopen_function);
}

void db_env_set_func_open (int (*open_function)(const char *, int, int)) {
    toku_set_func_open(open_function);
}

void db_env_set_func_fclose (int (*fclose_function)(FILE*)) {
    toku_set_func_fclose(fclose_function);
}

void db_env_set_func_pread (ssize_t (*fun)(int, void *, size_t, off_t)) {
    toku_set_func_pread(fun);
}

void db_env_set_func_loader_fwrite (size_t (*fwrite_fun)(const void*,size_t,size_t,FILE*)) {
    ft_loader_set_os_fwrite(fwrite_fun);
}

void db_env_set_func_malloc (void *(*f)(size_t)) {
    toku_set_func_malloc(f);
}

void db_env_set_func_realloc (void *(*f)(void*, size_t)) {
    toku_set_func_realloc(f);
}

void db_env_set_func_free (void (*f)(void*)) {
    toku_set_func_free(f);
}

// For test purposes only.
// With this interface, all checkpoint users get the same callbacks and the same extras.
void 
db_env_set_checkpoint_callback (void (*callback_f)(void*), void* extra) {
    toku_checkpoint_safe_client_lock();
    checkpoint_callback_f = callback_f;
    checkpoint_callback_extra = extra;
    toku_checkpoint_safe_client_unlock();
}

void 
db_env_set_checkpoint_callback2 (void (*callback_f)(void*), void* extra) {
    toku_checkpoint_safe_client_lock();
    checkpoint_callback2_f = callback_f;
    checkpoint_callback2_extra = extra;
    toku_checkpoint_safe_client_unlock();
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
    toku_ft_loader_set_size_factor(factor);
}

void 
db_env_set_mvcc_garbage_collection_verification(uint32_t verification_mode) {
    garbage_collection_debug = (verification_mode != 0);
}

// Purpose: allow test programs that expect to fail to suppress engine status output on failed assert.
void
db_env_enable_engine_status(bool enable) {
    engine_status_enable = enable;
}

void
db_env_set_num_bucket_mutexes(uint32_t num_mutexes) {
    toku_pair_list_set_lock_size(num_mutexes);
}

void db_env_try_gdb_stack_trace(const char *gdb_path) {
    toku_try_gdb_stack_trace(gdb_path);
}

