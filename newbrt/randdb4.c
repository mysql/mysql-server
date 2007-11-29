/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

/* Test random insertions using db4 */
#include <assert.h>
#include <db.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <string.h>
#include <limits.h>

enum { MAX_PATHNAME_LEN = 100 };
const char dir[]="db4dir";

DB_ENV *env=0;
DB *db=0;

#if DB_VERSION_MINOR == 0
#define IF40(x,y) x
#else
#define IF40(x,y) y
#endif

void create_directory (void) {
    char command[MAX_PATHNAME_LEN];
    int r;
    r=snprintf(command, MAX_PATHNAME_LEN, "rm -rf %s", dir);
    assert(r<MAX_PATHNAME_LEN);
    system(command);
    r=mkdir(dir, 0777);
    assert(r==0);
    r=db_env_create(&env, 0);
    assert(r==0);
    r=env->set_cachesize(env, 0, 512*(1<<20), 0);
    assert(r==0);

#if DB_VERSION_MAJOR >= 4 && DB_VERSION_MINOR >= 3
    IF40((void)0,
	 ({
	     unsigned int gbytes,bytes;
	     int ncaches;
      r=env->get_cachesize(env, &gbytes, &bytes, &ncaches);
      assert(r==0);
      printf("Using %.2fMiB Berkeley DB Cache Size\n", gbytes*1024 + ((double)bytes/(1<<20)));
    }));
#endif

    r= env->open(env, dir, DB_CREATE|DB_INIT_MPOOL,0777); // No logging.
    assert(r==0);
    r=db_create(&db, env, 0);
    assert(r==0);
    IF40(
	 r=db->open(db,    "files", 0, DB_BTREE, DB_CREATE, 0777),
	 r=db->open(db, 0, "files", 0, DB_BTREE, DB_CREATE, 0777));
    assert(r==0);
    
}

int write_one (long int n1, long int n2) {
    char keystring[100],valstring[100];
    int keysize;
    int datasize;
    DB_TXN *txn=0;
    DBT key,data;
    int r;
    keysize  = snprintf(keystring, 100, "%08lx%08lx", n1, n2);
    datasize = snprintf(valstring, 100, "%ld %ld %ld %ld %ld %ld", n1, n2, (long)(random()), (long)(random()), (long)(random()), (long)(random()));
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
    key.data = keystring;
    key.size = keysize;
    data.data = valstring;
    data.size = datasize;
    r = db->put(db, txn, &key, &data, 0);
    assert(r==0);
    return keysize+datasize;
}

/* Write a sequence evenly spaced. */ 
long long write_sequence (int n_inserts) {
  unsigned int step = UINT_MAX/n_inserts;
  int i,j;
  long long n_bytes=0;
  printf("%d inserts, step %d\n", n_inserts, step);
  for (i=0,j=0; i<n_inserts; i++,j+=step) {
    n_bytes+=write_one(j, random());
  }
  return n_bytes;
}

long long write_random (int n_inserts) {
  int i;
  long long n_bytes=0;
  for (i=0; i<n_inserts; i++) {
    n_bytes+=write_one(random(), random());
  }
  return n_bytes;
}

double tdiff (struct timeval *t1, struct timeval *t0) {
  return (t1->tv_sec-t0->tv_sec)+1e-6*(t1->tv_usec-t0->tv_usec);
}

int main (int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__))) {
  int n_s_inserts=200000000;
  int n_inserts=50000;
  struct timeval t0,t1,t00;
  long long n_bytes;
  int r;
  create_directory();
  gettimeofday(&t0, 0);
  n_bytes=write_sequence(n_s_inserts);
  gettimeofday(&t00, 0);
  r=db->sync(db, 0);   assert(r==0);
  gettimeofday(&t1, 0);
  {
    double t = tdiff(&t1, &t0);
    printf("%9d sequential inserts in %.3fs (%.3fs in sync), %.1f inserts/s.  %lld bytes, %.1f bytes/s\n", n_s_inserts, t, tdiff(&t1,&t00), n_s_inserts/t, n_bytes, n_bytes/t);
  }

  gettimeofday(&t0, 0);
  n_bytes=write_random(n_inserts);
  gettimeofday(&t00, 0);
  r=db->sync(db, 0);   assert(r==0);
  gettimeofday(&t1, 0);
  {
    double t = tdiff(&t1, &t0);
    printf("%9d random     inserts in %.3fs (%.3fs in sync), %.1f inserts/s.  %lld bytes, %.1f bytes/s\n", n_inserts, t, tdiff(&t1, &t00), n_inserts/t, n_bytes, n_bytes/t);
  }
  gettimeofday(&t0, 0);
  r=db->close(db,0);   assert(r==0);
  r=env->close(env,0); assert(r==0);
  gettimeofday(&t1, 0);
  printf("Time to close %.3fs\n", tdiff(&t1,&t0));
  return 0;
}
