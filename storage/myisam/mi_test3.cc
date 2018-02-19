/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/* Test av locking */

#include <sys/types.h>
#include "myisam.h"
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifndef WEXITSTATUS
#define WEXITSTATUS(stat_val) ((unsigned)(stat_val) >> 8)
#endif
#ifndef WIFEXITED
#define WIFEXITED(stat_val) (((stat_val)&255) == 0)
#endif

#if defined(HAVE_LRAND48)
#define rnd(X) (lrand48() % X)
#define rnd_init(X) srand48(X)
#else
#define rnd(X) (random() % X)
#define rnd_init(X) srandom(X)
#endif

const char *filename = "test3";
uint tests = 10, forks = 10, key_cacheing = 0, use_log = 0;

static void get_options(int argc, char *argv[]);
void start_test(int id);
int test_read(MI_INFO *, int), test_write(MI_INFO *, int, int),
    test_update(MI_INFO *, int, int), test_rrnd(MI_INFO *, int);

struct record {
  uchar id[8];
  uchar nr[4];
  uchar text[10];
} record;

int main(int argc, char **argv) {
  int status, wait_ret;
  uint i = 0;
  MI_KEYDEF keyinfo[10];
  MI_COLUMNDEF recinfo[10];
  HA_KEYSEG keyseg[10][2];
  MY_INIT(argv[0]);
  get_options(argc, argv);

  memset(keyinfo, 0, sizeof(keyinfo));
  memset(recinfo, 0, sizeof(recinfo));
  memset(keyseg, 0, sizeof(keyseg));
  keyinfo[0].seg = &keyseg[0][0];
  keyinfo[0].seg[0].start = 0;
  keyinfo[0].seg[0].length = 8;
  keyinfo[0].seg[0].type = HA_KEYTYPE_TEXT;
  keyinfo[0].seg[0].flag = HA_SPACE_PACK;
  keyinfo[0].key_alg = HA_KEY_ALG_BTREE;
  keyinfo[0].keysegs = 1;
  keyinfo[0].flag = (uint8)HA_PACK_KEY;
  keyinfo[0].block_length = 0; /* Default block length */
  keyinfo[1].seg = &keyseg[1][0];
  keyinfo[1].seg[0].start = 8;
  keyinfo[1].seg[0].length = 4; /* Long is always 4 in myisam */
  keyinfo[1].seg[0].type = HA_KEYTYPE_LONG_INT;
  keyinfo[1].seg[0].flag = 0;
  keyinfo[1].key_alg = HA_KEY_ALG_BTREE;
  keyinfo[1].keysegs = 1;
  keyinfo[1].flag = HA_NOSAME;
  keyinfo[1].block_length = 0; /* Default block length */

  recinfo[0].type = 0;
  recinfo[0].length = sizeof(record.id);
  recinfo[1].type = 0;
  recinfo[1].length = sizeof(record.nr);
  recinfo[2].type = 0;
  recinfo[2].length = sizeof(record.text);

  puts("- Creating myisam-file");
  my_delete(filename, MYF(0)); /* Remove old locks under gdb */
  if (mi_create(filename, 2, &keyinfo[0], 2, &recinfo[0], 0, (MI_UNIQUEDEF *)0,
                (MI_CREATE_INFO *)0, 0))
    exit(1);

  rnd_init(0);
  printf("- Starting %d processes\n", forks);
  fflush(stdout);
  for (i = 0; i < forks; i++) {
    if (!fork()) {
      start_test(i + 1);
      sleep(1);
      return 0;
    }
    (void)rnd(1);
  }

  for (i = 0; i < forks; i++)
    while ((wait_ret = wait(&status)) && wait_ret == -1)
      ;
  return 0;
}

static void get_options(int argc, char **argv) {
  char *pos, *progname;

  progname = argv[0];

  while (--argc > 0 && *(pos = *(++argv)) == '-') {
    switch (*++pos) {
      case 'l':
        use_log = 1;
        break;
      case 'f':
        forks = atoi(++pos);
        break;
      case 't':
        tests = atoi(++pos);
        break;
      case 'K': /* Use key cacheing */
        key_cacheing = 1;
        break;
      case 'A': /* All flags */
        use_log = key_cacheing = 1;
        break;
      case '?':
      case 'I':
      case 'V':
        printf("%s  Ver 1.0 for %s at %s\n", progname, SYSTEM_TYPE,
               MACHINE_TYPE);
        puts("By Monty, for your professional use\n");
        puts("Test av locking with threads\n");
        printf("Usage: %s [-?lKA] [-f#] [-t#]\n", progname);
        exit(0);
      case '#':
        DBUG_PUSH(++pos);
        break;
      default:
        printf("Illegal option: '%c'\n", *pos);
        break;
    }
  }
  return;
}

