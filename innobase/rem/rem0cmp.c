/***********************************************************************
Comparison services for records

(c) 1994-1996 Innobase Oy

Created 7/1/1994 Heikki Tuuri
************************************************************************/

#include "rem0cmp.h"

#ifdef UNIV_NONINL
#include "rem0cmp.ic"
#endif

#include "srv0srv.h"

/*		ALPHABETICAL ORDER
		==================
		
The records are put into alphabetical order in the following
way: let F be the first field where two records disagree.
If there is a character in some position n where the the
records disagree, the order is determined by comparison of
the characters at position n, possibly after
collating transformation. If there is no such character,
but the corresponding fields have different lengths, then
if the data type of the fields is paddable,
shorter field is padded with a padding character. If the
data type is not paddable, longer field is considered greater.
Finally, the SQL null is bigger than any other value.

At the present, the comparison functions return 0 in the case,
where two records disagree only in the way that one 
has more fields than the other. */

#ifdef UNIV_DEBUG
/*****************************************************************
Used in debug checking of cmp_dtuple_... .
This function is used to compare a data tuple to a physical record. If
dtuple has n fields then rec must have either m >= n fields, or it must
differ from dtuple in some of the m fields rec has. */
static
int
cmp_debug_dtuple_rec_with_match(
/*============================*/	
				/* out: 1, 0, -1, if dtuple is greater, equal, 
				less than rec, respectively, when only the 
				common first fields are compared */
	dtuple_t*	dtuple,	/* in: data tuple */
	rec_t*		rec,	/* in: physical record which differs from
				dtuple in some of the common fields, or which
				has an equal number or more fields than
				dtuple */
	ulint*	 	matched_fields);/* in/out: number of already
				completely  matched fields; when function
				returns, contains the value for current
				comparison */
#endif /* UNIV_DEBUG */
/*****************************************************************
This function is used to compare two data fields for which the data type
is such that we must use MySQL code to compare them. The prototype here
must be a copy of the the one in ha_innobase.cc! */
extern
int
innobase_mysql_cmp(
/*===============*/
					/* out: 1, 0, -1, if a is greater,
					equal, less than b, respectively */
	int		mysql_type,	/* in: MySQL type */
	uint		charset_number,	/* in: number of the charset */
	unsigned char*	a,		/* in: data field */
	unsigned int	a_length,	/* in: data field length,
					not UNIV_SQL_NULL */
	unsigned char*	b,		/* in: data field */
	unsigned int	b_length);	/* in: data field length,
					not UNIV_SQL_NULL */

/*************************************************************************
Transforms the character code so that it is ordered appropriately for the
language. This is only used for the latin1 char set. MySQL does the
comparisons for other char sets. */
UNIV_INLINE
ulint
cmp_collate(
/*========*/
			/* out: collation order position */
	ulint	code)	/* in: code of a character stored in database record */
{	
	return((ulint) srv_latin1_ordering[code]);
}

/*****************************************************************
Returns TRUE if two types are equal for comparison purposes. */

ibool
cmp_types_are_equal(
/*================*/
				/* out: TRUE if the types are considered
				equal in comparisons */
	dtype_t*	type1,	/* in: type 1 */
	dtype_t*	type2)	/* in: type 2 */
{
	if (dtype_is_non_binary_string_type(type1->mtype, type1->prtype)
	    && dtype_is_non_binary_string_type(type2->mtype, type2->prtype)) {

		/* Both are non-binary string types: they can be compared if
		and only if the charset-collation is the same */

		if (dtype_get_charset_coll(type1->prtype)
				== dtype_get_charset_coll(type2->prtype)) {
			return(TRUE);
		}

		return(FALSE);
        }

	if (dtype_is_binary_string_type(type1->mtype, type1->prtype)
	    && dtype_is_binary_string_type(type2->mtype, type2->prtype)) {

		/* Both are binary string types: they can be compared */

		return(TRUE);
	}
	
        if (type1->mtype != type2->mtype) {

		return(FALSE);
	}

        if (type1->mtype == DATA_INT
	    && (type1->prtype & DATA_UNSIGNED)
		!= (type2->prtype & DATA_UNSIGNED)) {

		/* The storage format of an unsigned integer is different
		from a signed integer: in a signed integer we OR
		0x8000... to the value of positive integers. */
	
		return(FALSE);
	}

        if (type1->mtype == DATA_INT && type1->len != type2->len) {
	
		return(FALSE);
	}

	return(TRUE);
}

