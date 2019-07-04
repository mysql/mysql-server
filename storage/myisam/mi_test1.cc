/*
   Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

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

/* Testing of the basic functions of a MyISAM table */

#include <m_string.h>
#include <my_getopt.h>
#include "my_byteorder.h"
#include "myisam.h"
#include "sql/field.h"

#define MAX_REC_LENGTH 1024

static void usage();

static int rec_pointer_size = 0, flags[50];
static int key_field = FIELD_SKIP_PRESPACE, extra_field = FIELD_SKIP_ENDSPACE;
static int key_type = HA_KEYTYPE_NUM;
static int create_flag = 0;

static uint insert_count, update_count, remove_count;
static uint pack_keys = 0, pack_seg = 0, key_length;
static uint unique_key = HA_NOSAME;
static bool key_cacheing, null_fields, silent, skip_update, opt_unique, verbose;
static MI_COLUMNDEF recinfo[4];
static MI_KEYDEF keyinfo[10];
static HA_KEYSEG keyseg[10];
static HA_KEYSEG uniqueseg[10];

static int run_test(const char *filename);
static void get_options(int argc, char *argv[]);
static void create_key(uchar *key, uint rownr);
static void create_record(uchar *record, uint rownr);
static void update_record(uchar *record);

/*
  strappend(dest, len, fill) appends fill-characters to a string so that
  the result length == len. If the string is longer than len it's
  trunked. The des+len character is allways set to NULL.
*/
static inline void strappend(char *s, size_t len, char fill) {
  char *endpos;

  endpos = s + len;
  while (*s++)
    ;
  s--;
  while (s < endpos) *(s++) = fill;
  *(endpos) = '\0';
}

int main(int argc, char *argv[]) {
  MY_INIT(argv[0]);
  my_init();
  if (key_cacheing)
    init_key_cache(dflt_key_cache, KEY_CACHE_BLOCK_SIZE, IO_SIZE * 16, 0, 0);
  get_options(argc, argv);

  exit(run_test("test1"));
}

