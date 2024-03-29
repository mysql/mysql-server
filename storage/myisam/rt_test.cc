/* Copyright (c) 2002, 2023, Oracle and/or its affiliates.

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

/* Testing of the basic functions of a MyISAM rtree table         */
/* Written by Alex Barkov who has a shared copyright to this code */

#include "myisam.h"
#include "rt_index.h"

#define MAX_REC_LENGTH 1024
#define ndims 2
#define KEYALG HA_KEY_ALG_RTREE

static int read_with_pos(MI_INFO *file, int silent);
static void create_record(uchar *record, uint rownr);
static void create_record1(uchar *record, uint rownr);
static void print_record(uchar *record, my_off_t offs, const char *tail);
static int run_test(const char *filename);

static double rt_data[] = {
    /*1*/ 0,    10,  0,  10,
    /*2*/ 5,    15,  0,  10,
    /*3*/ 0,    10,  5,  15,
    /*4*/ 10,   20,  10, 20,
    /*5*/ 0,    10,  0,  10,
    /*6*/ 5,    15,  0,  10,
    /*7*/ 0,    10,  5,  15,
    /*8*/ 10,   20,  10, 20,
    /*9*/ 0,    10,  0,  10,
    /*10*/ 5,   15,  0,  10,
    /*11*/ 0,   10,  5,  15,
    /*12*/ 10,  20,  10, 20,
    /*13*/ 0,   10,  0,  10,
    /*14*/ 5,   15,  0,  10,
    /*15*/ 0,   10,  5,  15,
    /*16*/ 10,  20,  10, 20,
    /*17*/ 5,   15,  0,  10,
    /*18*/ 0,   10,  5,  15,
    /*19*/ 10,  20,  10, 20,
    /*20*/ 0,   10,  0,  10,

    /*1*/ 100,  110, 0,  10,
    /*2*/ 105,  115, 0,  10,
    /*3*/ 100,  110, 5,  15,
    /*4*/ 110,  120, 10, 20,
    /*5*/ 100,  110, 0,  10,
    /*6*/ 105,  115, 0,  10,
    /*7*/ 100,  110, 5,  15,
    /*8*/ 110,  120, 10, 20,
    /*9*/ 100,  110, 0,  10,
    /*10*/ 105, 115, 0,  10,
    /*11*/ 100, 110, 5,  15,
    /*12*/ 110, 120, 10, 20,
    /*13*/ 100, 110, 0,  10,
    /*14*/ 105, 115, 0,  10,
    /*15*/ 100, 110, 5,  15,
    /*16*/ 110, 120, 10, 20,
    /*17*/ 105, 115, 0,  10,
    /*18*/ 100, 110, 5,  15,
    /*19*/ 110, 120, 10, 20,
    /*20*/ 100, 110, 0,  10, -1};

int main(int argc [[maybe_unused]], char *argv[] [[maybe_unused]]) {
  MY_INIT(argv[0]);
  exit(run_test("rt_test"));
}