/*****************************************************************
Innobase uses this function to compare two data fields for which the data type
is such that we must compare whole fields or call MySQL to do the comparison */
static
int
cmp_whole_field(
/*============*/	
					/* out: 1, 0, -1, if a is greater,
					equal, less than b, respectively */
	dtype_t*	type,		/* in: data type */
	unsigned char*	a,		/* in: data field */
	unsigned int	a_length,	/* in: data field length,
					not UNIV_SQL_NULL */
	unsigned char*	b,		/* in: data field */
	unsigned int	b_length)	/* in: data field length,
					not UNIV_SQL_NULL */
{
	float		f_1;
	float		f_2;
	double		d_1;
	double		d_2;
	int		swap_flag	= 1;
	ulint		data_type;

	data_type = type->mtype;

	switch (data_type) {

    	case DATA_DECIMAL:
		/* Remove preceding spaces */
		for (; a_length && *a == ' '; a++, a_length--);
		for (; b_length && *b == ' '; b++, b_length--);

      		if (*a == '-') {
			if (*b != '-') {
	  			return(-1);
	  		}

			a++; b++;
			a_length--;
			b_length--;

			swap_flag = -1;
			
      		} else if (*b == '-') {

			return(1);
		}
		
      		while (a_length > 0 && (*a == '+' || *a == '0')) {
			a++; a_length--;
      		}

      		while (b_length > 0 && (*b == '+' || *b == '0')) {
			b++; b_length--;
      		}
      		
      		if (a_length != b_length) {
			if (a_length < b_length) {
				return(-swap_flag);
			}

			return(swap_flag);
		}

		while (a_length > 0 && *a == *b) {

			a++; b++; a_length--;
		}
			
		if (a_length == 0) {

			return(0);
		}

		if (*a > *b) {
			return(swap_flag);
		}

		return(-swap_flag);
    	case DATA_DOUBLE:
		d_1 = mach_double_read(a);
		d_2 = mach_double_read(b);
		
      		if (d_1 > d_2) {
      			return(1);
      		} else if (d_2 > d_1) {
      			return(-1);
      		}

      		return(0);
	
	case DATA_FLOAT:
		f_1 = mach_float_read(a);
		f_2 = mach_float_read(b);

      		if (f_1 > f_2) {
      			return(1);
      		} else if (f_2 > f_1) {
      			return(-1);
      		}

      		return(0);
	case DATA_VARMYSQL:
	case DATA_MYSQL:
	case DATA_BLOB:
		if (data_type == DATA_BLOB
		    && 0 != (type->prtype & DATA_BINARY_TYPE)) {

			ut_print_timestamp(stderr);
		        fprintf(stderr,
"  InnoDB: Error: comparing a binary BLOB with a character set sensitive\n"
"InnoDB: comparison!\n");
		}		   

		/* MySQL does not pad the ends of strings with spaces in a
		comparison. That would cause a foreign key check to fail for
		non-latin1 character sets if we have different length columns.
		To prevent that we remove trailing spaces here before doing
		the comparison. NOTE that if we in the future map more MySQL
		types to DATA_MYSQL or DATA_VARMYSQL, we have to change this
		code. */

		while (a_length > 0 && a[a_length - 1] == ' ') {
		      a_length--;
		}

		while (b_length > 0 && b[b_length - 1] == ' ') {
		      b_length--;
		}

		return(innobase_mysql_cmp(
				(int)(type->prtype & DATA_MYSQL_TYPE_MASK),
				(uint)dtype_get_charset_coll(type->prtype),
				a, a_length, b, b_length));
	default:
	        fprintf(stderr,
			"InnoDB: unknown type number %lu\n",
			(ulong) data_type);
	        ut_error;
	}

	return(0);
}

