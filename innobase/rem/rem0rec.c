/************************************************************************
Record manager

(c) 1994-2001 Innobase Oy

Created 5/30/1994 Heikki Tuuri
*************************************************************************/

#include "rem0rec.h"

#ifdef UNIV_NONINL
#include "rem0rec.ic"
#endif

#include "mtr0mtr.h"
#include "mtr0log.h"

/*			PHYSICAL RECORD (OLD STYLE)
			===========================

The physical record, which is the data type of all the records
found in index pages of the database, has the following format
(lower addresses and more significant bits inside a byte are below
represented on a higher text line):

| offset of the end of the last field of data, the most significant
  bit is set to 1 if and only if the field is SQL-null,
  if the offset is 2-byte, then the second most significant
  bit is set to 1 if the field is stored on another page:
  mostly this will occur in the case of big BLOB fields |
... 
| offset of the end of the first field of data + the SQL-null bit |
| 4 bits used to delete mark a record, and mark a predefined
  minimum record in alphabetical order |
| 4 bits giving the number of records owned by this record
  (this term is explained in page0page.h) |
| 13 bits giving the order number of this record in the
  heap of the index page |
| 10 bits giving the number of fields in this record |
| 1 bit which is set to 1 if the offsets above are given in
  one byte format, 0 if in two byte format |
| two bytes giving an absolute pointer to the next record in the page |
ORIGIN of the record
| first field of data | 
... 
| last field of data |

The origin of the record is the start address of the first field 
of data. The offsets are given relative to the origin. 
The offsets of the data fields are stored in an inverted
order because then the offset of the first fields are near the 
origin, giving maybe a better processor cache hit rate in searches.

The offsets of the data fields are given as one-byte 
(if there are less than 127 bytes of data in the record) 
or two-byte unsigned integers. The most significant bit
is not part of the offset, instead it indicates the SQL-null
if the bit is set to 1. */

/*			PHYSICAL RECORD (NEW STYLE)
			===========================

The physical record, which is the data type of all the records
found in index pages of the database, has the following format
(lower addresses and more significant bits inside a byte are below
represented on a higher text line):

| length of the last non-null variable-length field of data:
  if the maximum length is 255, one byte; otherwise,
  0xxxxxxx (one byte, length=0..127), or 1exxxxxxxxxxxxxx (two bytes,
  length=128..16383, extern storage flag) |
...
| length of first variable-length field of data |
| SQL-null flags (1 bit per nullable field), padded to full bytes |
| 4 bits used to delete mark a record, and mark a predefined
  minimum record in alphabetical order |
| 4 bits giving the number of records owned by this record
  (this term is explained in page0page.h) |
| 13 bits giving the order number of this record in the
  heap of the index page |
| 3 bits record type: 000=conventional, 001=node pointer (inside B-tree),
  010=infimum, 011=supremum, 1xx=reserved |
| two bytes giving a relative pointer to the next record in the page |
ORIGIN of the record
| first field of data |
...
| last field of data |

The origin of the record is the start address of the first field
of data. The offsets are given relative to the origin.
The offsets of the data fields are stored in an inverted
order because then the offset of the first fields are near the
origin, giving maybe a better processor cache hit rate in searches.

The offsets of the data fields are given as one-byte
(if there are less than 127 bytes of data in the record)
or two-byte unsigned integers. The most significant bit
is not part of the offset, instead it indicates the SQL-null
if the bit is set to 1. */

/* CANONICAL COORDINATES. A record can be seen as a single
string of 'characters' in the following way: catenate the bytes
in each field, in the order of fields. An SQL-null field
is taken to be an empty sequence of bytes. Then after
the position of each field insert in the string 
the 'character' <FIELD-END>, except that after an SQL-null field
insert <NULL-FIELD-END>. Now the ordinal position of each
byte in this canonical string is its canonical coordinate.
So, for the record ("AA", SQL-NULL, "BB", ""), the canonical
string is "AA<FIELD_END><NULL-FIELD-END>BB<FIELD-END><FIELD-END>".
We identify prefixes (= initial segments) of a record
with prefixes of the canonical string. The canonical
length of the prefix is the length of the corresponding
prefix of the canonical string. The canonical length of
a record is the length of its canonical string.

For example, the maximal common prefix of records
("AA", SQL-NULL, "BB", "C") and ("AA", SQL-NULL, "B", "C")
is "AA<FIELD-END><NULL-FIELD-END>B", and its canonical
length is 5.

A complete-field prefix of a record is a prefix which ends at the
end of some field (containing also <FIELD-END>).
A record is a complete-field prefix of another record, if
the corresponding canonical strings have the same property. */

ulint	rec_dummy;	/* this is used to fool compiler in
			rec_validate */

/*******************************************************************
Validates the consistency of an old-style physical record. */
static
ibool
rec_validate_old(
/*=============*/
			/* out: TRUE if ok */
	rec_t*	rec);	/* in: physical record */

