/*****************************************************************************

Copyright (c) 2010, 2018, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file row/row0ftsort.cc
 Create Full Text Index with (parallel) merge sort

 Created 10/13/2010 Jimmy Yang
 *******************************************************/

#include <sys/types.h>

#include "btr0bulk.h"
#include "btr0cur.h"
#include "dict0dd.h"
#include "dict0dict.h"
#include "fts0plugin.h"
#include "ha_prototypes.h"
#include "lob0lob.h"
#include "os0thread-create.h"
#include "pars0pars.h"
#include "row0ftsort.h"
#include "row0merge.h"
#include "row0row.h"

#include "my_dbug.h"

/** Read the next record to buffer N.
@param N index into array of merge info structure */
#define ROW_MERGE_READ_GET_NEXT(N)                                             \
  do {                                                                         \
    b[N] = row_merge_read_rec(block[N], buf[N], b[N], index, fd[N], &foffs[N], \
                              &mrec[N], offsets[N]);                           \
    if (UNIV_UNLIKELY(!b[N])) {                                                \
      if (mrec[N]) {                                                           \
        goto exit;                                                             \
      }                                                                        \
    }                                                                          \
  } while (0)

/** Parallel sort degree */
ulong fts_sort_pll_degree = 2;

/** Create a temporary "fts sort index" used to merge sort the
 tokenized doc string. The index has three "fields":

 1) Tokenized word,
 2) Doc ID (depend on number of records to sort, it can be a 4 bytes or 8 bytes
 integer value)
 3) Word's position in original doc.

 @return dict_index_t structure for the fts sort index */
dict_index_t *row_merge_create_fts_sort_index(
    dict_index_t *index,       /*!< in: Original FTS index
                               based on which this sort index
                               is created */
    const dict_table_t *table, /*!< in: table that FTS index
                               is being created on */
    ibool *opt_doc_id_size)
/*!< out: whether to use 4 bytes
instead of 8 bytes integer to
store Doc ID during sort */
{
  dict_index_t *new_index;
  dict_field_t *field;
  dict_field_t *idx_field;
  CHARSET_INFO *charset;

  // FIXME: This name shouldn't be hard coded here.
  new_index = dict_mem_index_create(index->table->name.m_name, "tmp_fts_idx", 0,
                                    DICT_FTS, 3);

  new_index->id = index->id;
  new_index->table = (dict_table_t *)table;
  new_index->n_uniq = FTS_NUM_FIELDS_SORT;
  new_index->n_def = FTS_NUM_FIELDS_SORT;
  new_index->cached = TRUE;
  new_index->parser = index->parser;
  new_index->is_ngram = index->is_ngram;

  idx_field = index->get_field(0);
  charset = fts_index_get_charset(index);

  /* The first field is on the Tokenized Word */
  field = new_index->get_field(0);
  field->name = NULL;
  field->prefix_len = 0;
  field->is_ascending = true;
  field->col = static_cast<dict_col_t *>(
      mem_heap_alloc(new_index->heap, sizeof(dict_col_t)));
  field->col->len = FTS_MAX_WORD_LEN;
  field->col->mtype =
      (charset == &my_charset_latin1) ? DATA_VARCHAR : DATA_VARMYSQL;

  field->col->prtype = idx_field->col->prtype | DATA_NOT_NULL;
  field->col->mbminmaxlen = idx_field->col->mbminmaxlen;
  field->fixed_len = 0;

  /* Doc ID */
  field = new_index->get_field(1);
  field->name = NULL;
  field->prefix_len = 0;
  field->is_ascending = true;
  field->col = static_cast<dict_col_t *>(
      mem_heap_alloc(new_index->heap, sizeof(dict_col_t)));
  field->col->mtype = DATA_INT;
  *opt_doc_id_size = FALSE;

  /* Check whether we can use 4 bytes instead of 8 bytes integer
  field to hold the Doc ID, thus reduce the overall sort size */
  if (DICT_TF2_FLAG_IS_SET(table, DICT_TF2_FTS_ADD_DOC_ID)) {
    /* If Doc ID column is being added by this create
    index, then just check the number of rows in the table */
    if (dict_table_get_n_rows(table) < MAX_DOC_ID_OPT_VAL) {
      *opt_doc_id_size = TRUE;
    }
  } else {
    doc_id_t max_doc_id;

    /* If the Doc ID column is supplied by user, then
    check the maximum Doc ID in the table */
    max_doc_id = fts_get_max_doc_id((dict_table_t *)table);

    if (max_doc_id && max_doc_id < MAX_DOC_ID_OPT_VAL) {
      *opt_doc_id_size = TRUE;
    }
  }

  if (*opt_doc_id_size) {
    field->col->len = sizeof(ib_uint32_t);
    field->fixed_len = sizeof(ib_uint32_t);
  } else {
    field->col->len = FTS_DOC_ID_LEN;
    field->fixed_len = FTS_DOC_ID_LEN;
  }

  field->col->prtype = DATA_NOT_NULL | DATA_BINARY_TYPE;

  field->col->mbminmaxlen = 0;

  /* The third field is on the word's position in the original doc */
  field = new_index->get_field(2);
  field->name = NULL;
  field->prefix_len = 0;
  field->is_ascending = true;
  field->col = static_cast<dict_col_t *>(
      mem_heap_alloc(new_index->heap, sizeof(dict_col_t)));
  field->col->mtype = DATA_INT;
  field->col->len = 4;
  field->fixed_len = 4;
  field->col->prtype = DATA_NOT_NULL;
  field->col->mbminmaxlen = 0;

  return (new_index);
}
/** Initialize FTS parallel sort structures.
 @return true if all successful */