static int run_test(const char *filename) {
  MI_INFO *file;
  int i, j, error, deleted, rec_length, uniques = 0;
  ha_rows found, row_count;
  my_off_t pos;
  uchar record[MAX_REC_LENGTH], key[MAX_REC_LENGTH],
      read_record[MAX_REC_LENGTH];
  MI_UNIQUEDEF uniquedef;
  MI_CREATE_INFO create_info;

  memset(recinfo, 0, sizeof(recinfo));

  /* First define 2 columns */
  recinfo[0].type = FIELD_NORMAL;
  recinfo[0].length = 1; /* For NULL bits */
  recinfo[1].type = key_field;
  recinfo[1].length =
      (key_field == FIELD_BLOB ? 4 + portable_sizeof_char_ptr : key_length);
  if (key_field == FIELD_VARCHAR)
    recinfo[1].length += HA_VARCHAR_PACKLENGTH(key_length);
  ;
  recinfo[2].type = extra_field;
  recinfo[2].length =
      (extra_field == FIELD_BLOB ? 4 + portable_sizeof_char_ptr : 24);
  if (extra_field == FIELD_VARCHAR)
    recinfo[2].length += HA_VARCHAR_PACKLENGTH(recinfo[2].length);
  if (opt_unique) {
    recinfo[3].type = FIELD_CHECK;
    recinfo[3].length = MI_UNIQUE_HASH_LENGTH;
  }
  rec_length = recinfo[0].length + recinfo[1].length + recinfo[2].length +
               recinfo[3].length;

  if (key_type == HA_KEYTYPE_VARTEXT1 && key_length > 255)
    key_type = HA_KEYTYPE_VARTEXT2;

  /* Define a key over the first column */
  keyinfo[0].seg = keyseg;
  keyinfo[0].keysegs = 1;
  keyinfo[0].block_length = 0; /* Default block length */
  keyinfo[0].key_alg = HA_KEY_ALG_BTREE;
  keyinfo[0].seg[0].type = key_type;
  keyinfo[0].seg[0].flag = pack_seg;
  keyinfo[0].seg[0].start = 1;
  keyinfo[0].seg[0].length = key_length;
  keyinfo[0].seg[0].null_bit = null_fields ? 2 : 0;
  keyinfo[0].seg[0].null_pos = 0;
  keyinfo[0].seg[0].language = default_charset_info->number;
  if (pack_seg & HA_BLOB_PART) {
    keyinfo[0].seg[0].bit_start = 4; /* Length of blob length */
  }
  keyinfo[0].flag = (uint8)(pack_keys | unique_key);

  memset(flags, 0, sizeof(flags));
  if (opt_unique) {
    uint start;
    uniques = 1;
    memset(&uniquedef, 0, sizeof(uniquedef));
    memset(uniqueseg, 0, sizeof(uniqueseg));
    uniquedef.seg = uniqueseg;
    uniquedef.keysegs = 2;

    /* Make a unique over all columns (except first NULL fields) */
    for (i = 0, start = 1; i < 2; i++) {
      uniqueseg[i].start = start;
      start += recinfo[i + 1].length;
      uniqueseg[i].length = recinfo[i + 1].length;
      uniqueseg[i].language = default_charset_info->number;
    }
    uniqueseg[0].type = key_type;
    uniqueseg[0].null_bit = null_fields ? 2 : 0;
    uniqueseg[1].type = HA_KEYTYPE_TEXT;
    if (extra_field == FIELD_BLOB) {
      uniqueseg[1].length = 0;    /* The whole blob */
      uniqueseg[1].bit_start = 4; /* long blob */
      uniqueseg[1].flag |= HA_BLOB_PART;
    } else if (extra_field == FIELD_VARCHAR)
      uniqueseg[1].flag |= HA_VAR_LENGTH_PART;
  } else
    uniques = 0;

  if (!silent) printf("- Creating isam-file\n");
  memset(&create_info, 0, sizeof(create_info));
  create_info.max_rows =
      (ulong)(rec_pointer_size ? (1L << (rec_pointer_size * 8)) / 40 : 0);
  if (mi_create(filename, 1, keyinfo, 3 + opt_unique, recinfo, uniques,
                &uniquedef, &create_info, create_flag))
    goto err;
  if (!(file = mi_open(filename, 2, HA_OPEN_ABORT_IF_LOCKED))) goto err;
  if (!silent) printf("- Writing key:s\n");

  my_errno = 0;
  row_count = deleted = 0;
  for (i = 49; i >= 1; i -= 2) {
    if (insert_count-- == 0) {
      (void)mi_close(file);
      exit(0);
    }
    j = i % 25 + 1;
    create_record(record, j);
    error = mi_write(file, record);
    if (!error) row_count++;
    flags[j] = 1;
    if (verbose || error)
      printf("J= %2d  mi_write: %d  errno: %d\n", j, error, my_errno);
  }

  /* Insert 2 rows with null values */
  if (null_fields) {
    create_record(record, 0);
    error = mi_write(file, record);
    if (!error) row_count++;
    if (verbose || error)
      printf("J= NULL  mi_write: %d  errno: %d\n", error, my_errno);
    error = mi_write(file, record);
    if (!error) row_count++;
    if (verbose || error)
      printf("J= NULL  mi_write: %d  errno: %d\n", error, my_errno);
    flags[0] = 2;
  }

  if (!skip_update) {
    if (opt_unique) {
      if (!silent) printf("- Checking unique constraint\n");
      create_record(record, j);
      if (!mi_write(file, record) || my_errno != HA_ERR_FOUND_DUPP_UNIQUE) {
        printf("unique check failed\n");
      }
    }
    if (!silent) printf("- Updating rows\n");

    /* Update first last row to force extend of file */
    if (mi_rsame(file, read_record, -1)) {
      printf("Can't find last row with mi_rsame\n");
    } else {
      memcpy(record, read_record, rec_length);
      update_record(record);
      if (mi_update(file, read_record, record)) {
        printf("Can't update last row: %.*s\n", keyinfo[0].seg[0].length,
               read_record + 1);
      }
    }

    /* Read through all rows and update them */
    pos = (my_off_t)0;
    found = 0;
    while ((error = mi_rrnd(file, read_record, pos)) == 0) {
      if (update_count-- == 0) {
        (void)mi_close(file);
        exit(0);
      }
      memcpy(record, read_record, rec_length);
      update_record(record);
      if (mi_update(file, read_record, record)) {
        printf("Can't update row: %.*s, error: %d\n", keyinfo[0].seg[0].length,
               record + 1, my_errno);
      }
      found++;
      pos = HA_OFFSET_ERROR;
    }
    if (found != row_count)
      printf("Found %ld of %ld rows\n", (ulong)found, (ulong)row_count);
  }

  if (!silent) printf("- Reopening file\n");
  if (mi_close(file)) goto err;
  if (!(file = mi_open(filename, 2, HA_OPEN_ABORT_IF_LOCKED))) goto err;
  if (!skip_update) {
    if (!silent) printf("- Removing keys\n");

    for (i = 0; i <= 10; i++) {
      /* testing */
      if (remove_count-- == 0) {
        (void)mi_close(file);
        exit(0);
      }
      j = i * 2;
      if (!flags[j]) continue;
      create_key(key, j);
      my_errno = 0;
      if ((error = mi_rkey(file, read_record, 0, key, HA_WHOLE_KEY,
                           HA_READ_KEY_EXACT))) {
        if (verbose ||
            (flags[j] >= 1 || (error && my_errno != HA_ERR_KEY_NOT_FOUND)))
          printf("key: '%.*s'  mi_rkey:  %3d  errno: %3d\n", (int)key_length,
                 key + null_fields, error, my_errno);
      } else {
        error = mi_delete(file, read_record);
        if (verbose || error)
          printf("key: '%.*s'  mi_delete: %3d  errno: %3d\n", (int)key_length,
                 key + null_fields, error, my_errno);
        if (!error) {
          deleted++;
          flags[j]--;
        }
      }
    }
  }
  if (!silent) printf("- Reading rows with key\n");
  for (i = 0; i <= 25; i++) {
    create_key(key, i);
    my_errno = 0;
    error = mi_rkey(file, read_record, 0, key, HA_WHOLE_KEY, HA_READ_KEY_EXACT);
    if (verbose || (error == 0 && flags[i] == 0 && unique_key) ||
        (error && (flags[i] != 0 || my_errno != HA_ERR_KEY_NOT_FOUND))) {
      printf("key: '%.*s'  mi_rkey: %3d  errno: %3d  record: %s\n",
             (int)key_length, key + null_fields, error, my_errno, record + 1);
    }
  }

  if (!silent) printf("- Reading rows with position\n");
  for (i = 1, found = 0; i <= 30; i++) {
    my_errno = 0;
    if ((error = mi_rrnd(file, read_record, i == 1 ? 0L : HA_OFFSET_ERROR)) ==
        -1) {
      if (found != row_count - deleted)
        printf("Found only %ld of %ld rows\n", (ulong)found,
               (ulong)(row_count - deleted));
      break;
    }
    if (!error) found++;
    if (verbose || (error != 0 && error != HA_ERR_RECORD_DELETED &&
                    error != HA_ERR_END_OF_FILE)) {
      printf("pos: %2d  mi_rrnd: %3d  errno: %3d  record: %s\n", i - 1, error,
             my_errno, read_record + 1);
    }
  }
  if (mi_close(file)) goto err;
  my_end(MY_CHECK_ERROR);

  return (0);
err:
  printf("got error: %3d when using myisam-database\n", my_errno);
  return 1; /* skip warning */
}

