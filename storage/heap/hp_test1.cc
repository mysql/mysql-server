/* Copyright (c) 2000, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/* Test av heap-database */
/* Programmet skapar en heap-databas. Till denna skrivs ett antal poster.
   Databasen st{ngs. D{refter |ppnas den p} nytt och en del av posterna
   raderas.
*/

#include <sys/types.h>

#include "heap.h"
#include "m_string.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "my_thread_local.h"

static int get_options(int argc, char *argv[]);

static int flag = 0, verbose = 0, remove_ant = 0, flags[50];

int main(int argc, char **argv) {
  int i, j, error;
  HP_INFO *file;
  uchar record[128], key[32];
  const char *filename;
  HP_KEYDEF keyinfo[10];
  HA_KEYSEG keyseg[4];
  HP_CREATE_INFO hp_create_info;
  HP_SHARE *tmp_share;
  bool unused;
  MY_INIT(argv[0]);

  filename = "test1";
  get_options(argc, argv);

  memset(&hp_create_info, 0, sizeof(hp_create_info));
  hp_create_info.max_table_size = 1024L * 1024L;
  hp_create_info.keys = 1;
  hp_create_info.keydef = keyinfo;
  hp_create_info.reclength = 30;
  hp_create_info.max_records = (ulong)flag * 100000L;
  hp_create_info.min_records = 10UL;

  keyinfo[0].keysegs = 1;
  keyinfo[0].seg = keyseg;
  keyinfo[0].algorithm = HA_KEY_ALG_HASH;
  keyinfo[0].seg[0].type = HA_KEYTYPE_BINARY;
  keyinfo[0].seg[0].start = 1;
  keyinfo[0].seg[0].length = 6;
  keyinfo[0].seg[0].charset = &my_charset_latin1;
  keyinfo[0].seg[0].null_bit = 0;
  keyinfo[0].flag = HA_NOSAME;

  memset(flags, 0, sizeof(flags));

  printf("- Creating heap-file\n");
  if (heap_create(filename, &hp_create_info, &tmp_share, &unused) ||
      !(file = heap_open(filename, 2)))
    goto err;
  printf("- Writing records:s\n");
  my_stpcpy((char *)record, "          ..... key           ");

  for (i = 49; i >= 1; i -= 2) {
    j = i % 25 + 1;
    sprintf((char *)key, "%6d", j);
    memmove(record + 1, key, 6);
    error = heap_write(file, record);
    if (heap_check_heap(file, false)) {
      puts("Heap keys crashed");
      goto err;
    }
    flags[j] = 1;
    if (verbose || error)
      printf("J= %2d  heap_write: %d  my_errno: %d\n", j, error, my_errno());
  }
  if (heap_close(file)) goto err;
  printf("- Reopening file\n");
  if (!(file = heap_open(filename, 2))) goto err;

  printf("- Removing records\n");
  for (i = 1; i <= 10; i++) {
    if (i == remove_ant) {
      (void)heap_close(file);
      return (0);
    }
    sprintf((char *)key, "%6d", (j = (int)((rand() & 32767) / 32767. * 25)));
    if ((error = heap_rkey(file, record, 0, key, 6, HA_READ_KEY_EXACT))) {
      if (verbose ||
          (flags[j] == 1 || (error && my_errno() != HA_ERR_KEY_NOT_FOUND)))
        printf("key: %s  rkey:   %3d  my_errno: %3d\n", (char *)key, error,
               my_errno());
    } else {
      error = heap_delete(file, record);
      if (error || verbose)
        printf("key: %s  delete: %d  my_errno: %d\n", (char *)key, error,
               my_errno());
      flags[j] = 0;
    }
    if (heap_check_heap(file, false)) {
      puts("Heap keys crashed");
      goto err;
    }
  }

  printf("- Reading records with key\n");
  for (i = 1; i <= 25; i++) {
    sprintf((char *)key, "%6d", i);
    memmove(record + 1, key, 6);
    set_my_errno(0);
    error = heap_rkey(file, record, 0, key, 6, HA_READ_KEY_EXACT);
    if (verbose || (error == 0 && flags[i] != 1) ||
        (error && (flags[i] != 0 || my_errno() != HA_ERR_KEY_NOT_FOUND))) {
      printf("key: %s  rkey: %3d  my_errno: %3d  record: %s\n", (char *)key,
             error, my_errno(), record + 1);
    }
  }

  if (heap_close(file) || hp_panic(HA_PANIC_CLOSE)) goto err;
  my_end(MY_GIVE_INFO);
  return (0);
err:
  printf("got error: %d when using heap-database\n", my_errno());
  return (1);
} /* main */

/* Read options */

static int get_options(int argc, char **argv) {
  char *pos;

  while (--argc > 0 && *(pos = *(++argv)) == '-') {
    switch (*++pos) {
      case 'B': /* Big file */
        flag = 1;
        break;
      case 'v': /* verbose */
        verbose = 1;
        break;
      case 'm':
        remove_ant = atoi(++pos);
        break;
      case 'V':
        printf("hp_test1    Ver 3.0 \n");
        exit(0);
      case '#':
        DBUG_PUSH(++pos);
        break;
    }
  }
  return 0;
} /* get options */