/**********************************************************
The following function determines the offsets to each field
in the record.  The offsets are written to an array of
ulint[n+2], with [0] being the number of fields (n), [1] being the
extra size (if REC_OFFS_COMPACT is set, the record is in the new
format), and [2]..[n+1] being the offsets past the end of
fields 0..n, or to the beginning of fields 1..n+1.  When the
high-order bit of the offset at [n+1] is set (REC_OFFS_SQL_NULL),
the field n is NULL.  When the second high-order bit of the offset
at [n+1] is set (REC_OFFS_EXTERNAL), the field n is being stored
externally. */
static
void
rec_init_offsets(
/*=============*/
				/* out: the offsets */
	rec_t*		rec,	/* in: physical record */
	dict_index_t*	index,	/* in: record descriptor */
	ulint*		offsets)/* in:/out: ulint[n+2];
				n=rec_offs_n_fields(offsets) */
{
	ulint	n_fields = rec_offs_n_fields(offsets);
	ulint	i	= 0;
	ulint	offs;

	rec_offs_make_valid(rec, index, offsets);

	if (index->table->comp) {
		const byte*	nulls;
		const byte*	lens;
		dict_field_t*	field;
		dtype_t*	type;
		ulint		null_mask;
		ulint		status = rec_get_status(rec);
		ulint		n_node_ptr_field = ULINT_UNDEFINED;

		switch (status) {
		case REC_STATUS_INFIMUM:
		case REC_STATUS_SUPREMUM:
			/* the field is 8 bytes long */
			rec_offs_base(offsets)[0] =
				REC_N_NEW_EXTRA_BYTES | REC_OFFS_COMPACT;
			rec_offs_base(offsets)[1] = 8;
			return;
		case REC_STATUS_NODE_PTR:
			n_node_ptr_field =
				dict_index_get_n_unique_in_tree(index);
			break;
		case REC_STATUS_ORDINARY:
			break;
		}

		nulls = rec - (REC_N_NEW_EXTRA_BYTES + 1);
		lens = nulls - (index->n_nullable + 7) / 8;
		offs = 0;
		null_mask = 1;

		/* read the lengths of fields 0..n */
		for (; i < n_fields; i++) {
			ibool	is_null = FALSE, is_external = FALSE;
			ulint	len;
			if (i == n_node_ptr_field) {
				len = 4;
				goto resolved;
			}

			field = dict_index_get_nth_field(index, i);
			type = dict_col_get_type(dict_field_get_col(field));
			if (!(dtype_get_prtype(type) & DATA_NOT_NULL)) {
				/* nullable field => read the null flag */
				is_null = (*nulls & null_mask) != 0;
				null_mask <<= 1;
				if (null_mask == 0x100) {
					nulls--;
					null_mask = 1;
				}
			}

			if (is_null) {
				/* No length is stored for NULL fields. */
				len = 0;
			} else if (!field->fixed_len) {
				/* Variable-length field: read the length */
				len = *lens--;
				if (dtype_get_len(type) > 255
				    || dtype_get_mtype(type) == DATA_BLOB) {
					if (len & 0x80) {
						/* 1exxxxxxx xxxxxxxx */
						is_external = !!(len & 0x40);
						len &= 0x3f;
						len <<= 8;
						len |= *lens--;
					}
				}
			} else {
				len = field->fixed_len;
			}
		resolved:
			offs += len;
			len = offs;
			if (is_external) {
				len |= REC_OFFS_EXTERNAL;
			}
			if (is_null) {
				len |= REC_OFFS_SQL_NULL;
			}
			rec_offs_base(offsets)[i + 1] = len;
		}

		*rec_offs_base(offsets) =
			(rec - (lens + 1)) | REC_OFFS_COMPACT;
	} else {
		/* Old-style record: determine extra size and end offsets */
		offs = REC_N_OLD_EXTRA_BYTES;
		if (rec_get_1byte_offs_flag(rec)) {
			offs += n_fields;
			*rec_offs_base(offsets) = offs;
			/* Determine offsets to fields */
			for (; i < n_fields; i++) {
				offs = rec_1_get_field_end_info(rec, i);
				if (offs & REC_1BYTE_SQL_NULL_MASK) {
					offs &= ~REC_1BYTE_SQL_NULL_MASK;
					offs |= REC_OFFS_SQL_NULL;
				}
				rec_offs_base(offsets)[1 + i] = offs;
			}
		} else {
			offs += 2 * n_fields;
			*rec_offs_base(offsets) = offs;
			/* Determine offsets to fields */
			for (; i < n_fields; i++) {
				offs = rec_2_get_field_end_info(rec, i);
				if (offs & REC_2BYTE_SQL_NULL_MASK) {
					offs &= ~REC_2BYTE_SQL_NULL_MASK;
					offs |= REC_OFFS_SQL_NULL;
				}
				if (offs & REC_2BYTE_EXTERN_MASK) {
					offs &= ~REC_2BYTE_EXTERN_MASK;
					offs |= REC_OFFS_EXTERNAL;
				}
				rec_offs_base(offsets)[1 + i] = offs;
			}
		}
	}
}

/**********************************************************
The following function determines the offsets to each field
in the record.  The offsets are returned in an array of
ulint, with [0] being the number of fields (n), [1] being the
extra size (if REC_OFFS_COMPACT is set, the record is in the new
format), and [2]..[n+1] being the offsets past the end of
fields 0..n, or to the beginning of fields 1..n+1.  When the
high-order bit of the offset at [n+1] is set (REC_OFFS_SQL_NULL),
the field n is NULL.  When the second high-order bit of the offset
at [n+1] is set (REC_OFFS_EXTERNAL), the field n is being stored
externally. */

ulint*
rec_get_offsets(
/*============*/
				/* out: the offsets */
	rec_t*		rec,	/* in: physical record */
	dict_index_t*	index,	/* in: record descriptor */
	ulint		n_fields,/* in: maximum number of initialized fields
				(ULINT_UNDEFINED if all fields) */
	mem_heap_t*	heap)	/* in: memory heap */
{
	ulint*	offsets;
	ulint	n;

	ut_ad(rec);
	ut_ad(index);
	ut_ad(heap);

	if (index->table->comp) {
		switch (rec_get_status(rec)) {
		case REC_STATUS_ORDINARY:
			n = dict_index_get_n_fields(index);
			break;
		case REC_STATUS_NODE_PTR:
			n = dict_index_get_n_unique_in_tree(index) + 1;
			break;
		case REC_STATUS_INFIMUM:
		case REC_STATUS_SUPREMUM:
			/* infimum or supremum record */
			n = 1;
			break;
		default:
			ut_error;
			return(NULL);
		}
	} else {
		n = rec_get_n_fields_old(rec);
	}

	if (n_fields < n) {
		n = n_fields;
	}

	offsets = mem_heap_alloc(heap,
		(n + (1 + REC_OFFS_HEADER_SIZE)) * sizeof(ulint));

	offsets[0] = n;

	rec_init_offsets(rec, index, offsets);
	return(offsets);
}