static void create_key_part(uchar *key, uint rownr) {
  if (!unique_key) rownr &= 7; /* Some identical keys */
  if (keyinfo[0].seg[0].type == HA_KEYTYPE_NUM) {
    sprintf((char *)key, "%*d", keyinfo[0].seg[0].length, rownr);
  } else if (keyinfo[0].seg[0].type == HA_KEYTYPE_VARTEXT1 ||
             keyinfo[0].seg[0].type == HA_KEYTYPE_VARTEXT2) { /* Alpha record */
    /* Create a key that may be easily packed */
    memset(key, rownr < 10 ? 'A' : 'B', keyinfo[0].seg[0].length);
    sprintf((char *)key + keyinfo[0].seg[0].length - 2, "%-2d", rownr);
    if ((rownr & 7) == 0) {
      /* Change the key to force a unpack of the next key */
      memset(key + 3, rownr < 10 ? 'a' : 'b', keyinfo[0].seg[0].length - 4);
    }
  } else { /* Alpha record */
    if (keyinfo[0].seg[0].flag & HA_SPACE_PACK)
      sprintf((char *)key, "%-*d", keyinfo[0].seg[0].length, rownr);
    else {
      /* Create a key that may be easily packed */
      memset(key, rownr < 10 ? 'A' : 'B', keyinfo[0].seg[0].length);
      sprintf((char *)key + keyinfo[0].seg[0].length - 2, "%-2d", rownr);
      if ((rownr & 7) == 0) {
        /* Change the key to force a unpack of the next key */
        key[1] = (rownr < 10 ? 'a' : 'b');
      }
    }
  }
}

