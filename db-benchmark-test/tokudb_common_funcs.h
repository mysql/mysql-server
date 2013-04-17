/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#if !defined(TOKUDB_COMMON_FUNCS_H)
#define TOKUDB_COMMON_FUNCS_H

#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."


#include <db.h>
#include <memory.h>
#if defined(TOKUDB) && TOKU_WINDOWS
#include <ydb.h>
#endif
static int test_main (int argc, char *const argv[]);
int
main(int argc, char *const argv[]) {
    int r;
#if defined(TOKUDB) && TOKU_WINDOWS
    toku_ydb_init();
#endif
#if !defined(TOKUDB) && DB_VERSION_MINOR==4 && DB_VERSION_MINOR == 7
    r = db_env_set_func_malloc(toku_malloc);   assert(r==0);
    r = db_env_set_func_free(toku_free);      assert(r==0);
    r = db_env_set_func_realloc(toku_realloc);   assert(r==0);
#endif
    r = test_main(argc, argv);
#if defined(TOKUDB) && TOKU_WINDOWS
    toku_ydb_destroy();
#endif
    return r;
}

static __attribute__((__unused__)) void 
print_engine_status(DB_ENV * UU(env)) {
#if defined(TOKUDB)
  int buffsize = 1024 * 128;
  char buff[buffsize];
  env->get_engine_status_text(env, buff, buffsize);
  printf("Engine status:\n");
  printf("%s", buff);
#endif
}


#endif /* #if !defined(TOKUDB_COMMON_H) */