/**********************************************************
The following function determines the offsets to each field
in the record.  It differs from rec_get_offsets() by trying to
reuse a previously returned array. */

ulint*
rec_reget_offsets(
/*==============*/
				/* out: the new offsets */
	rec_t*		rec,	/* in: physical record */
	dict_index_t*	index,	/* in: record descriptor */
	ulint*		offsets,/* in: array of offsets
				from rec_get_offsets()
				or rec_reget_offsets(), or NULL */
	ulint		n_fields,/* in: maximum number of initialized fields
				(ULINT_UNDEFINED if all fields) */
	mem_heap_t*	heap)	/* in: memory heap */
{
	ulint	n;

	ut_ad(rec);
	ut_ad(index);
	ut_ad(heap);

	if (index->table->comp) {
		switch (rec_get_status(rec)) {
		case REC_STATUS_ORDINARY:
			n = dict_index_get_n_fields(index);
			break;
		case REC_STATUS_NODE_PTR:
			n = dict_index_get_n_unique_in_tree(index) + 1;
			break;
		case REC_STATUS_INFIMUM:
		case REC_STATUS_SUPREMUM:
			/* infimum or supremum record */
			n = 1;
			break;
		default:
			ut_error;
			return(NULL);
		}
	} else {
		n = rec_get_n_fields_old(rec);
	}

	if (n_fields < n) {
		n = n_fields;
	}

	if (!offsets || rec_offs_n_fields(offsets) < n) {
		offsets = mem_heap_alloc(heap,
			(n + (1 + REC_OFFS_HEADER_SIZE)) * sizeof(ulint));
	}

	offsets[0] = n;

	rec_init_offsets(rec, index, offsets);
	return(offsets);
}

/****************************************************************
The following function is used to get a pointer to the nth
data field in an old-style record. */

byte*
rec_get_nth_field_old(
/*==================*/
 			/* out: pointer to the field */
 	rec_t*	rec, 	/* in: record */
 	ulint	n,	/* in: index of the field */
	ulint*	len)	/* out: length of the field; UNIV_SQL_NULL if SQL
			null */
{
	ulint	os;
	ulint	next_os;

	ut_ad(rec && len);
	ut_ad(n < rec_get_n_fields_old(rec));

	if (n > REC_MAX_N_FIELDS) {
		fprintf(stderr, "Error: trying to access field %lu in rec\n",
								(ulong) n);
		ut_error;
	}

	if (rec == NULL) {
		fputs("Error: rec is NULL pointer\n", stderr);
		ut_error;
	}
	
	if (rec_get_1byte_offs_flag(rec)) {
		os = rec_1_get_field_start_offs(rec, n);

		next_os = rec_1_get_field_end_info(rec, n);

		if (next_os & REC_1BYTE_SQL_NULL_MASK) {
			*len = UNIV_SQL_NULL;

			return(rec + os);
		}

		next_os = next_os & ~REC_1BYTE_SQL_NULL_MASK;
	} else {
		os = rec_2_get_field_start_offs(rec, n);
	
		next_os = rec_2_get_field_end_info(rec, n);

		if (next_os & REC_2BYTE_SQL_NULL_MASK) {
			*len = UNIV_SQL_NULL;

			return(rec + os);
		}

		next_os = next_os & ~(REC_2BYTE_SQL_NULL_MASK
						| REC_2BYTE_EXTERN_MASK);
	}
	
	*len = next_os - os;

	ut_ad(*len < UNIV_PAGE_SIZE);

	return(rec + os);
}

/**************************************************************
The following function returns the size of a data tuple when converted to
a new-style physical record. */

ulint
rec_get_converted_size_new(
/*=======================*/
				/* out: size */
	dict_index_t*	index,	/* in: record descriptor */
	dtuple_t*	dtuple)	/* in: data tuple */
{
	ulint		size		= REC_N_NEW_EXTRA_BYTES
					+ (index->n_nullable + 7) / 8;
	dict_field_t*	field;
	dtype_t*	type;
	ulint		i;
	ulint		n_fields;
	ut_ad(index && dtuple);
	ut_ad(index->table->comp);

	switch (dtuple_get_info_bits(dtuple) & REC_NEW_STATUS_MASK) {
	case REC_STATUS_ORDINARY:
		n_fields = dict_index_get_n_fields(index);
		ut_ad(n_fields == dtuple_get_n_fields(dtuple));
		break;
	case REC_STATUS_NODE_PTR:
		n_fields = dict_index_get_n_unique_in_tree(index);
		ut_ad(n_fields + 1 == dtuple_get_n_fields(dtuple));
		ut_ad(dtuple_get_nth_field(dtuple, n_fields)->len == 4);
		size += 4; /* child page number */
		break;
	case REC_STATUS_INFIMUM:
	case REC_STATUS_SUPREMUM:
		/* infimum or supremum record, 8 bytes */
		return(size + 8); /* no extra data needed */
	default:
		ut_a(0);
		return(ULINT_UNDEFINED);
	}

	/* read the lengths of fields 0..n */
	for (i = 0; i < n_fields; i++) {
		ulint	len	= dtuple_get_nth_field(dtuple, i)->len;
		field = dict_index_get_nth_field(index, i);
		type = dict_col_get_type(dict_field_get_col(field));
		ut_ad(len != UNIV_SQL_NULL ||
			!(dtype_get_prtype(type) & DATA_NOT_NULL));

		if (len == UNIV_SQL_NULL) {
			/* No length is stored for NULL fields. */
			continue;
		}

		ut_ad(len <= dtype_get_len(type)
			|| dtype_get_mtype(type) == DATA_BLOB);
		ut_ad(!field->fixed_len || len == field->fixed_len);

		if (field->fixed_len) {
		} else if (len < 128 || (dtype_get_len(type) < 256
				&& dtype_get_mtype(type) != DATA_BLOB)) {
			size++;
		} else {
			size += 2;
		}
		size += len;
	}

	return(size);
}