/*****************************************************************
This function is used to compare two data fields for which we know the
data type. */

int
cmp_data_data_slow(
/*===============*/	
				/* out: 1, 0, -1, if data1 is greater, equal, 
				less than data2, respectively */
	dtype_t*	cur_type,/* in: data type of the fields */
	byte*		data1,	/* in: data field (== a pointer to a memory
				buffer) */
	ulint		len1,	/* in: data field length or UNIV_SQL_NULL */
	byte*		data2,	/* in: data field (== a pointer to a memory
				buffer) */
	ulint		len2)	/* in: data field length or UNIV_SQL_NULL */
{
	ulint	data1_byte;
	ulint	data2_byte;
	ulint	cur_bytes;

	ut_ad(dtype_validate(cur_type));

	if (len1 == UNIV_SQL_NULL || len2 == UNIV_SQL_NULL) {

		if (len1 == len2) {

			return(0);
		}

		if (len1 == UNIV_SQL_NULL) {
			/* We define the SQL null to be the smallest possible
			value of a field in the alphabetical order */

			return(-1);
		}

		return(1);
	}
	
	if (cur_type->mtype >= DATA_FLOAT
	    || (cur_type->mtype == DATA_BLOB
	        && 0 == (cur_type->prtype & DATA_BINARY_TYPE)
		&& dtype_get_charset_coll(cur_type->prtype) !=
				data_mysql_latin1_swedish_charset_coll)) {

		return(cmp_whole_field(cur_type, data1, len1, data2, len2));
	}
	
	/* Compare then the fields */

	cur_bytes = 0;

	for (;;) {
		if (len1 <= cur_bytes) {
			if (len2 <= cur_bytes) {

				return(0);
			}
			
			data1_byte = dtype_get_pad_char(cur_type);

			if (data1_byte == ULINT_UNDEFINED) {

				return(-1);
			}
		} else {
			data1_byte = *data1;
		}

		if (len2 <= cur_bytes) {
			data2_byte = dtype_get_pad_char(cur_type);

			if (data2_byte == ULINT_UNDEFINED) {

				return(1);
			}
		} else {
			data2_byte = *data2;
		}
			
		if (data1_byte == data2_byte) {
			/* If the bytes are equal, they will remain such even
			after the collation transformation below */

			goto next_byte;
		}

		if (cur_type->mtype <= DATA_CHAR
		    || (cur_type->mtype == DATA_BLOB
		        && 0 == (cur_type->prtype & DATA_BINARY_TYPE))) {

			data1_byte = cmp_collate(data1_byte);
			data2_byte = cmp_collate(data2_byte);
		}
			
		if (data1_byte > data2_byte) {

				return(1);
		} else if (data1_byte < data2_byte) {

				return(-1);
		}
	next_byte:
		/* Next byte */
		cur_bytes++;
		data1++;
		data2++;
	}

	return(0);		/* Not reached */
}

/*****************************************************************
This function is used to compare a data tuple to a physical record.
Only dtuple->n_fields_cmp first fields are taken into account for
the the data tuple! If we denote by n = n_fields_cmp, then rec must
have either m >= n fields, or it must differ from dtuple in some of
the m fields rec has. If rec has an externally stored field we do not
compare it but return with value 0 if such a comparison should be
made. */

