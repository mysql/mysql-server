/* -*- mode: C; c-basic-offset: 4 -*- */
#ifndef TOKU_BRT_LOADER_H
#define TOKU_BRT_LOADER_H

#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."

#if defined(__cplusplus) || defined(__cilkplusplus)
extern "C" {
#endif

// The loader callbacks are C functions and need to be defined as such

typedef void (*brt_loader_error_func)(DB *, int which_db, int err, DBT *key, DBT *val, void *extra);

typedef int (*brt_loader_poll_func)(void *extra, float progress);

typedef struct brtloader_s *BRTLOADER;

int toku_brt_loader_open (BRTLOADER *bl,
                          CACHETABLE cachetable,
			  generate_row_for_put_func g,
			  DB *src_db,
			  int N,
			  DB *dbs[/*N*/],
			  const struct descriptor *descriptors[/*N*/],
			  const char * new_fnames_in_env[/*N*/],
			  brt_compare_func bt_compare_functions[/*N*/],
			  const char *temp_file_template,
                          LSN load_lsn);

int toku_brt_loader_put (BRTLOADER bl, DBT *key, DBT *val);

int toku_brt_loader_close (BRTLOADER bl,
			   brt_loader_error_func error_callback, void *error_callback_extra,
			   brt_loader_poll_func  poll_callback,  void *poll_callback_extra);

int toku_brt_loader_abort(BRTLOADER bl, 
                          BOOL is_error);

void brtloader_set_os_fwrite (size_t (*fwrite_fun)(const void*,size_t,size_t,FILE*));

#if defined(__cplusplus) || defined(__cilkplusplus)
};
#endif

#endif // BRTLOADER_H