/***************************************************************
Sets the value of the ith field SQL null bit of an old-style record. */

void
rec_set_nth_field_null_bit(
/*=======================*/
	rec_t*	rec,	/* in: record */
	ulint	i,	/* in: ith field */
	ibool	val)	/* in: value to set */
{
	ulint	info;

	if (rec_get_1byte_offs_flag(rec)) {

		info = rec_1_get_field_end_info(rec, i);

		if (val) {
			info = info | REC_1BYTE_SQL_NULL_MASK;
		} else {
			info = info & ~REC_1BYTE_SQL_NULL_MASK;
		}

		rec_1_set_field_end_info(rec, i, info);

		return;
	}

	info = rec_2_get_field_end_info(rec, i);

	if (val) {
		info = info | REC_2BYTE_SQL_NULL_MASK;
	} else {
		info = info & ~REC_2BYTE_SQL_NULL_MASK;
	}

	rec_2_set_field_end_info(rec, i, info);
}

/***************************************************************
Sets the value of the ith field extern storage bit of an old-style record. */

void
rec_set_nth_field_extern_bit_old(
/*=============================*/
	rec_t*	rec,	/* in: old-style record */
	ulint	i,	/* in: ith field */
	ibool	val,	/* in: value to set */
	mtr_t*	mtr)	/* in: mtr holding an X-latch to the page where
			rec is, or NULL; in the NULL case we do not
			write to log about the change */
{
	ulint	info;

	ut_a(!rec_get_1byte_offs_flag(rec));
	ut_a(i < rec_get_n_fields_old(rec));
	
	info = rec_2_get_field_end_info(rec, i);

	if (val) {
		info = info | REC_2BYTE_EXTERN_MASK;
	} else {
		info = info & ~REC_2BYTE_EXTERN_MASK;
	}

	if (mtr) {
		mlog_write_ulint(rec - REC_N_OLD_EXTRA_BYTES - 2 * (i + 1),
						info, MLOG_2BYTES, mtr);
	} else {
		rec_2_set_field_end_info(rec, i, info);
	}
}

/***************************************************************
Sets the value of the ith field extern storage bit of a new-style record. */

void
rec_set_nth_field_extern_bit_new(
/*=============================*/
	rec_t*		rec,	/* in: record */
	dict_index_t*	index,	/* in: record descriptor */
	ulint		ith,	/* in: ith field */
	ibool		val,	/* in: value to set */
	mtr_t*		mtr)	/* in: mtr holding an X-latch to the page
				where rec is, or NULL; in the NULL case
				we do not write to log about the change */
{
	byte*		nulls	= rec - (REC_N_NEW_EXTRA_BYTES + 1);
	byte*		lens	= nulls - (index->n_nullable + 7) / 8;
	dict_field_t*	field;
	dtype_t*	type;
	ulint		i;
	ulint		n_fields;
	ulint		null_mask	= 1;
	ut_ad(rec && index);
	ut_ad(index->table->comp);
	ut_ad(rec_get_status(rec) == REC_STATUS_ORDINARY);

	n_fields = dict_index_get_n_fields(index);

	ut_ad(ith < n_fields);

	/* read the lengths of fields 0..n */
	for (i = 0; i < n_fields; i++) {
		ibool	is_null;
		ulint	len;
		field = dict_index_get_nth_field(index, i);
		type = dict_col_get_type(dict_field_get_col(field));
		is_null = !(dtype_get_prtype(type) & DATA_NOT_NULL);
		if (is_null) {
			/* nullable field => read the null flag */
			is_null = !!(*nulls & null_mask);
			null_mask <<= 1;
			if (null_mask == 0x100)
				nulls--, null_mask = 1;
		}
		if (is_null || field->fixed_len) {
			/* No length (or extern bit) is stored for
			fields that are NULL or fixed-length. */
			ut_ad(i != ith);
			continue;
		}
		len = *lens--;
		if (dtype_get_len(type) > 255
				|| dtype_get_mtype(type) == DATA_BLOB) {
			if (len & 0x80) { /* 1exxxxxx: 2-byte length */
				if (i == ith) {
					if (!val == !(len & 0x20)) {
						return; /* no change */
					}
					/* toggle the extern bit */
					len ^= 0x40;
					if (mtr) {
						mlog_write_ulint(lens + 1, len,
							MLOG_1BYTE, mtr);
					} else {
						lens[1] = len;
					}
					return;
				}
				lens--;
			} else {
				/* short fields cannot be external */
				ut_ad(i != ith);
			}
		} else {
			/* short fields cannot be external */
			ut_ad(i != ith);
		}
	}
}

/***************************************************************
Sets TRUE the extern storage bits of fields mentioned in an array. */