int
cmp_dtuple_rec_with_match(
/*======================*/	
				/* out: 1, 0, -1, if dtuple is greater, equal, 
				less than rec, respectively, when only the 
				common first fields are compared, or
				until the first externally stored field in
				rec */
	dtuple_t*	dtuple,	/* in: data tuple */
	rec_t*		rec,	/* in: physical record which differs from
				dtuple in some of the common fields, or which
				has an equal number or more fields than
				dtuple */
	ulint*	 	matched_fields, /* in/out: number of already completely 
				matched fields; when function returns,
				contains the value for current comparison */
	ulint*	  	matched_bytes) /* in/out: number of already matched 
				bytes within the first field not completely
				matched; when function returns, contains the
				value for current comparison */
{
	dtype_t*	cur_type;	/* pointer to type of the current
					field in dtuple */
	dfield_t*	dtuple_field;	/* current field in logical record */
	ulint		dtuple_f_len;	/* the length of the current field 
					in the logical record */
	byte*		dtuple_b_ptr;	/* pointer to the current byte in 
					logical field data */
	ulint		dtuple_byte;	/* value of current byte to be compared 
					in dtuple*/
	ulint		rec_f_len;	/* length of current field in rec */
	byte*		rec_b_ptr;	/* pointer to the current byte in 
					rec field */
	ulint		rec_byte;	/* value of current byte to be 
					compared in rec */
	ulint		cur_field;	/* current field number */
	ulint		cur_bytes; 	/* number of already matched bytes 
					in current field */
	int		ret = 3333;	/* return value */

	ut_ad(dtuple && rec && matched_fields && matched_bytes);
	ut_ad(dtuple_check_typed(dtuple));

	cur_field = *matched_fields;
	cur_bytes = *matched_bytes;

	ut_ad(cur_field <= dtuple_get_n_fields_cmp(dtuple));
	ut_ad(cur_field <= rec_get_n_fields(rec));

	/* Match fields in a loop; stop if we run out of fields in dtuple
	or find an externally stored field */

	while (cur_field < dtuple_get_n_fields_cmp(dtuple)) {

		dtuple_field = dtuple_get_nth_field(dtuple, cur_field);
		cur_type = dfield_get_type(dtuple_field);

		dtuple_f_len = dfield_get_len(dtuple_field);	

		rec_b_ptr = rec_get_nth_field(rec, cur_field, &rec_f_len);

		/* If we have matched yet 0 bytes, it may be that one or
		both the fields are SQL null, or the record or dtuple may be
		the predefined minimum record, or the field is externally
		stored */

		if (cur_bytes == 0) {
			if (cur_field == 0) {

				if (rec_get_info_bits(rec)
				    & REC_INFO_MIN_REC_FLAG) {

					if (dtuple_get_info_bits(dtuple)
				    	    & REC_INFO_MIN_REC_FLAG) {

				    		ret = 0;
				    	} else {
				    		ret = 1;
				    	}

					goto order_resolved;
				}

				if (dtuple_get_info_bits(dtuple)
				    & REC_INFO_MIN_REC_FLAG) {
 				    	ret = -1;

					goto order_resolved;
				}
			}

			if (rec_get_nth_field_extern_bit(rec, cur_field)) {
				/* We do not compare to an externally
				stored field */

				ret = 0;

				goto order_resolved;
			}

		    	if (dtuple_f_len == UNIV_SQL_NULL
		            || rec_f_len == UNIV_SQL_NULL) {

				if (dtuple_f_len == rec_f_len) {

					goto next_field;
				}

				if (rec_f_len == UNIV_SQL_NULL) {
					/* We define the SQL null to be the 
					smallest possible value of a field 
					in the alphabetical order */

					ret = 1;
				} else {
					ret = -1;
				}

				goto order_resolved;
			}
		}

		if (cur_type->mtype >= DATA_FLOAT
	    	    || (cur_type->mtype == DATA_BLOB
	        	&& 0 == (cur_type->prtype & DATA_BINARY_TYPE)
			&& dtype_get_charset_coll(cur_type->prtype) !=
				data_mysql_latin1_swedish_charset_coll)) {

			ret = cmp_whole_field(
				cur_type,
				dfield_get_data(dtuple_field), dtuple_f_len,
				rec_b_ptr, rec_f_len);

			if (ret != 0) {
				cur_bytes = 0;

				goto order_resolved;
			} else {
				goto next_field;
			}
		}
		
		/* Set the pointers at the current byte */

		rec_b_ptr = rec_b_ptr + cur_bytes;
		dtuple_b_ptr = (byte*)dfield_get_data(dtuple_field) 
								+ cur_bytes;
		/* Compare then the fields */		
		
		for (;;) {
			if (rec_f_len <= cur_bytes) {
				if (dtuple_f_len <= cur_bytes) {

					goto next_field;
				}
			
				rec_byte = dtype_get_pad_char(cur_type);

				if (rec_byte == ULINT_UNDEFINED) {
					ret = 1;

					goto order_resolved;
				}
			} else {
				rec_byte = *rec_b_ptr;
			}

			if (dtuple_f_len <= cur_bytes) {
				dtuple_byte = dtype_get_pad_char(cur_type);

				if (dtuple_byte == ULINT_UNDEFINED) {
					ret = -1;

					goto order_resolved;
				}
			} else {
				dtuple_byte = *dtuple_b_ptr;
			}
			
			if (dtuple_byte == rec_byte) {
				/* If the bytes are equal, they will
				remain such even after the collation
				transformation below */

				goto next_byte;
			}

			if (cur_type->mtype <= DATA_CHAR
			    || (cur_type->mtype == DATA_BLOB
			        && 0 ==
				    (cur_type->prtype & DATA_BINARY_TYPE))) {

				rec_byte = cmp_collate(rec_byte);
				dtuple_byte = cmp_collate(dtuple_byte);
			}
			
			if (dtuple_byte > rec_byte) {
				ret = 1;
				goto order_resolved;

			} else if (dtuple_byte < rec_byte) {
				ret = -1;
				goto order_resolved;
			}
		next_byte:
			/* Next byte */
			cur_bytes++;
			rec_b_ptr++;
			dtuple_b_ptr++;
		}

	next_field:
		cur_field++;
		cur_bytes = 0;
	}

	ut_ad(cur_bytes == 0);

	ret = 0;	/* If we ran out of fields, dtuple was equal to rec
			up to the common fields */
order_resolved:
	ut_ad((ret >= - 1) && (ret <= 1));
	ut_ad(ret == cmp_debug_dtuple_rec_with_match(dtuple, rec,
							matched_fields));
	ut_ad(*matched_fields == cur_field); /* In the debug version, the
						above cmp_debug_... sets
						*matched_fields to a value */
	*matched_fields = cur_field;
	*matched_bytes = cur_bytes;

	return(ret);
}