ibool row_fts_psort_info_init(
    trx_t *trx,           /*!< in: transaction */
    row_merge_dup_t *dup, /*!< in,own: descriptor of
                          FTS index being created */
    const dict_table_t *old_table,
    const dict_table_t *new_table, /*!< in: table on which indexes are
                                 created */
    ibool opt_doc_id_size,
    /*!< in: whether to use 4 bytes
    instead of 8 bytes integer to
    store Doc ID during sort */
    fts_psort_t **psort, /*!< out: parallel sort info to be
                         instantiated */
    fts_psort_t **merge) /*!< out: parallel merge info
                         to be instantiated */
{
  ulint i;
  ulint j;
  fts_psort_common_t *common_info = NULL;
  fts_psort_t *psort_info = NULL;
  fts_psort_t *merge_info = NULL;
  ulint block_size;
  ibool ret = TRUE;

  block_size = 3 * srv_sort_buf_size;

  *psort = psort_info = static_cast<fts_psort_t *>(
      ut_zalloc_nokey(fts_sort_pll_degree * sizeof *psort_info));

  if (!psort_info) {
    ut_free(dup);
    return (FALSE);
  }

  /* Common Info for all sort threads */
  common_info =
      static_cast<fts_psort_common_t *>(ut_malloc_nokey(sizeof *common_info));

  if (!common_info) {
    ut_free(dup);
    ut_free(psort_info);
    return (FALSE);
  }

  common_info->dup = dup;
  common_info->old_table = (dict_table_t *)old_table;
  common_info->new_table = (dict_table_t *)new_table;
  common_info->trx = trx;
  common_info->all_info = psort_info;
  common_info->sort_event = os_event_create(0);
  common_info->merge_event = os_event_create(0);
  common_info->opt_doc_id_size = opt_doc_id_size;

  ut_ad(trx->mysql_thd != NULL);
  const char *path = thd_innodb_tmpdir(trx->mysql_thd);
  /* There will be FTS_NUM_AUX_INDEX number of "sort buckets" for
  each parallel sort thread. Each "sort bucket" holds records for
  a particular "FTS index partition" */
  for (j = 0; j < fts_sort_pll_degree; j++) {
    UT_LIST_INIT(psort_info[j].fts_doc_list, &fts_doc_item_t::doc_list);

    for (i = 0; i < FTS_NUM_AUX_INDEX; i++) {
      psort_info[j].merge_file[i] =
          static_cast<merge_file_t *>(ut_zalloc_nokey(sizeof(merge_file_t)));

      if (!psort_info[j].merge_file[i]) {
        ret = FALSE;
        goto func_exit;
      }

      psort_info[j].merge_buf[i] = row_merge_buf_create(dup->index);

      if (row_merge_file_create(psort_info[j].merge_file[i], path) < 0) {
        goto func_exit;
      }

      /* Need to align memory for O_DIRECT write */
      psort_info[j].block_alloc[i] =
          static_cast<row_merge_block_t *>(ut_malloc_nokey(block_size + 1024));

      psort_info[j].merge_block[i] = static_cast<row_merge_block_t *>(
          ut_align(psort_info[j].block_alloc[i], 1024));

      if (!psort_info[j].merge_block[i]) {
        ret = FALSE;
        goto func_exit;
      }
    }

    psort_info[j].child_status = 0;
    psort_info[j].state = 0;
    psort_info[j].psort_common = common_info;
    psort_info[j].error = DB_SUCCESS;
    psort_info[j].memory_used = 0;
    mutex_create(LATCH_ID_FTS_PLL_TOKENIZE, &psort_info[j].mutex);
  }

  /* Initialize merge_info structures parallel merge and insert
  into auxiliary FTS tables (FTS_INDEX_TABLE) */
  *merge = merge_info = static_cast<fts_psort_t *>(
      ut_malloc_nokey(FTS_NUM_AUX_INDEX * sizeof *merge_info));

  for (j = 0; j < FTS_NUM_AUX_INDEX; j++) {
    merge_info[j].child_status = 0;
    merge_info[j].state = 0;
    merge_info[j].psort_common = common_info;
  }

func_exit:
  if (!ret) {
    row_fts_psort_info_destroy(psort_info, merge_info);
  }

  return (ret);
}
/** Clean up and deallocate FTS parallel sort structures, and close the
 merge sort files  */
void row_fts_psort_info_destroy(
    fts_psort_t *psort_info, /*!< parallel sort info */
    fts_psort_t *merge_info) /*!< parallel merge info */
{
  ulint i;
  ulint j;

  if (psort_info) {
    for (j = 0; j < fts_sort_pll_degree; j++) {
      for (i = 0; i < FTS_NUM_AUX_INDEX; i++) {
        if (psort_info[j].merge_file[i]) {
          row_merge_file_destroy(psort_info[j].merge_file[i]);
        }

        ut_free(psort_info[j].block_alloc[i]);
        ut_free(psort_info[j].merge_file[i]);
      }

      mutex_free(&psort_info[j].mutex);
    }

    os_event_destroy(merge_info[0].psort_common->sort_event);
    os_event_destroy(merge_info[0].psort_common->merge_event);
    ut_free(merge_info[0].psort_common->dup);
    ut_free(merge_info[0].psort_common);
    ut_free(psort_info);
  }

  ut_free(merge_info);
}
/** Free up merge buffers when merge sort is done */
void row_fts_free_pll_merge_buf(
    fts_psort_t *psort_info) /*!< in: parallel sort info */
{
  ulint j;
  ulint i;

  if (!psort_info) {
    return;
  }

  for (j = 0; j < fts_sort_pll_degree; j++) {
    for (i = 0; i < FTS_NUM_AUX_INDEX; i++) {
      row_merge_buf_free(psort_info[j].merge_buf[i]);
    }
  }

  return;
}

/** FTS plugin parser 'myql_add_word' callback function for row merge.
 Refer to 'MYSQL_FTPARSER_PARAM' for more detail.
 @return always returns 0 */
static int row_merge_fts_doc_add_word_for_parser(
    MYSQL_FTPARSER_PARAM *param,               /* in: parser paramter */
    char *word,                                /* in: token word */
    int word_len,                              /* in: word len */
    MYSQL_FTPARSER_BOOLEAN_INFO *boolean_info) /* in: boolean info */
{
  fts_string_t str;
  fts_tokenize_ctx_t *t_ctx;
  row_fts_token_t *fts_token;
  byte *ptr;

  ut_ad(param);
  ut_ad(param->mysql_ftparam);
  ut_ad(word);
  ut_ad(boolean_info);

  t_ctx = static_cast<fts_tokenize_ctx_t *>(param->mysql_ftparam);
  ut_ad(t_ctx);

  str.f_str = reinterpret_cast<byte *>(word);
  str.f_len = word_len;
  str.f_n_char = fts_get_token_size((CHARSET_INFO *)param->cs, word, word_len);

  ut_ad(boolean_info->position >= 0);

  ptr = static_cast<byte *>(ut_malloc_nokey(sizeof(row_fts_token_t) +
                                            sizeof(fts_string_t) + str.f_len));
  fts_token = reinterpret_cast<row_fts_token_t *>(ptr);
  fts_token->text =
      reinterpret_cast<fts_string_t *>(ptr + sizeof(row_fts_token_t));
  fts_token->text->f_str =
      static_cast<byte *>(ptr + sizeof(row_fts_token_t) + sizeof(fts_string_t));

  fts_token->text->f_len = str.f_len;
  fts_token->text->f_n_char = str.f_n_char;
  memcpy(fts_token->text->f_str, str.f_str, str.f_len);
  fts_token->position = boolean_info->position;

  /* Add token to list */
  UT_LIST_ADD_LAST(t_ctx->fts_token_list, fts_token);

  return (0);
}

/** Tokenize by fts plugin parser */
static void row_merge_fts_doc_tokenize_by_parser(
    fts_doc_t *doc,            /* in: doc to tokenize */
    st_mysql_ftparser *parser, /* in: plugin parser instance */
    fts_tokenize_ctx_t *t_ctx) /* in/out: tokenize ctx instance */
{
  MYSQL_FTPARSER_PARAM param;

  ut_a(parser);

  /* Set paramters for param */
  param.mysql_parse = fts_tokenize_document_internal;
  param.mysql_add_word = row_merge_fts_doc_add_word_for_parser;
  param.mysql_ftparam = t_ctx;
  param.cs = doc->charset;
  param.doc = reinterpret_cast<char *>(doc->text.f_str);
  param.length = static_cast<int>(doc->text.f_len);
  param.mode = MYSQL_FTPARSER_SIMPLE_MODE;

  PARSER_INIT(parser, &param);
  /* We assume parse returns successfully here. */
  parser->parse(&param);
  PARSER_DEINIT(parser, &param);
}

