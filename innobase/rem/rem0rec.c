/************************************************************************
Record manager

(c) 1994-1996 Innobase Oy

Created 5/30/1994 Heikki Tuuri
*************************************************************************/

#include "rem0rec.h"

#ifdef UNIV_NONINL
#include "rem0rec.ic"
#endif

/*			PHYSICAL RECORD
			===============

The physical record, which is the data type of all the records
found in index pages of the database, has the following format
(lower addresses and more significant bits inside a byte are below
represented on a higher text line):

| offset of the end of the last field of data, the most significant
  bit is set to 1 if and only if the field is SQL-null | 
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
| two bytes giving the pointer to the next record in the page | 
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
if the bit is set to 1.

CANONICAL COORDINATES. A record can be seen as a single
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

/****************************************************************
The following function is used to get a pointer to the nth data field in a
record. */

byte*
rec_get_nth_field(
/*==============*/
 			/* out: pointer to the field */
 	rec_t*	rec, 	/* in: record */
 	ulint	n,	/* in: index of the field */
	ulint*	len)	/* out: length of the field; UNIV_SQL_NULL if SQL
			null */
{
	ulint	os;
	ulint	next_os;

	ut_ad(rec && len);
	ut_ad(n < rec_get_n_fields(rec));

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

		next_os = next_os & ~REC_2BYTE_SQL_NULL_MASK;
	}
	
	*len = next_os - os;

	ut_ad(*len < UNIV_PAGE_SIZE);

	return(rec + os);
}

/***************************************************************
Sets the value of the ith field SQL null bit. */

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
Sets a record field to SQL null. The physical size of the field is not
changed. */

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
Builds a physical record out of a data tuple and stores it beginning from
address destination. */

rec_t* 	
rec_convert_dtuple_to_rec_low(
/*==========================*/			
				/* out: pointer to the origin of physical
				record */
	byte*	destination,	/* in: start address of the physical record */
	dtuple_t* dtuple,	/* in: data tuple */
	ulint	data_size)	/* in: data size of dtuple */
{
	dfield_t* 	field;
	ulint		n_fields;
	rec_t* 		rec;
	ulint		end_offset;
	ulint		ored_offset;
	byte*		data;
	ulint		len;
	ulint		i;
	
	ut_ad(destination && dtuple);
	ut_ad(dtuple_validate(dtuple));
	ut_ad(dtuple_check_typed(dtuple));
	ut_ad(dtuple_get_data_size(dtuple) == data_size);

	n_fields = dtuple_get_n_fields(dtuple);

	ut_ad(n_fields > 0);

	/* Calculate the offset of the origin in the physical record */	

	rec = destination + rec_get_converted_extra_size(data_size, n_fields);
	
	/* Store the number of fields */
	rec_set_n_fields(rec, n_fields);

	/* Set the info bits of the record */
	rec_set_info_bits(rec, dtuple_get_info_bits(dtuple));

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

	ut_ad(rec_validate(rec));

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
	ulint		n_fields,	/* in: number of fields to copy */
	mem_heap_t*	heap)		/* in: memory heap */
{
	dfield_t*	field;
	byte*		data;
	ulint		len;
	byte*		buf = NULL;
	ulint		i;
	
	ut_ad(rec_validate(rec));	
	ut_ad(dtuple_check_typed(tuple));

	dtuple_set_info_bits(tuple, rec_get_info_bits(rec));

	for (i = 0; i < n_fields; i++) {

		field = dtuple_get_nth_field(tuple, i);
		data = rec_get_nth_field(rec, i, &len);

		if (len != UNIV_SQL_NULL) {
			buf = mem_heap_alloc(heap, len);

			ut_memcpy(buf, data, len);
		}

		dfield_set_data(field, buf, len);
	}
}

/******************************************************************
Copies the first n fields of a physical record to a new physical record in
a buffer. */

