/* Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

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

/* Test av isam-databas: stor test */

#include "my_config.h"

#include <signal.h>
#include <sys/types.h>

#include "my_compiler.h"
#include "my_inttypes.h"
#include "my_macros.h"
#include "storage/heap/heapdef.h" /* Because of hp_find_block */

#define MAX_RECORDS 100000
#define MAX_KEYS 4

static int get_options(int argc, char *argv[]);
static int rnd(int max_value);
static void endprog(int sig_number) MY_ATTRIBUTE((noreturn));

static uint flag = 0, verbose = 0, testflag = 0, recant = 10000, silent = 0;
static uint keys = MAX_KEYS;
static uint16 key1[1001];
static bool key3[MAX_RECORDS];
static int reclength = 39;

static int calc_check(uchar *buf, uint length);
static void make_record(uchar *record, uint n1, uint n2, uint n3,
                        const char *mark, uint count);

/* Main program */

int main(int argc, char *argv[]) {
  uint i, j;
  uint ant, n1, n2, n3;
  uint write_count, update, opt_delete, check2, dupp_keys, found_key;
  int error;
  ulong pos;
  unsigned long key_check;
  uchar record[128], record2[128], record3[128], key[10];
  const char *filename, *filename2;
  HP_INFO *file, *file2;
  HP_SHARE *tmp_share;
  HP_KEYDEF keyinfo[MAX_KEYS];
  HA_KEYSEG keyseg[MAX_KEYS * 5];
  HP_HEAP_POSITION position;
  HP_CREATE_INFO hp_create_info;
  CHARSET_INFO *cs = &my_charset_latin1;
  bool unused;
  MY_INIT(argv[0]); /* init my_sys library & pthreads */

  filename = "test2";
  filename2 = "test2_2";
  file = file2 = 0;
  get_options(argc, argv);

  memset(&hp_create_info, 0, sizeof(hp_create_info));
  hp_create_info.max_table_size = 1024L * 1024L * 2;
  hp_create_info.keys = keys;
  hp_create_info.keydef = keyinfo;
  hp_create_info.reclength = reclength;
  hp_create_info.max_records = (ulong)flag * 100000L;
  hp_create_info.min_records = (ulong)recant / 2;

  write_count = update = opt_delete = 0;
  key_check = 0;

  keyinfo[0].seg = keyseg;
  keyinfo[0].keysegs = 1;
  keyinfo[0].flag = 0;
  keyinfo[0].algorithm = HA_KEY_ALG_HASH;
  keyinfo[0].seg[0].type = HA_KEYTYPE_BINARY;
  keyinfo[0].seg[0].start = 0;
  keyinfo[0].seg[0].length = 6;
  keyinfo[0].seg[0].null_bit = 0;
  keyinfo[0].seg[0].charset = cs;
  keyinfo[1].seg = keyseg + 1;
  keyinfo[1].keysegs = 2;
  keyinfo[1].flag = 0;
  keyinfo[1].algorithm = HA_KEY_ALG_HASH;
  keyinfo[1].seg[0].type = HA_KEYTYPE_BINARY;
  keyinfo[1].seg[0].start = 7;
  keyinfo[1].seg[0].length = 6;
  keyinfo[1].seg[0].null_bit = 0;
  keyinfo[1].seg[0].charset = cs;
  keyinfo[1].seg[1].type = HA_KEYTYPE_TEXT;
  keyinfo[1].seg[1].start = 0; /* key in two parts */
  keyinfo[1].seg[1].length = 6;
  keyinfo[1].seg[1].null_bit = 0;
  keyinfo[1].seg[1].charset = cs;
  keyinfo[2].seg = keyseg + 3;
  keyinfo[2].keysegs = 1;
  keyinfo[2].flag = HA_NOSAME;
  keyinfo[2].algorithm = HA_KEY_ALG_HASH;
  keyinfo[2].seg[0].type = HA_KEYTYPE_BINARY;
  keyinfo[2].seg[0].start = 12;
  keyinfo[2].seg[0].length = 8;
  keyinfo[2].seg[0].null_bit = 0;
  keyinfo[2].seg[0].charset = cs;
  keyinfo[3].seg = keyseg + 4;
  keyinfo[3].keysegs = 1;
  keyinfo[3].flag = HA_NOSAME;
  keyinfo[3].algorithm = HA_KEY_ALG_HASH;
  keyinfo[3].seg[0].type = HA_KEYTYPE_BINARY;
  keyinfo[3].seg[0].start = 37;
  keyinfo[3].seg[0].length = 1;
  keyinfo[3].seg[0].null_bit = 1;
  keyinfo[3].seg[0].null_pos = 38;
  keyinfo[3].seg[0].charset = cs;

  memset(key1, 0, sizeof(key1));
  memset(key3, 0, sizeof(key3));

  printf("- Creating heap-file\n");
  if (heap_create(filename, &hp_create_info, &tmp_share, &unused) ||
      !(file = heap_open(filename, 2)))
    goto err;
  signal(SIGINT, endprog);

  printf("- Writing records:s\n");
  my_stpcpy((char *)record, "          ..... key");

  for (i = 0; i < recant; i++) {
    n1 = rnd(1000);
    n2 = rnd(100);
    n3 = rnd(MY_MIN(recant * 5, MAX_RECORDS));
    make_record(record, n1, n2, n3, "Pos", write_count);

    if (heap_write(file, record)) {
      if (my_errno() != HA_ERR_FOUND_DUPP_KEY || key3[n3] == 0) {
        printf("Error: %d in write at record: %d\n", my_errno(), i);
        goto err;
      }
      if (verbose) printf("   Double key: %d\n", n3);
    } else {
      if (key3[n3] == 1) {
        printf("Error: Didn't get error when writing second key: '%8d'\n", n3);
        goto err;
      }
      write_count++;
      key1[n1]++;
      key3[n3] = 1;
      key_check += n1;
    }
    if (testflag == 1 && heap_check_heap(file, 0)) {
      puts("Heap keys crashed");
      goto err;
    }
  }
  if (testflag == 1) goto end;
  if (heap_check_heap(file, 0)) {
    puts("Heap keys crashed");
    goto err;
  }

  printf("- Delete\n");
  for (i = 0; i < write_count / 10; i++) {
    for (j = rnd(1000) + 1; j > 0 && key1[j] == 0; j--)
      ;
    if (j != 0) {
      sprintf((char *)key, "%6d", j);
      if (heap_rkey(file, record, 0, key, 6, HA_READ_KEY_EXACT)) {
        printf("can't find key1: \"%s\"\n", (char *)key);
        goto err;
      }
      if (heap_delete(file, record)) {
        printf("error: %d; can't delete record: \"%s\"\n", my_errno(),
               (char *)record);
        goto err;
      }
      opt_delete++;
      key1[atoi((char *)record + keyinfo[0].seg[0].start)]--;
      key3[atoi((char *)record + keyinfo[2].seg[0].start)] = 0;
      key_check -= atoi((char *)record);
      if (testflag == 2 && heap_check_heap(file, 0)) {
        puts("Heap keys crashed");
        goto err;
      }
    } else
      puts("Warning: Skipping delete test because no dupplicate keys");
  }
  if (testflag == 2) goto end;
  if (heap_check_heap(file, 0)) {
    puts("Heap keys crashed");
    goto err;
  }

  printf("- Update\n");
  for (i = 0; i < write_count / 10; i++) {
    n1 = rnd(1000);
    n2 = rnd(100);
    n3 = rnd(MY_MIN(recant * 2, MAX_RECORDS));
    make_record(record2, n1, n2, n3, "XXX", update);
    if (rnd(2) == 1) {
      if (heap_scan_init(file)) goto err;
      j = rnd(write_count - opt_delete);
      while ((error = heap_scan(file, record) == HA_ERR_RECORD_DELETED) ||
             (!error && j)) {
        if (!error) j--;
      }
      if (error) goto err;
    } else {
      for (j = rnd(1000) + 1; j > 0 && key1[j] == 0; j--)
        ;
      if (!key1[j]) continue;
      sprintf((char *)key, "%6d", j);
      if (heap_rkey(file, record, 0, key, 6, HA_READ_KEY_EXACT)) {
        printf("can't find key1: \"%s\"\n", (char *)key);
        goto err;
      }
    }
    if (heap_update(file, record, record2)) {
      if (my_errno() != HA_ERR_FOUND_DUPP_KEY || key3[n3] == 0) {
        printf("error: %d; can't update:\nFrom: \"%s\"\nTo:   \"%s\"\n",
               my_errno(), (char *)record, (char *)record2);
        goto err;
      }
      if (verbose)
        printf("Double key when tried to update:\nFrom: \"%s\"\nTo:   \"%s\"\n",
               (char *)record, (char *)record2);
    } else {
      key1[atoi((char *)record + keyinfo[0].seg[0].start)]--;
      key3[atoi((char *)record + keyinfo[2].seg[0].start)] = 0;
      key1[n1]++;
      key3[n3] = 1;
      update++;
      key_check = key_check - atoi((char *)record) + n1;
    }
    if (testflag == 3 && heap_check_heap(file, 0)) {
      puts("Heap keys crashed");
      goto err;
    }
  }
  if (testflag == 3) goto end;
  if (heap_check_heap(file, 0)) {
    puts("Heap keys crashed");
    goto err;
  }

  for (i = 999, dupp_keys = found_key = 0; i > 0; i--) {
    if (key1[i] > dupp_keys) {
      dupp_keys = key1[i];
      found_key = i;
    }
    sprintf((char *)key, "%6d", found_key);
  }

  if (dupp_keys > 3) {
    if (!silent) printf("- Read first key - next - delete - next -> last\n");
    DBUG_PRINT("progpos", ("first - next - delete - next -> last"));

    if (heap_rkey(file, record, 0, key, 6, HA_READ_KEY_EXACT)) goto err;
    if (heap_rnext(file, record3)) goto err;
    if (heap_delete(file, record3)) goto err;
    key_check -= atoi((char *)record3);
    key1[atoi((char *)record + keyinfo[0].seg[0].start)]--;
    key3[atoi((char *)record + keyinfo[2].seg[0].start)] = 0;
    opt_delete++;
    ant = 2;
    while ((error = heap_rnext(file, record3)) == 0 ||
           error == HA_ERR_RECORD_DELETED)
      if (!error) ant++;
    if (ant != dupp_keys) {
      printf("next: I can only find: %d records of %d\n", ant, dupp_keys);
      goto end;
    }
    dupp_keys--;
    if (heap_check_heap(file, 0)) {
      puts("Heap keys crashed");
      goto err;
    }

    if (!silent)
      printf(
          "- Read last key - delete - prev - prev - opt_delete - prev -> "
          "first\n");

    if (heap_rlast(file, record3, 0)) goto err;
    if (heap_delete(file, record3)) goto err;
    key_check -= atoi((char *)record3);
    key1[atoi((char *)record + keyinfo[0].seg[0].start)]--;
    key3[atoi((char *)record + keyinfo[2].seg[0].start)] = 0;
    opt_delete++;
    if (heap_rprev(file, record3) || heap_rprev(file, record3)) goto err;
    if (heap_delete(file, record3)) goto err;
    key_check -= atoi((char *)record3);
    key1[atoi((char *)record + keyinfo[0].seg[0].start)]--;
    key3[atoi((char *)record + keyinfo[2].seg[0].start)] = 0;
    opt_delete++;
    ant = 3;
    while ((error = heap_rprev(file, record3)) == 0 ||
           error == HA_ERR_RECORD_DELETED) {
      if (!error) ant++;
    }
    if (ant != dupp_keys) {
      printf("next: I can only find: %d records of %d\n", ant, dupp_keys);
      goto end;
    }
    dupp_keys -= 2;
    if (heap_check_heap(file, 0)) {
      puts("Heap keys crashed");
      goto err;
    }
  } else
    puts("Warning: Not enough duplicated keys:  Skipping delete key check");

  if (!silent) printf("- Read (first) - next - delete - next -> last\n");
  DBUG_PRINT("progpos", ("first - next - delete - next -> last"));

  if (heap_scan_init(file)) goto err;
  while ((error = heap_scan(file, record3) == HA_ERR_RECORD_DELETED))
    ;
  if (error) goto err;
  if (heap_delete(file, record3)) goto err;
  key_check -= atoi((char *)record3);
  opt_delete++;
  key1[atoi((char *)record + keyinfo[0].seg[0].start)]--;
  key3[atoi((char *)record + keyinfo[2].seg[0].start)] = 0;
  ant = 0;
  while ((error = heap_scan(file, record3)) == 0 ||
         error == HA_ERR_RECORD_DELETED)
    if (!error) ant++;
  if (ant != write_count - opt_delete) {
    printf("next: Found: %d records of %d\n", ant, write_count - opt_delete);
    goto end;
  }
  if (heap_check_heap(file, 0)) {
    puts("Heap keys crashed");
    goto err;
  }

  puts("- Test if: Read rrnd - same - rkey - same");
  DBUG_PRINT("progpos", ("Read rrnd - same"));
  pos = rnd(write_count - opt_delete - 5) + 5;
  heap_scan_init(file);
  i = 5;
  while ((error = heap_scan(file, record)) == HA_ERR_RECORD_DELETED ||
         (error == 0 && pos)) {
    if (!error) pos--;
    if (!error && (i-- == 0)) {
      memmove(record3, record, reclength);
      heap_position(file, &position);
    }
  }
  if (error) goto err;
  memmove(record2, record, reclength);
  if (heap_rsame(file, record, -1) || heap_rsame(file, record2, 2)) goto err;
  if (memcmp(record2, record, reclength)) {
    puts("heap_rsame didn't find right record");
    goto end;
  }

  puts("- Test of read through position");
  if (heap_rrnd(file, record, &position)) goto err;
  if (memcmp(record3, record, reclength)) {
    puts("heap_frnd didn't find right record");
    goto end;
  }

  printf("- heap_info\n");
  {
    HEAPINFO info;
    heap_info(file, &info, 0);
    /* We have to test with opt_delete +1 as this may be the case if the last
       inserted row was a duplicate key */
    if (info.records != write_count - opt_delete ||
        (info.deleted != opt_delete && info.deleted != opt_delete + 1)) {
      puts("Wrong info from heap_info");
      printf("Got: records: %ld(%d)  deleted: %ld(%d)\n", info.records,
             write_count - opt_delete, info.deleted, opt_delete);
    }
  }

  printf("- Read through all records with scan\n");
  ant = check2 = 0;
  heap_scan_init(file);
  while ((error = heap_scan(file, record)) != HA_ERR_END_OF_FILE &&
         ant < write_count + 10) {
    if (!error) {
      ant++;
      check2 += calc_check(record, reclength);
    }
  }
  if (ant != write_count - opt_delete) {
    printf("scan: I can only find: %d records of %d\n", ant,
           write_count - opt_delete);
    goto end;
  }

  for (i = 999, dupp_keys = found_key = 0; i > 0; i--) {
    if (key1[i] > dupp_keys) {
      dupp_keys = key1[i];
      found_key = i;
    }
    sprintf((char *)key, "%6d", found_key);
  }
  printf("- Read through all keys with first-next-last-prev\n");
  ant = 0;
  for (error = heap_rkey(file, record, 0, key, 6, HA_READ_KEY_EXACT); !error;
       error = heap_rnext(file, record))
    ant++;
  if (ant != dupp_keys) {
    printf("first-next: I can only find: %d records of %d\n", ant, dupp_keys);
    goto end;
  }

  ant = 0;
  for (error = heap_rlast(file, record, 0); !error;
       error = heap_rprev(file, record)) {
    ant++;
    check2 += calc_check(record, reclength);
  }
  if (ant != dupp_keys) {
    printf("last-prev: I can only find: %d records of %d\n", ant, dupp_keys);
    goto end;
  }

  if (testflag == 4) goto end;

  printf("- Reading through all rows through keys\n");
  if (!(file2 = heap_open(filename, 2))) goto err;
  if (heap_scan_init(file)) goto err;
  while ((error = heap_scan(file, record)) != HA_ERR_END_OF_FILE) {
    if (error == 0) {
      if (heap_rkey(file2, record2, 2, record + keyinfo[2].seg[0].start, 8,
                    HA_READ_KEY_EXACT)) {
        printf("can't find key3: \"%.8s\"\n", record + keyinfo[2].seg[0].start);
        goto err;
      }
    }
  }
  heap_close(file2);

  printf("- Creating output heap-file 2\n");
  hp_create_info.keys = 1;
  hp_create_info.max_records = 0;
  hp_create_info.min_records = 0;
  if (heap_create(filename2, &hp_create_info, &tmp_share, &unused) ||
      !(file2 = heap_open_from_share_and_register(tmp_share, 2)))
    goto err;

  printf("- Copying and removing records\n");
  if (heap_scan_init(file)) goto err;
  while ((error = heap_scan(file, record)) != HA_ERR_END_OF_FILE) {
    if (error == 0) {
      if (heap_write(file2, record)) goto err;
      key_check -= atoi((char *)record);
      write_count++;
      if (heap_delete(file, record)) goto err;
      opt_delete++;
    }
    pos++;
  }
  printf("- Checking heap tables\n");
  if (heap_check_heap(file, 1) || heap_check_heap(file2, 1)) {
    puts("Heap keys crashed");
    goto err;
  }

  if (my_errno() != HA_ERR_END_OF_FILE)
    printf("error: %d from heap_rrnd\n", my_errno());
  if (key_check)
    printf("error: Some read got wrong: check is %ld\n", (long)key_check);

end:
  printf("\nFollowing test have been made:\n");
  printf("Write records: %d\nUpdate records: %d\nDelete records: %d\n",
         write_count, update, opt_delete);
  heap_clear(file);
  heap_clear(file2);
  if (heap_close(file) || (file2 && heap_close(file2))) goto err;
  heap_delete_table(filename2);
  hp_panic(HA_PANIC_CLOSE);
  my_end(MY_GIVE_INFO);
  return (0);
err:
  printf("Got error: %d when using heap-database\n", my_errno());
  (void)heap_close(file);
  return (1);
} /* main */

