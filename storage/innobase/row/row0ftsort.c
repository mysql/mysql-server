/*****************************************************************************

Copyright (c) 2010, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

/**************************************************//**
@file row/row0ftsort.c
Create Full Text Index with (parallel) merge sort

Created 10/13/2010 Jimmy Yang
*******************************************************/

#include "row0merge.h"
#include "pars0pars.h"
#include "row0ftsort.h"
#include "row0merge.h"
#include "row0row.h"

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
	const dict_table_t*	table)	/*!< in: table that FTS index
					is being created on */
{
	dict_index_t*   new_index;
	dict_field_t*   field;

	new_index = dict_mem_index_create(index->table->name, "tmp_idx",
					  0, DICT_FTS, 3);

	new_index->id = index->id;
	new_index->table = (dict_table_t*)table;
	new_index->n_uniq = FTS_NUM_FIELDS_SORT;

	/* The first field is on the Tokenized Word */
	field = dict_index_get_nth_field(new_index, 0);
	field->name = NULL;
	field->prefix_len = 0;
	field->col = mem_heap_alloc(index->heap, sizeof(dict_col_t));
	field->col->len = FTS_MAX_UTF8_WORD_LEN;
	field->col->mtype = DATA_VARCHAR;
	field->col->prtype = DATA_NOT_NULL;
	field->fixed_len = 0;

	/* The second field is on the Doc ID. To reduce the
	sort record size, we use 4 bytes field instead of Doc ID's
	8 byte fields. */
	field = dict_index_get_nth_field(new_index, 1);
	field->name = NULL;
	field->prefix_len = 0;
	field->col = mem_heap_alloc(index->heap, sizeof(dict_col_t));
	field->col->mtype = DATA_INT;
	field->col->len = FTS_DOC_ID_LEN;
	field->fixed_len = FTS_DOC_ID_LEN;
	field->col->prtype = DATA_NOT_NULL;

	/* The third field is on the word's Position in original doc */
	field = dict_index_get_nth_field(new_index, 2);
	field->name = NULL;
	field->prefix_len = 0;
	field->col = mem_heap_alloc(index->heap, sizeof(dict_col_t));
	field->col->mtype = DATA_INT;
	field->col->len = 4 ;
	field->fixed_len = 4;
	field->col->prtype = DATA_NOT_NULL;

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
	struct TABLE*		table,	/*!< in: MySQL table object */
	const dict_table_t*	new_table,/*!< in: table where indexes are
					created */
	dict_index_t*		index,	/*!< in: FTS index to be created */
	fts_psort_info_t**	psort,	/*!< out: parallel sort info to be
					instantiated */
	fts_psort_info_t**	merge)	/*!< out: parallel merge info
					to be instantiated */
{
	ulint			i;
	ulint			j;
	fts_psort_common_t*	common_info = NULL;
	ulint			block_size = 3 * sizeof(row_merge_block_t);
	fts_psort_info_t*	psort_info = NULL;
	fts_psort_info_t*	merge_info;
	os_event_t		sort_event;

	*psort = psort_info = mem_alloc(
		 FTS_PARALLEL_DEGREE * sizeof *psort_info);

	sort_event = os_event_create(NULL);

	common_info = mem_alloc(sizeof *common_info);

	common_info->table = table;
	common_info->new_table = (dict_table_t*)new_table;
	common_info->trx = trx;
	common_info->sort_index = index;
	common_info->all_info = psort_info;
	common_info->sort_event = sort_event;

	if (!psort_info || !common_info) {
		return FALSE;
	}

	for (j = 0; j < FTS_PARALLEL_DEGREE; j++) {

		UT_LIST_INIT(psort_info[j].fts_doc_list);

		for (i = 0; i < FTS_NUM_AUX_INDEX; i++) {

			psort_info[j].merge_file[i] = mem_alloc(
				sizeof(merge_file_t));

			if (!psort_info[j].merge_file[i]) {
				return FALSE;
			}

			psort_info[j].merge_buf[i] = row_merge_buf_create(
				index);

			row_merge_file_create(psort_info[j].merge_file[i]);

			psort_info[j].merge_block[i] = os_mem_alloc_large(
				&block_size);

			if (!psort_info[j].merge_block[i]) {
				return FALSE;
			}
		}

		psort_info[j].child_status = 0;
		psort_info[j].state = 0;
		psort_info[j].psort_common = common_info;
	}

	/* Initialize merge_info structures parallel merge and insert
	into auxiliary FTS tables (FTS_INDEX_TABLE) */
	*merge = merge_info = mem_alloc(FTS_NUM_AUX_INDEX
					* sizeof *merge_info);

	for (j = 0; j < FTS_NUM_AUX_INDEX; j++) {

		merge_info[j].child_status = 0;
		merge_info[j].state = 0;
		merge_info[j].psort_common = common_info;
	}

	return(TRUE);
}
/*********************************************************************//**
Clean up and deallocate FTS parallel sort structures, and close
merge sort files  */
UNIV_INTERN
void
row_fts_psort_info_destroy(
/*=======================*/
	fts_psort_info_t*	psort_info,	/*!< parallel sort info */
	fts_psort_info_t*	merge_info)	/*!< parallel merge info */
{
	ulint	block_size = 3 * sizeof(row_merge_block_t);
	ulint	i;
	ulint	j;

	if (psort_info) {
		for (j = 0; j < FTS_PARALLEL_DEGREE; j++) {
			for (i = 0; i < FTS_NUM_AUX_INDEX; i++) {
				row_merge_file_destroy(
					psort_info[j].merge_file[i]);
				os_mem_free_large(
					psort_info[j].merge_block[i],
					block_size);
			}
		}

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
	fts_psort_info_t*	psort_info) /*!< in: parallel sort info */
{
	ulint	j;
	ulint	i;

	if (!psort_info) {
		return;
	}

	for (j = 0; j < FTS_PARALLEL_DEGREE; j++) {
		for (i = 0; i < FTS_NUM_AUX_INDEX; i++) {
			row_merge_buf_free(psort_info[j].merge_buf[i]);
		}
	}
}
/*********************************************************************//**
Tokenize incoming text data and add to the sort buffer.
FIXME: Consider running out of buffer in the middle of string parsing.
@return	number of rows added, 0 if out of space */
UNIV_INTERN
ulint
row_merge_fts_doc_tokenize(
/*=======================*/
	row_merge_buf_t** sort_buf,	/*!< in/out: sort buffer */
	const dfield_t*	dfield,		/*!< in: Field contain doc to be
					parsed */
	doc_id_t	doc_id,		/*!< in: doc id for this document */
	ulint*		init_pos,	/*!< in/out: doc start position */
	ulint*		buf_used,	/*!< in/out: sort buffer used */
	ulint*		rows_added,	/*!< in/out: num rows added */
	merge_file_t**	merge_file)	/*!< in/out: merge file to fill */
{
	ulint		i;
	ulint		inc;
	fts_string_t	str;
	byte		str_buf[FTS_MAX_UTF8_WORD_LEN + 1];
	dfield_t*	field;
	ulint		data_size[FTS_NUM_AUX_INDEX];
	ulint		len;
	fts_doc_t	doc;
	ulint		n_tuple[FTS_NUM_AUX_INDEX];
	row_merge_buf_t* buf;

	doc.tokens = 0;

	doc.text.utf8 = dfield_get_data(dfield);

	doc.text.len = dfield_get_len(dfield);

	str.utf8 = str_buf;

	if (!FTS_PLL_ENABLED) {
		buf = sort_buf[0];
		n_tuple[0] = 0;
		data_size[0] = 0;
	} else {
		for (i = 0; i < FTS_NUM_AUX_INDEX; i++) {
			n_tuple[i] = 0;
			data_size[i] = 0;
		}
	}

	/* Tokenize the data and add each word string, its corresponding
	doc id and position to sort buffer */
	for (i = 0; i < doc.text.len; i += inc) {
		doc_id_t	write_doc_id;
		ib_uint32_t	position;
		ulint           offset = 0;
		ulint		idx = 0;

		inc = fts_get_next_token(
			doc.text.utf8 + i,
			doc.text.utf8 + doc.text.len, &str, &offset);

		ut_a(inc > 0);

		/* Ignore string len smaller thane FTS_MIN_TOKEN_SIZE */
		if (str.len < FTS_MIN_TOKEN_SIZE) {
			continue;
		}

		/* There are FTS_NUM_AUX_INDEX auxiliary tables, find
		out which sort buffer to put this word record in */
		if (FTS_PLL_ENABLED) {
			*buf_used = fts_select_index(*str.utf8);

			buf = sort_buf[*buf_used];

			ut_a(*buf_used < FTS_NUM_AUX_INDEX);
			idx = *buf_used;
		}

		buf->tuples[buf->n_tuples + n_tuple[idx]] =
		field = mem_heap_alloc(
			buf->heap, FTS_NUM_FIELDS_SORT * sizeof *field);

		ut_a(field);

		/* The first field is the tokenized word */
		dfield_set_data(field, str.utf8, str.len);
		len = dfield_get_len(field);
		field->type.mtype = DATA_VARCHAR;
		field->type.prtype = DATA_NOT_NULL;
		field->type.len = FTS_MAX_UTF8_WORD_LEN;
		field->type.mbminmaxlen = DATA_MBMINMAXLEN(1, 1);
		data_size[idx] += len;
		dfield_dup(field, buf->heap);
		field++;

		/* The second field is the Doc ID */
		fts_write_doc_id((byte*) &write_doc_id, doc_id);
		dfield_set_data(field, &write_doc_id, sizeof(write_doc_id));
		len = dfield_get_len(field);
		ut_a(len == FTS_DOC_ID_LEN);
		field->type.mtype = DATA_INT;
		field->type.prtype = DATA_NOT_NULL;
		field->type.len = len;
		data_size[idx] += len;
		dfield_dup(field, buf->heap);
		field++;

		/* The second field is the position */
		mach_write_to_4((byte*) &position,
				(i + offset + (*init_pos)));
		dfield_set_data(field, &position, 4);
		len = dfield_get_len(field);
		field->type.mtype = DATA_INT;
		field->type.prtype = DATA_NOT_NULL;
		field->type.len = len;
		data_size[idx] += len;
		dfield_dup(field, buf->heap);

		/* One variable length column, word with its lenght less than
		FTS_MAX_UTF8_WORD_LEN (128) bytes, add one extra size
		and one extra byte */
		data_size[idx] += 2;

		/* Reserve one byte for the end marker of row_merge_block_t. */
		if (buf->total_size + data_size[idx]
		    >= sizeof(row_merge_block_t) - 1) {
			return(FALSE);
		}

		/* Increment the number of tuples */
		n_tuple[idx]++;
	}

	ut_ad(data_size < sizeof(row_merge_block_t));

	if (FTS_PLL_ENABLED) {

		for (i = 0; i <  FTS_NUM_AUX_INDEX; i++) {
			/* Total data length */
			sort_buf[i]->total_size += data_size[i];

			sort_buf[i]->n_tuples += n_tuple[i];

			merge_file[i]->n_rec += n_tuple[i];
			rows_added[i] += n_tuple[i];
		}
	} else {
		sort_buf[0]->total_size += data_size[0];
		sort_buf[0]->n_tuples += n_tuple[0];
	}

	/* we pad one byte between text accross two fields */
	*init_pos += doc.text.len + 1;

	return(n_tuple[0]);
}
/*********************************************************************//**
Function performs parallel tokenization of the incoming doc strings.
It also perform the initial in memory sort of the parsed records.
@return OS_THREAD_DUMMY_RETURN */
UNIV_INTERN
os_thread_ret_t
fts_parallel_tokenization(
/*======================*/
	void*		arg)	/*!< in: psort_info for the thread */
{
	fts_psort_info_t*	psort_info = (fts_psort_info_t*)arg;
	ulint			id;
	ulint			i;
	fts_doc_item_t*		doc_item = NULL;
	fts_doc_item_t*		prev_doc_item = NULL;
	row_merge_buf_t**	buf;
	ulint			n_row_added = 0;
	merge_file_t**		merge_file;
	row_merge_block_t**	block;
	ulint			init_pos = 0;
	ulint			error;
	int			tmpfd[FTS_NUM_AUX_INDEX];
	ulint			rows_added[FTS_NUM_AUX_INDEX];
	ulint			buf_used = 0;
	ulint			total_rec = 0;
	ulint			num_doc_processed = 0;

	ut_ad(psort_info);

	id = psort_info->psort_id;
	buf = psort_info->merge_buf;
	merge_file = psort_info->merge_file;

	for (i = 0; i < FTS_NUM_AUX_INDEX; i++) {
		rows_added[i] = 0;
	}

	block = psort_info->merge_block;

	doc_item = UT_LIST_GET_FIRST(psort_info->fts_doc_list);
loop:
	while (doc_item) {
		n_row_added = row_merge_fts_doc_tokenize(
					buf, doc_item->field,
					doc_item->doc_id,
					&init_pos, &buf_used, rows_added,
					merge_file);
		num_doc_processed++;
		if (!n_row_added) {
			break;
		}

		doc_item = UT_LIST_GET_NEXT(doc_list, doc_item);
		if (doc_item) {
			prev_doc_item = doc_item;
		}
	}

	if (rows_added[buf_used] && !n_row_added) {
		row_merge_buf_sort(buf[buf_used], NULL);
		row_merge_buf_write(buf[buf_used], merge_file[buf_used],
				    block[buf_used]);
		row_merge_write(merge_file[buf_used]->fd,
				merge_file[buf_used]->offset++,
				block[buf_used]);
		UNIV_MEM_INVALID(block[buf_used][0], sizeof block[buf_used][0]);
		buf[buf_used] = row_merge_buf_empty(buf[buf_used]);
		rows_added[buf_used] = 0;
	}

	/* Parent done scanning, and if we process all the docs, exit */
	if (psort_info->state == FTS_PARENT_COMPLETE
	    && num_doc_processed == UT_LIST_GET_LEN(psort_info->fts_doc_list)) {
		goto exit;
	}

	if (doc_item) {
		doc_item = UT_LIST_GET_NEXT(doc_list, doc_item);
	} else if (prev_doc_item) {
		while (!doc_item) {
			os_thread_yield();
			doc_item = UT_LIST_GET_NEXT(doc_list,
						    prev_doc_item);
		}
	} else {
		doc_item = UT_LIST_GET_FIRST(psort_info->fts_doc_list);
	}

	if (doc_item) {
		 prev_doc_item = doc_item;
	}

	goto loop;

exit:
	for (i = 0; i < FTS_NUM_AUX_INDEX; i++) {
		if (rows_added[i]) {
			row_merge_buf_sort(buf[i], NULL);
			row_merge_buf_write(
				buf[i], (const merge_file_t *)merge_file[i],
				block[i]);
			row_merge_write(merge_file[i]->fd,
					merge_file[i]->offset++, block[i]);

			UNIV_MEM_INVALID(block[i][0], sizeof block[i][0]);
			buf[i] = row_merge_buf_empty(buf[i]);
			rows_added[i] = 0;
		}
	}

	DEBUG_FTS_SORT_PRINT("FTS SORT: start merge sort\n");

	for (i = 0; i < FTS_NUM_AUX_INDEX; i++) {
		tmpfd[i] = innobase_mysql_tmpfile();
		error = row_merge_sort(psort_info->psort_common->trx,
				       psort_info->psort_common->sort_index,
				       merge_file[i], block[i], &tmpfd[i],
				       psort_info->psort_common->table);
		total_rec += merge_file[i]->n_rec;
		close(tmpfd[i]);
	}

	DEBUG_FTS_SORT_PRINT("FTS SORT: complete merge sort\n");
	fprintf(stderr, "done merge sort, num record %lu\n", (ulong) total_rec);

	psort_info->child_status = FTS_CHILD_COMPLETE;
	os_event_set(psort_info->psort_common->sort_event);

	os_thread_exit(NULL);
	OS_THREAD_DUMMY_RETURN;
}
/*********************************************************************//**
Start the parallel tokenization and parallel merge sort */
UNIV_INTERN
void
row_fts_start_psort(
/*================*/
	fts_psort_info_t*	psort_info)
{
	int		i = 0;
	os_thread_id_t	thd_id;

	for (i = 0; i < FTS_PARALLEL_DEGREE; i++) {
		psort_info[i].psort_id = i;
		os_thread_create(fts_parallel_tokenization,
				 (void *)&psort_info[i], &thd_id);
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
	fts_psort_info_t*	psort_merge = (fts_psort_info_t*)arg;
	ulint			id;

	ut_ad(psort_merge);

	id = psort_merge->psort_id;

	row_fts_merge_insert(psort_merge->psort_common->trx,
			     psort_merge->psort_common->sort_index,
			     psort_merge->psort_common->new_table,
			     dict_table_zip_size(
			     psort_merge->psort_common->new_table),
			     psort_merge->psort_common->all_info, id);

	DEBUG_FTS_SORT_PRINT("FTS SORT: complete parallel insert\n");

	psort_merge->child_status = FTS_CHILD_COMPLETE;
	os_event_set(psort_merge->psort_common->sort_event);

	os_thread_exit(NULL);
	OS_THREAD_DUMMY_RETURN;
}
/*********************************************************************//**
Kick off the parallel merge and insert thread */
UNIV_INTERN
void
row_fts_start_parallel_merge(
/*=========================*/
	fts_psort_info_t*	merge_info,	/*!< in: parallel sort info */
	fts_psort_info_t*	psort_info)	/*!< in: parallel merge info */
{
	int		i = 0;
	os_thread_id_t	thd_id;

	/* Kick off merge/insert threads */
	for (i = 0; i <  FTS_NUM_AUX_INDEX; i++) {
		merge_info[i].psort_id = i;
		merge_info[i].child_status = 0;

		os_thread_create(fts_parallel_merge,
				 (void *)&merge_info[i], &thd_id);
	}
}
/********************************************************************//**
Insert processed FTS data to the auxillary tables.
@return	DB_SUCCESS if insertion runs fine */
UNIV_INTERN
ulint
row_merge_write_fts_word(
/*=====================*/
	trx_t*		trx,		/*!< in: transaction */
	que_t**		ins_graph,	/*!< in: Insert query graphs */
	fts_tokenizer_word_t* word,	/*!< in: sorted and tokenized
					word */
	fts_node_t*	fts_node,	/*!< in: fts node for FTS
					INDEX table */
	fts_table_t*	fts_table)	/*!< in: fts aux table instance */
{
	ulint	selected;
	ulint	ret = DB_SUCCESS;	

	selected = fts_select_index(*word->text.utf8);
	fts_table->suffix = fts_get_suffix(selected);

	/* Pop out each fts_node in word->nodes write them to auxiliary table */
	while(ib_vector_size(word->nodes) > 0) {
		ulint	error;	
		fts_node_t* fts_node = ib_vector_pop(word->nodes);

		error = fts_write_node(trx, &ins_graph[selected],
				       fts_table, &word->text,
				       fts_node);

		if (error != DB_SUCCESS) {
			fprintf(stderr, "InnoDB: failed to write"
				" word %s to FTS auxiliary index"
				" table\n", word->text.utf8);
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
	trx_t*		trx,		/*!< in: transaction */
	que_t**		ins_graph,	/*!< in: Insert query graphs */
	dict_index_t*	index,		/*!< in: fts sort index */
	fts_table_t*	fts_table,	/*!< in: fts aux table instance */
	fts_tokenizer_word_t* word,	/*!< in: last processed
					tokenized word */
	ib_vector_t*	positions,	/*!< in: word position */
	doc_id_t*	in_doc_id,	/*!< in: last item doc id */
	dtuple_t*	dtuple,		/*!< in: index entry */
	int*		count)		/*!< in/out: counter recording how many
					records have been inserted */
{
	fts_node_t*	fts_node = NULL;
	mem_heap_t*	heap = index->heap;
	dfield_t*	dfield;
	doc_id_t	doc_id;
	ulint		position;
	fts_string_t	token_word;
	fts_cache_t	cache;
	ulint		i;

	/* Get fts_node for the FTS auxillary INDEX table */
	if (ib_vector_size(word->nodes) > 0) {
		fts_node = ib_vector_last(word->nodes);
	}

	/* If dtuple == NULL, this is the last word to be processed */
	if (!dtuple && fts_node && ib_vector_size(positions) > 0) {
		fts_cache_node_add_positions(
			&cache, fts_node, *in_doc_id, positions);

		/* Write out the current word */
		row_merge_write_fts_word(trx, ins_graph, word,
					 fts_node, fts_table);

		if (count) {
			(*count)++;
		}
		return;
	}

	if (fts_node == NULL
	    || fts_node->ilist_size > FTS_ILIST_MAX_SIZE) {

		fts_node = ib_vector_push(word->nodes, NULL);

		memset(fts_node, 0x0, sizeof(*fts_node));
	}

	/* Get the first field for the tokenized word */
	dfield = dtuple_get_nth_field(dtuple, 0);
	token_word.utf8 = dfield_get_data(dfield);
	token_word.len = dfield->len;

	if (!word->text.utf8) {
		fts_utf8_string_dup(&word->text, &token_word, heap);
	}

	/* compare to the last word, to see if they are the same
	word */
	if (fts_utf8_string_cmp(&word->text, &token_word) != 0) {

		/* Getting a new word, flush the last position info
		for the currnt word in fts_node */
		if (ib_vector_size(positions) > 0) {
			fts_cache_node_add_positions(
				&cache, fts_node, *in_doc_id, positions);
		}

		/* Write out the current word */
		row_merge_write_fts_word(trx, ins_graph, word,
					 fts_node, fts_table);

		if (count) {
			(*count)++;
		}
		/* Copy the new word */
		fts_utf8_string_dup(&word->text, &token_word, heap);

		/* Clean up position queue */
		for (i = 0; i < ib_vector_size(positions); i++) {
			ib_vector_pop(positions);
		}

		/* Reset Doc ID */
		*in_doc_id = 0;
		memset(fts_node, 0x0, sizeof(*fts_node));
	}

	/* Get the word's Doc ID */
	dfield = dtuple_get_nth_field(dtuple, 1);
	doc_id = fts_read_doc_id(dfield_get_data(dfield));

	/* Get the word's position info */
	dfield = dtuple_get_nth_field(dtuple, 2);
	position = mach_read_from_4(dfield_get_data(dfield));

	/* If this is the same word as the last word, and they
	have the same Doc ID, we just need to add its position
	info. Otherwise, we will flush position info to the
	fts_node and initiate a new position vector  */
	if (!(*in_doc_id) || *in_doc_id == doc_id) {
		ib_vector_push(positions, &position);
	} else {
		fts_cache_node_add_positions(&cache, fts_node,
					     *in_doc_id, positions);
		for (i = 0; i < ib_vector_size(positions); i++) {
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
	ulint		level,		/*<! in: selection tree level */
	const mrec_t**	mrec,		/*<! in: sort record */
	ulint**		offsets,	/*<! in: record offsets */
	dict_index_t*	index)		/*<! in/out: FTS index */
{
	ulint	parent;
	int	child_left;
	int	child_right;
	int	selected;
	ibool	null_eq = FALSE;

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
	} else if (row_merge_cmp(mrec[child_left], mrec[child_right],
				 offsets[child_left],
				 offsets[child_right],
				 index, &null_eq) < 0) {
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
			propagated, sel_tree, i, mrec, offsets, index);
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
	ulint	child_left;
	ulint	child_right;
	ibool	null_eq = FALSE;
	ulint	i;
	ulint	num_item;

	start = (1 << level) - 1;
	num_item = (1 << level);

	for (i = 0; i < num_item;  i++) {
		child_left = sel_tree[(start + i) * 2 + 1];
		child_right = sel_tree[(start + i) * 2 + 2];

		/* Deal with NULL child conditions */
		if (!mrec[child_left]) {
			if (!mrec[child_right]) {
				sel_tree[start + i] = -1;
			} else {
				sel_tree[start + i] = child_right;
			}
			return;
		} else if (!mrec[child_right]) {
			sel_tree[start + i] = child_left;
			return;
		}

		/* Select the smaller one to set parent pointer */
		if (row_merge_cmp(mrec[child_left], mrec[child_right],
				  offsets[child_left],
				  offsets[child_right],
				  index, &null_eq) < 0) {
			sel_tree[start + i] = child_left;
		} else {
			sel_tree[start + i] = child_right;
		}
	}
}
/*********************************************************************//**
Build a selection tree for merge. The selection tree is a binary tree
and should have FTS_PARALLEL_DEGREE / 2 levels. With root as level 0
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
	if (FTS_PARALLEL_DEGREE <= 2) {
		return(0);
	}

	while (num < FTS_PARALLEL_DEGREE) {
		num = num << 1;
		treelevel++;
	}

	start = (1 << treelevel) - 1;

	for (i = 0; i < FTS_PARALLEL_DEGREE; i++) {
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
ulint
row_fts_merge_insert(
/*=================*/
	trx_t*			trx,	/*!< in: transaction */
	dict_index_t*		index,	/*!< in: index */
	dict_table_t*		table,	/*!< in: new table */
	ulint			zip_size,/*!< in: compressed page size of
					 the old table, or 0 if uncompressed */
	fts_psort_info_t*	psort_info, /*!< parallel sort info */
	ulint			id)	/* !< in: which auxiliary table's data
					to insert to */
{
	const byte*		b[FTS_PARALLEL_DEGREE];
	mem_heap_t*		tuple_heap;
	mem_heap_t*		graph_heap;
	ulint			error = DB_SUCCESS;
	ulint			foffs[FTS_PARALLEL_DEGREE];
	ulint*			offsets[FTS_PARALLEL_DEGREE];
	fts_tokenizer_word_t	new_word;
	ib_vector_t*		positions;
	doc_id_t		last_doc_id;
	que_t**			ins_graph;
	fts_table_t		fts_table;
	ib_alloc_t*		heap_alloc;
	ulint			n_bytes;
	ulint			i;
	ulint			num;
	mrec_buf_t*		buf[FTS_PARALLEL_DEGREE];
	int			fd[FTS_PARALLEL_DEGREE];
	row_merge_block_t*	block[FTS_PARALLEL_DEGREE];
	const mrec_t*		mrec[FTS_PARALLEL_DEGREE];
	ulint			count = 0;
	int			sel_tree[1 << (FTS_PARALLEL_DEGREE)];
	ulint			height;
	ulint			start;
	int			counta = 0;

	ut_ad(trx);
	ut_ad(index);
	ut_ad(table);

	/* We use the insert query graph as the dummy graph
	needed in the row module call */

	trx->op_info = "inserting index entries";

	graph_heap = mem_heap_create(500 + sizeof(mrec_buf_t));

	tuple_heap = mem_heap_create(1000);

	for (i = 0; i < FTS_PARALLEL_DEGREE; i++) {

		num = 1 + REC_OFFS_HEADER_SIZE
			+ dict_index_get_n_fields(index);
		offsets[i] = mem_heap_alloc(graph_heap,
					    num * sizeof *offsets[i]);
		offsets[i][0] = num;
		offsets[i][1] = dict_index_get_n_fields(index);
		block[i] =psort_info[i].merge_block[id];
		b[i] = *psort_info[i].merge_block[id];
		fd[i] = psort_info[i].merge_file[id]->fd;
		foffs[i] = 0;

		buf[i] = mem_heap_alloc(graph_heap, sizeof *buf[i]);
		counta += psort_info[i].merge_file[id]->n_rec;
	}
	fprintf(stderr, "to inserted %lu record \n", (ulong)counta);
	counta = 0;

	/* Initialize related variables if creating FTS indexes */
	heap_alloc = ib_heap_allocator_create(index->heap);

	memset(&new_word, 0, sizeof(new_word));

	new_word.nodes = ib_vector_create( heap_alloc, sizeof(fts_node_t), 4);
	positions = ib_vector_create(heap_alloc, sizeof(ulint), 32);
	last_doc_id = 0;

	/* Allocate insert query graphs for FTS auxillary
	Index Table, note we have 4 such index tables */
	n_bytes = sizeof(que_t*) * 5;
	ins_graph = mem_heap_alloc(index->heap, n_bytes);
	memset(ins_graph, 0x0, n_bytes);

	fts_table.type = FTS_INDEX_TABLE;
	fts_table.index_id = index->id;
	fts_table.table_id = table->id;
	fts_table.parent = index->table->name;

	for (i = 0; i < FTS_PARALLEL_DEGREE; i++) {
		if (psort_info[i].merge_file[id]->n_rec == 0) {
			/* No Rows to read */
			mrec[i] = b[i] = NULL;
		} else {
			if (!row_merge_read(fd[i], foffs[i], block[i])) {
				error = DB_CORRUPTION;
				goto exit;
			}

			ROW_MERGE_READ_GET_NEXT(i);
		}
	}

	height = row_fts_build_sel_tree(sel_tree, mrec, offsets, index);

	start = (1 << height) - 1;

	for (;;) {
		dtuple_t*	dtuple;
		ulint		n_ext;
		ulint		min_rec = 0;


		if (FTS_PARALLEL_DEGREE <= 2) {
			while (!mrec[min_rec]) {
				min_rec++;

				if (min_rec >= FTS_PARALLEL_DEGREE) {
					goto exit;
				}
			}

			for (i = min_rec + 1; i < FTS_PARALLEL_DEGREE; i++) {
				ibool           null_eq = FALSE;
				if (!mrec[i]) {
					continue;
				}

				if (row_merge_cmp(mrec[i], mrec[min_rec],
						  offsets[i], offsets[min_rec],
						  index, &null_eq) < 0) {
					min_rec = i;
				}
			}
		} else {
			min_rec = sel_tree[0];

			if (min_rec ==  -1) {
				goto exit;
			}
		}

		dtuple = row_rec_to_index_entry_low(
			mrec[min_rec], index, offsets[min_rec], &n_ext,
			tuple_heap);

		row_fts_insert_tuple(
			trx, ins_graph, index, &fts_table, &new_word,
			positions, &last_doc_id, dtuple, &counta);


		ROW_MERGE_READ_GET_NEXT(min_rec);

		if (FTS_PARALLEL_DEGREE > 2) {
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
	trx->op_info = "";

	mem_heap_free(tuple_heap);

	fprintf(stderr, "FTS: inserted %lu record and final record %lu\n", (ulong)count, (ulong)counta);
	return(error);
}