static void create_key(uchar *key, uint rownr) {
  if (keyinfo[0].seg[0].null_bit) {
    if (rownr == 0) {
      key[0] = 1; /* null key */
      key[1] = 0; /* Fore easy print of key */
      return;
    }
    *key++ = 0;
  }
  if (keyinfo[0].seg[0].flag & (HA_BLOB_PART | HA_VAR_LENGTH_PART)) {
    uint tmp;
    create_key_part(key + 2, rownr);
    tmp = strlen((char *)key + 2);
    int2store(key, tmp);
  } else
    create_key_part(key, rownr);
}

static uchar blob_key[MAX_REC_LENGTH];
static uchar blob_record[MAX_REC_LENGTH + 20 * 20];

static void create_record(uchar *record, uint rownr) {
  uchar *pos;
  memset(record, 0, MAX_REC_LENGTH);
  record[0] = 1; /* delete marker */
  if (rownr == 0 && keyinfo[0].seg[0].null_bit)
    record[0] |= keyinfo[0].seg[0].null_bit; /* Null key */

  pos = record + 1;
  if (recinfo[1].type == FIELD_BLOB) {
    uint tmp;
    uchar *ptr;
    create_key_part(blob_key, rownr);
    tmp = strlen((char *)blob_key);
    int4store(pos, tmp);
    ptr = blob_key;
    memcpy(pos + 4, &ptr, sizeof(char *));
    pos += recinfo[1].length;
  } else if (recinfo[1].type == FIELD_VARCHAR) {
    uint tmp, pack_length = HA_VARCHAR_PACKLENGTH(recinfo[1].length - 1);
    create_key_part(pos + pack_length, rownr);
    tmp = strlen((char *)pos + pack_length);
    if (pack_length == 1)
      *(uchar *)pos = (uchar)tmp;
    else
      int2store(pos, tmp);
    pos += recinfo[1].length;
  } else {
    create_key_part(pos, rownr);
    pos += recinfo[1].length;
  }
  if (recinfo[2].type == FIELD_BLOB) {
    uint tmp;
    uchar *ptr;
    ;
    sprintf((char *)blob_record, "... row: %d", rownr);
    strappend((char *)blob_record, max(MAX_REC_LENGTH - rownr, 10), ' ');
    tmp = strlen((char *)blob_record);
    int4store(pos, tmp);
    ptr = blob_record;
    memcpy(pos + 4, &ptr, sizeof(char *));
  } else if (recinfo[2].type == FIELD_VARCHAR) {
    uint tmp, pack_length = HA_VARCHAR_PACKLENGTH(recinfo[1].length - 1);
    sprintf((char *)pos + pack_length, "... row: %d", rownr);
    tmp = strlen((char *)pos + pack_length);
    if (pack_length == 1)
      *pos = (uchar)tmp;
    else
      int2store(pos, tmp);
  } else {
    sprintf((char *)pos, "... row: %d", rownr);
    strappend((char *)pos, recinfo[2].length, ' ');
  }
}

/* change row to test re-packing of rows and reallocation of keys */