static int run_test(const char *filename) {
  MI_INFO *file;
  MI_UNIQUEDEF uniquedef;
  MI_CREATE_INFO create_info;
  MI_COLUMNDEF recinfo[20];
  MI_KEYDEF keyinfo[20];
  HA_KEYSEG keyseg[20];
  key_range range;

  int silent = 0;
  int opt_unique = 0;
  int create_flag = 0;
  int key_type = HA_KEYTYPE_DOUBLE;
  int key_length = 8;
  int null_fields = 0;
  int nrecords = sizeof(rt_data) / (sizeof(double) * 4); /* 3000;*/
  int rec_length = 0;
  int uniques = 0;
  int i;
  int error;
  int row_count = 0;
  uchar record[MAX_REC_LENGTH];
  uchar read_record[MAX_REC_LENGTH];
  int upd = 10;
  ha_rows hrows;

  /* Define a column for NULLs and DEL markers*/

  recinfo[0].type = FIELD_NORMAL;
  recinfo[0].length = 1; /* For NULL bits */
  rec_length = 1;

  /* Define 2*ndims columns for coordinates*/

  for (i = 1; i <= 2 * ndims; i++) {
    recinfo[i].type = FIELD_NORMAL;
    recinfo[i].length = key_length;
    rec_length += key_length;
  }

  /* Define a key with 2*ndims segments */

  keyinfo[0].seg = keyseg;
  keyinfo[0].keysegs = 2 * ndims;
  keyinfo[0].flag = 0;
  keyinfo[0].key_alg = KEYALG;

  for (i = 0; i < 2 * ndims; i++) {
    keyinfo[0].seg[i].type = key_type;
    keyinfo[0].seg[i].flag = 0; /* Things like HA_REVERSE_SORT */
    keyinfo[0].seg[i].start = (key_length * i) + 1;
    keyinfo[0].seg[i].length = key_length;
    keyinfo[0].seg[i].null_bit = null_fields ? 2 : 0;
    keyinfo[0].seg[i].null_pos = 0;
    keyinfo[0].seg[i].language = default_charset_info->number;
  }

  if (!silent) printf("- Creating isam-file\n");

  memset(&create_info, 0, sizeof(create_info));
  create_info.max_rows = 10000000;

  if (mi_create(filename, 1,                         /*  keys   */
                keyinfo, 1 + 2 * ndims + opt_unique, /* columns */
                recinfo, uniques, &uniquedef, &create_info, create_flag))
    goto err;

  if (!silent) printf("- Open isam-file\n");

  if (!(file = mi_open(filename, 2, HA_OPEN_ABORT_IF_LOCKED))) goto err;

  if (!silent) printf("- Writing key:s\n");

  for (i = 0; i < nrecords; i++) {
    create_record(record, i);
    error = mi_write(file, record);
    print_record(record, mi_position(file), "\n");
    if (!error) {
      row_count++;
    } else {
      printf("mi_write: %d\n", error);
      goto err;
    }
  }

  if ((error = read_with_pos(file, silent))) goto err;

  if (!silent) printf("- Reading rows with key\n");

  for (i = 0; i < nrecords; i++) {
    my_errno = 0;
    create_record(record, i);

    memset(read_record, 0, MAX_REC_LENGTH);
    error = mi_rkey(file, read_record, 0, record + 1, 0, HA_READ_MBR_EQUAL);

    if (error && error != HA_ERR_KEY_NOT_FOUND) {
      printf("     mi_rkey: %3d  errno: %3d\n", error, my_errno);
      goto err;
    }
    if (error == HA_ERR_KEY_NOT_FOUND) {
      print_record(record, mi_position(file), "  NOT FOUND\n");
      continue;
    }
    print_record(read_record, mi_position(file), "\n");
  }

  if (!silent) printf("- Deleting rows\n");
  for (i = 0; i < nrecords / 4; i++) {
    my_errno = 0;
    memset(read_record, 0, MAX_REC_LENGTH);
    error = mi_rrnd(file, read_record, i == 0 ? 0L : HA_OFFSET_ERROR);
    if (error) {
      printf("pos: %2d  mi_rrnd: %3d  errno: %3d\n", i, error, my_errno);
      goto err;
    }
    print_record(read_record, mi_position(file), "\n");

    error = mi_delete(file, read_record);
    if (error) {
      printf("pos: %2d mi_delete: %3d errno: %3d\n", i, error, my_errno);
      goto err;
    }
  }

  if (!silent) printf("- Updating rows with position\n");
  for (i = 0; i < (nrecords - nrecords / 4); i++) {
    my_errno = 0;
    memset(read_record, 0, MAX_REC_LENGTH);
    error = mi_rrnd(file, read_record, i == 0 ? 0L : HA_OFFSET_ERROR);
    if (error) {
      if (error == HA_ERR_RECORD_DELETED) continue;
      printf("pos: %2d  mi_rrnd: %3d  errno: %3d\n", i, error, my_errno);
      goto err;
    }
    print_record(read_record, mi_position(file), "");
    create_record(record, i + nrecords * upd);
    printf("\t-> ");
    print_record(record, mi_position(file), "\n");
    error = mi_update(file, read_record, record);
    if (error) {
      printf("pos: %2d  mi_update: %3d  errno: %3d\n", i, error, my_errno);
      goto err;
    }
  }

  if ((error = read_with_pos(file, silent))) goto err;

  if (!silent) printf("- Test mi_rkey then a sequence of mi_rnext_same\n");

  create_record(record, nrecords * 4 / 5);
  print_record(record, 0, "  search for\n");

  if ((error = mi_rkey(file, read_record, 0, record + 1, 0,
                       HA_READ_MBR_INTERSECT))) {
    printf("mi_rkey: %3d  errno: %3d\n", error, my_errno);
    goto err;
  }
  print_record(read_record, mi_position(file), "  mi_rkey\n");
  row_count = 1;

  for (;;) {
    if ((error = mi_rnext_same(file, read_record))) {
      if (error == HA_ERR_END_OF_FILE) break;
      printf("mi_next: %3d  errno: %3d\n", error, my_errno);
      goto err;
    }
    print_record(read_record, mi_position(file), "  mi_rnext_same\n");
    row_count++;
  }
  printf("     %d rows\n", row_count);

  if (!silent) printf("- Test mi_rfirst then a sequence of mi_rnext\n");

  error = mi_rfirst(file, read_record, 0);
  if (error) {
    printf("mi_rfirst: %3d  errno: %3d\n", error, my_errno);
    goto err;
  }
  row_count = 1;
  print_record(read_record, mi_position(file), "  mi_frirst\n");

  for (i = 0; i < nrecords; i++) {
    if ((error = mi_rnext(file, read_record, 0))) {
      if (error == HA_ERR_END_OF_FILE) break;
      printf("mi_next: %3d  errno: %3d\n", error, my_errno);
      goto err;
    }
    print_record(read_record, mi_position(file), "  mi_rnext\n");
    row_count++;
  }
  printf("     %d rows\n", row_count);

  if (!silent) printf("- Test mi_records_in_range()\n");

  create_record1(record, nrecords * 4 / 5);
  print_record(record, 0, "\n");

  range.key = record + 1;
  range.length = 1000; /* Big enough */
  range.flag = HA_READ_MBR_INTERSECT;
  hrows = mi_records_in_range(file, 0, &range, (key_range *)0);
  printf("     %ld rows\n", (long)hrows);

  if (mi_close(file)) goto err;
  my_end(MY_CHECK_ERROR);

  return 0;

err:
  printf("got error: %3d when using myisam-database\n", my_errno);
  return 1; /* skip warning */
}

