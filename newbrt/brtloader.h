/* -*- mode: C; c-basic-offset: 4 -*- */
#ifndef BRTLOADER_H
#define BRTLOADER_H

#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

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
			   void (*error_callback)(DB *, int which_db, int err, DBT *key, DBT *val, void *extra), void *error_callback_extra,
			   int (*poll_callback)(void *extra, float progress), 	                                 void *poll_callback_extra);
int toku_brt_loader_abort(BRTLOADER bl, 
                          BOOL is_error);

void brtloader_set_os_fwrite (size_t (*fwrite_fun)(const void*,size_t,size_t,FILE*));

#endif // BRTLOADER_H