/** Tokenize incoming text data and add to the sort buffer.
 @return true if the record passed, false if out of space */
static ibool row_merge_fts_doc_tokenize(
    row_merge_buf_t **sort_buf, /*!< in/out: sort buffer */
    doc_id_t doc_id,            /*!< in: Doc ID */
    fts_doc_t *doc,             /*!< in: Doc to be tokenized */
    dtype_t *word_dtype,        /*!< in: data structure for
                                word col */
    merge_file_t **merge_file,  /*!< in/out: merge file */
    ibool opt_doc_id_size,      /*!< in: whether to use 4 bytes
                                instead of 8 bytes integer to
                                store Doc ID during sort*/
    fts_tokenize_ctx_t *t_ctx)  /*!< in/out: tokenize context */
{
  ulint inc = 0;
  fts_string_t str;
  ulint len;
  row_merge_buf_t *buf;
  dfield_t *field;
  fts_string_t t_str;
  ibool buf_full = FALSE;
  byte str_buf[FTS_MAX_WORD_LEN + 1];
  ulint data_size[FTS_NUM_AUX_INDEX];
  ulint n_tuple[FTS_NUM_AUX_INDEX];
  st_mysql_ftparser *parser;
  bool is_ngram;

  t_str.f_n_char = 0;
  t_ctx->buf_used = 0;

  memset(n_tuple, 0, FTS_NUM_AUX_INDEX * sizeof(ulint));
  memset(data_size, 0, FTS_NUM_AUX_INDEX * sizeof(ulint));

  parser = sort_buf[0]->index->parser;
  is_ngram = sort_buf[0]->index->is_ngram;

  /* Tokenize the data and add each word string, its corresponding
  doc id and position to sort buffer */
  while (t_ctx->processed_len < doc->text.f_len) {
    ulint idx = 0;
    ib_uint32_t position;
    ulint cur_len = 0;
    doc_id_t write_doc_id;
    row_fts_token_t *fts_token = NULL;

    if (parser != NULL) {
      if (t_ctx->processed_len == 0) {
        UT_LIST_INIT(t_ctx->fts_token_list, &row_fts_token_t::token_list);

        /* Parse the whole doc and cache tokens */
        row_merge_fts_doc_tokenize_by_parser(doc, parser, t_ctx);

        /* Just indictate we have parsed all the word */
        t_ctx->processed_len += 1;
      }

      /* Then get a token */
      fts_token = UT_LIST_GET_FIRST(t_ctx->fts_token_list);
      if (fts_token) {
        str.f_len = fts_token->text->f_len;
        str.f_n_char = fts_token->text->f_n_char;
        str.f_str = fts_token->text->f_str;
      } else {
        ut_ad(UT_LIST_GET_LEN(t_ctx->fts_token_list) == 0);
        /* Reach the end of the list */
        t_ctx->processed_len = doc->text.f_len;
        break;
      }
    } else {
      inc = innobase_mysql_fts_get_token(
          doc->charset, doc->text.f_str + t_ctx->processed_len,
          doc->text.f_str + doc->text.f_len, &str);

      ut_a(inc > 0);
    }

    /* Ignore string whose character number is less than
    "fts_min_token_size" or more than "fts_max_token_size" */
    if (!fts_check_token(&str, NULL, is_ngram, NULL)) {
      if (parser != NULL) {
        UT_LIST_REMOVE(t_ctx->fts_token_list, fts_token);
        ut_free(fts_token);
      } else {
        t_ctx->processed_len += inc;
      }

      continue;
    }

    t_str.f_len =
        innobase_fts_casedn_str(doc->charset, (char *)str.f_str, str.f_len,
                                (char *)&str_buf, FTS_MAX_WORD_LEN + 1);

    t_str.f_str = (byte *)&str_buf;

    /* if "cached_stopword" is defined, ignore words in the
    stopword list */
    if (!fts_check_token(&str, t_ctx->cached_stopword, is_ngram,
                         doc->charset)) {
      if (parser != NULL) {
        UT_LIST_REMOVE(t_ctx->fts_token_list, fts_token);
        ut_free(fts_token);
      } else {
        t_ctx->processed_len += inc;
      }

      continue;
    }

    /* There are FTS_NUM_AUX_INDEX auxiliary tables, find
    out which sort buffer to put this word record in */
    t_ctx->buf_used = fts_select_index(doc->charset, t_str.f_str, t_str.f_len);

    buf = sort_buf[t_ctx->buf_used];

    ut_a(t_ctx->buf_used < FTS_NUM_AUX_INDEX);
    idx = t_ctx->buf_used;

    mtuple_t *mtuple = &buf->tuples[buf->n_tuples + n_tuple[idx]];

    field = mtuple->fields = static_cast<dfield_t *>(
        mem_heap_alloc(buf->heap, FTS_NUM_FIELDS_SORT * sizeof *field));

    /* The first field is the tokenized word */
    dfield_set_data(field, t_str.f_str, t_str.f_len);
    len = dfield_get_len(field);

    field->type.mtype = word_dtype->mtype;
    field->type.prtype = word_dtype->prtype | DATA_NOT_NULL;

    /* Variable length field, set to max size. */
    field->type.len = FTS_MAX_WORD_LEN;
    field->type.mbminmaxlen = word_dtype->mbminmaxlen;

    cur_len += len;
    dfield_dup(field, buf->heap);
    field++;

    /* The second field is the Doc ID */

    ib_uint32_t doc_id_32_bit;

    if (!opt_doc_id_size) {
      fts_write_doc_id((byte *)&write_doc_id, doc_id);

      dfield_set_data(field, &write_doc_id, sizeof(write_doc_id));
    } else {
      mach_write_to_4((byte *)&doc_id_32_bit, (ib_uint32_t)doc_id);

      dfield_set_data(field, &doc_id_32_bit, sizeof(doc_id_32_bit));
    }

    len = field->len;
    ut_ad(len == FTS_DOC_ID_LEN || len == sizeof(ib_uint32_t));

    field->type.mtype = DATA_INT;
    field->type.prtype = DATA_NOT_NULL | DATA_BINARY_TYPE;
    field->type.len = len;
    field->type.mbminmaxlen = 0;

    cur_len += len;
    dfield_dup(field, buf->heap);

    ++field;

    /* The third field is the position */
    if (parser != NULL) {
      mach_write_to_4(reinterpret_cast<byte *>(&position),
                      (fts_token->position + t_ctx->init_pos));
    } else {
      mach_write_to_4(
          reinterpret_cast<byte *>(&position),
          (t_ctx->processed_len + inc - str.f_len + t_ctx->init_pos));
    }

    dfield_set_data(field, &position, sizeof(position));
    len = dfield_get_len(field);
    ut_ad(len == sizeof(ib_uint32_t));

    field->type.mtype = DATA_INT;
    field->type.prtype = DATA_NOT_NULL;
    field->type.len = len;
    field->type.mbminmaxlen = 0;
    cur_len += len;
    dfield_dup(field, buf->heap);

    /* One variable length column, word with its lenght less than
    fts_max_token_size, add one extra size and one extra byte.

    Since the max length for FTS token now is larger than 255,
    so we will need to signify length byte itself, so only 1 to 128
    bytes can be used for 1 bytes, larger than that 2 bytes. */
    if (t_str.f_len < 128) {
      /* Extra size is one byte. */
      cur_len += 2;
    } else {
      /* Extra size is two bytes. */
      cur_len += 3;
    }

    /* Reserve one byte for the end marker of row_merge_block_t. */
    if (buf->total_size + data_size[idx] + cur_len >= srv_sort_buf_size - 1) {
      buf_full = TRUE;
      break;
    }

    /* Increment the number of tuples */
    n_tuple[idx]++;
    if (parser != NULL) {
      UT_LIST_REMOVE(t_ctx->fts_token_list, fts_token);
      ut_free(fts_token);
    } else {
      t_ctx->processed_len += inc;
    }
    data_size[idx] += cur_len;
  }

  /* Update the data length and the number of new word tuples
  added in this round of tokenization */
  for (ulint i = 0; i < FTS_NUM_AUX_INDEX; i++) {
    /* The computation of total_size below assumes that no
    delete-mark flags will be stored and that all fields
    are NOT NULL and fixed-length. */

    sort_buf[i]->total_size += data_size[i];

    sort_buf[i]->n_tuples += n_tuple[i];

    merge_file[i]->n_rec += n_tuple[i];
    t_ctx->rows_added[i] += n_tuple[i];
  }

  if (!buf_full) {
    /* we pad one byte between text accross two fields */
    t_ctx->init_pos += doc->text.f_len + 1;
  }

  return (!buf_full);
}

