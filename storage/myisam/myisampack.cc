/* Copyright (c) 2000, 2022, Oracle and/or its affiliates.

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

/* Pack MyISAM file */

#include "my_config.h"

#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <algorithm>

#include "my_byteorder.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_default.h"
#include "my_getopt.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "my_macros.h"
#include "my_pointer_arithmetic.h"
#include "my_tree.h"
#include "mysys_err.h"
#include "print_version.h"
#include "sql/field.h"
#include "storage/myisam/myisam_sys.h"
#include "storage/myisam/myisamdef.h"
#include "storage/myisam/queues.h"
#include "welcome_copyright_notice.h"  // ORACLE_WELCOME_COPYRIGHT_NOTICE

#if SIZEOF_LONG_LONG > 4
#define BITS_SAVED 64
#else
#define BITS_SAVED 32
#endif

#define IS_OFFSET ((uint)32768) /* Bit if offset or char in tree */
#define HEAD_LENGTH 32
#define ALLOWED_JOIN_DIFF 256 /* Diff allowed to join trees */

#define DATA_TMP_EXT ".TMD"
#define OLD_EXT ".OLD"
#define FRM_EXT ".frm"
#define WRITE_COUNT MY_HOW_OFTEN_TO_WRITE

struct st_file_buffer {
  File file;
  uchar *buffer, *pos, *end;
  my_off_t pos_in_file;
  int bits;
  ulonglong bitbucket;
};

struct HUFF_ELEMENT;
struct HUFF_TREE;

struct HUFF_COUNTS {
  uint field_length, max_zero_fill;
  uint pack_type;
  uint max_end_space, max_pre_space, length_bits, min_space;
  ulong max_length;
  enum en_fieldtype field_type;
  HUFF_TREE *tree; /* Tree for field */
  my_off_t counts[256];
  my_off_t end_space[8];
  my_off_t pre_space[8];
  my_off_t tot_end_space, tot_pre_space, zero_fields, empty_fields,
      bytes_packed;
  TREE int_tree;    /* Tree for detecting distinct column values. */
  uchar *tree_buff; /* Column values, 'field_length' each. */
  uchar *tree_pos;  /* Points to end of column values in 'tree_buff'. */
};

/*
  WARNING: It is crucial for the optimizations in calc_packed_length()
  that 'count' is the first element of 'HUFF_ELEMENT'.
*/
struct HUFF_ELEMENT {
  my_off_t count;
  union un_element {
    struct st_nod {
      HUFF_ELEMENT *left, *right;
    } nod;
    struct st_leaf {
      HUFF_ELEMENT *null;
      uint element_nr; /* Number of element */
    } leaf;
  } a;
};

struct HUFF_TREE {
  HUFF_ELEMENT *root, *element_buffer;
  HUFF_COUNTS *counts;
  uint tree_number;
  uint elements;
  my_off_t bytes_packed;
  uint tree_pack_length;
  uint min_chr, max_chr, char_bits, offset_bits, max_offset, height;
  ulonglong *code;
  uchar *code_len;
};

struct PACK_MRG_INFO {
  MI_INFO **file, **current, **end;
  uint free_file;
  uint count;
  uint min_pack_length; /* These are used by packed data */
  uint max_pack_length;
  uint ref_length;
  uint max_blob_length;
  my_off_t records;
  /* true if at least one source file has at least one disabled index */
  bool src_file_has_indexes_disabled;
};

extern int main(int argc, char **argv);
static void get_options(int *argc, char ***argv);
static MI_INFO *open_isam_file(char *name, int mode);
static bool open_isam_files(PACK_MRG_INFO *mrg, char **names, uint count);
static int compress(PACK_MRG_INFO *file, char *join_name);
static int create_dest_frm(char *source_table, char *dest_table);
static HUFF_COUNTS *init_huff_count(MI_INFO *info, my_off_t records);
static void free_counts_and_tree_and_queue(HUFF_TREE *huff_trees, uint trees,
                                           HUFF_COUNTS *huff_counts,
                                           uint fields);
static int compare_tree(const void *cmp_arg [[maybe_unused]], const void *a,
                        const void *b);
static int get_statistic(PACK_MRG_INFO *mrg, HUFF_COUNTS *huff_counts);
static void check_counts(HUFF_COUNTS *huff_counts, uint trees,
                         my_off_t records);
static int test_space_compress(HUFF_COUNTS *huff_counts, my_off_t records,
                               uint max_space_length, my_off_t *space_counts,
                               my_off_t tot_space_count,
                               enum en_fieldtype field_type);
static HUFF_TREE *make_huff_trees(HUFF_COUNTS *huff_counts, uint trees);
static int make_huff_tree(HUFF_TREE *tree, HUFF_COUNTS *huff_counts);
static int compare_huff_elements(void *not_used, uchar *a, uchar *b);
static int save_counts_in_queue(void *v_key, element_count count, void *v_tree);
static my_off_t calc_packed_length(HUFF_COUNTS *huff_counts, uint flag);
static uint join_same_trees(HUFF_COUNTS *huff_counts, uint trees);
static int make_huff_decode_table(HUFF_TREE *huff_tree, uint trees);
static void make_traverse_code_tree(HUFF_TREE *huff_tree, HUFF_ELEMENT *element,
                                    uint size, ulonglong code);
static int write_header(PACK_MRG_INFO *isam_file, uint header_length,
                        uint trees, my_off_t tot_elements, my_off_t filelength);
static void write_field_info(HUFF_COUNTS *counts, uint fields, uint trees);
static my_off_t write_huff_tree(HUFF_TREE *huff_tree, uint trees);
static uint *make_offset_code_tree(HUFF_TREE *huff_tree, HUFF_ELEMENT *element,
                                   uint *offset);
static uint max_bit(uint value);
static int compress_isam_file(PACK_MRG_INFO *file, HUFF_COUNTS *huff_counts);
static char *make_new_name(char *new_name, char *old_name);
static char *make_old_name(char *new_name, char *old_name);
static void init_file_buffer(File file, bool read_buffer);
static int flush_buffer(ulong neaded_length);
static void end_file_buffer(void);
static void write_bits(ulonglong value, uint bits);
static void flush_bits(void);
static int save_state(MI_INFO *isam_file, PACK_MRG_INFO *mrg,
                      my_off_t new_length, ha_checksum crc);
static int save_state_mrg(File file, PACK_MRG_INFO *isam_file,
                          my_off_t new_length, ha_checksum crc);
static int mrg_close(PACK_MRG_INFO *mrg);
static int mrg_rrnd(PACK_MRG_INFO *info, uchar *buf);
static void mrg_reset(PACK_MRG_INFO *mrg);
#if !defined(NDEBUG)
static void fakebigcodes(HUFF_COUNTS *huff_counts, HUFF_COUNTS *end_count);
#endif

static int error_on_write = 0, test_only = 0, verbose = 0, silent = 0,
           write_loop = 0, force_pack = 0, isamchk_neaded = 0;