void
rec_set_field_extern_bits(
/*======================*/
	rec_t*		rec,	/* in: record */
	dict_index_t*	index,	/* in: record descriptor */
	const ulint*	vec,	/* in: array of field numbers */
	ulint		n_fields,/* in: number of fields numbers */
	mtr_t*		mtr)	/* in: mtr holding an X-latch to the
				page where rec is, or NULL;
				in the NULL case we do not write
				to log about the change */
{
	ulint	i;
	
	for (i = 0; i < n_fields; i++) {
		rec_set_nth_field_extern_bit(rec, index, vec[i], TRUE, mtr);
	}
}

/**************************************************************
Returns the total size of a physical record.  */

ulint
rec_get_size(
/*=========*/
				/* out: size */
	rec_t*		rec,	/* in: physical record */
	dict_index_t*	index)	/* in: record descriptor */
{
	mem_heap_t*	heap
			= mem_heap_create(100);
	ulint*		offsets
			= rec_get_offsets(rec, index, ULINT_UNDEFINED, heap);
	ulint		size
			= rec_offs_size(offsets);

	mem_heap_free(heap);
	return(size);
}

/*************************************************************** 
Sets an old-style record field to SQL null.
The physical size of the field is not changed. */

void
rec_set_nth_field_sql_null(
/*=======================*/
	rec_t*	rec, 	/* in: record */
	ulint	n)	/* in: index of the field */
{
	ulint	offset;

	offset = rec_get_field_start_offs(rec, n);

	data_write_sql_null(rec + offset, rec_get_nth_field_size(rec, n));

	rec_set_nth_field_null_bit(rec, n, TRUE);
}

/*************************************************************
Builds an old-style physical record out of a data tuple and
stores it beginning from the start of the given buffer. */
static
rec_t*
rec_convert_dtuple_to_rec_old(
/*==========================*/			
			/* out: pointer to the origin of
			physical record */
	byte*	buf,	/* in: start address of the physical record */
	dtuple_t* dtuple)/* in: data tuple */
{
	dfield_t* 	field;
	ulint		n_fields;
	ulint		data_size;
	rec_t* 		rec;
	ulint		end_offset;
	ulint		ored_offset;
	byte*		data;
	ulint		len;
	ulint		i;
	
	ut_ad(buf && dtuple);
	ut_ad(dtuple_validate(dtuple));
	ut_ad(dtuple_check_typed(dtuple));

	n_fields = dtuple_get_n_fields(dtuple);
	data_size = dtuple_get_data_size(dtuple);

	ut_ad(n_fields > 0);

	/* Calculate the offset of the origin in the physical record */	

	rec = buf + rec_get_converted_extra_size(data_size, n_fields);
	
	/* Store the number of fields */
	rec_set_n_fields_old(rec, n_fields);

	/* Set the info bits of the record */
	rec_set_info_bits(rec, FALSE,
			dtuple_get_info_bits(dtuple) & REC_INFO_BITS_MASK);

	/* Store the data and the offsets */

	end_offset = 0;

	if (data_size <= REC_1BYTE_OFFS_LIMIT) {

	    rec_set_1byte_offs_flag(rec, TRUE);

	    for (i = 0; i < n_fields; i++) {

		field = dtuple_get_nth_field(dtuple, i);

		data = dfield_get_data(field);
		len = dfield_get_len(field);
		
		if (len == UNIV_SQL_NULL) {
			len = dtype_get_sql_null_size(dfield_get_type(field));
			data_write_sql_null(rec + end_offset, len);
		
			end_offset += len;
			ored_offset = end_offset | REC_1BYTE_SQL_NULL_MASK;
		} else {
			/* If the data is not SQL null, store it */
			ut_memcpy(rec + end_offset, data, len);

			end_offset += len;
			ored_offset = end_offset;
		}

		rec_1_set_field_end_info(rec, i, ored_offset);
	    }
	} else {
	    rec_set_1byte_offs_flag(rec, FALSE);

	    for (i = 0; i < n_fields; i++) {

		field = dtuple_get_nth_field(dtuple, i);

		data = dfield_get_data(field);
		len = dfield_get_len(field);
		
		if (len == UNIV_SQL_NULL) {
			len = dtype_get_sql_null_size(dfield_get_type(field));
			data_write_sql_null(rec + end_offset, len);
		
			end_offset += len;
			ored_offset = end_offset | REC_2BYTE_SQL_NULL_MASK;
		} else {
			/* If the data is not SQL null, store it */
			ut_memcpy(rec + end_offset, data, len);

			end_offset += len;
			ored_offset = end_offset;
		}

		rec_2_set_field_end_info(rec, i, ored_offset);
	    }
	}

	return(rec);
}