/** Get next doc item from fts_doc_list */
UNIV_INLINE
void row_merge_fts_get_next_doc_item(
    fts_psort_t *psort_info,   /*!< in: psort_info */
    fts_doc_item_t **doc_item) /*!< in/out: doc item */
{
  if (*doc_item != NULL) {
    ut_free(*doc_item);
  }

  mutex_enter(&psort_info->mutex);

  *doc_item = UT_LIST_GET_FIRST(psort_info->fts_doc_list);
  if (*doc_item != NULL) {
    UT_LIST_REMOVE(psort_info->fts_doc_list, *doc_item);

    ut_ad(psort_info->memory_used >=
          sizeof(fts_doc_item_t) + (*doc_item)->field->len);
    psort_info->memory_used -= sizeof(fts_doc_item_t) + (*doc_item)->field->len;
  }

  mutex_exit(&psort_info->mutex);
}

/** Function performs parallel tokenization of the incoming doc strings.
It also performs the initial in memory sort of the parsed records. */
static void fts_parallel_tokenization_thread(fts_psort_t *psort_info) {
  ulint i;
  fts_doc_item_t *doc_item = NULL;
  row_merge_buf_t **buf;
  ibool processed = FALSE;
  merge_file_t **merge_file;
  row_merge_block_t **block;
  int tmpfd[FTS_NUM_AUX_INDEX];
  ulint mycount[FTS_NUM_AUX_INDEX];
  ib_uint64_t total_rec = 0;
  ulint num_doc_processed = 0;
  doc_id_t last_doc_id = 0;
  mem_heap_t *blob_heap = NULL;
  fts_doc_t doc;
  dict_table_t *table = psort_info->psort_common->new_table;
  dict_table_t *old_table = psort_info->psort_common->old_table;
  dtype_t word_dtype;
  dict_field_t *idx_field;
  fts_tokenize_ctx_t t_ctx;
  ulint retried = 0;
  dberr_t error = DB_SUCCESS;

  my_thread_init();
  ut_ad(psort_info->psort_common->trx->mysql_thd != NULL);
  const char *path =
      thd_innodb_tmpdir(psort_info->psort_common->trx->mysql_thd);

  ut_ad(psort_info);

  buf = psort_info->merge_buf;
  merge_file = psort_info->merge_file;
  blob_heap = mem_heap_create(512);
  memset(&doc, 0, sizeof(doc));
  memset(mycount, 0, FTS_NUM_AUX_INDEX * sizeof(int));

  doc.charset = fts_index_get_charset(psort_info->psort_common->dup->index);

  idx_field = psort_info->psort_common->dup->index->get_field(0);
  word_dtype.prtype = idx_field->col->prtype;
  word_dtype.mbminmaxlen = idx_field->col->mbminmaxlen;
  word_dtype.mtype =
      (doc.charset == &my_charset_latin1) ? DATA_VARCHAR : DATA_VARMYSQL;

  block = psort_info->merge_block;

  const page_size_t &page_size = dict_table_page_size(table);

  row_merge_fts_get_next_doc_item(psort_info, &doc_item);

  t_ctx.cached_stopword = table->fts->cache->stopword_info.cached_stopword;
  processed = TRUE;
loop:
  while (doc_item) {
    dfield_t *dfield = doc_item->field;

    last_doc_id = doc_item->doc_id;

    ut_ad(dfield->data != NULL && dfield_get_len(dfield) != UNIV_SQL_NULL);

    /* If finish processing the last item, update "doc" with
    strings in the doc_item, otherwise continue processing last
    item */
    if (processed) {
      byte *data;
      ulint data_len;

      dfield = doc_item->field;
      data = static_cast<byte *>(dfield_get_data(dfield));
      data_len = dfield_get_len(dfield);

      if (dfield_is_ext(dfield)) {
        dict_index_t *clust_index = old_table->first_index();
        doc.text.f_str = lob::btr_copy_externally_stored_field(
            clust_index, &doc.text.f_len, nullptr, data, page_size, data_len,
            false, blob_heap);
      } else {
        doc.text.f_str = data;
        doc.text.f_len = data_len;
      }

      doc.tokens = 0;
      t_ctx.processed_len = 0;
    } else {
      /* Not yet finish processing the "doc" on hand,
      continue processing it */
      ut_ad(doc.text.f_str);
      ut_ad(t_ctx.processed_len < doc.text.f_len);
    }

    processed = row_merge_fts_doc_tokenize(
        buf, doc_item->doc_id, &doc, &word_dtype, merge_file,
        psort_info->psort_common->opt_doc_id_size, &t_ctx);

    /* Current sort buffer full, need to recycle */
    if (!processed) {
      ut_ad(t_ctx.processed_len < doc.text.f_len);
      ut_ad(t_ctx.rows_added[t_ctx.buf_used]);
      break;
    }

    num_doc_processed++;

    if (fts_enable_diag_print && num_doc_processed % 10000 == 1) {
      ib::info(ER_IB_MSG_928)
          << "Number of documents processed: " << num_doc_processed;
#ifdef FTS_INTERNAL_DIAG_PRINT
      for (i = 0; i < FTS_NUM_AUX_INDEX; i++) {
        ib::info(ER_IB_MSG_929)
            << "ID " << psort_info->psort_id << ", partition " << i << ", word "
            << mycount[i];
      }
#endif
    }

    mem_heap_empty(blob_heap);

    row_merge_fts_get_next_doc_item(psort_info, &doc_item);

    if (doc_item && last_doc_id != doc_item->doc_id) {
      t_ctx.init_pos = 0;
    }
  }

  /* If we run out of current sort buffer, need to sort
  and flush the sort buffer to disk */
  if (t_ctx.rows_added[t_ctx.buf_used] && !processed) {
    row_merge_buf_sort(buf[t_ctx.buf_used], NULL);
    row_merge_buf_write(buf[t_ctx.buf_used], merge_file[t_ctx.buf_used],
                        block[t_ctx.buf_used]);

    if (!row_merge_write(merge_file[t_ctx.buf_used]->fd,
                         merge_file[t_ctx.buf_used]->offset++,
                         block[t_ctx.buf_used])) {
      error = DB_TEMP_FILE_WRITE_FAIL;
      goto func_exit;
    }

    UNIV_MEM_INVALID(block[t_ctx.buf_used][0], srv_sort_buf_size);
    buf[t_ctx.buf_used] = row_merge_buf_empty(buf[t_ctx.buf_used]);
    mycount[t_ctx.buf_used] += t_ctx.rows_added[t_ctx.buf_used];
    t_ctx.rows_added[t_ctx.buf_used] = 0;

    ut_a(doc_item);
    goto loop;
  }

  /* Parent done scanning, and if finish processing all the docs, exit */
  if (psort_info->state == FTS_PARENT_COMPLETE) {
    if (UT_LIST_GET_LEN(psort_info->fts_doc_list) == 0) {
      goto exit;
    } else if (retried > 10000) {
      ut_ad(!doc_item);
      /* retied too many times and cannot get new record */
      ib::error(ER_IB_MSG_930)
          << "FTS parallel sort processed " << num_doc_processed
          << " records, the sort queue has "
          << UT_LIST_GET_LEN(psort_info->fts_doc_list)
          << " records. But sort cannot get the next"
             " records";
      goto exit;
    }
  } else if (psort_info->state == FTS_PARENT_EXITING) {
    /* Parent abort */
    goto func_exit;
  }

  if (doc_item == NULL) {
    os_thread_yield();
  }

  row_merge_fts_get_next_doc_item(psort_info, &doc_item);

  if (doc_item != NULL) {
    if (last_doc_id != doc_item->doc_id) {
      t_ctx.init_pos = 0;
    }

    retried = 0;
  } else if (psort_info->state == FTS_PARENT_COMPLETE) {
    retried++;
  }

  goto loop;

exit:
  /* Do a final sort of the last (or latest) batch of records
  in block memory. Flush them to temp file if records cannot
  be hold in one block memory */
  for (i = 0; i < FTS_NUM_AUX_INDEX; i++) {
    if (t_ctx.rows_added[i]) {
      row_merge_buf_sort(buf[i], NULL);
      row_merge_buf_write(buf[i], merge_file[i], block[i]);

      /* Write to temp file, only if records have
      been flushed to temp file before (offset > 0):
      The pseudo code for sort is following:

              while (there are rows) {
                      tokenize rows, put result in block[]
                      if (block[] runs out) {
                              sort rows;
                              write to temp file with
                              row_merge_write();
                              offset++;
                      }
              }

              # write out the last batch
              if (offset > 0) {
                      row_merge_write();
                      offset++;
              } else {
                      # no need to write anything
                      offset stay as 0
              }

      so if merge_file[i]->offset is 0 when we come to
      here as the last batch, this means rows have
      never flush to temp file, it can be held all in
      memory */
      if (merge_file[i]->offset != 0) {
        if (!row_merge_write(merge_file[i]->fd, merge_file[i]->offset++,
                             block[i])) {
          error = DB_TEMP_FILE_WRITE_FAIL;
          goto func_exit;
        }

        UNIV_MEM_INVALID(block[i][0], srv_sort_buf_size);
      }

      buf[i] = row_merge_buf_empty(buf[i]);
      t_ctx.rows_added[i] = 0;
    }
  }

  if (fts_enable_diag_print) {
    DEBUG_FTS_SORT_PRINT("  InnoDB_FTS: start merge sort\n");
  }

  for (i = 0; i < FTS_NUM_AUX_INDEX; i++) {
    if (!merge_file[i]->offset) {
      continue;
    }

    tmpfd[i] = row_merge_file_create_low(path);
    if (tmpfd[i] < 0) {
      error = DB_OUT_OF_MEMORY;
      goto func_exit;
    }

    error = row_merge_sort(psort_info->psort_common->trx,
                           psort_info->psort_common->dup, merge_file[i],
                           block[i], &tmpfd[i]);
    if (error != DB_SUCCESS) {
      close(tmpfd[i]);
      goto func_exit;
    }

    total_rec += merge_file[i]->n_rec;
    close(tmpfd[i]);
  }

func_exit:
  if (fts_enable_diag_print) {
    DEBUG_FTS_SORT_PRINT("  InnoDB_FTS: complete merge sort\n");
  }

  mem_heap_free(blob_heap);

  mutex_enter(&psort_info->mutex);
  psort_info->error = error;
  mutex_exit(&psort_info->mutex);

  if (UT_LIST_GET_LEN(psort_info->fts_doc_list) > 0) {
    /* child can exit either with error or told by parent. */
    ut_ad(error != DB_SUCCESS || psort_info->state == FTS_PARENT_EXITING);
  }

  /* Free fts doc list in case of error. */
  do {
    row_merge_fts_get_next_doc_item(psort_info, &doc_item);
  } while (doc_item != NULL);

  psort_info->child_status = FTS_CHILD_COMPLETE;
  os_event_set(psort_info->psort_common->sort_event);
  psort_info->child_status = FTS_CHILD_EXITING;

  my_thread_end();
}

