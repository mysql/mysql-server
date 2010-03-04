/* -*- mode: C; c-basic-offset: 4 -*- */
#ifndef BRTLOADER_H
#define BRTLOADER_H

#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

typedef struct brtloader_s *BRTLOADER;
int toku_brt_loader_open (BRTLOADER *bl,
			  generate_row_for_put_func g,
			  DB *src_db,
			  int N,
			  DB *dbs[/*N*/],
			  const struct descriptor *descriptors[/*N*/],
			  const char * new_fnames_in_env[/*N*/],
			  const char * new_fnames_in_cwd[/*N*/],
			  brt_compare_func bt_compare_functions[/*N*/],
			  const char *temp_file_template);
int toku_brt_loader_put (BRTLOADER bl, DBT *key, DBT *val);
int toku_brt_loader_close (BRTLOADER bl);

#endif // BRTLOADER_H