/******************************************************************
Compares a data tuple to a physical record. */

int
cmp_dtuple_rec(
/*===========*/
				/* out: 1, 0, -1, if dtuple is greater, equal, 
				less than rec, respectively; see the comments
				for cmp_dtuple_rec_with_match */
	dtuple_t* 	dtuple,	/* in: data tuple */
	rec_t*	  	rec)	/* in: physical record */
{
	ulint	matched_fields	= 0;
	ulint	matched_bytes	= 0;

	return(cmp_dtuple_rec_with_match(dtuple, rec, &matched_fields,
							&matched_bytes));
}

/******************************************************************
Checks if a dtuple is a prefix of a record. The last field in dtuple
is allowed to be a prefix of the corresponding field in the record. */

ibool
cmp_dtuple_is_prefix_of_rec(
/*========================*/
				/* out: TRUE if prefix */
	dtuple_t* 	dtuple,	/* in: data tuple */
	rec_t*	  	rec)	/* in: physical record */
{
	ulint	n_fields;
	ulint	matched_fields	= 0;
	ulint	matched_bytes	= 0;

	n_fields = dtuple_get_n_fields(dtuple);
	
	if (n_fields > rec_get_n_fields(rec)) {

		return(FALSE);
	}
	
	cmp_dtuple_rec_with_match(dtuple, rec, &matched_fields,
							&matched_bytes);
	if (matched_fields == n_fields) {

		return(TRUE);
	}

	if (matched_fields == n_fields - 1
			&& matched_bytes == dfield_get_len(
				dtuple_get_nth_field(dtuple, n_fields - 1))) {
		return(TRUE);
	}

	return(FALSE);	
}