static int tmpfile_createflag = O_RDWR | O_TRUNC | O_EXCL;
static bool backup, opt_wait;
/*
  tree_buff_length is somewhat arbitrary. The bigger it is the better
  the chance to win in terms of compression factor. On the other hand,
  this table becomes part of the compressed file header. And its length
  is coded with 16 bits in the header. Hence the limit is 2**16 - 1.
*/
static uint tree_buff_length = 65536 - MALLOC_OVERHEAD;
static char tmp_dir[FN_REFLEN] = {0}, *join_table;
static my_off_t intervall_length;
static ha_checksum glob_crc;
static struct st_file_buffer file_buffer;
static QUEUE queue;
static HUFF_COUNTS *global_count;
static char zero_string[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const char *load_default_groups[] = {"myisampack", nullptr};

extern st_keycache_thread_var *keycache_thread_var() {
  return &main_thread_keycache_var;
}

/* The main program */

int main(int argc, char **argv) {
  int error, ok;
  PACK_MRG_INFO merge;
  MY_INIT(argv[0]);

  memset(&main_thread_keycache_var, 0, sizeof(st_keycache_thread_var));
  mysql_cond_init(PSI_NOT_INSTRUMENTED, &main_thread_keycache_var.suspend);

  MEM_ROOT alloc{PSI_NOT_INSTRUMENTED, 512};
  if (load_defaults("my", load_default_groups, &argc, &argv, &alloc)) exit(1);

  get_options(&argc, &argv);

  error = ok = isamchk_neaded = 0;
  if (join_table) {
    /*
      Join files into one and create FRM file for the compressed table only if
      the compression succeeds
    */
    if (open_isam_files(&merge, argv, (uint)argc) ||
        compress(&merge, join_table) || create_dest_frm(argv[0], join_table))
      error = 1;
  } else
    while (argc--) {
      MI_INFO *isam_file;
      if (!(isam_file = open_isam_file(*argv++, O_RDWR)))
        error = 1;
      else {
        merge.file = &isam_file;
        merge.current = nullptr;
        merge.free_file = 0;
        merge.count = 1;
        if (compress(&merge, nullptr))
          error = 1;
        else
          ok = 1;
      }
    }
  if (ok && isamchk_neaded && !silent)
    puts("Remember to run myisamchk -rq on compressed tables");
  (void)fflush(stdout);
  (void)fflush(stderr);
  my_end(verbose ? MY_CHECK_ERROR | MY_GIVE_INFO : MY_CHECK_ERROR);
  mysql_cond_destroy(&main_thread_keycache_var.suspend);
  exit(error ? 2 : 0);
}

enum options_mp { OPT_CHARSETS_DIR_MP = 256 };

static struct my_option my_long_options[] = {
    {"backup", 'b', "Make a backup of the table as table_name.OLD.", &backup,
     &backup, nullptr, GET_BOOL, NO_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"character-sets-dir", OPT_CHARSETS_DIR_MP,
     "Directory where character sets are.", &charsets_dir, &charsets_dir,
     nullptr, GET_STR, REQUIRED_ARG, 0, 0, 0, nullptr, 0, nullptr},
#ifdef NDEBUG
    {"debug", '#', "This is a non-debug version. Catch this and exit.", nullptr,
     nullptr, nullptr, GET_DISABLED, OPT_ARG, 0, 0, 0, nullptr, 0, nullptr},
#else
    {"debug", '#', "Output debug log. Often this is 'd:t:o,filename'.", nullptr,
     nullptr, nullptr, GET_STR, OPT_ARG, 0, 0, 0, nullptr, 0, nullptr},
#endif
    {"force", 'f',
     "Force packing of table even if it gets bigger or if tempfile exists.",
     nullptr, nullptr, nullptr, GET_NO_ARG, NO_ARG, 0, 0, 0, nullptr, 0,
     nullptr},
    {"join", 'j',
     "Join all given tables into 'new_table_name'. All tables MUST have "
     "identical layouts.",
     &join_table, &join_table, nullptr, GET_STR, REQUIRED_ARG, 0, 0, 0, nullptr,
     0, nullptr},
    {"help", '?', "Display this help and exit.", nullptr, nullptr, nullptr,
     GET_NO_ARG, NO_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"silent", 's', "Be more silent.", nullptr, nullptr, nullptr, GET_NO_ARG,
     NO_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"tmpdir", 'T', "Use temporary directory to store temporary table.",
     nullptr, nullptr, nullptr, GET_STR, REQUIRED_ARG, 0, 0, 0, nullptr, 0,
     nullptr},
    {"test", 't', "Don't pack table, only test packing it.", nullptr, nullptr,
     nullptr, GET_NO_ARG, NO_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"verbose", 'v',
     "Write info about progress and packing result. Use many -v for more "
     "verbosity!",
     nullptr, nullptr, nullptr, GET_NO_ARG, NO_ARG, 0, 0, 0, nullptr, 0,
     nullptr},
    {"version", 'V', "Output version information and exit.", nullptr, nullptr,
     nullptr, GET_NO_ARG, NO_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"wait", 'w', "Wait and retry if table is in use.", &opt_wait, &opt_wait,
     nullptr, GET_BOOL, NO_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {nullptr, 0, nullptr, nullptr, nullptr, nullptr, GET_NO_ARG, NO_ARG, 0, 0,
     0, nullptr, 0, nullptr}};

static void usage(void) {
  print_version();
  puts(ORACLE_WELCOME_COPYRIGHT_NOTICE("2002"));

  puts("Pack a MyISAM-table to take much less space.");
  puts("Keys are not updated, you must run myisamchk -rq on the datafile");
  puts("afterwards to update the keys.");
  puts("You should give the .MYI file as the filename argument.");

  printf("\nUsage: %s [OPTIONS] filename...\n", my_progname);
  my_print_help(my_long_options);
  print_defaults("my", load_default_groups);
  my_print_variables(my_long_options);
}

static bool get_one_option(int optid,
                           const struct my_option *opt [[maybe_unused]],
                           char *argument) {
  uint length;

  switch (optid) {
    case 'f':
      force_pack = 1;
      tmpfile_createflag = O_RDWR | O_TRUNC;
      break;
    case 's':
      write_loop = verbose = 0;
      silent = 1;
      break;
    case 't':
      test_only = 1;
      /* Avoid to reset 'verbose' if it was already set > 1. */
      if (!verbose) verbose = 1;
      break;
    case 'T':
      length = (uint)(my_stpcpy(tmp_dir, argument) - tmp_dir);
      if (length != dirname_length(tmp_dir)) {
        tmp_dir[length] = FN_LIBCHAR;
        tmp_dir[length + 1] = 0;
      }
      break;
    case 'v':
      verbose++; /* Allow for selecting the level of verbosity. */
      silent = 0;
      break;
    case '#':
      DBUG_PUSH(argument ? argument : "d:t:o");
      break;
    case 'V':
      print_version();
      exit(0);
    case 'I':
    case '?':
      usage();
      exit(0);
  }
  return false;
}

/* reads options */
/* Initiates DEBUG - but no debugging here ! */

static void get_options(int *argc, char ***argv) {
  int ho_error;

  my_progname = argv[0][0];
  if (isatty(fileno(stdout))) write_loop = 1;

  if ((ho_error = handle_options(argc, argv, my_long_options, get_one_option)))
    exit(ho_error);

  if (!*argc) {
    usage();
    exit(1);
  }
  if (join_table) {
    backup = false; /* Not needed */
    tmp_dir[0] = 0;
  }
  return;
}

static MI_INFO *open_isam_file(char *name, int mode) {
  MI_INFO *isam_file;
  MYISAM_SHARE *share;
  DBUG_TRACE;

  if (!(isam_file = mi_open(
            name, mode,
            (opt_wait ? HA_OPEN_WAIT_IF_LOCKED : HA_OPEN_ABORT_IF_LOCKED)))) {
    (void)fprintf(stderr, "%s gave error %d on open\n", name, my_errno());
    return nullptr;
  }
  share = isam_file->s;
  if (share->options & HA_OPTION_COMPRESS_RECORD && !join_table) {
    if (!force_pack) {
      (void)fprintf(stderr, "%s is already compressed\n", name);
      (void)mi_close(isam_file);
      return nullptr;
    }
    if (verbose) puts("Recompressing already compressed table");
    share->options &= ~HA_OPTION_READ_ONLY_DATA; /* We are modifying it */
  }
  if (!force_pack && share->state.state.records != 0 &&
      (share->state.state.records <= 1 ||
       share->state.state.data_file_length < 1024)) {
    (void)fprintf(stderr, "%s is too small to compress\n", name);
    (void)mi_close(isam_file);
    return nullptr;
  }
  (void)mi_lock_database(isam_file, F_WRLCK);
  return isam_file;
}

static bool open_isam_files(PACK_MRG_INFO *mrg, char **names, uint count) {
  uint i, j;
  mrg->count = 0;
  mrg->current = nullptr;
  mrg->file = (MI_INFO **)my_malloc(PSI_NOT_INSTRUMENTED,
                                    sizeof(MI_INFO *) * count, MYF(MY_FAE));
  mrg->free_file = 1;
  mrg->src_file_has_indexes_disabled = false;
  for (i = 0; i < count; i++) {
    if (!(mrg->file[i] = open_isam_file(names[i], O_RDONLY))) goto error;

    mrg->src_file_has_indexes_disabled |= !mi_is_all_keys_active(
        mrg->file[i]->s->state.key_map, mrg->file[i]->s->base.keys);
  }
  /* Check that files are identical */
  for (j = 0; j < count - 1; j++) {
    MI_COLUMNDEF *m1, *m2, *end;
    if (mrg->file[j]->s->base.reclength !=
            mrg->file[j + 1]->s->base.reclength ||
        mrg->file[j]->s->base.fields != mrg->file[j + 1]->s->base.fields)
      goto diff_file;
    m1 = mrg->file[j]->s->rec;
    end = m1 + mrg->file[j]->s->base.fields;
    m2 = mrg->file[j + 1]->s->rec;
    for (; m1 != end; m1++, m2++) {
      if (m1->type != m2->type || m1->length != m2->length) goto diff_file;
    }
  }
  mrg->count = count;
  return false;

diff_file:
  (void)fprintf(stderr, "%s: Tables '%s' and '%s' are not identical\n",
                my_progname, names[j], names[j + 1]);
error:
  while (i--) mi_close(mrg->file[i]);
  my_free(mrg->file);
  return true;
}

static int compress(PACK_MRG_INFO *mrg, char *result_table) {
  int error;
  File new_file, join_isam_file;
  MI_INFO *isam_file;
  MYISAM_SHARE *share;
  char org_name[FN_REFLEN], new_name[FN_REFLEN], temp_name[FN_REFLEN];
  uint i, header_length, fields, trees, used_trees;
  my_off_t old_length, new_length, tot_elements;
  HUFF_COUNTS *huff_counts;
  HUFF_TREE *huff_trees;
  DBUG_TRACE;

  isam_file = mrg->file[0]; /* Take this as an example */
  share = isam_file->s;
  new_file = join_isam_file = -1;
  trees = fields = 0;
  huff_trees = nullptr;
  huff_counts = nullptr;

  /* Create temporary or join file */

  if (backup)
    (void)fn_format(org_name, isam_file->filename, "", MI_NAME_DEXT, 2);
  else
    (void)fn_format(org_name, isam_file->filename, "", MI_NAME_DEXT,
                    2 + 4 + 16);
  if (!test_only && result_table) {
    /* Make a new indexfile based on first file in list */
    uint length;
    uchar *buff;
    my_stpcpy(org_name, result_table); /* Fix error messages */
    (void)fn_format(new_name, result_table, "", MI_NAME_IEXT, 2);
    if ((join_isam_file =
             my_create(new_name, 0, tmpfile_createflag, MYF(MY_WME))) < 0)
      goto err;
    length = (uint)share->base.keystart;
    if (!(buff = (uchar *)my_malloc(PSI_NOT_INSTRUMENTED, length, MYF(MY_WME))))
      goto err;
    if (my_pread(share->kfile, buff, length, 0L, MYF(MY_WME | MY_NABP)) ||
        my_write(join_isam_file, buff, length,
                 MYF(MY_WME | MY_NABP | MY_WAIT_IF_FULL))) {
      my_free(buff);
      goto err;
    }
    my_free(buff);
    (void)fn_format(new_name, result_table, "", MI_NAME_DEXT, 2);
  } else if (!tmp_dir[0])
    (void)make_new_name(new_name, org_name);
  else
    (void)fn_format(new_name, org_name, tmp_dir, DATA_TMP_EXT, 1 + 2 + 4);
  if (!test_only &&
      (new_file = my_create(new_name, 0, tmpfile_createflag, MYF(MY_WME))) < 0)
    goto err;

  /* Start calculating statistics */

  mrg->records = 0;
  for (i = 0; i < mrg->count; i++)
    mrg->records += mrg->file[i]->s->state.state.records;

  DBUG_PRINT("info", ("Compressing %s: (%lu records)",
                      result_table ? new_name : org_name, (ulong)mrg->records));
  if (write_loop || verbose) {
    printf("Compressing %s: (%lu records)\n",
           result_table ? new_name : org_name, (ulong)mrg->records);
  }
  trees = fields = share->base.fields;
  huff_counts = init_huff_count(isam_file, mrg->records);

  /*
    Read the whole data file(s) for statistics.
  */
  DBUG_PRINT("info", ("- Calculating statistics"));
  if (write_loop || verbose) printf("- Calculating statistics\n");
  if (get_statistic(mrg, huff_counts)) goto err;

  old_length = 0;
  for (i = 0; i < mrg->count; i++)
    old_length += (mrg->file[i]->s->state.state.data_file_length -
                   mrg->file[i]->s->state.state.empty);

  /*
    Create a global priority queue in preparation for making
    temporary Huffman trees.
  */
  if (init_queue(&queue, key_memory_QUEUE, 256, 0, false, compare_huff_elements,
                 nullptr))
    goto err;

  /*
    Check each column if we should use pre-space-compress, end-space-
    compress, empty-field-compress or zero-field-compress.
  */
  check_counts(huff_counts, fields, mrg->records);

  /*
    Build a Huffman tree for each column.
  */
  huff_trees = make_huff_trees(huff_counts, trees);

  /*
    If the packed lengths of combined columns is less then the sum of
    the non-combined columns, then create common Huffman trees for them.
    We do this only for byte compressed columns, not for distinct values
    compressed columns.
  */
  if ((int)(used_trees = join_same_trees(huff_counts, trees)) < 0) goto err;

  /*
    Assign codes to all byte or column values.
  */
  if (make_huff_decode_table(huff_trees, fields)) goto err;

  /* Prepare a file buffer. */
  init_file_buffer(new_file, false);

  /*
    Reserve space in the target file for the fixed compressed file header.
  */
  file_buffer.pos_in_file = HEAD_LENGTH;
  if (!test_only)
    my_seek(new_file, file_buffer.pos_in_file, MY_SEEK_SET, MYF(0));

  /*
    Write field infos: field type, pack type, length bits, tree number.
  */
  write_field_info(huff_counts, fields, used_trees);

  /*
    Write decode trees.
  */
  if (!(tot_elements = write_huff_tree(huff_trees, trees))) goto err;

  /*
    Calculate the total length of the compression info header.
    This includes the fixed compressed file header, the column compression
    type descriptions, and the decode trees.
  */
  header_length = (uint)file_buffer.pos_in_file +
                  (uint)(file_buffer.pos - file_buffer.buffer);

  /*
    Compress the source file into the target file.
  */
  DBUG_PRINT("info", ("- Compressing file"));
  if (write_loop || verbose) printf("- Compressing file\n");
  error = compress_isam_file(mrg, huff_counts);
  new_length = file_buffer.pos_in_file;
  if (!error && !test_only) {
    uchar buff[MEMMAP_EXTRA_MARGIN]; /* End marginal for memmap */
    memset(buff, 0, sizeof(buff));
    error = my_write(file_buffer.file, buff, sizeof(buff),
                     MYF(MY_WME | MY_NABP | MY_WAIT_IF_FULL)) != 0;
  }

  /*
    Write the fixed compressed file header.
  */
  if (!error)
    error =
        write_header(mrg, header_length, used_trees, tot_elements, new_length);

  /* Flush the file buffer. */
  end_file_buffer();

  /* Display statistics. */
  DBUG_PRINT("info", ("Min record length: %6d  Max length: %6d  "
                      "Mean total length: %6ld\n",
                      mrg->min_pack_length, mrg->max_pack_length,
                      (ulong)(mrg->records ? (new_length / mrg->records) : 0)));
  if (verbose && mrg->records)
    printf(
        "Min record length: %6d   Max length: %6d   "
        "Mean total length: %6ld\n",
        mrg->min_pack_length, mrg->max_pack_length,
        (ulong)(new_length / mrg->records));

  /* Close source and target file. */
  if (!test_only) {
    error |= my_close(new_file, MYF(MY_WME));
    if (!result_table) {
      error |= my_close(isam_file->dfile, MYF(MY_WME));
      isam_file->dfile = -1; /* Tell mi_close file is closed */
    }
  }

  /* Cleanup. */
  free_counts_and_tree_and_queue(huff_trees, trees, huff_counts, fields);
  if (!test_only && !error) {
    if (result_table) {
      error = save_state_mrg(join_isam_file, mrg, new_length, glob_crc);
    } else {
      if (backup) {
        if (my_rename(org_name, make_old_name(temp_name, isam_file->filename),
                      MYF(MY_WME)))
          error = 1;
        else {
          if (tmp_dir[0])
            error = my_copy(new_name, org_name, MYF(MY_WME));
          else
            error = my_rename(new_name, org_name, MYF(MY_WME));
          if (!error) {
            (void)my_copystat(temp_name, org_name, MYF(MY_COPYTIME));
            if (tmp_dir[0]) (void)my_delete(new_name, MYF(MY_WME));
          }
        }
      } else {
        if (tmp_dir[0]) {
          error = my_copy(new_name, org_name,
                          MYF(MY_WME | MY_HOLD_ORIGINAL_MODES | MY_COPYTIME));
          if (!error) (void)my_delete(new_name, MYF(MY_WME));
        } else
          error = my_redel(org_name, new_name, MYF(MY_WME | MY_COPYTIME));
      }
      if (!error) error = save_state(isam_file, mrg, new_length, glob_crc);
    }
  }
  error |= mrg_close(mrg);
  if (join_isam_file >= 0) error |= my_close(join_isam_file, MYF(MY_WME));
  if (error) {
    (void)fprintf(stderr, "Aborting: %s is not compressed\n", org_name);
    (void)my_delete(new_name, MYF(MY_WME));
    return -1;
  }
  if (write_loop || verbose) {
    if (old_length)
      printf("%.4g%%     \n", (((longlong)(old_length - new_length)) * 100.0 /
                               (longlong)old_length));
    else
      puts("Empty file saved in compressed format");
  }
  return 0;

err:
  free_counts_and_tree_and_queue(huff_trees, trees, huff_counts, fields);
  if (new_file >= 0) (void)my_close(new_file, MYF(0));
  if (join_isam_file >= 0) (void)my_close(join_isam_file, MYF(0));
  mrg_close(mrg);
  (void)fprintf(stderr, "Aborted: %s is not compressed\n", org_name);
  return -1;
}

/**
  Create FRM for the destination table for --join operation
  Copy the first table FRM as the destination table FRM file. Doing so
  will help the mysql server to recognize the newly created table.
  See Bug#36573.

  @param    source_table    Name of the source table
  @param    dest_table      Name of the destination table
  @retval   0               Successful copy operation

  @note  We always return 0 because we don't want myisampack to report error
  even if the copy operation fails.
*/

static int create_dest_frm(char *source_table, char *dest_table) {
  char source_name[FN_REFLEN], dest_name[FN_REFLEN];

  DBUG_TRACE;

  (void)fn_format(source_name, source_table, "", FRM_EXT,
                  MY_UNPACK_FILENAME | MY_RESOLVE_SYMLINKS);
  (void)fn_format(dest_name, dest_table, "", FRM_EXT,
                  MY_UNPACK_FILENAME | MY_RESOLVE_SYMLINKS);
  /*
    Error messages produced by my_copy() are suppressed as this
    is not vital for --join operation. User shouldn't see any error messages
    like "source file frm not found" and "unable to create destination frm
    file. So we don't pass the flag MY_WME -Write Message on Error to
    my_copy()
  */
  (void)my_copy(source_name, dest_name, MYF(MY_DONT_OVERWRITE_FILE));

  return 0;
}

/* Init a huff_count-struct for each field and init it */

static HUFF_COUNTS *init_huff_count(MI_INFO *info, my_off_t records) {
  uint i;
  HUFF_COUNTS *count;
  if ((count = (HUFF_COUNTS *)my_malloc(
           PSI_NOT_INSTRUMENTED, info->s->base.fields * sizeof(HUFF_COUNTS),
           MYF(MY_ZEROFILL | MY_WME)))) {
    for (i = 0; i < info->s->base.fields; i++) {
      enum en_fieldtype type;
      count[i].field_length = info->s->rec[i].length;
      type = count[i].field_type = (enum en_fieldtype)info->s->rec[i].type;
      if (type == FIELD_INTERVALL || type == FIELD_CONSTANT ||
          type == FIELD_ZERO)
        type = FIELD_NORMAL;
      if (count[i].field_length <= 8 &&
          (type == FIELD_NORMAL || type == FIELD_SKIP_ZERO))
        count[i].max_zero_fill = count[i].field_length;
      /*
        For every column initialize a tree, which is used to detect distinct
        column values. 'int_tree' works together with 'tree_buff' and
        'tree_pos'. It's keys are implemented by pointers into 'tree_buff'.
        This is accomplished by '-1' as the element size.
      */
      init_tree(&count[i].int_tree, 0, -1, compare_tree, false, nullptr,
                nullptr);
      if (records && type != FIELD_BLOB && type != FIELD_VARCHAR)
        count[i].tree_pos = count[i].tree_buff = (uchar *)my_malloc(
            PSI_NOT_INSTRUMENTED,
            count[i].field_length > 1 ? tree_buff_length : 2, MYF(MY_WME));
    }
  }
  return count;
}

/* Free memory used by counts and trees */

static void free_counts_and_tree_and_queue(HUFF_TREE *huff_trees, uint trees,
                                           HUFF_COUNTS *huff_counts,
                                           uint fields) {
  uint i;

  if (huff_trees) {
    for (i = 0; i < trees; i++) {
      if (huff_trees[i].element_buffer) my_free(huff_trees[i].element_buffer);
      if (huff_trees[i].code) my_free(huff_trees[i].code);
    }
    my_free(huff_trees);
  }
  if (huff_counts) {
    for (i = 0; i < fields; i++) {
      if (huff_counts[i].tree_buff) {
        my_free(huff_counts[i].tree_buff);
        delete_tree(&huff_counts[i].int_tree);
      }
    }
    my_free(huff_counts);
  }
  delete_queue(&queue); /* This is safe to free */
  return;
}

/* Read through old file and gather some statistics */

static int get_statistic(PACK_MRG_INFO *mrg, HUFF_COUNTS *huff_counts) {
  int error;
  uint length;
  ulong reclength, max_blob_length;
  uchar *record, *pos, *next_pos, *end_pos, *start_pos;
  ha_rows record_count;
  bool static_row_size;
  HUFF_COUNTS *count, *end_count;
  TREE_ELEMENT *element;
  DBUG_TRACE;

  reclength = mrg->file[0]->s->base.reclength;
  record = (uchar *)my_alloca(reclength);
  end_count = huff_counts + mrg->file[0]->s->base.fields;
  record_count = 0;
  glob_crc = 0;
  max_blob_length = 0;

  /* Check how to calculate checksum */
  static_row_size = true;
  for (count = huff_counts; count < end_count; count++) {
    if (count->field_type == FIELD_BLOB || count->field_type == FIELD_VARCHAR) {
      static_row_size = false;
      break;
    }
  }

  mrg_reset(mrg);
  while ((error = mrg_rrnd(mrg, record)) != HA_ERR_END_OF_FILE) {
    ulong tot_blob_length = 0;
    if (!error) {
      /* glob_crc is a checksum over all bytes of all records. */
      if (static_row_size)
        glob_crc += mi_static_checksum(mrg->file[0], record);
      else
        glob_crc += mi_checksum(mrg->file[0], record);

      /* Count the incidence of values separately for every column. */
      for (pos = record, count = huff_counts; count < end_count;
           count++, pos = next_pos) {
        next_pos = end_pos = (start_pos = pos) + count->field_length;

        /*
          Put the whole column value in a tree if there is room for it.
          'int_tree' is used to quickly check for duplicate values.
          'tree_buff' collects as many distinct column values as
          possible. If the field length is > 1, it is tree_buff_length,
          else 2 bytes. Each value is 'field_length' bytes big. If there
          are more distinct column values than fit into the buffer, we
          give up with this tree. BLOBs and VARCHARs do not have a
          tree_buff as it can only be used with fixed length columns.
          For the special case of field length == 1, we handle only the
          case that there is only one distinct value in the table(s).
          Otherwise, we can have a maximum of 256 distinct values. This
          is then handled by the normal Huffman tree build.

          Another limit for collecting distinct column values is the
          number of values itself. Since we would need to build a
          Huffman tree for the values, we are limited by the 'IS_OFFSET'
          constant. This constant expresses a bit which is used to
          determine if a tree element holds a final value or an offset
          to a child element. Hence, all values and offsets need to be
          smaller than 'IS_OFFSET'. A tree element is implemented with
          two integer values, one for the left branch and one for the
          right branch. For the extreme case that the first element
          points to the last element, the number of integers in the tree
          must be less or equal to IS_OFFSET. So the number of elements
          must be less or equal to IS_OFFSET / 2.

          WARNING: At first, we insert a pointer into the record buffer
          as the key for the tree. If we got a new distinct value, which
          is really inserted into the tree, instead of being counted
          only, we will copy the column value from the record buffer to
          'tree_buff' and adjust the key pointer of the tree accordingly.
        */
        if (count->tree_buff) {
          global_count = count;
          if (!(element = tree_insert(&count->int_tree, pos, 0,
                                      count->int_tree.custom_arg)) ||
              (element->count == 1 &&
               (count->tree_buff + tree_buff_length <
                count->tree_pos + count->field_length)) ||
              (count->int_tree.elements_in_tree > IS_OFFSET / 2) ||
              (count->field_length == 1 &&
               count->int_tree.elements_in_tree > 1)) {
            delete_tree(&count->int_tree);
            my_free(count->tree_buff);
            count->tree_buff = nullptr;
          } else {
            /*
              If tree_insert() succeeds, it either creates a new element
              or increments the counter of an existing element.
            */
            if (element->count == 1) {
              /* Copy the new column value into 'tree_buff'. */
              memcpy(count->tree_pos, pos, (size_t)count->field_length);
              /* Adjust the key pointer in the tree. */
              tree_set_pointer(element, count->tree_pos);
              /* Point behind the last column value so far. */
              count->tree_pos += count->field_length;
            }
          }
        }

        /* Save character counters and space-counts and zero-field-counts */
        if (count->field_type == FIELD_NORMAL ||
            count->field_type == FIELD_SKIP_ENDSPACE) {
          /* Ignore trailing space. */
          for (; end_pos > pos; end_pos--)
            if (end_pos[-1] != ' ') break;
          /* Empty fields are just counted. Go to the next record. */
          if (end_pos == pos) {
            count->empty_fields++;
            count->max_zero_fill = 0;
            continue;
          }
          /*
            Count the total of all trailing spaces and the number of
            short trailing spaces. Remember the longest trailing space.
          */
          length = (uint)(next_pos - end_pos);
          count->tot_end_space += length;
          if (length < 8) count->end_space[length]++;
          if (count->max_end_space < length) count->max_end_space = length;
        }

        if (count->field_type == FIELD_NORMAL ||
            count->field_type == FIELD_SKIP_PRESPACE) {
          /* Ignore leading space. */
          for (pos = start_pos; pos < end_pos; pos++)
            if (pos[0] != ' ') break;
          /* Empty fields are just counted. Go to the next record. */
          if (end_pos == pos) {
            count->empty_fields++;
            count->max_zero_fill = 0;
            continue;
          }
          /*
            Count the total of all leading spaces and the number of
            short leading spaces. Remember the longest leading space.
          */
          length = (uint)(pos - start_pos);
          count->tot_pre_space += length;
          if (length < 8) count->pre_space[length]++;
          if (count->max_pre_space < length) count->max_pre_space = length;
        }

        /* Calculate pos, end_pos, and max_length for variable length fields. */
        if (count->field_type == FIELD_BLOB) {
          uint field_length = count->field_length - portable_sizeof_char_ptr;
          ulong blob_length = _mi_calc_blob_length(field_length, start_pos);
          memcpy(&pos, start_pos + field_length, sizeof(char *));
          end_pos = pos + blob_length;
          tot_blob_length += blob_length;
          count->max_length = std::max(count->max_length, blob_length);
        } else if (count->field_type == FIELD_VARCHAR) {
          uint pack_length = HA_VARCHAR_PACKLENGTH(count->field_length - 1);
          length = (pack_length == 1 ? (uint) * (uchar *)start_pos
                                     : uint2korr(start_pos));
          pos = start_pos + pack_length;
          end_pos = pos + length;
          count->max_length = std::max(count->max_length, ulong(length));
        }

        /* Evaluate 'max_zero_fill' for short fields. */
        if (count->field_length <= 8 &&
            (count->field_type == FIELD_NORMAL ||
             count->field_type == FIELD_SKIP_ZERO)) {
          uint i;
          /* Zero fields are just counted. Go to the next record. */
          if (!memcmp((uchar *)start_pos, zero_string, count->field_length)) {
            count->zero_fields++;
            continue;
          }
          /*
            max_zero_fill starts with field_length. It is decreased every
            time a shorter "zero trailer" is found. It is set to zero when
            an empty field is found (see above). This suggests that the
            variable should be called 'min_zero_fill'.
          */
          for (i = 0; i < count->max_zero_fill && !end_pos[-1 - (int)i]; i++)
            ;
          if (i < count->max_zero_fill) count->max_zero_fill = i;
        }

        /* Ignore zero fields and check fields. */
        if (count->field_type == FIELD_ZERO || count->field_type == FIELD_CHECK)
          continue;

        /*
          Count the incidence of every byte value in the
          significant field value.
        */
        for (; pos < end_pos; pos++) count->counts[(uchar)*pos]++;

        /* Step to next field. */
      }

      if (tot_blob_length > max_blob_length) max_blob_length = tot_blob_length;
      record_count++;
      if (write_loop && record_count % WRITE_COUNT == 0) {
        printf("%lu\r", (ulong)record_count);
        (void)fflush(stdout);
      }
    } else if (error != HA_ERR_RECORD_DELETED) {
      (void)fprintf(stderr, "Got error %d while reading rows", error);
      break;
    }

    /* Step to next record. */
  }
  if (write_loop) {
    printf("            \r");
    (void)fflush(stdout);
  }

  /*
    If --debug=d,fakebigcodes is set, fake the counts to get big Huffman
    codes.
  */
  DBUG_EXECUTE_IF("fakebigcodes", fakebigcodes(huff_counts, end_count););

  DBUG_PRINT("info", ("Found the following number of incidents "
                      "of the byte codes:"));
  if (verbose >= 2)
    printf(
        "Found the following number of incidents "
        "of the byte codes:\n");
  for (count = huff_counts; count < end_count; count++) {
    uint idx;
    my_off_t total_count;
    char llbuf[32];

    DBUG_PRINT("info", ("column: %3u", (uint)(count - huff_counts + 1)));
    if (verbose >= 2) printf("column: %3u\n", (uint)(count - huff_counts + 1));
    if (count->tree_buff) {
      DBUG_PRINT("info", ("number of distinct values: %u",
                          (uint)((count->tree_pos - count->tree_buff) /
                                 count->field_length)));
      if (verbose >= 2)
        printf(
            "number of distinct values: %u\n",
            (uint)((count->tree_pos - count->tree_buff) / count->field_length));
    }
    total_count = 0;
    for (idx = 0; idx < 256; idx++) {
      if (count->counts[idx]) {
        total_count += count->counts[idx];
        DBUG_PRINT("info", ("counts[0x%02x]: %12s", idx,
                            llstr((longlong)count->counts[idx], llbuf)));
        if (verbose >= 2)
          printf("counts[0x%02x]: %12s\n", idx,
                 llstr((longlong)count->counts[idx], llbuf));
      }
    }
    DBUG_PRINT("info",
               ("total:        %12s", llstr((longlong)total_count, llbuf)));
    if ((verbose >= 2) && total_count) {
      printf("total:        %12s\n", llstr((longlong)total_count, llbuf));
    }
  }

  mrg->records = record_count;
  mrg->max_blob_length = max_blob_length;
  return error != HA_ERR_END_OF_FILE;
}

static int compare_huff_elements(void *not_used [[maybe_unused]], uchar *a,
                                 uchar *b) {
  return *((my_off_t *)a) < *((my_off_t *)b)
             ? -1
             : (*((my_off_t *)a) == *((my_off_t *)b) ? 0 : 1);
}

/* Check each tree if we should use pre-space-compress, end-space-
   compress, empty-field-compress or zero-field-compress */

static void check_counts(HUFF_COUNTS *huff_counts, uint trees,
                         my_off_t records) {
  uint space_fields, fill_zero_fields, field_count[(int)FIELD_enum_val_count];
  my_off_t old_length, new_length, length;
  DBUG_TRACE;

  memset(field_count, 0, sizeof(field_count));
  space_fields = fill_zero_fields = 0;

  for (; trees--; huff_counts++) {
    if (huff_counts->field_type == FIELD_BLOB) {
      huff_counts->length_bits = max_bit(huff_counts->max_length);
      goto found_pack;
    } else if (huff_counts->field_type == FIELD_VARCHAR) {
      huff_counts->length_bits = max_bit(huff_counts->max_length);
      goto found_pack;
    } else if (huff_counts->field_type == FIELD_CHECK) {
      huff_counts->bytes_packed = 0;
      huff_counts->counts[0] = 0;
      goto found_pack;
    }

    huff_counts->field_type = FIELD_NORMAL;
    huff_counts->pack_type = 0;

    /* Check for zero-filled records (in this column), or zero records. */
    if (huff_counts->zero_fields || !records) {
      my_off_t old_space_count;
      /*
        If there are only zero filled records (in this column),
        or no records at all, we are done.
      */
      if (huff_counts->zero_fields == records) {
        huff_counts->field_type = FIELD_ZERO;
        huff_counts->bytes_packed = 0;
        huff_counts->counts[0] = 0;
        goto found_pack;
      }
      /* Remember the number of significant spaces. */
      old_space_count = huff_counts->counts[static_cast<int>(' ')];
      /* Add all leading and trailing spaces. */
      huff_counts->counts[static_cast<int>(' ')] +=
          (huff_counts->tot_end_space + huff_counts->tot_pre_space +
           huff_counts->empty_fields * huff_counts->field_length);
      /* Check, what the compressed length of this would be. */
      old_length = calc_packed_length(huff_counts, 0) + records / 8;
      /* Get the number of zero bytes. */
      length = huff_counts->zero_fields * huff_counts->field_length;
      /* Add it to the counts. */
      huff_counts->counts[0] += length;
      /* Check, what the compressed length of this would be. */
      new_length = calc_packed_length(huff_counts, 0);
      /* If the compression without the zeroes would be shorter, we are done. */
      if (old_length < new_length && huff_counts->field_length > 1) {
        huff_counts->field_type = FIELD_SKIP_ZERO;
        huff_counts->counts[0] -= length;
        huff_counts->bytes_packed = old_length - records / 8;
        goto found_pack;
      }
      /* Remove the insignificant spaces, but keep the zeroes. */
      huff_counts->counts[static_cast<int>(' ')] = old_space_count;
    }
    /* Check, what the compressed length of this column would be. */
    huff_counts->bytes_packed = calc_packed_length(huff_counts, 0);

    /*
      If there are enough empty records (in this column),
      treating them specially may pay off.
    */
    if (huff_counts->empty_fields) {
      if (huff_counts->field_length > 2 &&
          huff_counts->empty_fields +
                  (records - huff_counts->empty_fields) *
                      (1 + max_bit(std::max(huff_counts->max_pre_space,
                                            huff_counts->max_end_space))) <
              records * max_bit(huff_counts->field_length)) {
        huff_counts->pack_type |= PACK_TYPE_SPACE_FIELDS;
      } else {
        length = huff_counts->empty_fields * huff_counts->field_length;
        if (huff_counts->tot_end_space || !huff_counts->tot_pre_space) {
          huff_counts->tot_end_space += length;
          huff_counts->max_end_space = huff_counts->field_length;
          if (huff_counts->field_length < 8)
            huff_counts->end_space[huff_counts->field_length] +=
                huff_counts->empty_fields;
        }
        if (huff_counts->tot_pre_space) {
          huff_counts->tot_pre_space += length;
          huff_counts->max_pre_space = huff_counts->field_length;
          if (huff_counts->field_length < 8)
            huff_counts->pre_space[huff_counts->field_length] +=
                huff_counts->empty_fields;
        }
      }
    }

    /*
      If there are enough trailing spaces (in this column),
      treating them specially may pay off.
    */
    if (huff_counts->tot_end_space) {
      huff_counts->counts[static_cast<int>(' ')] += huff_counts->tot_pre_space;
      if (test_space_compress(huff_counts, records, huff_counts->max_end_space,
                              huff_counts->end_space,
                              huff_counts->tot_end_space, FIELD_SKIP_ENDSPACE))
        goto found_pack;
      huff_counts->counts[static_cast<int>(' ')] -= huff_counts->tot_pre_space;
    }

    /*
      If there are enough leading spaces (in this column),
      treating them specially may pay off.
    */
    if (huff_counts->tot_pre_space) {
      if (test_space_compress(huff_counts, records, huff_counts->max_pre_space,
                              huff_counts->pre_space,
                              huff_counts->tot_pre_space, FIELD_SKIP_PRESPACE))
        goto found_pack;
    }

  found_pack: /* Found field-packing */

    /* Test if we can use zero-fill */

    if (huff_counts->max_zero_fill &&
        (huff_counts->field_type == FIELD_NORMAL ||
         huff_counts->field_type == FIELD_SKIP_ZERO)) {
      huff_counts->counts[0] -= huff_counts->max_zero_fill *
                                (huff_counts->field_type == FIELD_SKIP_ZERO
                                     ? records - huff_counts->zero_fields
                                     : records);
      huff_counts->pack_type |= PACK_TYPE_ZERO_FILL;
      huff_counts->bytes_packed = calc_packed_length(huff_counts, 0);
    }

    /* Test if intervall-field is better */

    if (huff_counts->tree_buff) {
      HUFF_TREE tree;

      DBUG_EXECUTE_IF("forceintervall",
                      huff_counts->bytes_packed = ~(my_off_t)0;);
      tree.element_buffer = nullptr;
      if (!make_huff_tree(&tree, huff_counts) &&
          tree.bytes_packed + tree.tree_pack_length <
              huff_counts->bytes_packed) {
        if (tree.elements == 1)
          huff_counts->field_type = FIELD_CONSTANT;
        else
          huff_counts->field_type = FIELD_INTERVALL;
        huff_counts->pack_type = 0;
      } else {
        my_free(huff_counts->tree_buff);
        delete_tree(&huff_counts->int_tree);
        huff_counts->tree_buff = nullptr;
      }
      if (tree.element_buffer) my_free(tree.element_buffer);
    }
    if (huff_counts->pack_type & PACK_TYPE_SPACE_FIELDS) space_fields++;
    if (huff_counts->pack_type & PACK_TYPE_ZERO_FILL) fill_zero_fields++;
    field_count[huff_counts->field_type]++;
  }
  DBUG_PRINT("info", ("normal:    %3d  empty-space:     %3d  "
                      "empty-zero:       %3d  empty-fill: %3d",
                      field_count[FIELD_NORMAL], space_fields,
                      field_count[FIELD_SKIP_ZERO], fill_zero_fields));
  DBUG_PRINT("info", ("pre-space: %3d  end-space:       %3d  "
                      "intervall-fields: %3d  zero:       %3d",
                      field_count[FIELD_SKIP_PRESPACE],
                      field_count[FIELD_SKIP_ENDSPACE],
                      field_count[FIELD_INTERVALL], field_count[FIELD_ZERO]));
  if (verbose)
    printf(
        "\nnormal:    %3d  empty-space:     %3d  "
        "empty-zero:       %3d  empty-fill: %3d\n"
        "pre-space: %3d  end-space:       %3d  "
        "intervall-fields: %3d  zero:       %3d\n",
        field_count[FIELD_NORMAL], space_fields, field_count[FIELD_SKIP_ZERO],
        fill_zero_fields, field_count[FIELD_SKIP_PRESPACE],
        field_count[FIELD_SKIP_ENDSPACE], field_count[FIELD_INTERVALL],
        field_count[FIELD_ZERO]);
}

/* Test if we can use space-compression and empty-field-compression */

static int test_space_compress(HUFF_COUNTS *huff_counts, my_off_t records,
                               uint max_space_length, my_off_t *space_counts,
                               my_off_t tot_space_count,
                               enum en_fieldtype field_type) {
  int min_pos;
  uint length_bits, i;
  my_off_t space_count, min_space_count, min_pack, new_length, skip;

  length_bits = max_bit(max_space_length);

  /* Default no end_space-packing */
  space_count = huff_counts->counts[(uint)' '];
  min_space_count = (huff_counts->counts[(uint)' '] += tot_space_count);
  min_pack = calc_packed_length(huff_counts, 0);
  min_pos = -2;
  huff_counts->counts[(uint)' '] = space_count;

  /* Test with always space-count */
  new_length = huff_counts->bytes_packed + length_bits * records / 8;
  if (new_length + 1 < min_pack) {
    min_pos = -1;
    min_pack = new_length;
    min_space_count = space_count;
  }
  /* Test with length-flag */
  for (skip = 0L, i = 0; i < 8; i++) {
    if (space_counts[i]) {
      if (i) huff_counts->counts[(uint)' '] += space_counts[i];
      skip += huff_counts->pre_space[i];
      new_length = calc_packed_length(huff_counts, 0) +
                   (records + (records - skip) * (1 + length_bits)) / 8;
      if (new_length < min_pack) {
        min_pos = (int)i;
        min_pack = new_length;
        min_space_count = huff_counts->counts[(uint)' '];
      }
    }
  }

  huff_counts->counts[(uint)' '] = min_space_count;
  huff_counts->bytes_packed = min_pack;
  switch (min_pos) {
    case -2:
      return (0); /* No space-compress */
    case -1:      /* Always space-count */
      huff_counts->field_type = field_type;
      huff_counts->min_space = 0;
      huff_counts->length_bits = max_bit(max_space_length);
      break;
    default:
      huff_counts->field_type = field_type;
      huff_counts->min_space = (uint)min_pos;
      huff_counts->pack_type |= PACK_TYPE_SELECTED;
      huff_counts->length_bits = max_bit(max_space_length);
      break;
  }
  return (1); /* Using space-compress */
}

/* Make a huff_tree of each huff_count */

static HUFF_TREE *make_huff_trees(HUFF_COUNTS *huff_counts, uint trees) {
  uint tree;
  HUFF_TREE *huff_tree;
  DBUG_TRACE;

  if (!(huff_tree = (HUFF_TREE *)my_malloc(PSI_NOT_INSTRUMENTED,
                                           trees * sizeof(HUFF_TREE),
                                           MYF(MY_WME | MY_ZEROFILL))))
    return nullptr;

  for (tree = 0; tree < trees; tree++) {
    if (make_huff_tree(huff_tree + tree, huff_counts + tree)) {
      while (tree--) my_free(huff_tree[tree].element_buffer);
      my_free(huff_tree);
      return nullptr;
    }
  }
  return huff_tree;
}

/*
  Build a Huffman tree.

  SYNOPSIS
    make_huff_tree()
    huff_tree                   The Huffman tree.
    huff_counts                 The counts.

  DESCRIPTION
    Build a Huffman tree according to huff_counts->counts or
    huff_counts->tree_buff. tree_buff, if non-NULL contains up to
    tree_buff_length of distinct column values. In that case, whole
    values can be Huffman encoded instead of single bytes.

  RETURN
    0           OK
    != 0        Error
*/

static int make_huff_tree(HUFF_TREE *huff_tree, HUFF_COUNTS *huff_counts) {
  uint i, found, bits_packed, first, last;
  my_off_t bytes_packed;
  HUFF_ELEMENT *a, *b, *new_huff_el;

  first = last = 0;
  if (huff_counts->tree_buff) {
    /* Calculate the number of distinct values in tree_buff. */
    found = (uint)(huff_counts->tree_pos - huff_counts->tree_buff) /
            huff_counts->field_length;
    first = 0;
    last = found - 1;
  } else {
    /* Count the number of byte codes found in the column. */
    for (i = found = 0; i < 256; i++) {
      if (huff_counts->counts[i]) {
        if (!found++) first = i;
        last = i;
      }
    }
    if (found < 2) found = 2;
  }

  /* When using 'tree_buff' we can have more that 256 values. */
  if (queue.max_elements < found) {
    delete_queue(&queue);
    if (init_queue(&queue, key_memory_QUEUE, found, 0, false,
                   compare_huff_elements, nullptr))
      return -1;
  }

  /* Allocate or reallocate an element buffer for the Huffman tree. */
  if (!huff_tree->element_buffer) {
    if (!(huff_tree->element_buffer = (HUFF_ELEMENT *)my_malloc(
              PSI_NOT_INSTRUMENTED, found * 2 * sizeof(HUFF_ELEMENT),
              MYF(MY_WME))))
      return 1;
  } else {
    HUFF_ELEMENT *temp;
    if (!(temp = (HUFF_ELEMENT *)my_realloc(
              PSI_NOT_INSTRUMENTED, (uchar *)huff_tree->element_buffer,
              found * 2 * sizeof(HUFF_ELEMENT), MYF(MY_WME))))
      return 1;
    huff_tree->element_buffer = temp;
  }

  huff_counts->tree = huff_tree;
  huff_tree->counts = huff_counts;
  huff_tree->min_chr = first;
  huff_tree->max_chr = last;
  huff_tree->char_bits = max_bit(last - first);
  huff_tree->offset_bits = max_bit(found - 1) + 1;

  if (huff_counts->tree_buff) {
    huff_tree->elements = 0;
    huff_tree->tree_pack_length =
        (1 + 15 + 16 + 5 + 5 + (huff_tree->char_bits + 1) * found +
         (huff_tree->offset_bits + 1) * (found - 2) + 7) /
            8 +
        (uint)(huff_tree->counts->tree_pos - huff_tree->counts->tree_buff);
    /*
      Put a HUFF_ELEMENT into the queue for every distinct column value.

      tree_walk() calls save_counts_in_queue() for every element in
      'int_tree'. This takes elements from the target trees element
      buffer and places references to them into the buffer of the
      priority queue. We insert in column value order, but the order is
      in fact irrelevant here. We will establish the correct order
      later.
    */
    tree_walk(&huff_counts->int_tree, save_counts_in_queue, (uchar *)huff_tree,
              left_root_right);
  } else {
    huff_tree->elements = found;
    huff_tree->tree_pack_length =
        (9 + 9 + 5 + 5 + (huff_tree->char_bits + 1) * found +
         (huff_tree->offset_bits + 1) * (found - 2) + 7) /
        8;
    /*
      Put a HUFF_ELEMENT into the queue for every byte code found in the column.

      The elements are taken from the target trees element buffer.
      Instead of using queue_insert(), we just place references to the
      elements into the buffer of the priority queue. We insert in byte
      value order, but the order is in fact irrelevant here. We will
      establish the correct order later.
    */
    for (i = first, found = 0; i <= last; i++) {
      if (huff_counts->counts[i]) {
        new_huff_el = huff_tree->element_buffer + (found++);
        new_huff_el->count = huff_counts->counts[i];
        new_huff_el->a.leaf.null = nullptr;
        new_huff_el->a.leaf.element_nr = i;
        queue.root[found] = (uchar *)new_huff_el;
      }
    }
    /*
      If there is only a single byte value in this field in all records,
      add a second element with zero incidence. This is required to enter
      the loop, which builds the Huffman tree.
    */
    while (found < 2) {
      new_huff_el = huff_tree->element_buffer + (found++);
      new_huff_el->count = 0;
      new_huff_el->a.leaf.null = nullptr;
      if (last)
        new_huff_el->a.leaf.element_nr = huff_tree->min_chr = last - 1;
      else
        new_huff_el->a.leaf.element_nr = huff_tree->max_chr = last + 1;
      queue.root[found] = (uchar *)new_huff_el;
    }
  }

  /* Make a queue from the queue buffer. */
  queue.elements = found;

  /*
    Make a priority queue from the queue. Construct its index so that we
    have a partially ordered tree.
  */
  for (i = found / 2; i > 0; i--) _downheap(&queue, i);

  /* The Huffman algorithm. */
  bytes_packed = 0;
  bits_packed = 0;
  for (i = 1; i < found; i++) {
    /*
      Pop the top element from the queue (the one with the least incidence).
      Popping from a priority queue includes a re-ordering of the queue,
      to get the next least incidence element to the top.
    */
    a = (HUFF_ELEMENT *)queue_remove(&queue, 0);
    /*
      Copy the next least incidence element. The queue implementation
      reserves root[0] for temporary purposes. root[1] is the top.
    */
    b = (HUFF_ELEMENT *)queue.root[1];
    /* Get a new element from the element buffer. */
    new_huff_el = huff_tree->element_buffer + found + i;
    /* The new element gets the sum of the two least incidence elements. */
    new_huff_el->count = a->count + b->count;
    /*
      The Huffman algorithm assigns another bit to the code for a byte
      every time that bytes incidence is combined (directly or indirectly)
      to a new element as one of the two least incidence elements.
      This means that one more bit per incidence of that byte is required
      in the resulting file. So we add the new combined incidence as the
      number of bits by which the result grows.
    */
    bits_packed += (uint)(new_huff_el->count & 7);
    bytes_packed += new_huff_el->count / 8;
    /* The new element points to its children, lesser in left.  */
    new_huff_el->a.nod.left = a;
    new_huff_el->a.nod.right = b;
    /*
      Replace the copied top element by the new element and re-order the
      queue.
    */
    queue.root[1] = (uchar *)new_huff_el;
    queue_replaced(&queue);
  }
  huff_tree->root = (HUFF_ELEMENT *)queue.root[1];
  huff_tree->bytes_packed = bytes_packed + (bits_packed + 7) / 8;
  return 0;
}

static int compare_tree(const void *cmp_arg [[maybe_unused]], const void *a,
                        const void *b) {
  uint length;
  const uchar *s = (const uchar *)a;
  const uchar *t = (const uchar *)b;
  for (length = global_count->field_length; length--;)
    if (*s++ != *t++) return (int)s[-1] - (int)t[-1];
  return 0;
}

/*
  Organize distinct column values and their incidences into a priority queue.

  SYNOPSIS
    save_counts_in_queue()
    key                         The column value.
    count                       The incidence of this value.
    tree                        The Huffman tree to be built later.

  DESCRIPTION
    We use the element buffer of the targeted tree. The distinct column
    values are organized in a priority queue first. The Huffman
    algorithm will later organize the elements into a Huffman tree. For
    the time being, we just place references to the elements into the
    queue buffer. The buffer will later be organized into a priority
    queue.

  RETURN
    0
 */

static int save_counts_in_queue(void *v_key, element_count count,
                                void *v_tree) {
  uchar *key = static_cast<uchar *>(v_key);
  HUFF_TREE *tree = static_cast<HUFF_TREE *>(v_tree);
  HUFF_ELEMENT *new_huff_el;

  new_huff_el = tree->element_buffer + (tree->elements++);
  new_huff_el->count = count;
  new_huff_el->a.leaf.null = nullptr;
  new_huff_el->a.leaf.element_nr =
      (uint)(key - tree->counts->tree_buff) / tree->counts->field_length;
  queue.root[tree->elements] = (uchar *)new_huff_el;
  return 0;
}

/*
  Calculate length of file if given counts should be used.

  SYNOPSIS
    calc_packed_length()
    huff_counts                 The counts for a column of the table(s).
    add_tree_lenght             If the decode tree length should be added.

  DESCRIPTION
    We need to follow the Huffman algorithm until we know, how many bits
    are required for each byte code. But we do not need the resulting
    Huffman tree. Hence, we can leave out some steps which are essential
    in make_huff_tree().

  RETURN
    Number of bytes required to compress this table column.
*/

static my_off_t calc_packed_length(HUFF_COUNTS *huff_counts,
                                   uint add_tree_lenght) {
  uint i, found, bits_packed, first, last;
  my_off_t bytes_packed;
  HUFF_ELEMENT element_buffer[256];
  DBUG_TRACE;

  /*
    WARNING: We use a small hack for efficiency: Instead of placing
    references to HUFF_ELEMENTs into the queue, we just insert
    references to the counts of the byte codes which appeared in this
    table column. During the Huffman algorithm they are successively
    replaced by references to HUFF_ELEMENTs. This works, because
    HUFF_ELEMENTs have the incidence count at their beginning.
    Regardless, whether the queue array contains references to counts of
    type my_off_t or references to HUFF_ELEMENTs which have the count of
    type my_off_t at their beginning, it always points to a count of the
    same type.

    Instead of using queue_insert(), we just copy the references into
    the buffer of the priority queue. We insert in byte value order, but
    the order is in fact irrelevant here. We will establish the correct
    order later.
  */
  first = last = 0;
  for (i = found = 0; i < 256; i++) {
    if (huff_counts->counts[i]) {
      if (!found++) first = i;
      last = i;
      /* We start with root[1], which is the queues top element. */
      queue.root[found] = (uchar *)&huff_counts->counts[i];
    }
  }
  if (!found) return 0; /* Empty tree */
  /*
    If there is only a single byte value in this field in all records,
    add a second element with zero incidence. This is required to enter
    the loop, which follows the Huffman algorithm.
  */
  if (found < 2)
    queue.root[++found] = (uchar *)&huff_counts->counts[last ? 0 : 1];

  /* Make a queue from the queue buffer. */
  queue.elements = found;

  bytes_packed = 0;
  bits_packed = 0;
  /* Add the length of the coding table, which would become part of the file. */
  if (add_tree_lenght)
    bytes_packed = (8 + 9 + 5 + 5 + (max_bit(last - first) + 1) * found +
                    (max_bit(found - 1) + 1 + 1) * (found - 2) + 7) /
                   8;

  /*
    Make a priority queue from the queue. Construct its index so that we
    have a partially ordered tree.
  */
  for (i = (found + 1) / 2; i > 0; i--) _downheap(&queue, i);

  /* The Huffman algorithm. */
  for (i = 0; i < found - 1; i++) {
    my_off_t *a;
    my_off_t *b;
    HUFF_ELEMENT *new_huff_el;

    /*
      Pop the top element from the queue (the one with the least
      incidence). Popping from a priority queue includes a re-ordering
      of the queue, to get the next least incidence element to the top.
    */
    a = (my_off_t *)queue_remove(&queue, 0);
    /*
      Copy the next least incidence element. The queue implementation
      reserves root[0] for temporary purposes. root[1] is the top.
    */
    b = (my_off_t *)queue.root[1];
    /* Create a new element in a local (automatic) buffer. */
    new_huff_el = element_buffer + i;
    /* The new element gets the sum of the two least incidence elements. */
    new_huff_el->count = *a + *b;
    /*
      The Huffman algorithm assigns another bit to the code for a byte
      every time that bytes incidence is combined (directly or indirectly)
      to a new element as one of the two least incidence elements.
      This means that one more bit per incidence of that byte is required
      in the resulting file. So we add the new combined incidence as the
      number of bits by which the result grows.
    */
    bits_packed += (uint)(new_huff_el->count & 7);
    bytes_packed += new_huff_el->count / 8;
    /*
      Replace the copied top element by the new element and re-order the
      queue. This successively replaces the references to counts by
      references to HUFF_ELEMENTs.
    */
    queue.root[1] = (uchar *)new_huff_el;
    queue_replaced(&queue);
  }
  return bytes_packed + (bits_packed + 7) / 8;
}

/* Remove trees that don't give any compression */

static uint join_same_trees(HUFF_COUNTS *huff_counts, uint trees) {
  uint k, tree_number;
  HUFF_COUNTS count, *i, *j, *last_count;

  last_count = huff_counts + trees;
  for (tree_number = 0, i = huff_counts; i < last_count; i++) {
    if (!i->tree->tree_number) {
      i->tree->tree_number = ++tree_number;
      if (i->tree_buff) continue; /* Don't join intervall */
      for (j = i + 1; j < last_count; j++) {
        if (!j->tree->tree_number && !j->tree_buff) {
          for (k = 0; k < 256; k++)
            count.counts[k] = i->counts[k] + j->counts[k];
          if (calc_packed_length(&count, 1) <=
              i->tree->bytes_packed + j->tree->bytes_packed +
                  i->tree->tree_pack_length + j->tree->tree_pack_length +
                  ALLOWED_JOIN_DIFF) {
            memcpy(i->counts, count.counts, sizeof(count.counts[0]) * 256);
            my_free(j->tree->element_buffer);
            j->tree->element_buffer = nullptr;
            j->tree = i->tree;
            memmove((uchar *)i->counts, (uchar *)count.counts,
                    sizeof(count.counts[0]) * 256);
            if (make_huff_tree(i->tree, i)) return (uint)-1;
          }
        }
      }
    }
  }
  DBUG_PRINT("info",
             ("Original trees:  %d  After join: %d", trees, tree_number));
  if (verbose)
    printf("Original trees:  %d  After join: %d\n", trees, tree_number);
  return tree_number; /* Return trees left */
}

/*
  Fill in huff_tree encode tables.

  SYNOPSIS
    make_huff_decode_table()
    huff_tree               An array of HUFF_TREE which are to be encoded.
    trees                   The number of HUFF_TREE in the array.

  RETURN
    0           success
    != 0        error
*/

static int make_huff_decode_table(HUFF_TREE *huff_tree, uint trees) {
  uint elements;
  for (; trees--; huff_tree++) {
    if (huff_tree->tree_number > 0) {
      elements = huff_tree->counts->tree_buff ? huff_tree->elements : 256;
      if (!(huff_tree->code = (ulonglong *)my_malloc(
                PSI_NOT_INSTRUMENTED,
                elements * (sizeof(ulonglong) + sizeof(uchar)),
                MYF(MY_WME | MY_ZEROFILL))))
        return 1;
      huff_tree->code_len = (uchar *)(huff_tree->code + elements);
      make_traverse_code_tree(huff_tree, huff_tree->root, 8 * sizeof(ulonglong),
                              0LL);
    }
  }
  return 0;
}

static void make_traverse_code_tree(HUFF_TREE *huff_tree, HUFF_ELEMENT *element,
                                    uint size, ulonglong code) {
  uint chr;
  if (!element->a.leaf.null) {
    chr = element->a.leaf.element_nr;
    huff_tree->code_len[chr] = (uchar)(8 * sizeof(ulonglong) - size);
    // >> 64 is undefined (platform specific)
    huff_tree->code[chr] = size >= 64 ? 0 : (code >> size);
    if (huff_tree->height < 8 * sizeof(ulonglong) - size)
      huff_tree->height = 8 * sizeof(ulonglong) - size;
  } else {
    size--;
    make_traverse_code_tree(huff_tree, element->a.nod.left, size, code);
    make_traverse_code_tree(huff_tree, element->a.nod.right, size,
                            code + (((ulonglong)1) << size));
  }
  return;
}

/*
  Convert a value into binary digits.

  SYNOPSIS
    bindigits()
    value                       The value.
    length                      The number of low order bits to convert.

  NOTE
    The result string is in static storage. It is reused on every call.
    So you cannot use it twice in one expression.

  RETURN
    A pointer to a static NUL-terminated string.
 */

static char *bindigits(ulonglong value, uint bits) {
  static char digits[72];
  char *ptr = digits;
  uint idx = bits;

  assert(idx < sizeof(digits));
  while (idx) *(ptr++) = '0' + ((char)(value >> (--idx)) & (char)1);
  *ptr = '\0';
  return digits;
}

/*
  Convert a value into hexadecimal digits.

  SYNOPSIS
    hexdigits()
    value                       The value.

  NOTE
    The result string is in static storage. It is reused on every call.
    So you cannot use it twice in one expression.

  RETURN
    A pointer to a static NUL-terminated string.
 */

static char *hexdigits(ulonglong value) {
  static char digits[20];
  char *ptr = digits;
  uint idx = 2 * sizeof(value); /* Two hex digits per byte. */

  assert(idx < sizeof(digits));
  while (idx) {
    if ((*(ptr++) = '0' + ((char)(value >> (4 * (--idx))) & (char)0xf)) > '9')
      *(ptr - 1) += 'a' - '9' - 1;
  }
  *ptr = '\0';
  return digits;
}

/* Write header to new packed data file */

static int write_header(PACK_MRG_INFO *mrg, uint head_length, uint trees,
                        my_off_t tot_elements, my_off_t filelength) {
  uchar *buff = (uchar *)file_buffer.pos;

  memset(buff, 0, HEAD_LENGTH);
  memcpy(buff, myisam_pack_file_magic, 4);
  int4store(buff + 4, head_length);
  int4store(buff + 8, mrg->min_pack_length);
  int4store(buff + 12, mrg->max_pack_length);
  int4store(buff + 16, (uint32)tot_elements);
  int4store(buff + 20, (uint32)intervall_length);
  int2store(buff + 24, trees);
  buff[26] = (char)mrg->ref_length;
  /* Save record pointer length */
  buff[27] = (uchar)mi_get_pointer_length((ulonglong)filelength, 2);
  if (test_only) return 0;
  my_seek(file_buffer.file, 0L, MY_SEEK_SET, MYF(0));
  return my_write(file_buffer.file, (const uchar *)file_buffer.pos, HEAD_LENGTH,
                  MYF(MY_WME | MY_NABP | MY_WAIT_IF_FULL)) != 0;
}

/* Write fieldinfo to new packed file */

static void write_field_info(HUFF_COUNTS *counts, uint fields, uint trees) {
  uint i;
  uint huff_tree_bits;
  huff_tree_bits = max_bit(trees ? trees - 1 : 0);

  DBUG_PRINT("info", (" "));
  DBUG_PRINT("info", ("column types:"));
  DBUG_PRINT("info", ("FIELD_NORMAL          0"));
  DBUG_PRINT("info", ("FIELD_SKIP_ENDSPACE   1"));
  DBUG_PRINT("info", ("FIELD_SKIP_PRESPACE   2"));
  DBUG_PRINT("info", ("FIELD_SKIP_ZERO       3"));
  DBUG_PRINT("info", ("FIELD_BLOB            4"));
  DBUG_PRINT("info", ("FIELD_CONSTANT        5"));
  DBUG_PRINT("info", ("FIELD_INTERVALL       6"));
  DBUG_PRINT("info", ("FIELD_ZERO            7"));
  DBUG_PRINT("info", ("FIELD_VARCHAR         8"));
  DBUG_PRINT("info", ("FIELD_CHECK           9"));
  DBUG_PRINT("info", (" "));
  DBUG_PRINT("info", ("pack type as a set of flags:"));
  DBUG_PRINT("info", ("PACK_TYPE_SELECTED      1"));
  DBUG_PRINT("info", ("PACK_TYPE_SPACE_FIELDS  2"));
  DBUG_PRINT("info", ("PACK_TYPE_ZERO_FILL     4"));
  DBUG_PRINT("info", (" "));
  if (verbose >= 2) {
    printf("\n");
    printf("column types:\n");
    printf("FIELD_NORMAL          0\n");
    printf("FIELD_SKIP_ENDSPACE   1\n");
    printf("FIELD_SKIP_PRESPACE   2\n");
    printf("FIELD_SKIP_ZERO       3\n");
    printf("FIELD_BLOB            4\n");
    printf("FIELD_CONSTANT        5\n");
    printf("FIELD_INTERVALL       6\n");
    printf("FIELD_ZERO            7\n");
    printf("FIELD_VARCHAR         8\n");
    printf("FIELD_CHECK           9\n");
    printf("\n");
    printf("pack type as a set of flags:\n");
    printf("PACK_TYPE_SELECTED      1\n");
    printf("PACK_TYPE_SPACE_FIELDS  2\n");
    printf("PACK_TYPE_ZERO_FILL     4\n");
    printf("\n");
  }
  for (i = 0; i++ < fields; counts++) {
    write_bits((ulonglong)(int)counts->field_type, 5);
    write_bits(counts->pack_type, 6);
    if (counts->pack_type & PACK_TYPE_ZERO_FILL)
      write_bits(counts->max_zero_fill, 5);
    else
      write_bits(counts->length_bits, 5);
    write_bits((ulonglong)counts->tree->tree_number - 1, huff_tree_bits);
    DBUG_PRINT("info", ("column: %3u  type: %2u  pack: %2u  zero: %4u  "
                        "lbits: %2u  tree: %2u  length: %4u",
                        i, counts->field_type, counts->pack_type,
                        counts->max_zero_fill, counts->length_bits,
                        counts->tree->tree_number, counts->field_length));
    if (verbose >= 2)
      printf(
          "column: %3u  type: %2u  pack: %2u  zero: %4u  lbits: %2u  "
          "tree: %2u  length: %4u\n",
          i, counts->field_type, counts->pack_type, counts->max_zero_fill,
          counts->length_bits, counts->tree->tree_number, counts->field_length);
  }
  flush_bits();
  return;
}

/* Write all huff_trees to new datafile. Return tot count of
   elements in all trees
   Returns 0 on error */

static my_off_t write_huff_tree(HUFF_TREE *huff_tree, uint trees) {
  uint i, int_length;
  uint tree_no;
  uint codes;
  uint errors = 0;
  uint *packed_tree, *offset, length;
  my_off_t elements;

  /* Find the highest number of elements in the trees. */
  for (i = length = 0; i < trees; i++)
    if (huff_tree[i].tree_number > 0 && huff_tree[i].elements > length)
      length = huff_tree[i].elements;
  /*
    Allocate a buffer for packing a decode tree. Two numbers per element
    (left child and right child).
  */
  if (!(packed_tree = (uint *)my_alloca(sizeof(uint) * length * 2))) {
    my_error(EE_OUTOFMEMORY, MYF(ME_FATALERROR), sizeof(uint) * length * 2);
    return 0;
  }

  DBUG_PRINT("info", (" "));
  if (verbose >= 2) printf("\n");
  tree_no = 0;
  intervall_length = 0;
  for (elements = 0; trees--; huff_tree++) {
    /* Skip columns that have been joined with other columns. */
    if (huff_tree->tree_number == 0) continue; /* Deleted tree */
    tree_no++;
    DBUG_PRINT("info", (" "));
    if (verbose >= 3) printf("\n");
    /* Count the total number of elements (byte codes or column values). */
    elements += huff_tree->elements;
    huff_tree->max_offset = 2;
    /* Build a tree of offsets and codes for decoding in 'packed_tree'. */
    if (huff_tree->elements <= 1)
      offset = packed_tree;
    else
      offset = make_offset_code_tree(huff_tree, huff_tree->root, packed_tree);

    /* This should be the same as 'length' above. */
    huff_tree->offset_bits = max_bit(huff_tree->max_offset);

    /*
      Since we check this during collecting the distinct column values,
      this should never happen.
    */
    if (huff_tree->max_offset >= IS_OFFSET) { /* This should be impossible */
      (void)fprintf(stderr, "Tree offset got too big: %d, aborted\n",
                    huff_tree->max_offset);
      return 0;
    }

    DBUG_PRINT("info", ("pos: %lu  elements: %u  tree-elements: %lu  "
                        "char_bits: %u\n",
                        (ulong)(file_buffer.pos - file_buffer.buffer),
                        huff_tree->elements, (ulong)(offset - packed_tree),
                        huff_tree->char_bits));
    if (!huff_tree->counts->tree_buff) {
      /* We do a byte compression on this column. Mark with bit 0. */
      write_bits(0, 1);
      write_bits(huff_tree->min_chr, 8);
      write_bits(huff_tree->elements, 9);
      write_bits(huff_tree->char_bits, 5);
      write_bits(huff_tree->offset_bits, 5);
      int_length = 0;
    } else {
      int_length =
          (uint)(huff_tree->counts->tree_pos - huff_tree->counts->tree_buff);
      /* We have distinct column values for this column. Mark with bit 1. */
      write_bits(1, 1);
      write_bits(huff_tree->elements, 15);
      write_bits(int_length, 16);
      write_bits(huff_tree->char_bits, 5);
      write_bits(huff_tree->offset_bits, 5);
      intervall_length += int_length;
    }
    DBUG_PRINT("info",
               ("tree: %2u  elements: %4u  char_bits: %2u  "
                "offset_bits: %2u  %s: %5u  codelen: %2u",
                tree_no, huff_tree->elements, huff_tree->char_bits,
                huff_tree->offset_bits,
                huff_tree->counts->tree_buff ? "bufflen" : "min_chr",
                huff_tree->counts->tree_buff ? int_length : huff_tree->min_chr,
                huff_tree->height));
    if (verbose >= 2)
      printf(
          "tree: %2u  elements: %4u  char_bits: %2u  offset_bits: %2u  "
          "%s: %5u  codelen: %2u\n",
          tree_no, huff_tree->elements, huff_tree->char_bits,
          huff_tree->offset_bits,
          huff_tree->counts->tree_buff ? "bufflen" : "min_chr",
          huff_tree->counts->tree_buff ? int_length : huff_tree->min_chr,
          huff_tree->height);

    /* Check that the code tree length matches the element count. */
    length = (uint)(offset - packed_tree);
    if (length != huff_tree->elements * 2 - 2) {
      (void)fprintf(stderr, "error: Huff-tree-length: %d != calc_length: %d\n",
                    length, huff_tree->elements * 2 - 2);
      errors++;
      break;
    }

    for (i = 0; i < length; i++) {
      if (packed_tree[i] & IS_OFFSET)
        write_bits(packed_tree[i] - IS_OFFSET + (1 << huff_tree->offset_bits),
                   huff_tree->offset_bits + 1);
      else
        write_bits(packed_tree[i] - huff_tree->min_chr,
                   huff_tree->char_bits + 1);
      DBUG_PRINT("info",
                 ("tree[0x%04x]: %s0x%04x", i,
                  (packed_tree[i] & IS_OFFSET) ? " -> " : "",
                  (packed_tree[i] & IS_OFFSET) ? packed_tree[i] - IS_OFFSET + i
                                               : packed_tree[i]));
      if (verbose >= 3)
        printf("tree[0x%04x]: %s0x%04x\n", i,
               (packed_tree[i] & IS_OFFSET) ? " -> " : "",
               (packed_tree[i] & IS_OFFSET) ? packed_tree[i] - IS_OFFSET + i
                                            : packed_tree[i]);
    }
    flush_bits();

    /*
      Display coding tables and check their correctness.
    */
    codes = huff_tree->counts->tree_buff ? huff_tree->elements : 256;
    for (i = 0; i < codes; i++) {
      ulonglong code;
      uint bits;
      uint len;
      uint idx;

      if (!(len = huff_tree->code_len[i])) continue;
      DBUG_PRINT("info",
                 ("code[0x%04x]:      0x%s  bits: %2u  bin: %s", i,
                  hexdigits(huff_tree->code[i]), huff_tree->code_len[i],
                  bindigits(huff_tree->code[i], huff_tree->code_len[i])));
      if (verbose >= 3)
        printf("code[0x%04x]:      0x%s  bits: %2u  bin: %s\n", i,
               hexdigits(huff_tree->code[i]), huff_tree->code_len[i],
               bindigits(huff_tree->code[i], huff_tree->code_len[i]));

      /* Check that the encode table decodes correctly. */
      code = 0;
      bits = 0;
      idx = 0;
      DBUG_EXECUTE_IF("forcechkerr1", len--;);
      DBUG_EXECUTE_IF("forcechkerr2", bits = 8 * sizeof(code););
      DBUG_EXECUTE_IF("forcechkerr3", idx = length;);
      for (;;) {
        if (!len) {
          (void)fflush(stdout);
          (void)fprintf(stderr, "error: code 0x%s with %u bits not found\n",
                        hexdigits(huff_tree->code[i]), huff_tree->code_len[i]);
          errors++;
          break;
        }
        code <<= 1;
        code |= (huff_tree->code[i] >> (--len)) & 1;
        bits++;
        if (bits > 8 * sizeof(code)) {
          (void)fflush(stdout);
          (void)fprintf(stderr, "error: Huffman code too long: %u/%u\n", bits,
                        (uint)(8 * sizeof(code)));
          errors++;
          break;
        }
        idx += (uint)code & 1;
        if (idx >= length) {
          (void)fflush(stdout);
          (void)fprintf(stderr, "error: illegal tree offset: %u/%u\n", idx,
                        length);
          errors++;
          break;
        }
        if (packed_tree[idx] & IS_OFFSET)
          idx += packed_tree[idx] & ~IS_OFFSET;
        else
          break; /* Hit a leaf. This contains the result value. */
      }
      if (errors) break;

      DBUG_EXECUTE_IF("forcechkerr4", packed_tree[idx]++;);
      if (packed_tree[idx] != i) {
        (void)fflush(stdout);
        (void)fprintf(stderr,
                      "error: decoded value 0x%04x  should be: 0x%04x\n",
                      packed_tree[idx], i);
        errors++;
        break;
      }
    } /*end for (codes)*/
    if (errors) break;

    /* Write column values in case of distinct column value compression. */
    if (huff_tree->counts->tree_buff) {
      for (i = 0; i < int_length; i++) {
        write_bits((ulonglong)(uchar)huff_tree->counts->tree_buff[i], 8);
        DBUG_PRINT("info", ("column_values[0x%04x]: 0x%02x", i,
                            (uchar)huff_tree->counts->tree_buff[i]));
        if (verbose >= 3)
          printf("column_values[0x%04x]: 0x%02x\n", i,
                 (uchar)huff_tree->counts->tree_buff[i]);
      }
    }
    flush_bits();
  }
  DBUG_PRINT("info", (" "));
  if (verbose >= 2) printf("\n");
  if (errors) {
    (void)fprintf(stderr, "Error: Generated decode trees are corrupt. Stop.\n");
    return 0;
  }
  return elements;
}

static uint *make_offset_code_tree(HUFF_TREE *huff_tree, HUFF_ELEMENT *element,
                                   uint *offset) {
  uint *prev_offset;

  prev_offset = offset;
  /*
    'a.leaf.null' takes the same place as 'a.nod.left'. If this is null,
    then there is no left child and, hence no right child either. This
    is a property of a binary tree. An element is either a node with two
    children, or a leaf without children.

    The current element is always a node with two children. Go left first.
  */
  if (!element->a.nod.left->a.leaf.null) {
    /* Store the byte code or the index of the column value. */
    prev_offset[0] = (uint)element->a.nod.left->a.leaf.element_nr;
    offset += 2;
  } else {
    /*
      Recursively traverse the tree to the left. Mark it as an offset to
      another tree node (in contrast to a byte code or column value index).
    */
    prev_offset[0] = IS_OFFSET + 2;
    offset = make_offset_code_tree(huff_tree, element->a.nod.left, offset + 2);
  }

  /* Now, check the right child. */
  if (!element->a.nod.right->a.leaf.null) {
    /* Store the byte code or the index of the column value. */
    prev_offset[1] = element->a.nod.right->a.leaf.element_nr;
    return offset;
  } else {
    /*
      Recursively traverse the tree to the right. Mark it as an offset to
      another tree node (in contrast to a byte code or column value index).
    */
    uint temp = (uint)(offset - prev_offset - 1);
    prev_offset[1] = IS_OFFSET + temp;
    if (huff_tree->max_offset < temp) huff_tree->max_offset = temp;
    return make_offset_code_tree(huff_tree, element->a.nod.right, offset);
  }
}

/* Get number of bits needed to represent value */

static uint max_bit(uint value) {
  uint power = 1;

  while ((value >>= 1)) power++;
  return (power);
}

static int compress_isam_file(PACK_MRG_INFO *mrg, HUFF_COUNTS *huff_counts) {
  int error;
  uint i, max_calc_length, pack_ref_length, min_record_length,
      max_record_length, intervall, field_length, max_pack_length,
      pack_blob_length;
  my_off_t record_count;
  char llbuf[32];
  ulong length, pack_length;
  uchar *record, *pos, *end_pos, *record_pos, *start_pos;
  HUFF_COUNTS *count, *end_count;
  HUFF_TREE *tree;
  MI_INFO *isam_file = mrg->file[0];
  uint pack_version = (uint)isam_file->s->pack.version;
  DBUG_TRACE;

  /* Allocate a buffer for the records (excluding blobs). */
  if (!(record = (uchar *)my_alloca(isam_file->s->base.reclength))) return -1;

  end_count = huff_counts + isam_file->s->base.fields;
  min_record_length = (uint)~0;
  max_record_length = 0;

  /*
    Calculate the maximum number of bits required to pack the records.
    Remember to understand 'max_zero_fill' as 'min_zero_fill'.
    The tree height determines the maximum number of bits per value.
    Some fields skip leading or trailing spaces or zeroes. The skipped
    number of bytes is encoded by 'length_bits' bits.
    Empty blobs and varchar are encoded with a single 1 bit. Other blobs
    and varchar get a leading 0 bit.
  */
  for (i = max_calc_length = 0; i < isam_file->s->base.fields; i++) {
    if (!(huff_counts[i].pack_type & PACK_TYPE_ZERO_FILL))
      huff_counts[i].max_zero_fill = 0;
    if (huff_counts[i].field_type == FIELD_CONSTANT ||
        huff_counts[i].field_type == FIELD_ZERO ||
        huff_counts[i].field_type == FIELD_CHECK)
      continue;
    if (huff_counts[i].field_type == FIELD_INTERVALL)
      max_calc_length += huff_counts[i].tree->height;
    else if (huff_counts[i].field_type == FIELD_BLOB ||
             huff_counts[i].field_type == FIELD_VARCHAR)
      max_calc_length +=
          huff_counts[i].tree->height * huff_counts[i].max_length +
          huff_counts[i].length_bits + 1;
    else
      max_calc_length +=
          (huff_counts[i].field_length - huff_counts[i].max_zero_fill) *
              huff_counts[i].tree->height +
          huff_counts[i].length_bits;
  }
  max_calc_length = (max_calc_length + 7) / 8;
  pack_ref_length = calc_pack_length(pack_version, max_calc_length);
  record_count = 0;
  /* 'max_blob_length' is the max length of all blobs of a record. */
  pack_blob_length = isam_file->s->base.blobs
                         ? calc_pack_length(pack_version, mrg->max_blob_length)
                         : 0;
  max_pack_length = pack_ref_length + pack_blob_length;

  DBUG_PRINT("fields", ("==="));
  mrg_reset(mrg);
  while ((error = mrg_rrnd(mrg, record)) != HA_ERR_END_OF_FILE) {
    ulong tot_blob_length = 0;
    if (!error) {
      if (flush_buffer((ulong)max_calc_length + (ulong)max_pack_length)) break;
      record_pos = (uchar *)file_buffer.pos;
      file_buffer.pos += max_pack_length;
      for (start_pos = record, count = huff_counts; count < end_count;
           count++) {
        end_pos = start_pos + (field_length = count->field_length);
        tree = count->tree;

        DBUG_PRINT("fields",
                   ("column: %3lu  type: %2u  pack: %2u  zero: %4u  "
                    "lbits: %2u  tree: %2u  length: %4u",
                    (ulong)(count - huff_counts + 1), count->field_type,
                    count->pack_type, count->max_zero_fill, count->length_bits,
                    count->tree->tree_number, count->field_length));

        /* Check if the column contains spaces only. */
        if (count->pack_type & PACK_TYPE_SPACE_FIELDS) {
          for (pos = start_pos; *pos == ' ' && pos < end_pos; pos++)
            ;
          if (pos == end_pos) {
            DBUG_PRINT("fields",
                       ("PACK_TYPE_SPACE_FIELDS spaces only, bits:  1"));
            DBUG_PRINT("fields", ("---"));
            write_bits(1, 1);
            start_pos = end_pos;
            continue;
          }
          DBUG_PRINT("fields",
                     ("PACK_TYPE_SPACE_FIELDS not only spaces, bits:  1"));
          write_bits(0, 1);
        }
        end_pos -= count->max_zero_fill;
        field_length -= count->max_zero_fill;

        switch (count->field_type) {
          case FIELD_SKIP_ZERO:
            if (!memcmp((uchar *)start_pos, zero_string, field_length)) {
              DBUG_PRINT("fields", ("FIELD_SKIP_ZERO zeroes only, bits:  1"));
              write_bits(1, 1);
              start_pos = end_pos;
              break;
            }
            DBUG_PRINT("fields", ("FIELD_SKIP_ZERO not only zeroes, bits:  1"));
            write_bits(0, 1);
            [[fallthrough]];
          case FIELD_NORMAL:
            DBUG_PRINT("fields", ("FIELD_NORMAL %lu bytes",
                                  (ulong)(end_pos - start_pos)));
            for (; start_pos < end_pos; start_pos++) {
              DBUG_PRINT(
                  "fields",
                  ("value: 0x%02x  code: 0x%s  bits: %2u  bin: %s",
                   (uchar)*start_pos, hexdigits(tree->code[(uchar)*start_pos]),
                   (uint)tree->code_len[(uchar)*start_pos],
                   bindigits(tree->code[(uchar)*start_pos],
                             (uint)tree->code_len[(uchar)*start_pos])));
              write_bits(tree->code[(uchar)*start_pos],
                         (uint)tree->code_len[(uchar)*start_pos]);
            }
            break;
          case FIELD_SKIP_ENDSPACE:
            for (pos = end_pos; pos > start_pos && pos[-1] == ' '; pos--)
              ;
            length = (ulong)(end_pos - pos);
            if (count->pack_type & PACK_TYPE_SELECTED) {
              if (length > count->min_space) {
                DBUG_PRINT(
                    "fields",
                    ("FIELD_SKIP_ENDSPACE more than min_space, bits:  1"));
                DBUG_PRINT("fields",
                           ("FIELD_SKIP_ENDSPACE skip %lu/%u bytes, bits: %2u",
                            length, field_length, count->length_bits));
                write_bits(1, 1);
                write_bits(length, count->length_bits);
              } else {
                DBUG_PRINT("fields",
                           ("FIELD_SKIP_ENDSPACE not more than min_space, "
                            "bits:  1"));
                write_bits(0, 1);
                pos = end_pos;
              }
            } else {
              DBUG_PRINT("fields",
                         ("FIELD_SKIP_ENDSPACE skip %lu/%u bytes, bits: %2u",
                          length, field_length, count->length_bits));
              write_bits(length, count->length_bits);
            }
            /* Encode all significant bytes. */
            DBUG_PRINT("fields", ("FIELD_SKIP_ENDSPACE %lu bytes",
                                  (ulong)(pos - start_pos)));
            for (; start_pos < pos; start_pos++) {
              DBUG_PRINT(
                  "fields",
                  ("value: 0x%02x  code: 0x%s  bits: %2u  bin: %s",
                   (uchar)*start_pos, hexdigits(tree->code[(uchar)*start_pos]),
                   (uint)tree->code_len[(uchar)*start_pos],
                   bindigits(tree->code[(uchar)*start_pos],
                             (uint)tree->code_len[(uchar)*start_pos])));
              write_bits(tree->code[(uchar)*start_pos],
                         (uint)tree->code_len[(uchar)*start_pos]);
            }
            start_pos = end_pos;
            break;
          case FIELD_SKIP_PRESPACE:
            for (pos = start_pos; pos < end_pos && pos[0] == ' '; pos++)
              ;
            length = (ulong)(pos - start_pos);
            if (count->pack_type & PACK_TYPE_SELECTED) {
              if (length > count->min_space) {
                DBUG_PRINT(
                    "fields",
                    ("FIELD_SKIP_PRESPACE more than min_space, bits:  1"));
                DBUG_PRINT("fields",
                           ("FIELD_SKIP_PRESPACE skip %lu/%u bytes, bits: %2u",
                            length, field_length, count->length_bits));
                write_bits(1, 1);
                write_bits(length, count->length_bits);
              } else {
                DBUG_PRINT("fields",
                           ("FIELD_SKIP_PRESPACE not more than min_space, "
                            "bits:  1"));
                pos = start_pos;
                write_bits(0, 1);
              }
            } else {
              DBUG_PRINT("fields",
                         ("FIELD_SKIP_PRESPACE skip %lu/%u bytes, bits: %2u",
                          length, field_length, count->length_bits));
              write_bits(length, count->length_bits);
            }
            /* Encode all significant bytes. */
            DBUG_PRINT("fields", ("FIELD_SKIP_PRESPACE %lu bytes",
                                  (ulong)(end_pos - start_pos)));
            for (start_pos = pos; start_pos < end_pos; start_pos++) {
              DBUG_PRINT(
                  "fields",
                  ("value: 0x%02x  code: 0x%s  bits: %2u  bin: %s",
                   (uchar)*start_pos, hexdigits(tree->code[(uchar)*start_pos]),
                   (uint)tree->code_len[(uchar)*start_pos],
                   bindigits(tree->code[(uchar)*start_pos],
                             (uint)tree->code_len[(uchar)*start_pos])));
              write_bits(tree->code[(uchar)*start_pos],
                         (uint)tree->code_len[(uchar)*start_pos]);
            }
            break;
          case FIELD_CONSTANT:
          case FIELD_ZERO:
          case FIELD_CHECK:
            DBUG_PRINT("fields", ("FIELD_CONSTANT/ZERO/CHECK"));
            start_pos = end_pos;
            break;
          case FIELD_INTERVALL:
            global_count = count;
            pos = (uchar *)tree_search(&count->int_tree, start_pos,
                                       count->int_tree.custom_arg);
            intervall = (uint)(pos - count->tree_buff) / field_length;
            DBUG_PRINT("fields", ("FIELD_INTERVALL"));
            DBUG_PRINT("fields", ("index: %4u code: 0x%s  bits: %2u", intervall,
                                  hexdigits(tree->code[intervall]),
                                  (uint)tree->code_len[intervall]));
            write_bits(tree->code[intervall], (uint)tree->code_len[intervall]);
            start_pos = end_pos;
            break;
          case FIELD_BLOB: {
            ulong blob_length = _mi_calc_blob_length(
                field_length - portable_sizeof_char_ptr, start_pos);
            /* Empty blobs are encoded with a single 1 bit. */
            if (!blob_length) {
              DBUG_PRINT("fields", ("FIELD_BLOB empty, bits:  1"));
              write_bits(1, 1);
            } else {
              uchar *blob, *blob_end;
              DBUG_PRINT("fields", ("FIELD_BLOB not empty, bits:  1"));
              write_bits(0, 1);
              /* Write the blob length. */
              DBUG_PRINT("fields", ("FIELD_BLOB %lu bytes, bits: %2u",
                                    blob_length, count->length_bits));
              write_bits(blob_length, count->length_bits);
              memcpy(&blob, end_pos - portable_sizeof_char_ptr, sizeof(char *));
              blob_end = blob + blob_length;
              /* Encode the blob bytes. */
              for (; blob < blob_end; blob++) {
                DBUG_PRINT(
                    "fields",
                    ("value: 0x%02x  code: 0x%s  bits: %2u  bin: %s",
                     (uchar)*blob, hexdigits(tree->code[(uchar)*blob]),
                     (uint)tree->code_len[(uchar)*blob],
                     bindigits(tree->code[(uchar)*start_pos],
                               (uint)tree->code_len[(uchar)*start_pos])));
                write_bits(tree->code[(uchar)*blob],
                           (uint)tree->code_len[(uchar)*blob]);
              }
              tot_blob_length += blob_length;
            }
            start_pos = end_pos;
            break;
          }
          case FIELD_VARCHAR: {
            uint var_pack_length =
                HA_VARCHAR_PACKLENGTH(count->field_length - 1);
            ulong col_length =
                (var_pack_length == 1 ? (uint) * (uchar *)start_pos
                                      : uint2korr(start_pos));
            /* Empty varchar are encoded with a single 1 bit. */
            if (!col_length) {
              DBUG_PRINT("fields", ("FIELD_VARCHAR empty, bits:  1"));
              write_bits(1, 1); /* Empty varchar */
            } else {
              uchar *end = start_pos + var_pack_length + col_length;
              DBUG_PRINT("fields", ("FIELD_VARCHAR not empty, bits:  1"));
              write_bits(0, 1);
              /* Write the varchar length. */
              DBUG_PRINT("fields", ("FIELD_VARCHAR %lu bytes, bits: %2u",
                                    col_length, count->length_bits));
              write_bits(col_length, count->length_bits);
              /* Encode the varchar bytes. */
              for (start_pos += var_pack_length; start_pos < end; start_pos++) {
                DBUG_PRINT(
                    "fields",
                    ("value: 0x%02x  code: 0x%s  bits: %2u  bin: %s",
                     (uchar)*start_pos,
                     hexdigits(tree->code[(uchar)*start_pos]),
                     (uint)tree->code_len[(uchar)*start_pos],
                     bindigits(tree->code[(uchar)*start_pos],
                               (uint)tree->code_len[(uchar)*start_pos])));
                write_bits(tree->code[(uchar)*start_pos],
                           (uint)tree->code_len[(uchar)*start_pos]);
              }
            }
            start_pos = end_pos;
            break;
          }
          case FIELD_LAST:
          case FIELD_enum_val_count:
            my_abort(); /* Impossible */
        }
        start_pos += count->max_zero_fill;
        DBUG_PRINT("fields", ("---"));
      }
      flush_bits();
      length = (ulong)((uchar *)file_buffer.pos - record_pos) - max_pack_length;
      pack_length = save_pack_length(pack_version, record_pos, length);
      if (pack_blob_length)
        pack_length += save_pack_length(pack_version, record_pos + pack_length,
                                        tot_blob_length);
      DBUG_PRINT("fields",
                 ("record: %lu  length: %lu  blob-length: %lu  "
                  "length-bytes: %lu",
                  (ulong)record_count, length, tot_blob_length, pack_length));
      DBUG_PRINT("fields", ("==="));

      /* Correct file buffer if the header was smaller */
      if (pack_length != max_pack_length) {
        memmove(record_pos + pack_length, record_pos + max_pack_length, length);
        file_buffer.pos -= (max_pack_length - pack_length);
      }
      if (length < (ulong)min_record_length) min_record_length = (uint)length;
      if (length > (ulong)max_record_length) max_record_length = (uint)length;
      record_count++;
      if (write_loop && record_count % WRITE_COUNT == 0) {
        printf("%lu\r", (ulong)record_count);
        (void)fflush(stdout);
      }
    } else if (error != HA_ERR_RECORD_DELETED)
      break;
  }
  if (error == HA_ERR_END_OF_FILE)
    error = 0;
  else {
    (void)fprintf(stderr, "%s: Got error %d reading records\n", my_progname,
                  error);
  }
  if (verbose >= 2)
    printf("wrote %s records.\n", llstr((longlong)record_count, llbuf));

  mrg->ref_length = max_pack_length;
  mrg->min_pack_length = max_record_length ? min_record_length : 0;
  mrg->max_pack_length = max_record_length;
  return error || error_on_write || flush_buffer(~(ulong)0);
}

static char *make_new_name(char *new_name, char *old_name) {
  return fn_format(new_name, old_name, "", DATA_TMP_EXT, 2 + 4);
}

static char *make_old_name(char *new_name, char *old_name) {
  return fn_format(new_name, old_name, "", OLD_EXT, 2 + 4);
}

/* rutines for bit writing buffer */

static void init_file_buffer(File file, bool read_buffer) {
  file_buffer.file = file;
  file_buffer.buffer = (uchar *)my_malloc(
      PSI_NOT_INSTRUMENTED, ALIGN_SIZE(RECORD_CACHE_SIZE), MYF(MY_WME));
  file_buffer.end = file_buffer.buffer + ALIGN_SIZE(RECORD_CACHE_SIZE) - 8;
  file_buffer.pos_in_file = 0;
  error_on_write = 0;
  if (read_buffer) {
    file_buffer.pos = file_buffer.end;
    file_buffer.bits = 0;
  } else {
    file_buffer.pos = file_buffer.buffer;
    file_buffer.bits = BITS_SAVED;
  }
  file_buffer.bitbucket = 0;
}

static int flush_buffer(ulong neaded_length) {
  ulong length;

  /*
    file_buffer.end is 8 bytes lower than the real end of the buffer.
    This is done so that the end-of-buffer condition does not need to be
    checked for every byte (see write_bits()). Consequently,
    file_buffer.pos can become greater than file_buffer.end. The
    algorithms in the other functions ensure that there will never be
    more than 8 bytes written to the buffer without an end-of-buffer
    check. So the buffer cannot be overrun. But we need to check for the
    near-to-buffer-end condition to avoid a negative result, which is
    casted to unsigned and thus becomes giant.
  */
  if ((file_buffer.pos < file_buffer.end) &&
      ((ulong)(file_buffer.end - file_buffer.pos) > neaded_length))
    return 0;
  length = (ulong)(file_buffer.pos - file_buffer.buffer);
  file_buffer.pos = file_buffer.buffer;
  file_buffer.pos_in_file += length;
  if (test_only) return 0;
  if (error_on_write ||
      my_write(file_buffer.file, (const uchar *)file_buffer.buffer, length,
               MYF(MY_WME | MY_NABP | MY_WAIT_IF_FULL))) {
    error_on_write = 1;
    return 1;
  }

  if (neaded_length != ~(ulong)0 &&
      (ulong)(file_buffer.end - file_buffer.buffer) < neaded_length) {
    char *tmp;
    neaded_length += 256; /* some margin */
    tmp = (char *)my_realloc(PSI_NOT_INSTRUMENTED, (char *)file_buffer.buffer,
                             neaded_length, MYF(MY_WME));
    if (!tmp) return 1;
    file_buffer.pos =
        ((uchar *)tmp + (ulong)(file_buffer.pos - file_buffer.buffer));
    file_buffer.buffer = (uchar *)tmp;
    file_buffer.end = (uchar *)(tmp + neaded_length - 8);
  }
  return 0;
}

static void end_file_buffer(void) { my_free(file_buffer.buffer); }

/* output `bits` low bits of `value' */

static void write_bits(ulonglong value, uint bits) {
  assert(((bits < 8 * sizeof(value)) && !(value >> bits)) ||
         (bits == 8 * sizeof(value)));

  if ((file_buffer.bits -= (int)bits) >= 0) {
    file_buffer.bitbucket |= value << file_buffer.bits;
  } else {
    ulonglong bit_buffer;
    bits = (uint)-file_buffer.bits;
    bit_buffer = (file_buffer.bitbucket |
                  ((bits != 8 * sizeof(value)) ? (value >> bits) : 0));
#if BITS_SAVED == 64
    *file_buffer.pos++ = (uchar)(bit_buffer >> 56);
    *file_buffer.pos++ = (uchar)(bit_buffer >> 48);
    *file_buffer.pos++ = (uchar)(bit_buffer >> 40);
    *file_buffer.pos++ = (uchar)(bit_buffer >> 32);
#endif
    *file_buffer.pos++ = (uchar)(bit_buffer >> 24);
    *file_buffer.pos++ = (uchar)(bit_buffer >> 16);
    *file_buffer.pos++ = (uchar)(bit_buffer >> 8);
    *file_buffer.pos++ = (uchar)(bit_buffer);

    if (bits != 8 * sizeof(value)) value &= (((ulonglong)1) << bits) - 1;
    if (file_buffer.pos >= file_buffer.end) (void)flush_buffer(~(ulong)0);
    file_buffer.bits = (int)(BITS_SAVED - bits);
    file_buffer.bitbucket = value << (BITS_SAVED - bits);
  }
  return;
}

/* Flush bits in bit_buffer to buffer */

static void flush_bits(void) {
  int bits;
  ulonglong bit_buffer;

  bits = file_buffer.bits & ~7;
  // >> 64 is undefined (platform specific)
  bit_buffer = bits >= 64 ? 0 : file_buffer.bitbucket >> bits;
  bits = BITS_SAVED - bits;
  while (bits > 0) {
    bits -= 8;
    *file_buffer.pos++ = (uchar)(bit_buffer >> bits);
  }
  if (file_buffer.pos >= file_buffer.end) (void)flush_buffer(~(ulong)0);
  file_buffer.bits = BITS_SAVED;
  file_buffer.bitbucket = 0;
}

/****************************************************************************
** functions to handle the joined files
****************************************************************************/

static int save_state(MI_INFO *isam_file, PACK_MRG_INFO *mrg,
                      my_off_t new_length, ha_checksum crc) {
  MYISAM_SHARE *share = isam_file->s;
  uint options = mi_uint2korr(share->state.header.options);
  uint key;
  DBUG_TRACE;

  options |= HA_OPTION_COMPRESS_RECORD | HA_OPTION_READ_ONLY_DATA;
  mi_int2store(share->state.header.options, options);

  share->state.state.data_file_length = new_length;
  share->state.state.del = 0;
  share->state.state.empty = 0;
  share->state.dellink = HA_OFFSET_ERROR;
  share->state.split = (ha_rows)mrg->records;
  share->state.version = (ulong)time((time_t *)nullptr);
  if (!mi_is_all_keys_active(share->state.key_map, share->base.keys)) {
    /*
      Some indexes are disabled, cannot use current key_file_length value
      as an estimate of upper bound of index file size. Use packed data file
      size instead.
    */
    share->state.state.key_file_length = new_length;
  }
  /*
    If there are no disabled indexes, keep key_file_length value from
    original file so "myisamchk -rq" can use this value (this is necessary
    because index size cannot be easily calculated for fulltext keys)
  */
  mi_clear_all_keys_active(share->state.key_map);
  for (key = 0; key < share->base.keys; key++)
    share->state.key_root[key] = HA_OFFSET_ERROR;
  for (key = 0; key < share->state.header.max_block_size_index; key++)
    share->state.key_del[key] = HA_OFFSET_ERROR;
  isam_file->state->checksum = crc; /* Save crc here */
  share->changed = true;            /* Force write of header */
  share->state.open_count = 0;
  share->global_changed = false;
  (void)my_chsize(share->kfile, share->base.keystart, 0, MYF(0));
  if (share->base.keys) isamchk_neaded = 1;
  return mi_state_info_write(share->kfile, &share->state, 1 + 2);
}

static int save_state_mrg(File file, PACK_MRG_INFO *mrg, my_off_t new_length,
                          ha_checksum crc) {
  MI_STATE_INFO state;
  MI_INFO *isam_file = mrg->file[0];
  uint options;
  DBUG_TRACE;

  state = isam_file->s->state;
  options = (mi_uint2korr(state.header.options) | HA_OPTION_COMPRESS_RECORD |
             HA_OPTION_READ_ONLY_DATA);
  mi_int2store(state.header.options, options);
  state.state.data_file_length = new_length;
  state.state.del = 0;
  state.state.empty = 0;
  state.state.records = state.split = (ha_rows)mrg->records;
  /* See comment above in save_state about key_file_length handling. */
  if (mrg->src_file_has_indexes_disabled) {
    isam_file->s->state.state.key_file_length =
        std::max(isam_file->s->state.state.key_file_length, new_length);
  }
  state.dellink = HA_OFFSET_ERROR;
  state.version = (ulong)time((time_t *)nullptr);
  mi_clear_all_keys_active(state.key_map);
  state.state.checksum = crc;
  if (isam_file->s->base.keys) isamchk_neaded = 1;
  state.changed = STATE_CHANGED | STATE_NOT_ANALYZED; /* Force check of table */
  return mi_state_info_write(file, &state, 1 + 2);
}

/* reset for mrg_rrnd */

static void mrg_reset(PACK_MRG_INFO *mrg) {
  if (mrg->current) {
    mrg->current = nullptr;
  }
}

static int mrg_rrnd(PACK_MRG_INFO *info, uchar *buf) {
  int error;
  MI_INFO *isam_info;
  my_off_t filepos;

  if (!info->current) {
    isam_info = *(info->current = info->file);
    info->end = info->current + info->count;
    mi_reset(isam_info);
    filepos = isam_info->s->pack.header_length;
  } else {
    isam_info = *info->current;
    filepos = isam_info->nextpos;
  }

  for (;;) {
    isam_info->update &= HA_STATE_CHANGED;
    if (!(error = (*isam_info->s->read_rnd)(isam_info, (uchar *)buf, filepos,
                                            true)) ||
        error != HA_ERR_END_OF_FILE)
      return (error);
    if (info->current + 1 == info->end) return (HA_ERR_END_OF_FILE);
    info->current++;
    isam_info = *info->current;
    filepos = isam_info->s->pack.header_length;
    mi_reset(isam_info);
  }
}

static int mrg_close(PACK_MRG_INFO *mrg) {
  uint i;
  int error = 0;
  for (i = 0; i < mrg->count; i++) error |= mi_close(mrg->file[i]);
  if (mrg->free_file) my_free(mrg->file);
  return error;
}

#if !defined(NDEBUG)
/*
  Fake the counts to get big Huffman codes.

  SYNOPSIS
    fakebigcodes()
    huff_counts                 A pointer to the counts array.
    end_count                   A pointer past the counts array.

  DESCRIPTION

    Huffman coding works by removing the two least frequent values from
    the list of values and add a new value with the sum of their
    incidences in a loop until only one value is left. Every time a
    value is reused for a new value, it gets one more bit for its
    encoding. Hence, the least frequent values get the longest codes.

    To get a maximum code length for a value, two of the values must
    have an incidence of 1. As their sum is 2, the next infrequent value
    must have at least an incidence of 2, then 4, 8, 16 and so on. This
    means that one needs 2**n bytes (values) for a code length of n
    bits. However, using more distinct values forces the use of longer
    codes, or reaching the code length with less total bytes (values).

    To get 64(32)-bit codes, I sort the counts by decreasing incidence.
    I assign counts of 1 to the two most frequent values, a count of 2
    for the next one, then 4, 8, and so on until 2**64-1(2**30-1). All
    the remaining values get 1. That way every possible byte has an
    assigned code, though not all codes are used if not all byte values
    are present in the column.

    This strategy would work with distinct column values too, but
    requires that at least 64(32) values are present. To make things
    easier here, I cancel all distinct column values and force byte
    compression for all columns.

  RETURN
    void
*/

static void fakebigcodes(HUFF_COUNTS *huff_counts, HUFF_COUNTS *end_count) {
  HUFF_COUNTS *count;
  my_off_t *cur_count_p;
  my_off_t *end_count_p;
  my_off_t **cur_sort_p;
  my_off_t **end_sort_p;
  my_off_t *sort_counts[256];
  my_off_t total;
  DBUG_TRACE;

  for (count = huff_counts; count < end_count; count++) {
    /*
      Remove distinct column values.
    */
    if (huff_counts->tree_buff) {
      my_free(huff_counts->tree_buff);
      delete_tree(&huff_counts->int_tree);
      huff_counts->tree_buff = nullptr;
      DBUG_PRINT("fakebigcodes", ("freed distinct column values"));
    }

    /*
      Sort counts by decreasing incidence.
    */
    cur_count_p = count->counts;
    end_count_p = cur_count_p + 256;
    cur_sort_p = sort_counts;
    while (cur_count_p < end_count_p) *(cur_sort_p++) = cur_count_p++;
    std::sort(sort_counts, sort_counts + 256,
              [](const my_off_t *a, const my_off_t *b) { return *b > *a; });

    /*
      Assign faked counts.
    */
    cur_sort_p = sort_counts;
#if SIZEOF_LONG_LONG > 4
    end_sort_p = sort_counts + 8 * sizeof(ulonglong) - 1;
#else
    end_sort_p = sort_counts + 8 * sizeof(ulonglong) - 2;
#endif
    /* Most frequent value gets a faked count of 1. */
    **(cur_sort_p++) = 1;
    total = 1;
    while (cur_sort_p < end_sort_p) {
      **(cur_sort_p++) = total;
      total <<= 1;
    }
    /* Set the last value. */
    **(cur_sort_p++) = --total;
    /*
      Set the remaining counts.
    */
    end_sort_p = sort_counts + 256;
    while (cur_sort_p < end_sort_p) **(cur_sort_p++) = 1;
  }
}

#endif

#include "storage/myisam/mi_extrafunc.h"