void start_test(int id) {
  uint i;
  int error, lock_type;
  MI_ISAMINFO isam_info;
  MI_INFO *file, *file1, *file2 = 0, *lock;

  if (use_log) mi_log(1);
  if (!(file1 = mi_open(filename, O_RDWR, HA_OPEN_WAIT_IF_LOCKED)) ||
      !(file2 = mi_open(filename, O_RDWR, HA_OPEN_WAIT_IF_LOCKED))) {
    fprintf(stderr, "Can't open isam-file: %s\n", filename);
    exit(1);
  }
  if (key_cacheing && rnd(2) == 0)
    init_key_cache(dflt_key_cache, KEY_CACHE_BLOCK_SIZE, 65536L, 0, 0);
  printf("Process %d, pid: %d\n", id, getpid());
  fflush(stdout);

  for (error = i = 0; i < tests && !error; i++) {
    file = (rnd(2) == 1) ? file1 : file2;
    lock = 0;
    lock_type = 0;
    if (rnd(10) == 0) {
      if (mi_lock_database(lock = (rnd(2) ? file1 : file2),
                           lock_type = (rnd(2) == 0 ? F_RDLCK : F_WRLCK))) {
        fprintf(stderr, "%2d: start: Can't lock table %d\n", id, my_errno);
        error = 1;
        break;
      }
    }
    switch (rnd(4)) {
      case 0:
        error = test_read(file, id);
        break;
      case 1:
        error = test_rrnd(file, id);
        break;
      case 2:
        error = test_write(file, id, lock_type);
        break;
      case 3:
        error = test_update(file, id, lock_type);
        break;
    }
    if (lock) mi_lock_database(lock, F_UNLCK);
  }
  if (!error) {
    mi_status(file1, &isam_info, HA_STATUS_VARIABLE);
    printf("%2d: End of test.  Records:  %ld  Deleted:  %ld\n", id,
           (long)isam_info.records, (long)isam_info.deleted);
    fflush(stdout);
  }

  mi_close(file1);
  mi_close(file2);
  if (use_log) mi_log(0);
  if (error) {
    printf("%2d: Aborted\n", id);
    fflush(stdout);
    exit(1);
  }
}

int test_read(MI_INFO *file, int id) {
  uint i, lock, found, next, prev;
  ulong find;

  lock = 0;
  if (rnd(2) == 0) {
    lock = 1;
    if (mi_lock_database(file, F_RDLCK)) {
      fprintf(stderr, "%2d: Can't lock table %d\n", id, my_errno);
      return 1;
    }
  }

  found = next = prev = 0;
  for (i = 0; i < 100; i++) {
    find = rnd(100000);
    if (!mi_rkey(file, record.id, 1, (uchar *)&find, HA_WHOLE_KEY,
                 HA_READ_KEY_EXACT))
      found++;
    else {
      if (my_errno != HA_ERR_KEY_NOT_FOUND) {
        fprintf(stderr, "%2d: Got error %d from read in read\n", id, my_errno);
        return 1;
      } else if (!mi_rnext(file, record.id, 1))
        next++;
      else {
        if (my_errno != HA_ERR_END_OF_FILE) {
          fprintf(stderr, "%2d: Got error %d from rnext in read\n", id,
                  my_errno);
          return 1;
        } else if (!mi_rprev(file, record.id, 1))
          prev++;
        else {
          if (my_errno != HA_ERR_END_OF_FILE) {
            fprintf(stderr, "%2d: Got error %d from rnext in read\n", id,
                    my_errno);
            return 1;
          }
        }
      }
    }
  }
  if (lock) {
    if (mi_lock_database(file, F_UNLCK)) {
      fprintf(stderr, "%2d: Can't unlock table\n", id);
      return 1;
    }
  }
  printf("%2d: read:   found: %5d  next: %5d   prev: %5d\n", id, found, next,
         prev);
  fflush(stdout);
  return 0;
}

int test_rrnd(MI_INFO *file, int id) {
  uint count, lock;

  lock = 0;
  if (rnd(2) == 0) {
    lock = 1;
    if (mi_lock_database(file, F_RDLCK)) {
      fprintf(stderr, "%2d: Can't lock table (%d)\n", id, my_errno);
      mi_close(file);
      return 1;
    }
    if (rnd(2) == 0) mi_extra(file, HA_EXTRA_CACHE, 0);
  }

  count = 0;
  if (mi_rrnd(file, record.id, 0L)) {
    if (my_errno == HA_ERR_END_OF_FILE) goto end;
    fprintf(stderr, "%2d: Can't read first record (%d)\n", id, my_errno);
    return 1;
  }
  for (count = 1; !mi_rrnd(file, record.id, HA_OFFSET_ERROR); count++)
    ;
  if (my_errno != HA_ERR_END_OF_FILE) {
    fprintf(stderr, "%2d: Got error %d from rrnd\n", id, my_errno);
    return 1;
  }

end:
  if (lock) {
    mi_extra(file, HA_EXTRA_NO_CACHE, 0);
    if (mi_lock_database(file, F_UNLCK)) {
      fprintf(stderr, "%2d: Can't unlock table\n", id);
      exit(0);
    }
  }
  printf("%2d: rrnd:   %5d\n", id, count);
  fflush(stdout);
  return 0;
}