/** Start the parallel tokenization and parallel merge sort
@param[in,out]	psort_info		Parallel sort structure */
void row_fts_start_psort(fts_psort_t *psort_info) {
  for (ulint i = 0; i < fts_sort_pll_degree; i++) {
    psort_info[i].psort_id = i;

    os_thread_create(fts_parallel_tokenization_thread_key,
                     fts_parallel_tokenization_thread, &psort_info[i]);
  }
}

/** Function performs the merge and insertion of the sorted records.
@param[in]	psort_info		parallel merge info */
static void fts_parallel_merge_thread(fts_psort_t *psort_info) {
  ulint id = psort_info->psort_id;
  my_thread_init();

  row_fts_merge_insert(psort_info->psort_common->dup->index,
                       psort_info->psort_common->new_table,
                       psort_info->psort_common->all_info, id);

  psort_info->child_status = FTS_CHILD_COMPLETE;
  os_event_set(psort_info->psort_common->merge_event);
  psort_info->child_status = FTS_CHILD_EXITING;

  my_thread_end();
}

/** Kick off the parallel merge and insert thread
@param[in,out]	merge_info	parallel sort info */
void row_fts_start_parallel_merge(fts_psort_t *merge_info) {
  /* Kick off merge/insert threads */
  for (int i = 0; i < FTS_NUM_AUX_INDEX; ++i) {
    merge_info[i].psort_id = i;
    merge_info[i].child_status = 0;

    os_thread_create(fts_parallel_merge_thread_key, fts_parallel_merge_thread,
                     &merge_info[i]);
  }
}

