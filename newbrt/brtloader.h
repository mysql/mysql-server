/* -*- mode: C; c-basic-offset: 4 -*- */
#ifndef TOKU_BRT_LOADER_H
#define TOKU_BRT_LOADER_H

#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

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

// For test purposes only
void toku_brtloader_set_size_factor (uint32_t factor);
// For test purposes only.  (In production, the rowset size is determined by negotation with the cachetable for some memory.  See #2613.)
uint64_t toku_brtloader_get_rowset_budget_for_testing (void);


int toku_brt_loader_finish_extractor(BRTLOADER bl);

int toku_brt_loader_get_error(BRTLOADER bl, int *loader_errno);

#if defined(__cplusplus) || defined(__cilkplusplus)
}
#endif

#endif // BRTLOADER_H