int test_write(MI_INFO *file, int id, int lock_type) {
  uint i, tries, count, lock;

  lock = 0;
  if (rnd(2) == 0 || lock_type == F_RDLCK) {
    lock = 1;
    if (mi_lock_database(file, F_WRLCK)) {
      if (lock_type == F_RDLCK && my_errno == EDEADLK) {
        printf("%2d: write:  deadlock\n", id);
        fflush(stdout);
        return 0;
      }
      fprintf(stderr, "%2d: Can't lock table (%d)\n", id, my_errno);
      mi_close(file);
      return 1;
    }
    if (rnd(2) == 0) mi_extra(file, HA_EXTRA_WRITE_CACHE, 0);
  }

  sprintf((char *)record.id, "%7d", getpid());
  my_stpnmov((char *)record.text, "Testing...", sizeof(record.text));

  tries = (uint)rnd(100) + 10;
  for (i = count = 0; i < tries; i++) {
    uint32 tmp = rnd(80000) + 20000;
    int4store(record.nr, tmp);
    if (!mi_write(file, record.id))
      count++;
    else {
      if (my_errno != HA_ERR_FOUND_DUPP_KEY) {
        fprintf(stderr, "%2d: Got error %d (errno %d) from write\n", id,
                my_errno, errno);
        return 1;
      }
    }
  }
  if (lock) {
    mi_extra(file, HA_EXTRA_NO_CACHE, 0);
    if (mi_lock_database(file, F_UNLCK)) {
      fprintf(stderr, "%2d: Can't unlock table\n", id);
      exit(0);
    }
  }
  printf("%2d: write:  %5d\n", id, count);
  fflush(stdout);
  return 0;
}

int test_update(MI_INFO *file, int id, int lock_type) {
  uint i, lock, found, next, prev, update;
  uint32 tmp;
  char find[4];
  struct record new_record;

  lock = 0;
  if (rnd(2) == 0 || lock_type == F_RDLCK) {
    lock = 1;
    if (mi_lock_database(file, F_WRLCK)) {
      if (lock_type == F_RDLCK && my_errno == EDEADLK) {
        printf("%2d: write:  deadlock\n", id);
        fflush(stdout);
        return 0;
      }
      fprintf(stderr, "%2d: Can't lock table (%d)\n", id, my_errno);
      return 1;
    }
  }
  memset(&new_record, 0, sizeof(new_record));
  my_stpcpy((char *)new_record.text, "Updated");

  found = next = prev = update = 0;
  for (i = 0; i < 100; i++) {
    tmp = rnd(100000);
    int4store(find, tmp);
    if (!mi_rkey(file, record.id, 1, (uchar *)find, HA_WHOLE_KEY,
                 HA_READ_KEY_EXACT))
      found++;
    else {
      if (my_errno != HA_ERR_KEY_NOT_FOUND) {
        fprintf(stderr, "%2d: Got error %d from read in update\n", id,
                my_errno);
        return 1;
      } else if (!mi_rnext(file, record.id, 1))
        next++;
      else {
        if (my_errno != HA_ERR_END_OF_FILE) {
          fprintf(stderr, "%2d: Got error %d from rnext in update\n", id,
                  my_errno);
          return 1;
        } else if (!mi_rprev(file, record.id, 1))
          prev++;
        else {
          if (my_errno != HA_ERR_END_OF_FILE) {
            fprintf(stderr, "%2d: Got error %d from rnext in update\n", id,
                    my_errno);
            return 1;
          }
          continue;
        }
      }
    }
    memcpy(new_record.id, record.id, sizeof(record.id));
    tmp = rnd(20000) + 40000;
    int4store(new_record.nr, tmp);
    if (!mi_update(file, record.id, new_record.id))
      update++;
    else {
      if (my_errno != HA_ERR_RECORD_CHANGED &&
          my_errno != HA_ERR_RECORD_DELETED &&
          my_errno != HA_ERR_FOUND_DUPP_KEY) {
        fprintf(stderr, "%2d: Got error %d from update\n", id, my_errno);
        return 1;
      }
    }
  }
  if (lock) {
    if (mi_lock_database(file, F_UNLCK)) {
      fprintf(stderr, "Can't unlock table,id, error%d\n", my_errno);
      return 1;
    }
  }
  printf("%2d: update: %5d\n", id, update);
  fflush(stdout);
  return 0;
}

#include "mi_extrafunc.h"
