/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ifndef FTLOADER_H
#define FTLOADER_H

#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

// The loader callbacks are C functions and need to be defined as such

typedef void (*ft_loader_error_func)(DB *, int which_db, int err, DBT *key, DBT *val, void *extra);

typedef int (*ft_loader_poll_func)(void *extra, float progress);

typedef struct ft_loader_s *FTLOADER;

int toku_ft_loader_open (FTLOADER *bl,
                          CACHETABLE cachetable,
			  generate_row_for_put_func g,
			  DB *src_db,
			  int N,
			  FT_HANDLE brts[/*N*/], DB* dbs[/*N*/],
			  const char * new_fnames_in_env[/*N*/],
			  ft_compare_func bt_compare_functions[/*N*/],
			  const char *temp_file_template,
                          LSN load_lsn,
                          TOKUTXN txn,
                          BOOL reserve_memory);

int toku_ft_loader_put (FTLOADER bl, DBT *key, DBT *val);

int toku_ft_loader_close (FTLOADER bl,
			   ft_loader_error_func error_callback, void *error_callback_extra,
			   ft_loader_poll_func  poll_callback,  void *poll_callback_extra);

int toku_ft_loader_abort(FTLOADER bl, 
                          BOOL is_error);

// For test purposes only
void toku_ft_loader_set_size_factor (uint32_t factor);

void ft_loader_set_os_fwrite (size_t (*fwrite_fun)(const void*,size_t,size_t,FILE*));

size_t ft_loader_leafentry_size(size_t key_size, size_t val_size, TXNID xid);

#endif // FTLOADER_H