/**
Write out a single word's data as new entry/entries in the INDEX table.
@param[in]	ins_ctx	insert context
@param[in]	word	word string
@param[in]	node	node colmns
@return	DB_SUCCUESS if insertion runs fine, otherwise error code */
static dberr_t row_merge_write_fts_node(const fts_psort_insert_t *ins_ctx,
                                        const fts_string_t *word,
                                        const fts_node_t *node) {
  dtuple_t *tuple;
  dfield_t *field;
  dberr_t ret = DB_SUCCESS;
  doc_id_t write_first_doc_id[8];
  doc_id_t write_last_doc_id[8];
  ib_uint32_t write_doc_count;

  tuple = ins_ctx->tuple;

  /* The first field is the tokenized word */
  field = dtuple_get_nth_field(tuple, 0);
  dfield_set_data(field, word->f_str, word->f_len);

  /* The second field is first_doc_id */
  field = dtuple_get_nth_field(tuple, 1);
  fts_write_doc_id((byte *)&write_first_doc_id, node->first_doc_id);
  dfield_set_data(field, &write_first_doc_id, sizeof(doc_id_t));

  /* The third and fourth fileds(TRX_ID, ROLL_PTR) are filled already.*/
  /* The fifth field is last_doc_id */
  field = dtuple_get_nth_field(tuple, 4);
  fts_write_doc_id((byte *)&write_last_doc_id, node->last_doc_id);
  dfield_set_data(field, &write_last_doc_id, sizeof(doc_id_t));

  /* The sixth field is doc_count */
  field = dtuple_get_nth_field(tuple, 5);
  mach_write_to_4((byte *)&write_doc_count, (ib_uint32_t)node->doc_count);
  dfield_set_data(field, &write_doc_count, sizeof(ib_uint32_t));

  /* The seventh field is ilist */
  field = dtuple_get_nth_field(tuple, 6);
  dfield_set_data(field, node->ilist, node->ilist_size);

  ret = ins_ctx->btr_bulk->insert(tuple);

  return (ret);
}

/** Insert processed FTS data to auxillary index tables.
 @return DB_SUCCESS if insertion runs fine */
static dberr_t row_merge_write_fts_word(
    fts_psort_insert_t *ins_ctx, /*!< in: insert context */
    fts_tokenizer_word_t *word)  /*!< in: sorted and tokenized
                                 word */
{
  dberr_t ret = DB_SUCCESS;

  ut_ad(ins_ctx->aux_index_id ==
        fts_select_index(ins_ctx->charset, word->text.f_str, word->text.f_len));

  /* Pop out each fts_node in word->nodes write them to auxiliary table */
  for (ulint i = 0; i < ib_vector_size(word->nodes); i++) {
    dberr_t error;
    fts_node_t *fts_node;

    fts_node = static_cast<fts_node_t *>(ib_vector_get(word->nodes, i));

    error = row_merge_write_fts_node(ins_ctx, &word->text, fts_node);

    if (error != DB_SUCCESS) {
      ib::error(ER_IB_MSG_931) << "Failed to write word " << word->text.f_str
                               << " to FTS auxiliary"
                                  " index table, error ("
                               << ut_strerr(error) << ")";
      ret = error;
    }

    ut_free(fts_node->ilist);
    fts_node->ilist = NULL;
  }

  ib_vector_reset(word->nodes);

  return (ret);
}

/** Read sorted FTS data files and insert data tuples to auxillary tables.
 */
static void row_fts_insert_tuple(
    fts_psort_insert_t *ins_ctx, /*!< in: insert context */
    fts_tokenizer_word_t *word,  /*!< in: last processed
                                 tokenized word */
    ib_vector_t *positions,      /*!< in: word position */
    doc_id_t *in_doc_id,         /*!< in: last item doc id */
    dtuple_t *dtuple)            /*!< in: entry to insert */
{
  fts_node_t *fts_node = NULL;
  dfield_t *dfield;
  doc_id_t doc_id;
  ulint position;
  fts_string_t token_word;
  ulint i;

  /* Get fts_node for the FTS auxillary INDEX table */
  if (ib_vector_size(word->nodes) > 0) {
    fts_node = static_cast<fts_node_t *>(ib_vector_last(word->nodes));
  }

  if (fts_node == NULL || fts_node->ilist_size > FTS_ILIST_MAX_SIZE) {
    fts_node = static_cast<fts_node_t *>(ib_vector_push(word->nodes, NULL));

    memset(fts_node, 0x0, sizeof(*fts_node));
  }

  /* If dtuple == NULL, this is the last word to be processed */
  if (!dtuple) {
    if (fts_node && ib_vector_size(positions) > 0) {
      fts_cache_node_add_positions(NULL, fts_node, *in_doc_id, positions);

      /* Write out the current word */
      row_merge_write_fts_word(ins_ctx, word);
    }

    return;
  }

  /* Get the first field for the tokenized word */
  dfield = dtuple_get_nth_field(dtuple, 0);

  token_word.f_n_char = 0;
  token_word.f_len = dfield->len;
  token_word.f_str = static_cast<byte *>(dfield_get_data(dfield));

  if (!word->text.f_str) {
    fts_string_dup(&word->text, &token_word, ins_ctx->heap);
  }

  /* compare to the last word, to see if they are the same
  word */
  if (innobase_fts_text_cmp(ins_ctx->charset, &word->text, &token_word) != 0) {
    ulint num_item;

    /* Getting a new word, flush the last position info
    for the currnt word in fts_node */
    if (ib_vector_size(positions) > 0) {
      fts_cache_node_add_positions(NULL, fts_node, *in_doc_id, positions);
    }

    /* Write out the current word */
    row_merge_write_fts_word(ins_ctx, word);

    /* Copy the new word */
    fts_string_dup(&word->text, &token_word, ins_ctx->heap);

    num_item = ib_vector_size(positions);

    /* Clean up position queue */
    for (i = 0; i < num_item; i++) {
      ib_vector_pop(positions);
    }

    /* Reset Doc ID */
    *in_doc_id = 0;
    memset(fts_node, 0x0, sizeof(*fts_node));
  }

  /* Get the word's Doc ID */
  dfield = dtuple_get_nth_field(dtuple, 1);

  if (!ins_ctx->opt_doc_id_size) {
    doc_id = fts_read_doc_id(static_cast<byte *>(dfield_get_data(dfield)));
  } else {
    doc_id = (doc_id_t)mach_read_from_4(
        static_cast<byte *>(dfield_get_data(dfield)));
  }

  /* Get the word's position info */
  dfield = dtuple_get_nth_field(dtuple, 2);
  position = mach_read_from_4(static_cast<byte *>(dfield_get_data(dfield)));

  /* If this is the same word as the last word, and they
  have the same Doc ID, we just need to add its position
  info. Otherwise, we will flush position info to the
  fts_node and initiate a new position vector  */
  if (!(*in_doc_id) || *in_doc_id == doc_id) {
    ib_vector_push(positions, &position);
  } else {
    ulint num_pos = ib_vector_size(positions);

    fts_cache_node_add_positions(NULL, fts_node, *in_doc_id, positions);
    for (i = 0; i < num_pos; i++) {
      ib_vector_pop(positions);
    }
    ib_vector_push(positions, &position);
  }

  /* record the current Doc ID */
  *in_doc_id = doc_id;
}