/*************************************************************
Builds a new-style physical record out of a data tuple and
stores it beginning from the start of the given buffer. */
static
rec_t*
rec_convert_dtuple_to_rec_new(
/*==========================*/
				/* out: pointer to the origin
				of physical record */
	byte*		buf,	/* in: start address of the physical record */
	dict_index_t*	index,	/* in: record descriptor */
	dtuple_t*	dtuple)	/* in: data tuple */
{
	dfield_t*	field;
	dtype_t*	type;
	rec_t* 		rec		= buf + REC_N_NEW_EXTRA_BYTES;
	byte*		end;
	byte*		nulls;
	byte*		lens;
	ulint		len;
	ulint		i;
	ulint		fixed_len;
	ulint		null_mask	= 1;
	const ulint	n_fields	= dtuple_get_n_fields(dtuple);
	const ulint	status		= dtuple_get_info_bits(dtuple)
					& REC_NEW_STATUS_MASK;
	ut_ad(index->table->comp);

	ut_ad(n_fields > 0);
	switch (status) {
	case REC_STATUS_ORDINARY:
		ut_ad(n_fields <= dict_index_get_n_fields(index));
		break;
	case REC_STATUS_NODE_PTR:
		ut_ad(n_fields == dict_index_get_n_unique_in_tree(index) + 1);
		break;
	case REC_STATUS_INFIMUM:
	case REC_STATUS_SUPREMUM:
		ut_ad(n_fields == 1);
		goto init;
	default:
		ut_a(0);
		return(0);
	}

	/* Calculate the offset of the origin in the physical record.
	We must loop over all fields to do this. */
	rec += (index->n_nullable + 7) / 8;

	for (i = 0; i < n_fields; i++) {
		field = dtuple_get_nth_field(dtuple, i);
		type = dfield_get_type(field);
		len = dfield_get_len(field);
		if (status == REC_STATUS_NODE_PTR && i == n_fields - 1) {
			fixed_len = 4;
			ut_ad(dtype_get_prtype(type) & DATA_NOT_NULL);
			ut_ad(len == 4);
			continue;
		}
		fixed_len = dict_index_get_nth_field(index, i)->fixed_len;

		if (!(dtype_get_prtype(type) & DATA_NOT_NULL)) {
			if (len == UNIV_SQL_NULL)
				continue;
		}
		/* only nullable fields can be null */
		ut_ad(len != UNIV_SQL_NULL);
		if (fixed_len) {
			ut_ad(len == fixed_len);
		} else {
			ut_ad(len <= dtype_get_len(type)
				|| dtype_get_mtype(type) == DATA_BLOB);
			rec++;
			if (len >= 128 && (dtype_get_len(type) >= 256
				|| dtype_get_mtype(type) == DATA_BLOB)) {
				rec++;
			}
		}
	}

init:
	end = rec;
	nulls = rec - (REC_N_NEW_EXTRA_BYTES + 1);
	lens = nulls - (index->n_nullable + 7) / 8;
	/* clear the SQL-null flags */
	memset (lens + 1, 0, nulls - lens);

	/* Set the info bits of the record */
	rec_set_status(rec, status);

	rec_set_info_bits(rec, TRUE,
			dtuple_get_info_bits(dtuple) & REC_INFO_BITS_MASK);

	/* Store the data and the offsets */

	for (i = 0; i < n_fields; i++) {
		field = dtuple_get_nth_field(dtuple, i);
		type = dfield_get_type(field);
		len = dfield_get_len(field);

		if (status == REC_STATUS_NODE_PTR && i == n_fields - 1) {
			fixed_len = 4;
			ut_ad(dtype_get_prtype(type) & DATA_NOT_NULL);
			ut_ad(len == 4);
			goto copy;
		}
		fixed_len = dict_index_get_nth_field(index, i)->fixed_len;

		if (!(dtype_get_prtype(type) & DATA_NOT_NULL)) {
			/* nullable field */
			ut_ad(index->n_nullable > 0);
			ut_ad(*nulls < null_mask);
			/* set the null flag if necessary */
			if (len == UNIV_SQL_NULL) {
				*nulls |= null_mask;
			}
			null_mask <<= 1;
			if (null_mask == 0x100)
				nulls--, null_mask = 1;
			if (len == UNIV_SQL_NULL)
				continue;
		}
		/* only nullable fields can be null */
		ut_ad(len != UNIV_SQL_NULL);
		if (fixed_len) {
			ut_ad(len == fixed_len);
		} else {
			ut_ad(len <= dtype_get_len(type)
				|| dtype_get_mtype(type) == DATA_BLOB);
			if (len < 128 || (dtype_get_len(type) < 256
				&& dtype_get_mtype(type) != DATA_BLOB)) {
				*lens-- = len;
			}
			else {
				/* the extern bits will be set later */
				ut_ad(len < 16384);
				*lens-- = len >> 8 | 0x80;
				*lens-- = len;
			}
		}
	copy:
		memcpy(end, dfield_get_data(field), len);
		end += len;
	}

	return(rec);
}

/*************************************************************
Builds a physical record out of a data tuple and
stores it beginning from the start of the given buffer. */

rec_t*
rec_convert_dtuple_to_rec(
/*======================*/
					/* out: pointer to the origin
					of physical record */
	byte*		buf,		/* in: start address of the
					physical record */
	dict_index_t*	index,		/* in: record descriptor */
	dtuple_t*	dtuple)		/* in: data tuple */
{
	rec_t*	rec;

	ut_ad(buf && index && dtuple);
	ut_ad(dtuple_validate(dtuple));
	ut_ad(dtuple_check_typed(dtuple));

	if (index->table->comp) {
		rec = rec_convert_dtuple_to_rec_new(buf, index, dtuple);
	} else {
		rec = rec_convert_dtuple_to_rec_old(buf, dtuple);
	}

#ifdef UNIV_DEBUG
	{
		mem_heap_t*	heap = mem_heap_create(100);
		ut_ad(rec_validate(rec,
			rec_get_offsets(rec, index, ULINT_UNDEFINED, heap)));
		mem_heap_free(heap);
	}
#endif /* UNIV_DEBUG */
	return(rec);
}

/******************************************************************
Copies the first n fields of a physical record to a data tuple. The fields
are copied to the memory heap. */