/******************************************************************
Compares a prefix of a data tuple to a prefix of a physical record for
equality. If there are less fields in rec than parameter n_fields, FALSE
is returned. NOTE that n_fields_cmp of dtuple does not affect this
comparison. */

ibool
cmp_dtuple_rec_prefix_equal(
/*========================*/
				/* out: TRUE if equal */
	dtuple_t*	dtuple,	/* in: data tuple */
	rec_t*		rec,	/* in: physical record */
	ulint		n_fields) /* in: number of fields which should be 
				compared; must not exceed the number of 
				fields in dtuple */
{
	ulint	matched_fields	= 0;
	ulint	matched_bytes	= 0;

	ut_ad(n_fields <= dtuple_get_n_fields(dtuple));

	if (rec_get_n_fields(rec) < n_fields) {

		return(FALSE);
	}

	cmp_dtuple_rec_with_match(dtuple, rec, &matched_fields, 
							&matched_bytes);
	if (matched_fields >= n_fields) {

		return(TRUE);
	}

	return(FALSE);
}

/*****************************************************************
This function is used to compare two physical records. Only the common
first fields are compared, and if an externally stored field is
encountered, then 0 is returned. */

int
cmp_rec_rec_with_match(
/*===================*/	
				/* out: 1, 0 , -1 if rec1 is greater, equal,
				less, respectively, than rec2; only the common
				first fields are compared */
	rec_t*		rec1,	/* in: physical record */
	rec_t*		rec2,	/* in: physical record */
	dict_index_t*	index,	/* in: data dictionary index */
	ulint*	 	matched_fields, /* in/out: number of already completely 
				matched fields; when the function returns,
				contains the value the for current
				comparison */
	ulint*	  	matched_bytes) /* in/out: number of already matched 
				bytes within the first field not completely
				matched; when the function returns, contains
				the value for the current comparison */
{
	dtype_t* cur_type;	/* pointer to type struct of the
				current field in index */
	ulint	rec1_n_fields;	/* the number of fields in rec */
	ulint	rec1_f_len;	/* length of current field in rec */
	byte*	rec1_b_ptr;	/* pointer to the current byte in rec field */
	ulint	rec1_byte;	/* value of current byte to be compared in
				rec */
	ulint	rec2_n_fields;	/* the number of fields in rec */
	ulint	rec2_f_len;	/* length of current field in rec */
	byte*	rec2_b_ptr;	/* pointer to the current byte in rec field */
	ulint	rec2_byte;	/* value of current byte to be compared in
				rec */
	ulint	cur_field;	/* current field number */
	ulint	cur_bytes; 	/* number of already matched bytes in current
				field */
	int	ret = 3333;	/* return value */

	ut_ad(rec1 && rec2 && index);

	rec1_n_fields = rec_get_n_fields(rec1);
	rec2_n_fields = rec_get_n_fields(rec2);

	cur_field = *matched_fields;
	cur_bytes = *matched_bytes;

	/* Match fields in a loop; stop if we run out of fields in either
	record */

	while ((cur_field < rec1_n_fields) && (cur_field < rec2_n_fields)) {

		if (index->type & DICT_UNIVERSAL) {
			cur_type = dtype_binary;
		} else {
			cur_type = dict_col_get_type(
			      dict_field_get_col(
				dict_index_get_nth_field(index, cur_field)));
		}

		rec1_b_ptr = rec_get_nth_field(rec1, cur_field, &rec1_f_len);
		rec2_b_ptr = rec_get_nth_field(rec2, cur_field, &rec2_f_len);
		
		if (cur_bytes == 0) {
			if (cur_field == 0) {
				/* Test if rec is the predefined minimum
				record */
				if (rec_get_info_bits(rec1)
						& REC_INFO_MIN_REC_FLAG) {
	
					if (rec_get_info_bits(rec2) 
					    & REC_INFO_MIN_REC_FLAG) {
						ret = 0;
					} else {
						ret = -1;
					}
	
					goto order_resolved;
	
				} else if (rec_get_info_bits(rec2) 
					   & REC_INFO_MIN_REC_FLAG) {
	
					ret = 1;
	
					goto order_resolved;
				}
			}

			if (rec_get_nth_field_extern_bit(rec1, cur_field)
			    || rec_get_nth_field_extern_bit(rec2, cur_field)) {
				/* We do not compare to an externally
				stored field */

				ret = 0;

				goto order_resolved;
			}
			
		    	if (rec1_f_len == UNIV_SQL_NULL
		            || rec2_f_len == UNIV_SQL_NULL) {

				if (rec1_f_len == rec2_f_len) {

					goto next_field;

				} else if (rec2_f_len == UNIV_SQL_NULL) {

					/* We define the SQL null to be the 
					smallest possible value of a field 
					in the alphabetical order */

					ret = 1;
				} else {
					ret = -1;
				}

				goto order_resolved;
			}
		}

		if (cur_type->mtype >= DATA_FLOAT
	    	    || (cur_type->mtype == DATA_BLOB
	        	&& 0 == (cur_type->prtype & DATA_BINARY_TYPE)
			&& dtype_get_charset_coll(cur_type->prtype) !=
				data_mysql_latin1_swedish_charset_coll)) {

			ret = cmp_whole_field(cur_type,
						rec1_b_ptr, rec1_f_len,
						rec2_b_ptr, rec2_f_len);
			if (ret != 0) {
				cur_bytes = 0;

				goto order_resolved;
			} else {
				goto next_field;
			}
		}

		/* Set the pointers at the current byte */
		rec1_b_ptr = rec1_b_ptr + cur_bytes;
		rec2_b_ptr = rec2_b_ptr + cur_bytes;

		/* Compare then the fields */
		for (;;) {
			if (rec2_f_len <= cur_bytes) {

				if (rec1_f_len <= cur_bytes) {

					goto next_field;
				}

				rec2_byte = dtype_get_pad_char(cur_type);

				if (rec2_byte == ULINT_UNDEFINED) {
					ret = 1;

					goto order_resolved;
				}
			} else {
				rec2_byte = *rec2_b_ptr;
			}

			if (rec1_f_len <= cur_bytes) {
				rec1_byte = dtype_get_pad_char(cur_type);

				if (rec1_byte == ULINT_UNDEFINED) {
					ret = -1;

					goto order_resolved;
				}
			} else {
				rec1_byte = *rec1_b_ptr;
			}
			
			if (rec1_byte == rec2_byte) {
				/* If the bytes are equal, they will remain
				such even after the collation transformation 
				below */

				goto next_byte;
			}

			if (cur_type->mtype <= DATA_CHAR
			    || (cur_type->mtype == DATA_BLOB
			        && 0 ==
				    (cur_type->prtype & DATA_BINARY_TYPE))) {

				rec1_byte = cmp_collate(rec1_byte);
				rec2_byte = cmp_collate(rec2_byte);
			}

			if (rec1_byte < rec2_byte) {
				ret = -1;
				goto order_resolved;
			} else if (rec1_byte > rec2_byte) {
				ret = 1;
				goto order_resolved;
			}
		next_byte:
			/* Next byte */

			cur_bytes++;
			rec1_b_ptr++;
			rec2_b_ptr++;
		}

	next_field:
		cur_field++;
		cur_bytes = 0;
	}

	ut_ad(cur_bytes == 0);

	ret = 0;	/* If we ran out of fields, rec1 was equal to rec2 up
			to the common fields */
order_resolved:

	ut_ad((ret >= - 1) && (ret <= 1));

	*matched_fields = cur_field;	
	*matched_bytes = cur_bytes;

	return(ret);
}