rec_t*
rec_copy_prefix_to_buf(
/*===================*/
				/* out, own: copied record */
	rec_t*	rec,		/* in: physical record */
	ulint	n_fields,	/* in: number of fields to copy */
	byte**	buf,		/* in/out: memory buffer for the copied prefix,
				or NULL */
	ulint*	buf_size)	/* in/out: buffer size */
{
	rec_t*	copy_rec;
	ulint	area_start;
	ulint	area_end;
	ulint	prefix_len;

	ut_ad(rec_validate(rec));

	area_end = rec_get_field_start_offs(rec, n_fields);

	if (rec_get_1byte_offs_flag(rec)) {
		area_start = REC_N_EXTRA_BYTES + n_fields;
	} else {
		area_start = REC_N_EXTRA_BYTES + 2 * n_fields;
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

	rec_set_n_fields(copy_rec, n_fields);

	return(copy_rec);
}

/*******************************************************************
Validates the consistency of a physical record. */

ibool
rec_validate(
/*=========*/
			/* out: TRUE if ok */
	rec_t*	rec)	/* in: physical record */
{
	ulint	i;
	byte*	data;
	ulint	len;
	ulint	n_fields;
	ulint	len_sum		= 0;
	ulint	sum		= 0;

	ut_a(rec);
	n_fields = rec_get_n_fields(rec);

	if ((n_fields == 0) || (n_fields > REC_MAX_N_FIELDS)) {
		ut_a(0);
	}
	
	for (i = 0; i < n_fields; i++) {
		data = rec_get_nth_field(rec, i, &len);
		
		ut_a((len < UNIV_PAGE_SIZE) || (len == UNIV_SQL_NULL));

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

	ut_a(len_sum == (ulint)(rec_get_end(rec) - rec));

	rec_dummy = sum; /* This is here only to fool the compiler */

	return(TRUE);
}

/*******************************************************************
Prints a physical record. */

void
rec_print(
/*======*/
	rec_t*	rec)	/* in: physical record */
{
	byte*	data;
	ulint	len;
	char*	offs;
	ulint	n;
	ulint	i;

	ut_ad(rec);
	
	if (rec_get_1byte_offs_flag(rec)) {
		offs = "TRUE";
	} else {
		offs = "FALSE";
	}

	n = rec_get_n_fields(rec);

	printf(
	    "PHYSICAL RECORD: n_fields %lu; 1-byte offs %s; info bits %lu\n",
		n, offs, rec_get_info_bits(rec));
	
	for (i = 0; i < n; i++) {

		data = rec_get_nth_field(rec, i, &len);

		printf(" %lu:", i);	
	
		if (len != UNIV_SQL_NULL) {
			if (len <= 30) {

				ut_print_buf(data, len);
			} else {
				ut_print_buf(data, 30);

				printf("...(truncated)");
			}
		} else {
			printf(" SQL NULL, size %lu ",
					rec_get_nth_field_size(rec, i));
						
		}
		printf(";");
	}

	printf("\n");

	rec_validate(rec);
}

/*******************************************************************
Prints a physical record to a buffer. */

ulint
rec_sprintf(
/*========*/
			/* out: printed length in bytes */
	char*	buf,	/* in: buffer to print to */
	ulint	buf_len,/* in: buffer length */
	rec_t*	rec)	/* in: physical record */
{
	byte*	data;
	ulint	len;
	ulint	k;
	ulint	n;
	ulint	i;

	ut_ad(rec);
	
	n = rec_get_n_fields(rec);
	k = 0;

	if (k + 30 > buf_len) {

		return(k);
	}
	
	k += sprintf(buf + k, "RECORD: info bits %lu", rec_get_info_bits(rec));
	
	for (i = 0; i < n; i++) {

		if (k + 30 > buf_len) {

			return(k);
		}
		
		data = rec_get_nth_field(rec, i, &len);

		k += sprintf(buf + k, " %lu:", i);
	
		if (len != UNIV_SQL_NULL) {
			if (k + 30 + 5 * len > buf_len) {

				return(k);
			}
			
			k += ut_sprintf_buf(buf + k, data, len);
		} else {
			k += sprintf(buf + k, " SQL NULL");
		}
		
		k += sprintf(buf + k, ";");
	}

	return(k);
}
