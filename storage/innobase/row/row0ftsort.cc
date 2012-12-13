/*****************************************************************************

Copyright (c) 2010, 2012, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/**************************************************//**
@file row/row0ftsort.cc
Create Full Text Index with (parallel) merge sort

Created 10/13/2010 Jimmy Yang
*******************************************************/

#include "dict0dict.h" /* dict_table_stats_lock() */
#include "row0merge.h"
#include "pars0pars.h"
#include "row0ftsort.h"
#include "row0merge.h"
#include "row0row.h"
#include "btr0cur.h"

/** Read the next record to buffer N.
@param N	index into array of merge info structure */
#define ROW_MERGE_READ_GET_NEXT(N)					\
	do {								\
		b[N] = row_merge_read_rec(				\
			block[N], buf[N], b[N], index,			\
			fd[N], &foffs[N], &mrec[N], offsets[N]);	\
		if (UNIV_UNLIKELY(!b[N])) {				\
			if (mrec[N]) {					\
				goto exit;				\
			}						\
		}							\
	} while (0)

/** Parallel sort degree */
UNIV_INTERN ulong	fts_sort_pll_degree	= 2;

/*********************************************************************//**
Create a temporary "fts sort index" used to merge sort the
tokenized doc string. The index has three "fields":

1) Tokenized word,
2) Doc ID (depend on number of records to sort, it can be a 4 bytes or 8 bytes
integer value)
3) Word's position in original doc.

@return dict_index_t structure for the fts sort index */
UNIV_INTERN
dict_index_t*
row_merge_create_fts_sort_index(
/*============================*/
	dict_index_t*		index,	/*!< in: Original FTS index
					based on which this sort index
					is created */
	const dict_table_t*	table,	/*!< in: table that FTS index
					is being created on */
	ibool*			opt_doc_id_size)
					/*!< out: whether to use 4 bytes
					instead of 8 bytes integer to
					store Doc ID during sort */
{
	dict_index_t*   new_index;
	dict_field_t*   field;
	dict_field_t*   idx_field;
	CHARSET_INFO*	charset;

	// FIXME: This name shouldn't be hard coded here.
	new_index = dict_mem_index_create(
		index->table->name, "tmp_fts_idx", 0, DICT_FTS, 3);

	new_index->id = index->id;
	new_index->table = (dict_table_t*) table;
	new_index->n_uniq = FTS_NUM_FIELDS_SORT;
	new_index->n_def = FTS_NUM_FIELDS_SORT;
	new_index->cached = TRUE;

	idx_field = dict_index_get_nth_field(index, 0);
	charset = fts_index_get_charset(index);

	/* The first field is on the Tokenized Word */
	field = dict_index_get_nth_field(new_index, 0);
	field->name = NULL;
	field->prefix_len = 0;
	field->col = static_cast<dict_col_t*>(
		mem_heap_alloc(new_index->heap, sizeof(dict_col_t)));
	field->col->len = fts_max_token_size;

	if (strcmp(charset->name, "latin1_swedish_ci") == 0) {
		field->col->mtype = DATA_VARCHAR;
	} else {
		field->col->mtype = DATA_VARMYSQL;
	}

	field->col->prtype = idx_field->col->prtype | DATA_NOT_NULL;
	field->col->mbminmaxlen = idx_field->col->mbminmaxlen;
	field->fixed_len = 0;

	/* Doc ID */
	field = dict_index_get_nth_field(new_index, 1);
	field->name = NULL;
	field->prefix_len = 0;
	field->col = static_cast<dict_col_t*>(
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
		doc_id_t	max_doc_id;

		/* If the Doc ID column is supplied by user, then
		check the maximum Doc ID in the table */
		max_doc_id = fts_get_max_doc_id((dict_table_t*) table);

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
	field = dict_index_get_nth_field(new_index, 2);
	field->name = NULL;
	field->prefix_len = 0;
	field->col = static_cast<dict_col_t*>(
		mem_heap_alloc(new_index->heap, sizeof(dict_col_t)));
	field->col->mtype = DATA_INT;
	field->col->len = 4 ;
	field->fixed_len = 4;
	field->col->prtype = DATA_NOT_NULL;
	field->col->mbminmaxlen = 0;

	return(new_index);
}
/*********************************************************************//**
Initialize FTS parallel sort structures.
@return TRUE if all successful */
UNIV_INTERN
ibool
row_fts_psort_info_init(
/*====================*/
	trx_t*			trx,	/*!< in: transaction */
	row_merge_dup_t*	dup,	/*!< in,own: descriptor of
					FTS index being created */
	const dict_table_t*	new_table,/*!< in: table on which indexes are
					created */
	ibool			opt_doc_id_size,
					/*!< in: whether to use 4 bytes
					instead of 8 bytes integer to
					store Doc ID during sort */
	fts_psort_t**		psort,	/*!< out: parallel sort info to be
					instantiated */
	fts_psort_t**		merge)	/*!< out: parallel merge info
					to be instantiated */
{
	ulint			i;
	ulint			j;
	fts_psort_common_t*	common_info = NULL;
	fts_psort_t*		psort_info = NULL;
	fts_psort_t*		merge_info = NULL;
	ulint			block_size;
	ibool			ret = TRUE;

	block_size = 3 * srv_sort_buf_size;

	*psort = psort_info = static_cast<fts_psort_t*>(mem_zalloc(
		 fts_sort_pll_degree * sizeof *psort_info));

	if (!psort_info) {
		ut_free(dup);
		return(FALSE);
	}

	/* Common Info for all sort threads */
	common_info = static_cast<fts_psort_common_t*>(
		mem_alloc(sizeof *common_info));

	if (!common_info) {
		ut_free(dup);
		mem_free(psort_info);
		return(FALSE);
	}

	common_info->dup = dup;
	common_info->new_table = (dict_table_t*) new_table;
	common_info->trx = trx;
	common_info->all_info = psort_info;
	common_info->sort_event = os_event_create(0);
	common_info->merge_event = os_event_create(0);
	common_info->opt_doc_id_size = opt_doc_id_size;

	/* There will be FTS_NUM_AUX_INDEX number of "sort buckets" for
	each parallel sort thread. Each "sort bucket" holds records for
	a particular "FTS index partition" */
	for (j = 0; j < fts_sort_pll_degree; j++) {

		UT_LIST_INIT(psort_info[j].fts_doc_list);

		for (i = 0; i < FTS_NUM_AUX_INDEX; i++) {

			psort_info[j].merge_file[i] =
				 static_cast<merge_file_t*>(
					mem_zalloc(sizeof(merge_file_t)));

			if (!psort_info[j].merge_file[i]) {
				ret = FALSE;
				goto func_exit;
			}

			psort_info[j].merge_buf[i] = row_merge_buf_create(
				dup->index);

			if (row_merge_file_create(psort_info[j].merge_file[i])
			    < 0) {
				goto func_exit;
			}

			/* Need to align memory for O_DIRECT write */
			psort_info[j].block_alloc[i] =
				static_cast<row_merge_block_t*>(ut_malloc(
					block_size + 1024));

			psort_info[j].merge_block[i] =
				static_cast<row_merge_block_t*>(
					ut_align(
					psort_info[j].block_alloc[i], 1024));

			if (!psort_info[j].merge_block[i]) {
				ret = FALSE;
				goto func_exit;
			}
		}

		psort_info[j].child_status = 0;
		psort_info[j].state = 0;
		psort_info[j].psort_common = common_info;
	}

	/* Initialize merge_info structures parallel merge and insert
	into auxiliary FTS tables (FTS_INDEX_TABLE) */
	*merge = merge_info = static_cast<fts_psort_t*>(
		mem_alloc(FTS_NUM_AUX_INDEX * sizeof *merge_info));

	for (j = 0; j < FTS_NUM_AUX_INDEX; j++) {

		merge_info[j].child_status = 0;
		merge_info[j].state = 0;
		merge_info[j].psort_common = common_info;
	}

func_exit:
	if (!ret) {
		row_fts_psort_info_destroy(psort_info, merge_info);
	}

	return(ret);
}
/*********************************************************************//**
Clean up and deallocate FTS parallel sort structures, and close the
merge sort files  */
UNIV_INTERN
void
row_fts_psort_info_destroy(
/*=======================*/
	fts_psort_t*	psort_info,	/*!< parallel sort info */
	fts_psort_t*	merge_info)	/*!< parallel merge info */
{
	ulint	i;
	ulint	j;

	if (psort_info) {
		for (j = 0; j < fts_sort_pll_degree; j++) {
			for (i = 0; i < FTS_NUM_AUX_INDEX; i++) {
				if (psort_info[j].merge_file[i]) {
					row_merge_file_destroy(
						psort_info[j].merge_file[i]);
				}

				if (psort_info[j].block_alloc[i]) {
					ut_free(psort_info[j].block_alloc[i]);
				}
				mem_free(psort_info[j].merge_file[i]);
			}
		}

		os_event_destroy(merge_info[0].psort_common->sort_event);
		os_event_destroy(merge_info[0].psort_common->merge_event);
		ut_free(merge_info[0].psort_common->dup);
		mem_free(merge_info[0].psort_common);
		mem_free(psort_info);
	}

	if (merge_info) {
		mem_free(merge_info);
	}
}
/*********************************************************************//**
Free up merge buffers when merge sort is done */
UNIV_INTERN
void
row_fts_free_pll_merge_buf(
/*=======================*/
	fts_psort_t*	psort_info)	/*!< in: parallel sort info */
{
	ulint	j;
	ulint	i;

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

/*********************************************************************//**
Tokenize incoming text data and add to the sort buffer.
@return	TRUE if the record passed, FALSE if out of space */
static
ibool
row_merge_fts_doc_tokenize(
/*=======================*/
	row_merge_buf_t**	sort_buf,	/*!< in/out: sort buffer */
	doc_id_t		doc_id,		/*!< in: Doc ID */
	fts_doc_t*		doc,		/*!< in: Doc to be tokenized */
	dtype_t*		word_dtype,	/*!< in: data structure for
						word col */
	merge_file_t**		merge_file,	/*!< in/out: merge file */
	ibool			opt_doc_id_size,/*!< in: whether to use 4 bytes
						instead of 8 bytes integer to
						store Doc ID during sort*/
	fts_tokenize_ctx_t*	t_ctx)          /*!< in/out: tokenize context */
{
	ulint		i;
	ulint		inc;
	fts_string_t	str;
	ulint		len;
	row_merge_buf_t* buf;
	dfield_t*	field;
	fts_string_t	t_str;
	ibool		buf_full = FALSE;
	byte		str_buf[FTS_MAX_WORD_LEN + 1];
	ulint		data_size[FTS_NUM_AUX_INDEX];
	ulint		n_tuple[FTS_NUM_AUX_INDEX];

	t_str.f_n_char = 0;
	t_ctx->buf_used = 0;

	memset(n_tuple, 0, FTS_NUM_AUX_INDEX * sizeof(ulint));
	memset(data_size, 0, FTS_NUM_AUX_INDEX * sizeof(ulint));

	/* Tokenize the data and add each word string, its corresponding
	doc id and position to sort buffer */
	for (i = t_ctx->processed_len; i < doc->text.f_len; i += inc) {
		ib_rbt_bound_t	parent;
		ulint		idx = 0;
		ib_uint32_t	position;
		ulint           offset = 0;
		ulint		cur_len = 0;
		doc_id_t	write_doc_id;

		inc = innobase_mysql_fts_get_token(
			doc->charset, doc->text.f_str + i,
			doc->text.f_str + doc->text.f_len, &str, &offset);

		ut_a(inc > 0);

		/* Ignore string whose character number is less than
		"fts_min_token_size" or more than "fts_max_token_size" */
		if (str.f_n_char < fts_min_token_size
		    || str.f_n_char > fts_max_token_size) {

			t_ctx->processed_len += inc;
			continue;
		}

		t_str.f_len = innobase_fts_casedn_str(
			doc->charset, (char*) str.f_str, str.f_len,
			(char*) &str_buf, FTS_MAX_WORD_LEN + 1);

		t_str.f_str = (byte*) &str_buf;

		/* if "cached_stopword" is defined, ingore words in the
		stopword list */
		if (t_ctx->cached_stopword
		    && rbt_search(t_ctx->cached_stopword,
				  &parent, &t_str) == 0) {

			t_ctx->processed_len += inc;
			continue;
		}

		/* There are FTS_NUM_AUX_INDEX auxiliary tables, find
		out which sort buffer to put this word record in */
		t_ctx->buf_used = fts_select_index(
			doc->charset, t_str.f_str, t_str.f_len);

		buf = sort_buf[t_ctx->buf_used];

		ut_a(t_ctx->buf_used < FTS_NUM_AUX_INDEX);
		idx = t_ctx->buf_used;

		mtuple_t* mtuple = &buf->tuples[buf->n_tuples + n_tuple[idx]];

		field = mtuple->fields = static_cast<dfield_t*>(
			mem_heap_alloc(buf->heap,
				       FTS_NUM_FIELDS_SORT * sizeof *field));

		/* The first field is the tokenized word */
		dfield_set_data(field, t_str.f_str, t_str.f_len);
		len = dfield_get_len(field);

		field->type.mtype = word_dtype->mtype;
		field->type.prtype = word_dtype->prtype | DATA_NOT_NULL;

		/* Variable length field, set to max size. */
		field->type.len = fts_max_token_size;
		field->type.mbminmaxlen = word_dtype->mbminmaxlen;

		cur_len += len;
		dfield_dup(field, buf->heap);
		field++;

		/* The second field is the Doc ID */

		ib_uint32_t	doc_id_32_bit;

		if (!opt_doc_id_size) {
			fts_write_doc_id((byte*) &write_doc_id, doc_id);

			dfield_set_data(
				field, &write_doc_id, sizeof(write_doc_id));
		} else {
			mach_write_to_4(
				(byte*) &doc_id_32_bit, (ib_uint32_t) doc_id);

			dfield_set_data(
				field, &doc_id_32_bit, sizeof(doc_id_32_bit));
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
		mach_write_to_4(
			(byte*) &position,
			(i + offset + inc - str.f_len + t_ctx->init_pos));

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
		fts_max_token_size, add one extra size and one extra byte */
		cur_len += 2;

		/* Reserve one byte for the end marker of row_merge_block_t. */
		if (buf->total_size + data_size[idx] + cur_len
		    >= srv_sort_buf_size - 1) {

			buf_full = TRUE;
			break;
		}

		/* Increment the number of tuples */
		n_tuple[idx]++;
		t_ctx->processed_len += inc;
		data_size[idx] += cur_len;
	}

	/* Update the data length and the number of new word tuples
	added in this round of tokenization */
	for (i = 0; i <  FTS_NUM_AUX_INDEX; i++) {
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

	return(!buf_full);
}

/*********************************************************************//**
Function performs parallel tokenization of the incoming doc strings.
It also performs the initial in memory sort of the parsed records.
@return OS_THREAD_DUMMY_RETURN */
UNIV_INTERN
os_thread_ret_t
fts_parallel_tokenization(
/*======================*/
	void*		arg)	/*!< in: psort_info for the thread */
{
	fts_psort_t*		psort_info = (fts_psort_t*) arg;
	ulint			i;
	fts_doc_item_t*		doc_item = NULL;
	fts_doc_item_t*		prev_doc_item = NULL;
	row_merge_buf_t**	buf;
	ibool			processed = FALSE;
	merge_file_t**		merge_file;
	row_merge_block_t**	block;
	int			tmpfd[FTS_NUM_AUX_INDEX];
	ulint			mycount[FTS_NUM_AUX_INDEX];
	ib_uint64_t		total_rec = 0;
	ulint			num_doc_processed = 0;
	doc_id_t		last_doc_id = 0;
	ulint			zip_size;
	mem_heap_t*		blob_heap = NULL;
	fts_doc_t		doc;
	dict_table_t*		table = psort_info->psort_common->new_table;
	dtype_t			word_dtype;
	dict_field_t*		idx_field;
	fts_tokenize_ctx_t	t_ctx;
	ulint			retried = 0;
	ut_ad(psort_info);

	ut_ad(psort_info);

	buf = psort_info->merge_buf;
	merge_file = psort_info->merge_file;
	blob_heap = mem_heap_create(512);
	memset(&doc, 0, sizeof(doc));
	memset(&t_ctx, 0, sizeof(t_ctx));
	memset(mycount, 0, FTS_NUM_AUX_INDEX * sizeof(int));

	doc.charset = fts_index_get_charset(
		psort_info->psort_common->dup->index);

	idx_field = dict_index_get_nth_field(
		psort_info->psort_common->dup->index, 0);
	word_dtype.prtype = idx_field->col->prtype;
	word_dtype.mbminmaxlen = idx_field->col->mbminmaxlen;
	word_dtype.mtype = (strcmp(doc.charset->name, "latin1_swedish_ci") == 0)
				? DATA_VARCHAR : DATA_VARMYSQL;

	block = psort_info->merge_block;
	zip_size = dict_table_zip_size(table);

	doc_item = UT_LIST_GET_FIRST(psort_info->fts_doc_list);

	if (doc_item) {
		prev_doc_item = doc_item;
	}

	t_ctx.cached_stopword = table->fts->cache->stopword_info.cached_stopword;
	processed = TRUE;
loop:
	while (doc_item) {
		dfield_t*	dfield = doc_item->field;

		last_doc_id = doc_item->doc_id;

		if (!(dfield->data)
		    || dfield_get_len(dfield) == UNIV_SQL_NULL) {
			num_doc_processed++;
			doc_item = UT_LIST_GET_NEXT(doc_list, doc_item);

			/* Always remember the last doc_item we processed */
			if (doc_item) {
				prev_doc_item = doc_item;
			}
			continue;
		}

		/* If finish processing the last item, update "doc" with
		strings in the doc_item, otherwise continue processing last
		item */
		if (processed) {
			byte*		data;
			ulint		data_len;

			dfield = doc_item->field;
			data = static_cast<byte*>(dfield_get_data(dfield));
			data_len = dfield_get_len(dfield);

			if (dfield_is_ext(dfield)) {
				doc.text.f_str =
					btr_copy_externally_stored_field(
						&doc.text.f_len, data,
						zip_size, data_len, blob_heap);
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
			buf, doc_item->doc_id, &doc,
			&word_dtype,
			merge_file, psort_info->psort_common->opt_doc_id_size,
			&t_ctx);

		/* Current sort buffer full, need to recycle */
		if (!processed) {
			ut_ad(t_ctx.processed_len < doc.text.f_len);
			ut_ad(t_ctx.rows_added[t_ctx.buf_used]);
			break;
		}

		num_doc_processed++;

		if (fts_enable_diag_print && num_doc_processed % 10000 == 1) {
			fprintf(stderr, "number of doc processed %d\n",
				(int) num_doc_processed);
#ifdef FTS_INTERNAL_DIAG_PRINT
			for (i = 0; i < FTS_NUM_AUX_INDEX; i++) {
				fprintf(stderr, "ID %d, partition %d, word "
					"%d\n",(int) psort_info->psort_id,
					(int) i, (int) mycount[i]);
			}
#endif
		}

		mem_heap_empty(blob_heap);

		if (doc_item->field->data) {
			ut_free(doc_item->field->data);
			doc_item->field->data = NULL;
		}

		doc_item = UT_LIST_GET_NEXT(doc_list, doc_item);

		/* Always remember the last doc_item we processed */
		if (doc_item) {
			prev_doc_item = doc_item;
			if (last_doc_id != doc_item->doc_id) {
				t_ctx.init_pos = 0;
			}
		}
	}

	/* If we run out of current sort buffer, need to sort
	and flush the sort buffer to disk */
	if (t_ctx.rows_added[t_ctx.buf_used] && !processed) {
		row_merge_buf_sort(buf[t_ctx.buf_used], NULL);
		row_merge_buf_write(buf[t_ctx.buf_used],
				    merge_file[t_ctx.buf_used],
				    block[t_ctx.buf_used]);
		row_merge_write(merge_file[t_ctx.buf_used]->fd,
				merge_file[t_ctx.buf_used]->offset++,
				block[t_ctx.buf_used]);
		UNIV_MEM_INVALID(block[t_ctx.buf_used][0], srv_sort_buf_size);
		buf[t_ctx.buf_used] = row_merge_buf_empty(buf[t_ctx.buf_used]);
		mycount[t_ctx.buf_used] += t_ctx.rows_added[t_ctx.buf_used];
		t_ctx.rows_added[t_ctx.buf_used] = 0;

		ut_a(doc_item);
		goto loop;
	}

	/* Parent done scanning, and if finish processing all the docs, exit */
	if (psort_info->state == FTS_PARENT_COMPLETE) {
	    	if (num_doc_processed >= UT_LIST_GET_LEN(
			psort_info->fts_doc_list)) {
			goto exit;
		} else if (retried > 10000) {
			ut_ad(!doc_item);
			/* retied too many times and cannot get new record */
			fprintf(stderr, "InnoDB: FTS parallel sort processed "
					"%lu records, the sort queue has "
					"%lu records. But sort cannot get "
					"the next records", num_doc_processed,
					UT_LIST_GET_LEN(
						psort_info->fts_doc_list));
			goto exit;
		}
	}

	if (doc_item) {
		doc_item = UT_LIST_GET_NEXT(doc_list, doc_item);
	} else if (prev_doc_item) {
		os_thread_yield();
		doc_item = UT_LIST_GET_NEXT(doc_list, prev_doc_item);
	} else {
		os_thread_yield();
		doc_item = UT_LIST_GET_FIRST(psort_info->fts_doc_list);
	}

	if (doc_item) {
		prev_doc_item = doc_item;

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
			row_merge_buf_write(
				buf[i], merge_file[i], block[i]);

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
				row_merge_write(merge_file[i]->fd,
						merge_file[i]->offset++,
						block[i]);

				UNIV_MEM_INVALID(block[i][0],
						 srv_sort_buf_size);
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

		tmpfd[i] = row_merge_file_create_low();
		if (tmpfd[i] < 0) {
			goto func_exit;
		}

		row_merge_sort(psort_info->psort_common->trx,
			       psort_info->psort_common->dup,
			       merge_file[i], block[i], &tmpfd[i]);
		total_rec += merge_file[i]->n_rec;
		close(tmpfd[i]);
	}

func_exit:
	if (fts_enable_diag_print) {
		DEBUG_FTS_SORT_PRINT("  InnoDB_FTS: complete merge sort\n");
	}

	mem_heap_free(blob_heap);

	psort_info->child_status = FTS_CHILD_COMPLETE;
	os_event_set(psort_info->psort_common->sort_event);
	psort_info->child_status = FTS_CHILD_EXITING;

#ifdef __WIN__
	CloseHandle(psort_info->thread_hdl);
#endif /*__WIN__ */

	os_thread_exit(NULL);

	OS_THREAD_DUMMY_RETURN;
}

/*********************************************************************//**
Start the parallel tokenization and parallel merge sort */
UNIV_INTERN
void
row_fts_start_psort(
/*================*/
	fts_psort_t*	psort_info)	/*!< parallel sort structure */
{
	ulint		i = 0;
	os_thread_id_t	thd_id;

	for (i = 0; i < fts_sort_pll_degree; i++) {
		psort_info[i].psort_id = i;
		psort_info[i].thread_hdl = os_thread_create(
			fts_parallel_tokenization,
			(void*) &psort_info[i], &thd_id);
	}
}

/*********************************************************************//**
Function performs the merge and insertion of the sorted records.
@return OS_THREAD_DUMMY_RETURN */
UNIV_INTERN
os_thread_ret_t
fts_parallel_merge(
/*===============*/
	void*		arg)		/*!< in: parallel merge info */
{
	fts_psort_t*	psort_info = (fts_psort_t*) arg;
	ulint		id;

	ut_ad(psort_info);

	id = psort_info->psort_id;

	row_fts_merge_insert(psort_info->psort_common->dup->index,
			     psort_info->psort_common->new_table,
			     psort_info->psort_common->all_info, id);

	psort_info->child_status = FTS_CHILD_COMPLETE;
	os_event_set(psort_info->psort_common->merge_event);
	psort_info->child_status = FTS_CHILD_EXITING;

#ifdef __WIN__
	CloseHandle(psort_info->thread_hdl);
#endif /*__WIN__ */

	os_thread_exit(NULL);

	OS_THREAD_DUMMY_RETURN;
}

/*********************************************************************//**
Kick off the parallel merge and insert thread */
UNIV_INTERN
void
row_fts_start_parallel_merge(
/*=========================*/
	fts_psort_t*	merge_info)	/*!< in: parallel sort info */
{
	int		i = 0;
	os_thread_id_t	thd_id;

	/* Kick off merge/insert threads */
	for (i = 0; i <  FTS_NUM_AUX_INDEX; i++) {
		merge_info[i].psort_id = i;
		merge_info[i].child_status = 0;

		merge_info[i].thread_hdl = os_thread_create(
			fts_parallel_merge, (void*) &merge_info[i], &thd_id);
	}
}

/********************************************************************//**
Insert processed FTS data to auxillary index tables.
@return	DB_SUCCESS if insertion runs fine */
static __attribute__((nonnull))
dberr_t
row_merge_write_fts_word(
/*=====================*/
	trx_t*		trx,		/*!< in: transaction */
	que_t**		ins_graph,	/*!< in: Insert query graphs */
	fts_tokenizer_word_t* word,	/*!< in: sorted and tokenized
					word */
	fts_table_t*	fts_table,	/*!< in: fts aux table instance */
	CHARSET_INFO*	charset)	/*!< in: charset */
{
	ulint	selected;
	dberr_t	ret = DB_SUCCESS;

	selected = fts_select_index(
		charset, word->text.f_str, word->text.f_len);
	fts_table->suffix = fts_get_suffix(selected);

	/* Pop out each fts_node in word->nodes write them to auxiliary table */
	while (ib_vector_size(word->nodes) > 0) {
		dberr_t		error;
		fts_node_t*	fts_node;

		fts_node = static_cast<fts_node_t*>(ib_vector_pop(word->nodes));

		error = fts_write_node(
			trx, &ins_graph[selected], fts_table, &word->text,
			fts_node);

		if (error != DB_SUCCESS) {
			fprintf(stderr, "InnoDB: failed to write"
				" word %s to FTS auxiliary index"
				" table, error (%s) \n",
				word->text.f_str, ut_strerr(error));
			ret = error;
		}

		ut_free(fts_node->ilist);
		fts_node->ilist = NULL;
	}

	return(ret);
}

/*********************************************************************//**
Read sorted FTS data files and insert data tuples to auxillary tables.
@return	DB_SUCCESS or error number */
UNIV_INTERN
void
row_fts_insert_tuple(
/*=================*/
	fts_psort_insert_t*
			ins_ctx,	/*!< in: insert context */
	fts_tokenizer_word_t* word,	/*!< in: last processed
					tokenized word */
	ib_vector_t*	positions,	/*!< in: word position */
	doc_id_t*	in_doc_id,	/*!< in: last item doc id */
	dtuple_t*	dtuple)		/*!< in: entry to insert */
{
	fts_node_t*	fts_node = NULL;
	dfield_t*	dfield;
	doc_id_t	doc_id;
	ulint		position;
	fts_string_t	token_word;
	ulint		i;

	/* Get fts_node for the FTS auxillary INDEX table */
	if (ib_vector_size(word->nodes) > 0) {
		fts_node = static_cast<fts_node_t*>(
			ib_vector_last(word->nodes));
	}

	if (fts_node == NULL
	    || fts_node->ilist_size > FTS_ILIST_MAX_SIZE) {

		fts_node = static_cast<fts_node_t*>(
			ib_vector_push(word->nodes, NULL));

		memset(fts_node, 0x0, sizeof(*fts_node));
	}

	/* If dtuple == NULL, this is the last word to be processed */
	if (!dtuple) {
		if (fts_node && ib_vector_size(positions) > 0) {
			fts_cache_node_add_positions(
				NULL, fts_node, *in_doc_id,
				positions);

			/* Write out the current word */
			row_merge_write_fts_word(ins_ctx->trx,
						 ins_ctx->ins_graph, word,
						 &ins_ctx->fts_table,
						 ins_ctx->charset);

		}

		return;
	}

	/* Get the first field for the tokenized word */
	dfield = dtuple_get_nth_field(dtuple, 0);

	token_word.f_n_char = 0;
	token_word.f_len = dfield->len;
	token_word.f_str = static_cast<byte*>(dfield_get_data(dfield));

	if (!word->text.f_str) {
		fts_utf8_string_dup(&word->text, &token_word, ins_ctx->heap);
	}

	/* compare to the last word, to see if they are the same
	word */
	if (innobase_fts_text_cmp(ins_ctx->charset,
				  &word->text, &token_word) != 0) {
		ulint	num_item;

		/* Getting a new word, flush the last position info
		for the currnt word in fts_node */
		if (ib_vector_size(positions) > 0) {
			fts_cache_node_add_positions(
				NULL, fts_node, *in_doc_id, positions);
		}

		/* Write out the current word */
		row_merge_write_fts_word(ins_ctx->trx, ins_ctx->ins_graph,
					 word, &ins_ctx->fts_table,
					 ins_ctx->charset);

		/* Copy the new word */
		fts_utf8_string_dup(&word->text, &token_word, ins_ctx->heap);

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
		doc_id = fts_read_doc_id(
			static_cast<byte*>(dfield_get_data(dfield)));
	} else {
		doc_id = (doc_id_t) mach_read_from_4(
			static_cast<byte*>(dfield_get_data(dfield)));
	}

	/* Get the word's position info */
	dfield = dtuple_get_nth_field(dtuple, 2);
	position = mach_read_from_4(static_cast<byte*>(dfield_get_data(dfield)));

	/* If this is the same word as the last word, and they
	have the same Doc ID, we just need to add its position
	info. Otherwise, we will flush position info to the
	fts_node and initiate a new position vector  */
	if (!(*in_doc_id) || *in_doc_id == doc_id) {
		ib_vector_push(positions, &position);
	} else {
		ulint	num_pos = ib_vector_size(positions);

		fts_cache_node_add_positions(NULL, fts_node,
					     *in_doc_id, positions);
		for (i = 0; i < num_pos; i++) {
			ib_vector_pop(positions);
		}
		ib_vector_push(positions, &position);
	}

	/* record the current Doc ID */
	*in_doc_id = doc_id;
}

/*********************************************************************//**
Propagate a newly added record up one level in the selection tree
@return parent where this value propagated to */
static
int
row_fts_sel_tree_propagate(
/*=======================*/
	int		propogated,	/*<! in: tree node propagated */
	int*		sel_tree,	/*<! in: selection tree */
	const mrec_t**	mrec,		/*<! in: sort record */
	ulint**		offsets,	/*<! in: record offsets */
	dict_index_t*	index)		/*<! in/out: FTS index */
{
	ulint	parent;
	int	child_left;
	int	child_right;
	int	selected;

	/* Find which parent this value will be propagated to */
	parent = (propogated - 1) / 2;

	/* Find out which value is smaller, and to propagate */
	child_left = sel_tree[parent * 2 + 1];
	child_right = sel_tree[parent * 2 + 2];

	if (child_left == -1 || mrec[child_left] == NULL) {
		if (child_right == -1
		    || mrec[child_right] == NULL) {
			selected = -1;
		} else {
			selected = child_right ;
		}
	} else if (child_right == -1
		   || mrec[child_right] == NULL) {
		selected = child_left;
	} else if (cmp_rec_rec_simple(mrec[child_left], mrec[child_right],
				      offsets[child_left],
				      offsets[child_right],
				      index, NULL) < 0) {
		selected = child_left;
	} else {
		selected = child_right;
	}

	sel_tree[parent] = selected;

	return(parent);
}

/*********************************************************************//**
Readjust selection tree after popping the root and read a new value
@return the new root */
static
int
row_fts_sel_tree_update(
/*====================*/
	int*		sel_tree,	/*<! in/out: selection tree */
	ulint		propagated,	/*<! in: node to propagate up */
	ulint		height,		/*<! in: tree height */
	const mrec_t**	mrec,		/*<! in: sort record */
	ulint**		offsets,	/*<! in: record offsets */
	dict_index_t*	index)		/*<! in: index dictionary */
{
	ulint	i;

	for (i = 1; i <= height; i++) {
		propagated = row_fts_sel_tree_propagate(
			propagated, sel_tree, mrec, offsets, index);
	}

	return(sel_tree[0]);
}

/*********************************************************************//**
Build selection tree at a specified level */
static
void
row_fts_build_sel_tree_level(
/*=========================*/
	int*		sel_tree,	/*<! in/out: selection tree */
	ulint		level,		/*<! in: selection tree level */
	const mrec_t**	mrec,		/*<! in: sort record */
	ulint**		offsets,	/*<! in: record offsets */
	dict_index_t*	index)		/*<! in: index dictionary */
{
	ulint	start;
	int	child_left;
	int	child_right;
	ulint	i;
	ulint	num_item;

	start = (1 << level) - 1;
	num_item = (1 << level);

	for (i = 0; i < num_item;  i++) {
		child_left = sel_tree[(start + i) * 2 + 1];
		child_right = sel_tree[(start + i) * 2 + 2];

		if (child_left == -1) {
			if (child_right == -1) {
				sel_tree[start + i] = -1;
			} else {
				sel_tree[start + i] =  child_right;
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
		int cmp = cmp_rec_rec_simple(
			mrec[child_left], mrec[child_right],
			offsets[child_left], offsets[child_right],
			index, NULL);

		sel_tree[start + i] = cmp < 0 ? child_left : child_right;
	}
}

/*********************************************************************//**
Build a selection tree for merge. The selection tree is a binary tree
and should have fts_sort_pll_degree / 2 levels. With root as level 0
@return number of tree levels */
static
ulint
row_fts_build_sel_tree(
/*===================*/
	int*		sel_tree,	/*<! in/out: selection tree */
	const mrec_t**	mrec,		/*<! in: sort record */
	ulint**		offsets,	/*<! in: record offsets */
	dict_index_t*	index)		/*<! in: index dictionary */
{
	ulint	treelevel = 1;
	ulint	num = 2;
	int	i = 0;
	ulint	start;

	/* No need to build selection tree if we only have two merge threads */
	if (fts_sort_pll_degree <= 2) {
		return(0);
	}

	while (num < fts_sort_pll_degree) {
		num = num << 1;
		treelevel++;
	}

	start = (1 << treelevel) - 1;

	for (i = 0; i < (int) fts_sort_pll_degree; i++) {
		sel_tree[i + start] = i;
	}

	for (i = treelevel - 1; i >=0; i--) {
		row_fts_build_sel_tree_level(sel_tree, i, mrec, offsets, index);
	}

	return(treelevel);
}

/*********************************************************************//**
Read sorted file containing index data tuples and insert these data
tuples to the index
@return	DB_SUCCESS or error number */
UNIV_INTERN
dberr_t
row_fts_merge_insert(
/*=================*/
	dict_index_t*		index,	/*!< in: index */
	dict_table_t*		table,	/*!< in: new table */
	fts_psort_t*		psort_info, /*!< parallel sort info */
	ulint			id)	/* !< in: which auxiliary table's data
					to insert to */
{
	const byte**		b;
	mem_heap_t*		tuple_heap;
	mem_heap_t*		heap;
	dberr_t			error = DB_SUCCESS;
	ulint*			foffs;
	ulint**			offsets;
	fts_tokenizer_word_t	new_word;
	ib_vector_t*		positions;
	doc_id_t		last_doc_id;
	ib_alloc_t*		heap_alloc;
	ulint			n_bytes;
	ulint			i;
	mrec_buf_t**		buf;
	int*			fd;
	byte**			block;
	const mrec_t**		mrec;
	ulint			count = 0;
	int*			sel_tree;
	ulint			height;
	ulint			start;
	fts_psort_insert_t	ins_ctx;
	ulint			count_diag = 0;

	ut_ad(index);
	ut_ad(table);

	/* We use the insert query graph as the dummy graph
	needed in the row module call */

	ins_ctx.trx = trx_allocate_for_background();

	ins_ctx.trx->op_info = "inserting index entries";

	ins_ctx.opt_doc_id_size = psort_info[0].psort_common->opt_doc_id_size;

	heap = mem_heap_create(500 + sizeof(mrec_buf_t));

	b = (const byte**) mem_heap_alloc(
		heap, sizeof (*b) * fts_sort_pll_degree);
	foffs = (ulint*) mem_heap_alloc(
		heap, sizeof(*foffs) * fts_sort_pll_degree);
	offsets = (ulint**) mem_heap_alloc(
		heap, sizeof(*offsets) * fts_sort_pll_degree);
	buf = (mrec_buf_t**) mem_heap_alloc(
		heap, sizeof(*buf) * fts_sort_pll_degree);
	fd = (int*) mem_heap_alloc(heap, sizeof(*fd) * fts_sort_pll_degree);
	block = (byte**) mem_heap_alloc(
		heap, sizeof(*block) * fts_sort_pll_degree);
	mrec = (const mrec_t**) mem_heap_alloc(
		heap, sizeof(*mrec) * fts_sort_pll_degree);
	sel_tree = (int*) mem_heap_alloc(
		heap, sizeof(*sel_tree) * (fts_sort_pll_degree * 2));

	tuple_heap = mem_heap_create(1000);

	ins_ctx.charset = fts_index_get_charset(index);
	ins_ctx.heap = heap;

	for (i = 0; i < fts_sort_pll_degree; i++) {
		ulint	num;

		num = 1 + REC_OFFS_HEADER_SIZE
			+ dict_index_get_n_fields(index);
		offsets[i] = static_cast<ulint*>(mem_heap_zalloc(
			heap, num * sizeof *offsets[i]));
		offsets[i][0] = num;
		offsets[i][1] = dict_index_get_n_fields(index);
		block[i] = psort_info[i].merge_block[id];
		b[i] = psort_info[i].merge_block[id];
		fd[i] = psort_info[i].merge_file[id]->fd;
		foffs[i] = 0;

		buf[i] = static_cast<unsigned char (*)[16384]>(
			mem_heap_alloc(heap, sizeof *buf[i]));
		count_diag += (int) psort_info[i].merge_file[id]->n_rec;
	}

	if (fts_enable_diag_print) {
		ut_print_timestamp(stderr);
		fprintf(stderr, "  InnoDB_FTS: to inserted %lu records\n",
			(ulong) count_diag);
	}

	/* Initialize related variables if creating FTS indexes */
	heap_alloc = ib_heap_allocator_create(heap);

	memset(&new_word, 0, sizeof(new_word));

	new_word.nodes = ib_vector_create(heap_alloc, sizeof(fts_node_t), 4);
	positions = ib_vector_create(heap_alloc, sizeof(ulint), 32);
	last_doc_id = 0;

	/* Allocate insert query graphs for FTS auxillary
	Index Table, note we have FTS_NUM_AUX_INDEX such index tables */
	n_bytes = sizeof(que_t*) * (FTS_NUM_AUX_INDEX + 1);
	ins_ctx.ins_graph = static_cast<que_t**>(mem_heap_alloc(heap, n_bytes));
	memset(ins_ctx.ins_graph, 0x0, n_bytes);

	ins_ctx.fts_table.type = FTS_INDEX_TABLE;
	ins_ctx.fts_table.index_id = index->id;
	ins_ctx.fts_table.table_id = table->id;
	ins_ctx.fts_table.parent = index->table->name;
	ins_ctx.fts_table.table = NULL;

	for (i = 0; i < fts_sort_pll_degree; i++) {
		if (psort_info[i].merge_file[id]->n_rec == 0) {
			/* No Rows to read */
			mrec[i] = b[i] = NULL;
		} else {
			/* Read from temp file only if it has been
			written to. Otherwise, block memory holds
			all the sorted records */
			if (psort_info[i].merge_file[id]->offset > 0
			    && (!row_merge_read(
					fd[i], foffs[i],
					(row_merge_block_t*) block[i]))) {
				error = DB_CORRUPTION;
				goto exit;
			}

			ROW_MERGE_READ_GET_NEXT(i);
		}
	}

	height = row_fts_build_sel_tree(sel_tree, (const mrec_t **) mrec,
					offsets, index);

	start = (1 << height) - 1;

	/* Fetch sorted records from sort buffer and insert them into
	corresponding FTS index auxiliary tables */
	for (;;) {
		dtuple_t*	dtuple;
		ulint		n_ext;
		int		min_rec = 0;

		if (fts_sort_pll_degree <= 2) {
			while (!mrec[min_rec]) {
				min_rec++;

				if (min_rec >= (int) fts_sort_pll_degree) {
					row_fts_insert_tuple(
						&ins_ctx, &new_word,
						positions, &last_doc_id,
						NULL);

					goto exit;
				}
			}

			for (i = min_rec + 1; i < fts_sort_pll_degree; i++) {
				if (!mrec[i]) {
					continue;
				}

				if (cmp_rec_rec_simple(
					    mrec[i], mrec[min_rec],
					    offsets[i], offsets[min_rec],
					    index, NULL) < 0) {
					min_rec = i;
				}
			}
		} else {
			min_rec = sel_tree[0];

			if (min_rec ==  -1) {
				row_fts_insert_tuple(
					&ins_ctx, &new_word,
					positions, &last_doc_id,
					NULL);

				goto exit;
			}
		}

		dtuple = row_rec_to_index_entry_low(
			mrec[min_rec], index, offsets[min_rec], &n_ext,
			tuple_heap);

		row_fts_insert_tuple(
			&ins_ctx, &new_word, positions,
			&last_doc_id, dtuple);


		ROW_MERGE_READ_GET_NEXT(min_rec);

		if (fts_sort_pll_degree > 2) {
			if (!mrec[min_rec]) {
				sel_tree[start + min_rec] = -1;
			}

			row_fts_sel_tree_update(sel_tree, start + min_rec,
						height, mrec,
						offsets, index);
		}

		count++;

		mem_heap_empty(tuple_heap);
	}

exit:
	fts_sql_commit(ins_ctx.trx);

	ins_ctx.trx->op_info = "";

	mem_heap_free(tuple_heap);

	for (i = 0; i < FTS_NUM_AUX_INDEX; i++) {
		if (ins_ctx.ins_graph[i]) {
			fts_que_graph_free(ins_ctx.ins_graph[i]);
		}
	}

	trx_free_for_background(ins_ctx.trx);

	mem_heap_free(heap);

	if (fts_enable_diag_print) {
		ut_print_timestamp(stderr);
		fprintf(stderr, "  InnoDB_FTS: inserted %lu records\n",
			(ulong) count);
	}

	return(error);
}