void
rec_copy_prefix_to_dtuple(
/*======================*/
	dtuple_t*	tuple,		/* in: data tuple */
	rec_t*		rec,		/* in: physical record */
	dict_index_t*	index,		/* in: record descriptor */
	ulint		n_fields,	/* in: number of fields to copy */
	mem_heap_t*	heap)		/* in: memory heap */
{
	dfield_t*	field;
	byte*		data;
	ulint		len;
	byte*		buf = NULL;
	ulint		i;
	ulint*		offsets;

	offsets = rec_get_offsets(rec, index, n_fields, heap);

	ut_ad(rec_validate(rec, offsets));
	ut_ad(dtuple_check_typed(tuple));

	dtuple_set_info_bits(tuple,
			rec_get_info_bits(rec, index->table->comp));

	for (i = 0; i < n_fields; i++) {

		field = dtuple_get_nth_field(tuple, i);
		data = rec_get_nth_field(rec, offsets, i, &len);

		if (len != UNIV_SQL_NULL) {
			buf = mem_heap_alloc(heap, len);

			ut_memcpy(buf, data, len);
		}

		dfield_set_data(field, buf, len);
	}
}

/******************************************************************
Copies the first n fields of an old-style physical record
to a new physical record in a buffer. */
static
rec_t*
rec_copy_prefix_to_buf_old(
/*=======================*/
				/* out, own: copied record */
	rec_t*	rec,		/* in: physical record */
	ulint	n_fields,	/* in: number of fields to copy */
	ulint	area_end,	/* in: end of the prefix data */
	byte**	buf,		/* in/out: memory buffer for the copied prefix,
				or NULL */
	ulint*	buf_size)	/* in/out: buffer size */
{
	rec_t*	copy_rec;
	ulint	area_start;
	ulint	prefix_len;

	if (rec_get_1byte_offs_flag(rec)) {
		area_start = REC_N_OLD_EXTRA_BYTES + n_fields;
	} else {
		area_start = REC_N_OLD_EXTRA_BYTES + 2 * n_fields;
	}

	prefix_len = area_start + area_end;

	if ((*buf == NULL) || (*buf_size < prefix_len)) {
		if (*buf != NULL) {
			mem_free(*buf);
		}

		*buf = mem_alloc(prefix_len);
		*buf_size = prefix_len;
	}

	ut_memcpy(*buf, rec - area_start, prefix_len);

	copy_rec = *buf + area_start;

	rec_set_n_fields_old(copy_rec, n_fields);

	return(copy_rec);
}

/******************************************************************
Copies the first n fields of a physical record to a new physical record in
a buffer. */

rec_t*
rec_copy_prefix_to_buf(
/*===================*/
					/* out, own: copied record */
	rec_t*		rec,		/* in: physical record */
	dict_index_t*	index,		/* in: record descriptor */
	ulint		n_fields,	/* in: number of fields to copy */
	byte**		buf,		/* in/out: memory buffer
					for the copied prefix, or NULL */
	ulint*		buf_size)	/* in/out: buffer size */
{
	byte*		nulls	= rec - (REC_N_NEW_EXTRA_BYTES + 1);
	byte*		lens	= nulls - (index->n_nullable + 7) / 8;
	dict_field_t*	field;
	dtype_t*	type;
	ulint		i;
	ulint		prefix_len	= 0;
	ibool		is_null;
	ulint		null_mask	= 1;
	ulint		status;

	if (!index->table->comp) {
		ut_ad(rec_validate_old(rec));
		return(rec_copy_prefix_to_buf_old(rec, n_fields,
			rec_get_field_start_offs(rec, n_fields),
			buf, buf_size));
	}

	status = rec_get_status(rec);

	switch (status) {
	case REC_STATUS_ORDINARY:
		ut_ad(n_fields <= dict_index_get_n_fields(index));
		break;
	case REC_STATUS_NODE_PTR:
		/* it doesn't make sense to copy the child page number field */
		ut_ad(n_fields <= dict_index_get_n_unique_in_tree(index));
		break;
	case REC_STATUS_INFIMUM:
	case REC_STATUS_SUPREMUM:
		/* infimum or supremum record: no sense to copy anything */
	default:
		ut_a(0);
		return(NULL);
	}

	/* read the lengths of fields 0..n */
	for (i = 0; i < n_fields; i++) {
		field = dict_index_get_nth_field(index, i);
		type = dict_col_get_type(dict_field_get_col(field));
		is_null = !(dtype_get_prtype(type) & DATA_NOT_NULL);
		if (is_null) {
			/* nullable field => read the null flag */
			is_null = !!(*nulls & null_mask);
			null_mask <<= 1;
			if (null_mask == 0x100)
				nulls--, null_mask = 1;
		}

		if (is_null) {
		} else if (field->fixed_len) {
			prefix_len += field->fixed_len;
		} else {
			ulint	len = *lens--;
			if (dtype_get_len(type) > 255
				|| dtype_get_mtype(type) == DATA_BLOB) {
				if (len & 0x80) {
					/* 1exxxxxx */
					len &= 0x3f;
					len <<= 8;
					len |= *lens--;
				}
			}
			prefix_len += len;
		}
	}

	prefix_len += rec - (lens + 1);

	if ((*buf == NULL) || (*buf_size < prefix_len)) {
		if (*buf != NULL) {
			mem_free(*buf);
		}

		*buf = mem_alloc(prefix_len);
		*buf_size = prefix_len;
	}

	memcpy(*buf, lens + 1, prefix_len);

	return(*buf + (rec - (lens + 1)));
}