static void update_record(uchar *record) {
  uchar *pos = record + 1;
  if (recinfo[1].type == FIELD_BLOB) {
    uchar *column, *ptr;
    int length;
    length = uint4korr(pos); /* Long blob */
    memcpy(&column, pos + 4, sizeof(char *));
    memcpy(blob_key, column, length); /* Move old key */
    ptr = blob_key;
    memcpy(pos + 4, &ptr, sizeof(char *)); /* Store pointer to new key */
    if (keyinfo[0].seg[0].type != HA_KEYTYPE_NUM)
      default_charset_info->cset->casedn(default_charset_info, (char *)blob_key,
                                         length, (char *)blob_key, length);
    pos += recinfo[1].length;
  } else if (recinfo[1].type == FIELD_VARCHAR) {
    uint pack_length = HA_VARCHAR_PACKLENGTH(recinfo[1].length - 1);
    uint length = pack_length == 1 ? (uint) * (uchar *)pos : uint2korr(pos);
    default_charset_info->cset->casedn(default_charset_info,
                                       (char *)pos + pack_length, length,
                                       (char *)pos + pack_length, length);
    pos += recinfo[1].length;
  } else {
    if (keyinfo[0].seg[0].type != HA_KEYTYPE_NUM)
      default_charset_info->cset->casedn(default_charset_info, (char *)pos,
                                         keyinfo[0].seg[0].length, (char *)pos,
                                         keyinfo[0].seg[0].length);
    pos += recinfo[1].length;
  }

  if (recinfo[2].type == FIELD_BLOB) {
    uchar *column;
    int length;
    length = uint4korr(pos);
    memcpy(&column, pos + 4, sizeof(char *));
    memcpy(blob_record, column, length);
    memset(blob_record + length, '.', 20); /* Make it larger */
    length += 20;
    int4store(pos, length);
    column = blob_record;
    memcpy(pos + 4, &column, sizeof(char *));
  } else if (recinfo[2].type == FIELD_VARCHAR) {
    /* Second field is longer than 10 characters */
    uint pack_length = HA_VARCHAR_PACKLENGTH(recinfo[1].length - 1);
    uint length = pack_length == 1 ? (uint) * (uchar *)pos : uint2korr(pos);
    memset(pos + pack_length + length, '.',
           recinfo[2].length - length - pack_length);
    length = recinfo[2].length - pack_length;
    if (pack_length == 1)
      *(uchar *)pos = (uchar)length;
    else
      int2store(pos, length);
  } else {
    memset(pos + recinfo[2].length - 10, '.', 10);
  }
}