/** Propagate a newly added record up one level in the selection tree
 @return parent where this value propagated to */
static int row_fts_sel_tree_propagate(
    int propogated,      /*!< in: tree node propagated */
    int *sel_tree,       /*!< in: selection tree */
    const mrec_t **mrec, /*!< in: sort record */
    ulint **offsets,     /*!< in: record offsets */
    dict_index_t *index) /*!< in/out: FTS index */
{
  ulint parent;
  int child_left;
  int child_right;
  int selected;

  /* Find which parent this value will be propagated to */
  parent = (propogated - 1) / 2;

  /* Find out which value is smaller, and to propagate */
  child_left = sel_tree[parent * 2 + 1];
  child_right = sel_tree[parent * 2 + 2];

  if (child_left == -1 || mrec[child_left] == NULL) {
    if (child_right == -1 || mrec[child_right] == NULL) {
      selected = -1;
    } else {
      selected = child_right;
    }
  } else if (child_right == -1 || mrec[child_right] == NULL) {
    selected = child_left;
  } else if (cmp_rec_rec_simple(mrec[child_left], mrec[child_right],
                                offsets[child_left], offsets[child_right],
                                index, NULL) < 0) {
    selected = child_left;
  } else {
    selected = child_right;
  }

  sel_tree[parent] = selected;

  return (static_cast<int>(parent));
}

/** Readjust selection tree after popping the root and read a new value
 @return the new root */
static int row_fts_sel_tree_update(
    int *sel_tree,       /*!< in/out: selection tree */
    ulint propagated,    /*!< in: node to propagate up */
    ulint height,        /*!< in: tree height */
    const mrec_t **mrec, /*!< in: sort record */
    ulint **offsets,     /*!< in: record offsets */
    dict_index_t *index) /*!< in: index dictionary */
{
  ulint i;

  for (i = 1; i <= height; i++) {
    propagated = static_cast<ulint>(row_fts_sel_tree_propagate(
        static_cast<int>(propagated), sel_tree, mrec, offsets, index));
  }

  return (sel_tree[0]);
}

/** Build selection tree at a specified level */
static void row_fts_build_sel_tree_level(
    int *sel_tree,       /*!< in/out: selection tree */
    ulint level,         /*!< in: selection tree level */
    const mrec_t **mrec, /*!< in: sort record */
    ulint **offsets,     /*!< in: record offsets */
    dict_index_t *index) /*!< in: index dictionary */
{
  ulint start;
  int child_left;
  int child_right;
  ulint i;
  ulint num_item;

  num_item = static_cast<ulint>(1) << level;
  start = num_item - 1;

  for (i = 0; i < num_item; i++) {
    child_left = sel_tree[(start + i) * 2 + 1];
    child_right = sel_tree[(start + i) * 2 + 2];

    if (child_left == -1) {
      if (child_right == -1) {
        sel_tree[start + i] = -1;
      } else {
        sel_tree[start + i] = child_right;
      }
      continue;
    } else if (child_right == -1) {
      sel_tree[start + i] = child_left;
      continue;
    }

    /* Deal with NULL child conditions */
    if (!mrec[child_left]) {
      if (!mrec[child_right]) {
        sel_tree[start + i] = -1;
      } else {
        sel_tree[start + i] = child_right;
      }
      continue;
    } else if (!mrec[child_right]) {
      sel_tree[start + i] = child_left;
      continue;
    }

    /* Select the smaller one to set parent pointer */
    int cmp = cmp_rec_rec_simple(mrec[child_left], mrec[child_right],
                                 offsets[child_left], offsets[child_right],
                                 index, NULL);

    sel_tree[start + i] = cmp < 0 ? child_left : child_right;
  }
}

/** Build a selection tree for merge. The selection tree is a binary tree
 and should have fts_sort_pll_degree / 2 levels. With root as level 0
 @return number of tree levels */
static ulint row_fts_build_sel_tree(
    int *sel_tree,       /*!< in/out: selection tree */
    const mrec_t **mrec, /*!< in: sort record */
    ulint **offsets,     /*!< in: record offsets */
    dict_index_t *index) /*!< in: index dictionary */
{
  ulint treelevel = 1;
  ulint num = 2;
  int i = 0;
  ulint start;

  /* No need to build selection tree if we only have two merge threads */
  if (fts_sort_pll_degree <= 2) {
    return (0);
  }

  while (num < fts_sort_pll_degree) {
    num = num << 1;
    treelevel++;
  }

  start = (1 << treelevel) - 1;

  for (i = 0; i < (int)fts_sort_pll_degree; i++) {
    sel_tree[i + start] = i;
  }

  for (i = static_cast<int>(treelevel) - 1; i >= 0; i--) {
    row_fts_build_sel_tree_level(sel_tree, static_cast<ulint>(i), mrec, offsets,
                                 index);
  }

  return (treelevel);
}