/*******************************************************************
Validates the consistency of an old-style physical record. */
static
ibool
rec_validate_old(
/*=============*/
			/* out: TRUE if ok */
	rec_t*	rec)	/* in: physical record */
{
	byte*	data;
	ulint	len;
	ulint	n_fields;
	ulint	len_sum		= 0;
	ulint	sum		= 0;
	ulint	i;

	ut_a(rec);
	n_fields = rec_get_n_fields_old(rec);

	if ((n_fields == 0) || (n_fields > REC_MAX_N_FIELDS)) {
		fprintf(stderr, "InnoDB: Error: record has %lu fields\n",
							(ulong) n_fields);
		return(FALSE);
	}
	
	for (i = 0; i < n_fields; i++) {
		data = rec_get_nth_field_old(rec, i, &len);
		
		if (!((len < UNIV_PAGE_SIZE) || (len == UNIV_SQL_NULL))) {
			fprintf(stderr,
			"InnoDB: Error: record field %lu len %lu\n", (ulong) i,
							(ulong) len);
			return(FALSE);
		}	

		if (len != UNIV_SQL_NULL) {
			len_sum += len;
			sum += *(data + len -1); /* dereference the
						end of the field to
						cause a memory trap
						if possible */
		} else {
			len_sum += rec_get_nth_field_size(rec, i);
		}
	}

	if (len_sum != rec_get_data_size_old(rec)) {
		fprintf(stderr,
		"InnoDB: Error: record len should be %lu, len %lu\n",
				(ulong) len_sum,
			        rec_get_data_size_old(rec));
		return(FALSE);
	}

	rec_dummy = sum; /* This is here only to fool the compiler */

	return(TRUE);
}

/*******************************************************************
Validates the consistency of a physical record. */

ibool
rec_validate(
/*=========*/
				/* out: TRUE if ok */
	rec_t*		rec,	/* in: physical record */
	const ulint*	offsets)/* in: array returned by rec_get_offsets() */
{
	const byte*	data;
	ulint		len;
	ulint		n_fields;
	ulint		len_sum		= 0;
	ulint		sum		= 0;
	ulint		i;

	ut_a(rec);
	n_fields = rec_offs_n_fields(offsets);

	if ((n_fields == 0) || (n_fields > REC_MAX_N_FIELDS)) {
		fprintf(stderr, "InnoDB: Error: record has %lu fields\n",
							(ulong) n_fields);
		return(FALSE);
	}

	ut_a(rec_offs_comp(offsets) || n_fields <= rec_get_n_fields_old(rec));

	for (i = 0; i < n_fields; i++) {
		data = rec_get_nth_field(rec, offsets, i, &len);

		if (!((len < UNIV_PAGE_SIZE) || (len == UNIV_SQL_NULL))) {
			fprintf(stderr,
			"InnoDB: Error: record field %lu len %lu\n", (ulong) i,
							(ulong) len);
			return(FALSE);
		}

		if (len != UNIV_SQL_NULL) {
			len_sum += len;
			sum += *(data + len -1); /* dereference the
						end of the field to
						cause a memory trap
						if possible */
		} else if (!rec_offs_comp(offsets)) {
			len_sum += rec_get_nth_field_size(rec, i);
		}
	}

	if (len_sum != (ulint)(rec_get_end(rec, offsets) - rec)) {
		fprintf(stderr,
		"InnoDB: Error: record len should be %lu, len %lu\n",
				(ulong) len_sum,
				(ulong) (rec_get_end(rec, offsets) - rec));
		return(FALSE);
	}	

	rec_dummy = sum; /* This is here only to fool the compiler */

	if (!rec_offs_comp(offsets)) {
		ut_a(rec_validate_old(rec));
	}

	return(TRUE);
}

/*******************************************************************
Prints an old-style physical record. */

void
rec_print_old(
/*==========*/
	FILE*		file,	/* in: file where to print */
	rec_t*		rec)	/* in: physical record */
{
	const byte*	data;
	ulint		len;
	ulint		n;
	ulint		i;

	ut_ad(rec);

	n = rec_get_n_fields_old(rec);

	fprintf(file, "PHYSICAL RECORD: n_fields %lu;"
		" %u-byte offsets; info bits %lu\n",
		(ulong) n,
		rec_get_1byte_offs_flag(rec) ? 1 : 2,
		(ulong) rec_get_info_bits(rec, FALSE));

	for (i = 0; i < n; i++) {

		data = rec_get_nth_field_old(rec, i, &len);

		fprintf(file, " %lu:", (ulong) i);

		if (len != UNIV_SQL_NULL) {
			if (len <= 30) {

				ut_print_buf(file, data, len);
			} else {
				ut_print_buf(file, data, 30);

				fputs("...(truncated)", file);
			}
		} else {
			fprintf(file, " SQL NULL, size %lu ",
				rec_get_nth_field_size(rec, i));
		}
		putc(';', file);
	}

	putc('\n', file);

	rec_validate_old(rec);
}

/*******************************************************************
Prints a physical record. */

void
rec_print(
/*======*/
	FILE*		file,	/* in: file where to print */
	rec_t*		rec,	/* in: physical record */
	const ulint*	offsets)/* in: array returned by rec_get_offsets() */
{
	const byte*	data;
	ulint		len;
	ulint		i;

	if (!rec_offs_comp(offsets)) {
		rec_print_old(file, rec);
		return;
	}

	ut_ad(rec);

	fprintf(file, "PHYSICAL RECORD: n_fields %lu;"
		" compact format; info bits %lu\n",
		(ulong) rec_offs_n_fields(offsets),
		(ulong) rec_get_info_bits(rec, TRUE));
	
	for (i = 0; i < rec_offs_n_fields(offsets); i++) {

		data = rec_get_nth_field(rec, offsets, i, &len);

		fprintf(file, " %lu:", (ulong) i);
	
		if (len != UNIV_SQL_NULL) {
			if (len <= 30) {

				ut_print_buf(file, data, len);
			} else {
				ut_print_buf(file, data, 30);

				fputs("...(truncated)", file);
			}
		} else {
			fputs(" SQL NULL", file);
		}
		putc(';', file);
	}

	putc('\n', file);

	rec_validate(rec, offsets);
}