static struct my_option my_long_options[] = {
    {"checksum", 'c', "Undocumented", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0,
     0, 0},
#ifndef DBUG_OFF
    {"debug", '#', "Undocumented", 0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0,
     0, 0},
#endif
    {"delete_rows", 'd', "Undocumented", &remove_count, &remove_count, 0,
     GET_UINT, REQUIRED_ARG, 1000, 0, 0, 0, 0, 0},
    {"help", '?', "Display help and exit", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0,
     0, 0, 0},
    {"insert_rows", 'i', "Undocumented", &insert_count, &insert_count, 0,
     GET_UINT, REQUIRED_ARG, 1000, 0, 0, 0, 0, 0},
    {"key_alpha", 'a', "Use a key of type HA_KEYTYPE_TEXT", 0, 0, 0, GET_NO_ARG,
     NO_ARG, 0, 0, 0, 0, 0, 0},
    {"key_binary_pack", 'B', "Undocumented", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0,
     0, 0, 0, 0},
    {"key_blob", 'b', "Undocumented", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0,
     0, 0},
    {"key_cache", 'K', "Undocumented", &key_cacheing, &key_cacheing, 0,
     GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
    {"key_length", 'k', "Undocumented", &key_length, &key_length, 0, GET_UINT,
     REQUIRED_ARG, 6, 0, 0, 0, 0, 0},
    {"key_multiple", 'm', "Undocumented", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0,
     0, 0, 0},
    {"key_prefix_pack", 'P', "Undocumented", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0,
     0, 0, 0, 0},
    {"key_space_pack", 'p', "Undocumented", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0,
     0, 0, 0, 0},
    {"key_varchar", 'w', "Test VARCHAR keys", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0,
     0, 0, 0, 0},
    {"null_fields", 'N', "Define fields with NULL", &null_fields, &null_fields,
     0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
    {"row_fixed_size", 'S', "Undocumented", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0,
     0, 0, 0, 0},
    {"row_pointer_size", 'R', "Undocumented", &rec_pointer_size,
     &rec_pointer_size, 0, GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"silent", 's', "Undocumented", &silent, &silent, 0, GET_BOOL, NO_ARG, 0, 0,
     0, 0, 0, 0},
    {"skip_update", 'U', "Undocumented", &skip_update, &skip_update, 0,
     GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
    {"unique", 'C', "Undocumented", &opt_unique, &opt_unique, 0, GET_BOOL,
     NO_ARG, 0, 0, 0, 0, 0, 0},
    {"update_rows", 'u', "Undocumented", &update_count, &update_count, 0,
     GET_UINT, REQUIRED_ARG, 1000, 0, 0, 0, 0, 0},
    {"verbose", 'v', "Be more verbose", &verbose, &verbose, 0, GET_BOOL, NO_ARG,
     0, 0, 0, 0, 0, 0},
    {"version", 'V', "Print version number and exit", 0, 0, 0, GET_NO_ARG,
     NO_ARG, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}};

static bool get_one_option(int optid,
                           const struct my_option *opt MY_ATTRIBUTE((unused)),
                           char *argument MY_ATTRIBUTE((unused))) {
  switch (optid) {
    case 'a':
      key_type = HA_KEYTYPE_TEXT;
      break;
    case 'c':
      create_flag |= HA_CREATE_CHECKSUM;
      break;
    case 'R': /* Length of record pointer */
      if (rec_pointer_size > 3) rec_pointer_size = 0;
      break;
    case 'P':
      pack_keys = HA_PACK_KEY; /* Use prefix compression */
      break;
    case 'B':
      pack_keys = HA_BINARY_PACK_KEY; /* Use binary compression */
      break;
    case 'S':
      if (key_field == FIELD_VARCHAR) {
        create_flag = 0; /* Static sized varchar */
      } else if (key_field != FIELD_BLOB) {
        key_field = FIELD_NORMAL; /* static-size record */
        extra_field = FIELD_NORMAL;
      }
      break;
    case 'p':
      pack_keys = HA_PACK_KEY; /* Use prefix + space packing */
      pack_seg = HA_SPACE_PACK;
      key_type = HA_KEYTYPE_TEXT;
      break;
    case 'm':
      unique_key = 0;
      break;
    case 'b':
      key_field = FIELD_BLOB; /* blob key */
      extra_field = FIELD_BLOB;
      pack_seg |= HA_BLOB_PART;
      key_type = HA_KEYTYPE_VARTEXT1;
      break;
    case 'k':
      if (key_length < 4 || key_length > MI_MAX_KEY_LENGTH) {
        fprintf(stderr, "Wrong key length\n");
        exit(1);
      }
      break;
    case 'w':
      key_field = FIELD_VARCHAR; /* varchar keys */
      extra_field = FIELD_VARCHAR;
      key_type = HA_KEYTYPE_VARTEXT1;
      pack_seg |= HA_VAR_LENGTH_PART;
      create_flag |= HA_PACK_RECORD;
      break;
    case 'K': /* Use key cacheing */
      key_cacheing = 1;
      break;
    case 'V':
      printf("test1 Ver 1.2 \n");
      exit(0);
    case '#':
      DBUG_PUSH(argument);
      break;
    case '?':
      usage();
      exit(1);
  }
  return 0;
}

/* Read options */

static void get_options(int argc, char *argv[]) {
  int ho_error;

  if ((ho_error =
           handle_options(&argc, &argv, my_long_options, get_one_option)))
    exit(ho_error);

  return;
} /* get options */

static void usage() {
  printf("Usage: %s [options]\n\n", my_progname);
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
}

#include "mi_extrafunc.h"