static int read_with_pos(MI_INFO *file, int silent) {
  int error;
  int i;
  uchar read_record[MAX_REC_LENGTH];

  if (!silent) printf("- Reading rows with position\n");
  for (i = 0;; i++) {
    my_errno = 0;
    memset(read_record, 0, MAX_REC_LENGTH);
    error = mi_rrnd(file, read_record, i == 0 ? 0L : HA_OFFSET_ERROR);
    if (error) {
      if (error == HA_ERR_END_OF_FILE) break;
      if (error == HA_ERR_RECORD_DELETED) continue;
      printf("pos: %2d  mi_rrnd: %3d  errno: %3d\n", i, error, my_errno);
      return error;
    }
    print_record(read_record, mi_position(file), "\n");
  }
  return 0;
}

static void print_record(uchar *record, my_off_t offs [[maybe_unused]],
                         const char *tail) {
  int i;
  uchar *pos;
  double c;

  printf("     rec=(%d)", (unsigned char)record[0]);
  for (pos = record + 1, i = 0; i < 2 * ndims; i++) {
    memcpy(&c, pos, sizeof(c));
    float8get(&c, pos);
    printf(" %.14g ", c);
    pos += sizeof(c);
  }
  printf("pos=%ld", (long int)offs);
  printf("%s", tail);
}

static void create_record1(uchar *record, uint rownr) {
  int i;
  uchar *pos;
  double c = rownr + 10;

  memset(record, 0, MAX_REC_LENGTH);
  record[0] = 0x01; /* DEL marker */

  for (pos = record + 1, i = 0; i < 2 * ndims; i++) {
    memcpy(pos, &c, sizeof(c));
    float8store(pos, c);
    pos += sizeof(c);
  }
}

static void create_record(uchar *record, uint rownr) {
  int i;
  uchar *pos;
  double *data = rt_data + rownr * 4;
  record[0] = 0x01; /* DEL marker */
  for (pos = record + 1, i = 0; i < ndims * 2; i++) {
    float8store(pos, data[i]);
    pos += 8;
  }
}

#include "mi_extrafunc.h"