/* Read options */

static int get_options(int argc, char *argv[]) {
  char *pos, *progname;

  progname = argv[0];

  while (--argc > 0 && *(pos = *(++argv)) == '-') {
    switch (*++pos) {
      case 'B': /* Big file */
        flag = 1;
        break;
      case 'v': /* verbose */
        verbose = 1;
        break;
      case 'm': /* records */
        recant = atoi(++pos);
        break;
      case 's':
        silent = 1;
        break;
      case 't':
        testflag = atoi(++pos); /* testmod */
        break;
      case 'V':
      case 'I':
      case '?':
        printf("%s  Ver 1.1 for %s at %s\n", progname, SYSTEM_TYPE,
               MACHINE_TYPE);
        puts("TCX Datakonsult AB, by Monty, for your professional use\n");
        printf("Usage: %s [-?ABIKLsWv] [-m#] [-t#]\n", progname);
        exit(0);
      case '#':
        DBUG_PUSH(++pos);
        break;
    }
  }
  return 0;
} /* get options */

/* Generate a random value in intervall 0 <=x <= n */

static int rnd(int max_value) {
  return (int)((rand() & 32767) / 32767.0 * max_value);
} /* rnd */

static void endprog(int sig_number MY_ATTRIBUTE((unused))) {
  {
    hp_panic(HA_PANIC_CLOSE);
    my_end(1);
    exit(1);
  }
}

static int calc_check(uchar *buf, uint length) {
  int check = 0;
  while (length--) check += (int)(uchar) * (buf++);
  return check;
}

static void make_record(uchar *record, uint n1, uint n2, uint n3,
                        const char *mark, uint count) {
  memset(record, ' ', reclength);
  sprintf((char *)record, "%6d:%4d:%8d:%3.3s: %4d", n1, n2, n3, mark, count);
  record[37] = 'A'; /* Store A in null key */
  record[38] = 1;   /* set as null */
}