#ifdef UNIV_DEBUG
/*****************************************************************
Used in debug checking of cmp_dtuple_... .
This function is used to compare a data tuple to a physical record. If
dtuple has n fields then rec must have either m >= n fields, or it must
differ from dtuple in some of the m fields rec has. If encounters an
externally stored field, returns 0. */
static
int
cmp_debug_dtuple_rec_with_match(
/*============================*/	
				/* out: 1, 0, -1, if dtuple is greater, equal, 
				less than rec, respectively, when only the 
				common first fields are compared */
	dtuple_t*	dtuple,	/* in: data tuple */
	rec_t*		rec,	/* in: physical record which differs from
				dtuple in some of the common fields, or which
				has an equal number or more fields than
				dtuple */
	ulint*	 	matched_fields) /* in/out: number of already
				completely matched fields; when function
				returns, contains the value for current
				comparison */
{
	dtype_t*	cur_type;	/* pointer to type of the current
					field in dtuple */
	dfield_t*	dtuple_field;	/* current field in logical record */
	ulint		dtuple_f_len;	/* the length of the current field 
					in the logical record */
	byte*		dtuple_f_data;	/* pointer to the current logical
					field data */
	ulint		rec_f_len;	/* length of current field in rec */
	byte*		rec_f_data;	/* pointer to the current rec field */
	int		ret = 3333;	/* return value */
	ulint		cur_field;	/* current field number */
	
	ut_ad(dtuple && rec && matched_fields);
	ut_ad(dtuple_check_typed(dtuple));
	
	ut_ad(*matched_fields <= dtuple_get_n_fields_cmp(dtuple));
	ut_ad(*matched_fields <= rec_get_n_fields(rec));

	cur_field = *matched_fields;

	if (cur_field == 0) {
		if (rec_get_info_bits(rec) & REC_INFO_MIN_REC_FLAG) {

			if (dtuple_get_info_bits(dtuple)
				    	    & REC_INFO_MIN_REC_FLAG) {
				ret = 0;
			} else {
				ret = 1;
			}

			goto order_resolved;
		}

		if (dtuple_get_info_bits(dtuple) & REC_INFO_MIN_REC_FLAG) {
 			ret = -1;

			goto order_resolved;
		}
	}

	/* Match fields in a loop; stop if we run out of fields in dtuple */

	while (cur_field < dtuple_get_n_fields_cmp(dtuple)) {

		dtuple_field = dtuple_get_nth_field(dtuple, cur_field);

		cur_type = dfield_get_type(dtuple_field);

		dtuple_f_data = dfield_get_data(dtuple_field);
		dtuple_f_len = dfield_get_len(dtuple_field);
		
		rec_f_data = rec_get_nth_field(rec, cur_field, &rec_f_len);

		if (rec_get_nth_field_extern_bit(rec, cur_field)) {
			/* We do not compare to an externally stored field */

			ret = 0;

			goto order_resolved;
		}

		ret = cmp_data_data(cur_type, dtuple_f_data, dtuple_f_len,
							rec_f_data, rec_f_len);
		if (ret != 0) {
			goto order_resolved;
		} 

		cur_field++;
	}

	ret = 0;	/* If we ran out of fields, dtuple was equal to rec
			up to the common fields */
order_resolved:
	ut_ad((ret >= - 1) && (ret <= 1));

	*matched_fields = cur_field;

	return(ret);
}
#endif /* UNIV_DEBUG */