/** Read sorted file containing index data tuples and insert these data
tuples to the index
@param[in]	index		index
@param[in]	table		new table
@param[in]	psort_info	parallel sort info
@param[in]	id		which auxiliary table's data to insert to
@return DB_SUCCESS or error number */
dberr_t row_fts_merge_insert(dict_index_t *index, dict_table_t *table,
                             fts_psort_t *psort_info, ulint id) {
  const byte **b;
  mem_heap_t *tuple_heap;
  mem_heap_t *heap;
  dberr_t error = DB_SUCCESS;
  ulint *foffs;
  ulint **offsets;
  fts_tokenizer_word_t new_word;
  ib_vector_t *positions;
  doc_id_t last_doc_id;
  ib_alloc_t *heap_alloc;
  ulint i;
  mrec_buf_t **buf;
  int *fd;
  byte **block;
  const mrec_t **mrec;
  ulint count = 0;
  int *sel_tree;
  ulint height;
  ulint start;
  fts_psort_insert_t ins_ctx;
  ulint count_diag = 0;
  fts_table_t fts_table;
  char aux_table_name[MAX_FULL_NAME_LEN];
  dict_table_t *aux_table;
  dict_index_t *aux_index;
  trx_t *trx;
  byte trx_id_buf[6];
  roll_ptr_t roll_ptr = 0;
  dfield_t *field;

  ut_ad(index);
  ut_ad(table);

  /* We use the insert query graph as the dummy graph
  needed in the row module call */

  trx = trx_allocate_for_background();
  trx_start_if_not_started(trx, true);

  trx->op_info = "inserting index entries";

  ins_ctx.opt_doc_id_size = psort_info[0].psort_common->opt_doc_id_size;

  heap = mem_heap_create(500 + sizeof(mrec_buf_t));

  b = (const byte **)mem_heap_alloc(heap, sizeof(*b) * fts_sort_pll_degree);
  foffs = (ulint *)mem_heap_alloc(heap, sizeof(*foffs) * fts_sort_pll_degree);
  offsets =
      (ulint **)mem_heap_alloc(heap, sizeof(*offsets) * fts_sort_pll_degree);
  buf = (mrec_buf_t **)mem_heap_alloc(heap, sizeof(*buf) * fts_sort_pll_degree);
  fd = (int *)mem_heap_alloc(heap, sizeof(*fd) * fts_sort_pll_degree);
  block = (byte **)mem_heap_alloc(heap, sizeof(*block) * fts_sort_pll_degree);
  mrec = (const mrec_t **)mem_heap_alloc(heap,
                                         sizeof(*mrec) * fts_sort_pll_degree);
  sel_tree = (int *)mem_heap_alloc(
      heap, sizeof(*sel_tree) * (fts_sort_pll_degree * 2));

  tuple_heap = mem_heap_create(1000);

  ins_ctx.charset = fts_index_get_charset(index);
  ins_ctx.heap = heap;

  for (i = 0; i < fts_sort_pll_degree; i++) {
    ulint num;

    num = 1 + REC_OFFS_HEADER_SIZE + dict_index_get_n_fields(index);
    offsets[i] =
        static_cast<ulint *>(mem_heap_zalloc(heap, num * sizeof *offsets[i]));
    offsets[i][0] = num;
    offsets[i][1] = dict_index_get_n_fields(index);
    block[i] = psort_info[i].merge_block[id];
    b[i] = psort_info[i].merge_block[id];
    fd[i] = psort_info[i].merge_file[id]->fd;
    foffs[i] = 0;

    buf[i] = static_cast<mrec_buf_t *>(mem_heap_alloc(heap, sizeof *buf[i]));
    count_diag += (int)psort_info[i].merge_file[id]->n_rec;
  }

  if (fts_enable_diag_print) {
    ib::info(ER_IB_MSG_932)
        << "InnoDB_FTS: to inserted " << count_diag << " records";
  }

  /* Initialize related variables if creating FTS indexes */
  heap_alloc = ib_heap_allocator_create(heap);

  memset(&new_word, 0, sizeof(new_word));

  new_word.nodes = ib_vector_create(heap_alloc, sizeof(fts_node_t), 4);
  positions = ib_vector_create(heap_alloc, sizeof(ulint), 32);
  last_doc_id = 0;

  fts_table.type = FTS_INDEX_TABLE;
  fts_table.index_id = index->id;
  fts_table.table_id = table->id;
  fts_table.parent = index->table->name.m_name;
  fts_table.table = index->table;
  fts_table.suffix = fts_get_suffix(id);

  /* Get aux index */
  fts_get_table_name(&fts_table, aux_table_name);

  aux_table = dd_table_open_on_name(nullptr, nullptr, aux_table_name, false,
                                    DICT_ERR_IGNORE_NONE);
  ut_ad(aux_table != NULL);
  dd_table_close(aux_table, nullptr, nullptr, false);
  aux_index = aux_table->first_index();

  FlushObserver *observer;
  observer = psort_info[0].psort_common->trx->flush_observer;

  /* Create bulk load instance */
  ins_ctx.btr_bulk = UT_NEW_NOKEY(BtrBulk(aux_index, trx->id, observer));
  error = ins_ctx.btr_bulk->init();
  if (error != DB_SUCCESS) {
    /* delete immediately so finish() would not be called */
    UT_DELETE(ins_ctx.btr_bulk);
    ins_ctx.btr_bulk = nullptr;
    goto exit;
  }

  /* Create tuple for insert */
  ins_ctx.tuple = dtuple_create(heap, dict_index_get_n_fields(aux_index));
  dict_index_copy_types(ins_ctx.tuple, aux_index,
                        dict_index_get_n_fields(aux_index));

  /* Set TRX_ID and ROLL_PTR */
  trx_write_trx_id(trx_id_buf, trx->id);
  field = dtuple_get_nth_field(ins_ctx.tuple, 2);
  dfield_set_data(field, &trx_id_buf, 6);

  field = dtuple_get_nth_field(ins_ctx.tuple, 3);
  dfield_set_data(field, &roll_ptr, 7);

#ifdef UNIV_DEBUG
  ins_ctx.aux_index_id = id;
#endif

  for (i = 0; i < fts_sort_pll_degree; i++) {
    if (psort_info[i].merge_file[id]->n_rec == 0) {
      /* No Rows to read */
      mrec[i] = b[i] = NULL;
    } else {
      /* Read from temp file only if it has been
      written to. Otherwise, block memory holds
      all the sorted records */
      if (psort_info[i].merge_file[id]->offset > 0 &&
          (!row_merge_read(fd[i], foffs[i], (row_merge_block_t *)block[i]))) {
        error = DB_CORRUPTION;
        goto exit;
      }

      ROW_MERGE_READ_GET_NEXT(i);
    }
  }

  height =
      row_fts_build_sel_tree(sel_tree, (const mrec_t **)mrec, offsets, index);

  start = (1 << height) - 1;

  /* Fetch sorted records from sort buffer and insert them into
  corresponding FTS index auxiliary tables */
  for (;;) {
    dtuple_t *dtuple;
    ulint n_ext;
    int min_rec = 0;

    if (fts_sort_pll_degree <= 2) {
      while (!mrec[min_rec]) {
        min_rec++;

        if (min_rec >= (int)fts_sort_pll_degree) {
          row_fts_insert_tuple(&ins_ctx, &new_word, positions, &last_doc_id,
                               NULL);

          goto exit;
        }
      }

      for (i = min_rec + 1; i < fts_sort_pll_degree; i++) {
        if (!mrec[i]) {
          continue;
        }

        if (cmp_rec_rec_simple(mrec[i], mrec[min_rec], offsets[i],
                               offsets[min_rec], index, NULL) < 0) {
          min_rec = static_cast<int>(i);
        }
      }
    } else {
      min_rec = sel_tree[0];

      if (min_rec == -1) {
        row_fts_insert_tuple(&ins_ctx, &new_word, positions, &last_doc_id,
                             NULL);

        goto exit;
      }
    }

    dtuple = row_rec_to_index_entry_low(mrec[min_rec], index, offsets[min_rec],
                                        &n_ext, tuple_heap);

    row_fts_insert_tuple(&ins_ctx, &new_word, positions, &last_doc_id, dtuple);

    ROW_MERGE_READ_GET_NEXT(min_rec);

    if (fts_sort_pll_degree > 2) {
      if (!mrec[min_rec]) {
        sel_tree[start + min_rec] = -1;
      }

      row_fts_sel_tree_update(sel_tree, start + min_rec, height, mrec, offsets,
                              index);
    }

    count++;

    mem_heap_empty(tuple_heap);
  }

exit:
  fts_sql_commit(trx);

  trx->op_info = "";

  mem_heap_free(tuple_heap);

  if (ins_ctx.btr_bulk) {
    error = ins_ctx.btr_bulk->finish(error);
    UT_DELETE(ins_ctx.btr_bulk);
  }

  trx_free_for_background(trx);

  mem_heap_free(heap);

  if (fts_enable_diag_print) {
    ib::info(ER_IB_MSG_933) << "InnoDB_FTS: inserted " << count << " records";
  }

  return (error);
}
