/* Copyright (c) 2005 PrimeBase Technologies GmbH
 *
 * PrimeBase XT
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * 2006-05-16	Paul McCullagh
 *
 * H&G2JCtL
 *
 * These functions implement the parts of PBXT which must conform to the
 * key and row format used by MySQL. 
 */

#include "xt_config.h"

#ifdef DRIZZLED
#include <drizzled/server_includes.h>
#include <drizzled/plugin.h>
#include <drizzled/show.h>
#include <drizzled/field/blob.h>
#include <drizzled/field/enum.h>
#include <drizzled/field/varstring.h>
#include <drizzled/current_session.h>
#include <drizzled/sql_lex.h>
#include <drizzled/session.h>
extern "C" struct charset_info_st *session_charset(Session *session);
extern pthread_key_t THR_Session;
#else
#include "mysql_priv.h"
#include <mysql/plugin.h>
#endif

#ifdef HAVE_ISNAN
#include <math.h>
#endif

#include "ha_pbxt.h"

#include "myxt_xt.h"
#include "strutil_xt.h"
#include "database_xt.h"
#include "cache_xt.h"
#include "datalog_xt.h"

static void		myxt_bitmap_init(XTThreadPtr self, MX_BITMAP *map, u_int n_bits);
static void		myxt_bitmap_free(XTThreadPtr self, MX_BITMAP *map);

#ifdef DRIZZLED
#define swap_variables(TYPE, a, b) \
  do {                             \
    TYPE dummy;                    \
    dummy= a;                      \
    a= b;                          \
    b= dummy;                      \
  } while (0)


#define CMP_NUM(a,b) (((a) < (b)) ? -1 : ((a) == (b)) ? 0 : 1)
#else
#define get_rec_bits(bit_ptr, bit_ofs, bit_len) \
	(((((uint16) (bit_ptr)[1] << 8) | (uint16) (bit_ptr)[0]) >> (bit_ofs)) & \
   ((1 << (bit_len)) - 1))
#endif

#define FIX_LENGTH(cs, pos, length, char_length) \
						do { \
							if ((length) > char_length) \
								char_length= my_charpos(cs, pos, pos+length, char_length); \
							set_if_smaller(char_length,length); \
						} while(0)

#ifdef store_key_length_inc
#undef store_key_length_inc
#endif
#define store_key_length_inc(key,length) \
{ if ((length) < 255) \
	{ *(key)++=(length); } \
	else \
	{ *(key)=255; mi_int2store((key)+1,(length)); (key)+=3; } \
}

#define set_rec_bits(bits, bit_ptr, bit_ofs, bit_len) \
{ \
	(bit_ptr)[0]= ((bit_ptr)[0] & ~(((1 << (bit_len)) - 1) << (bit_ofs))) | \
                ((bits) << (bit_ofs)); \
	if ((bit_ofs) + (bit_len) > 8) \
    (bit_ptr)[1]= ((bit_ptr)[1] & ~((1 << ((bit_len) - 8 + (bit_ofs))) - 1)) | \
                  ((bits) >> (8 - (bit_ofs))); \
}

#define clr_rec_bits(bit_ptr, bit_ofs, bit_len) \
	set_rec_bits(0, bit_ptr, bit_ofs, bit_len)

static ulong my_calc_blob_length(uint length, xtWord1 *pos)
{
	switch (length) {
	case 1:
		return (uint) (uchar) *pos;
	case 2:
		return (uint) uint2korr(pos);
	case 3:
		return uint3korr(pos);
	case 4:
		return uint4korr(pos);
	default:
		break;
	}
	return 0; /* Impossible */
}

static void my_store_blob_length(byte *pos,uint pack_length,uint length)
{
	switch (pack_length) {
	case 1:
		*pos= (uchar) length;
		break;
	case 2:
		int2store(pos,length);
		break;
	case 3:
		int3store(pos,length);
		break;
	case 4:
		int4store(pos,length);
	default:
		break;
	}
	return;
}

static int my_compare_text(MX_CONST_CHARSET_INFO *charset_info, uchar *a, uint a_length,
				uchar *b, uint b_length, my_bool part_key,
				my_bool XT_UNUSED(skip_end_space))
{
	if (!part_key)
		/* The last parameter is diff_if_only_endspace_difference, which means
		 * that end spaces are not ignored. We actually always want
		 * to ignore end spaces!
		 */
		return charset_info->coll->strnncollsp(charset_info, a, a_length,
				b, b_length, /*(my_bool)!skip_end_space*/0);
	return charset_info->coll->strnncoll(charset_info, a, a_length,
			b, b_length, part_key);
}

/*
 * -----------------------------------------------------------------------
 * Create a key
 */

/*
 * Derived from _mi_pack_key()
 */
xtPublic u_int myxt_create_key_from_key(XTIndexPtr ind, xtWord1 *key, xtWord1 *old, u_int k_length)
{
	xtWord1			*start_key = key;
	XTIndexSegRec	*keyseg = ind->mi_seg;

	for (u_int i=0; i<ind->mi_seg_count && (int) k_length > 0; i++, old += keyseg->length, keyseg++)
	{
		enum ha_base_keytype	type = (enum ha_base_keytype) keyseg->type;
		u_int					length = keyseg->length < k_length ? keyseg->length : k_length;
		u_int					char_length;
		xtWord1					*pos;
		MX_CONST_CHARSET_INFO	*cs = keyseg->charset;

		if (keyseg->null_bit) {
			k_length--;
			if (!(*key++ = (xtWord1) 1 - *old++)) {					/* Copy null marker */
				k_length -= length;
				if (keyseg->flag & (HA_VAR_LENGTH_PART | HA_BLOB_PART)) {
					k_length -= 2;									/* Skip length */
 					old += 2;
				}
				continue;											/* Found NULL */
			}
		}
		char_length= (cs && cs->mbmaxlen > 1) ? length/cs->mbmaxlen : length;
		pos = old;
		if (keyseg->flag & HA_SPACE_PACK) {
			uchar *end = pos + length;
			if (type != HA_KEYTYPE_NUM) {
				while (end > pos && end[-1] == ' ')
					end--;
			}
			else {
				while (pos < end && pos[0] == ' ')
					pos++;
			}
			k_length -= length;
			length = (u_int) (end-pos);
			FIX_LENGTH(cs, pos, length, char_length);
			store_key_length_inc(key, char_length);
			memcpy((byte*) key,pos,(size_t) char_length);
			key += char_length;
			continue;
		}
		if (keyseg->flag & (HA_VAR_LENGTH_PART | HA_BLOB_PART)) {
			/* Length of key-part used with mi_rkey() always 2 */
			u_int tmp_length = uint2korr(pos);
			k_length -= 2 + length;
			pos += 2;
			set_if_smaller(length, tmp_length);	/* Safety */
			FIX_LENGTH(cs, pos, length, char_length);
			store_key_length_inc(key,char_length);
			old +=2;					/* Skip length */
			memcpy((char *) key, pos, (size_t) char_length);
			key += char_length;
			continue;
		}
		if (keyseg->flag & HA_SWAP_KEY)
		{						/* Numerical column */
			pos+=length;
			k_length-=length;
			while (length--) {
				*key++ = *--pos;
			}
			continue;
		}
		FIX_LENGTH(cs, pos, length, char_length);
		memcpy((byte*) key, pos, char_length);
		if (length > char_length)
			cs->cset->fill(cs, (char *) (key + char_length), length - char_length, ' ');
		key += length;
		k_length -= length;
	}

	return (u_int) (key - start_key);
}

/* Derived from _mi_make_key */
xtPublic u_int myxt_create_key_from_row(XTIndexPtr ind, xtWord1 *key, xtWord1 *record, xtBool *no_duplicate)
{
	register XTIndexSegRec	*keyseg = ind->mi_seg;
	xtWord1					*pos;
	xtWord1					*end;
	xtWord1					*start;

	start = key;
 	for (u_int i=0; i<ind->mi_seg_count; i++, keyseg++)
	{
		enum ha_base_keytype	type = (enum ha_base_keytype) keyseg->type;
 		u_int					length = keyseg->length;
 		u_int					char_length;
 		MX_CONST_CHARSET_INFO	*cs = keyseg->charset;

		if (keyseg->null_bit) {
			if (record[keyseg->null_pos] & keyseg->null_bit) {
				*key++ = 0;				/* NULL in key */
				
				/* The point is, if a key contains a NULL value
				 * the duplicate checking must be disabled.
				 * This is because a NULL value is not considered
				 * equal to any other value.
				 */ 
				if (no_duplicate)
					*no_duplicate = FALSE;
				continue;
			}
			*key++ = 1;					/* Not NULL */
		}

		char_length= ((cs && cs->mbmaxlen > 1) ? length/cs->mbmaxlen : length);

		pos = record + keyseg->start;
		if (type == HA_KEYTYPE_BIT)
		{
			if (keyseg->bit_length)
			{
				uchar bits = get_rec_bits((uchar*) record + keyseg->bit_pos,
																 keyseg->bit_start, keyseg->bit_length);
				*key++ = bits;
				length--;
			}
			memcpy((byte*) key, pos, length);
			key+= length;
			continue;
		}
		if (keyseg->flag & HA_SPACE_PACK)
		{
			end = pos + length;
			if (type != HA_KEYTYPE_NUM) {
				while (end > pos && end[-1] == ' ')
					end--;
			}
			else {
				while (pos < end && pos[0] == ' ')
					pos++;
			}
			length = (u_int) (end-pos);
			FIX_LENGTH(cs, pos, length, char_length);
			store_key_length_inc(key,char_length);
			memcpy((byte*) key,(byte*) pos,(size_t) char_length);
			key += char_length;
			continue;
		}
		if (keyseg->flag & HA_VAR_LENGTH_PART) {
			uint pack_length= (keyseg->bit_start == 1 ? 1 : 2);
			uint tmp_length= (pack_length == 1 ? (uint) *(uchar*) pos :
												uint2korr(pos));
			pos += pack_length;			/* Skip VARCHAR length */
			set_if_smaller(length,tmp_length);
			FIX_LENGTH(cs, pos, length, char_length);
			store_key_length_inc(key,char_length);
			memcpy((byte*) key,(byte*) pos,(size_t) char_length);
			key += char_length;
			continue;
		}
		if (keyseg->flag & HA_BLOB_PART)
		{
			u_int tmp_length = my_calc_blob_length(keyseg->bit_start, pos);
			memcpy((byte*) &pos,pos+keyseg->bit_start,sizeof(char*));
			set_if_smaller(length,tmp_length);
			FIX_LENGTH(cs, pos, length, char_length);
			store_key_length_inc(key,char_length);
			memcpy((byte*) key,(byte*) pos,(size_t) char_length);
			key+= char_length;
			continue;
		}
		if (keyseg->flag & HA_SWAP_KEY)
		{						/* Numerical column */
#ifdef HAVE_ISNAN
			if (type == HA_KEYTYPE_FLOAT)
			{
				float nr;
				float4get(nr,pos);
				if (isnan(nr))
				{
					/* Replace NAN with zero */
					bzero(key,length);
					key+=length;
					continue;
				}
			}
			else if (type == HA_KEYTYPE_DOUBLE) {
				double nr;

				float8get(nr,pos);
				if (isnan(nr)) {
					bzero(key,length);
					key+=length;
					continue;
				}
			}
#endif
			pos+=length;
			while (length--) {
				*key++ = *--pos;
			}
			continue;
		}
 		FIX_LENGTH(cs, pos, length, char_length);
		memcpy((byte*) key, pos, char_length);
		if (length > char_length)
			cs->cset->fill(cs, (char *) key + char_length, length - char_length, ' ');
		key += length;
	}

	return ind->mi_fix_key ? ind->mi_key_size : (u_int) (key - start);		/* Return keylength */
}

xtPublic u_int myxt_create_foreign_key_from_row(XTIndexPtr ind, xtWord1 *key, xtWord1 *record, XTIndexPtr fkey_ind, xtBool *no_null)
{
	register XTIndexSegRec	*keyseg = ind->mi_seg;
	register XTIndexSegRec	*fkey_keyseg = fkey_ind->mi_seg;
	xtWord1					*pos;
	xtWord1					*end;
	xtWord1					*start;

	start = key;
 	for (u_int i=0; i<ind->mi_seg_count; i++, keyseg++, fkey_keyseg++)
	{
		enum ha_base_keytype	type = (enum ha_base_keytype) keyseg->type;
 		u_int					length = keyseg->length;
 		u_int					char_length;
 		MX_CONST_CHARSET_INFO	*cs = keyseg->charset;
		xtBool					is_null = FALSE;

		if (keyseg->null_bit) {
			if (record[keyseg->null_pos] & keyseg->null_bit) {
				is_null = TRUE;
				if (no_null)
					*no_null = FALSE;
			}
		}

		if (fkey_keyseg->null_bit) {
			if (is_null) {
				*key++ = 0;				/* NULL in key */
				
				/* The point is, if a key contains a NULL value
				 * the duplicate checking must be disabled.
				 * This is because a NULL value is not considered
				 * equal to any other value.
				 */ 
				continue;
			}
			*key++ = 1;					/* Not NULL */
		}

		char_length= ((cs && cs->mbmaxlen > 1) ? length/cs->mbmaxlen : length);

		pos = record + keyseg->start;
		if (type == HA_KEYTYPE_BIT)
		{
			if (keyseg->bit_length)
			{
				uchar bits = get_rec_bits((uchar*) record + keyseg->bit_pos,
																 keyseg->bit_start, keyseg->bit_length);
				*key++ = bits;
				length--;
			}
			memcpy((byte*) key, pos, length);
			key+= length;
			continue;
		}
		if (keyseg->flag & HA_SPACE_PACK)
		{
			end = pos + length;
			if (type != HA_KEYTYPE_NUM) {
				while (end > pos && end[-1] == ' ')
					end--;
			}
			else {
				while (pos < end && pos[0] == ' ')
					pos++;
			}
			length = (u_int) (end-pos);
			FIX_LENGTH(cs, pos, length, char_length);
			store_key_length_inc(key,char_length);
			memcpy((byte*) key,(byte*) pos,(size_t) char_length);
			key += char_length;
			continue;
		}
		if (keyseg->flag & HA_VAR_LENGTH_PART) {
			uint pack_length= (keyseg->bit_start == 1 ? 1 : 2);
			uint tmp_length= (pack_length == 1 ? (uint) *(uchar*) pos :
												uint2korr(pos));
			pos += pack_length;			/* Skip VARCHAR length */
			set_if_smaller(length,tmp_length);
			FIX_LENGTH(cs, pos, length, char_length);
			store_key_length_inc(key,char_length);
			memcpy((byte*) key,(byte*) pos,(size_t) char_length);
			key += char_length;
			continue;
		}
		if (keyseg->flag & HA_BLOB_PART)
		{
			u_int tmp_length = my_calc_blob_length(keyseg->bit_start, pos);
			memcpy((byte*) &pos,pos+keyseg->bit_start,sizeof(char*));
			set_if_smaller(length,tmp_length);
			FIX_LENGTH(cs, pos, length, char_length);
			store_key_length_inc(key,char_length);
			memcpy((byte*) key,(byte*) pos,(size_t) char_length);
			key+= char_length;
			continue;
		}
		if (keyseg->flag & HA_SWAP_KEY)
		{						/* Numerical column */
#ifdef HAVE_ISNAN
			if (type == HA_KEYTYPE_FLOAT)
			{
				float nr;
				float4get(nr,pos);
				if (isnan(nr))
				{
					/* Replace NAN with zero */
					bzero(key,length);
					key+=length;
					continue;
				}
			}
			else if (type == HA_KEYTYPE_DOUBLE) {
				double nr;

				float8get(nr,pos);
				if (isnan(nr)) {
					bzero(key,length);
					key+=length;
					continue;
				}
			}
#endif
			pos+=length;
			while (length--) {
				*key++ = *--pos;
			}
			continue;
		}
 		FIX_LENGTH(cs, pos, length, char_length);
		memcpy((byte*) key, pos, char_length);
		if (length > char_length)
			cs->cset->fill(cs, (char *) key + char_length, length - char_length, ' ');
		key += length;
	}

	return fkey_ind->mi_fix_key ? fkey_ind->mi_key_size : (u_int) (key - start);		/* Return keylength */
}

/* I may be overcautious here, but can I assume that
 * null_ptr refers to my buffer. If I cannot, then I
 * cannot use the set_notnull() method.
 */
static void mx_set_notnull_in_record(Field *field, char *record)
{
	if (field->null_ptr)
		record[(uint) (field->null_ptr - (uchar *) field->table->record[0])] &= (uchar) ~field->null_bit;
}

static xtBool mx_is_null_in_record(Field *field, char *record)
{
	if (field->null_ptr) {
		if (record[(uint) (field->null_ptr - (uchar *) field->table->record[0])] & (uchar) field->null_bit)
			return TRUE;
	}
	return FALSE;
}

/*
 * PBXT uses a completely different disk format to MySQL so I need a
 * method that just returns the byte length and
 * pointer to the data in a row.
 */
static char *mx_get_length_and_data(Field *field, char *dest, xtWord4 *len)
{
	char *from;
	
#if MYSQL_VERSION_ID < 50114
	from = dest + field->offset();
#else
	from = dest + field->offset(field->table->record[0]);
#endif
	switch (field->real_type()) {
#ifndef DRIZZLED
		case MYSQL_TYPE_TINY_BLOB:
		case MYSQL_TYPE_MEDIUM_BLOB:
		case MYSQL_TYPE_LONG_BLOB:
#endif
		case MYSQL_TYPE_BLOB: {
			/* TODO - Check: this was the original comment: I must set
			 * *data to non-NULL value, *data == 0, means SQL NULL value.
			 */
			char	*data;

			/* GOTCHA: There is no way this can work! field is shared
			 * between threads.
			char	*save = field->ptr;

			field->ptr = (char *) from;
			((Field_blob *) field)->get_ptr(&data);
			field->ptr = save;					// Restore org row pointer
			*/

			xtWord4 packlength = ((Field_blob *) field)->pack_length() - field->table->s->blob_ptr_size;
			memcpy(&data, ((char *) from)+packlength, sizeof(char*));
			
			*len = ((Field_blob *) field)->get_length((byte *) from);
			return data;
		}
#ifndef DRIZZLED
		case MYSQL_TYPE_STRING:
			/* To write this function you would think Field_string::pack
			 * would serve as a good example, but as far as I can tell
			 * it has a bug: the test from[length-1] == ' ' assumes
			 * 1-byte chars.
			 *
			 * But this is not relevant because I believe lengthsp
			 * will give me the correct answer!
			 */
			*len = field->charset()->cset->lengthsp(field->charset(), from, field->field_length);
			return from;
		case MYSQL_TYPE_VAR_STRING: {
			uint length=uint2korr(from);

			*len = length;
			return from+HA_KEY_BLOB_LENGTH;
		}
#endif
		case MYSQL_TYPE_VARCHAR: {
			uint length;

			if (((Field_varstring *) field)->length_bytes == 1)
				length = *((unsigned char *) from);
			else
				length = uint2korr(from);
			
			*len = length;
			return from+((Field_varstring *) field)->length_bytes;
		}
#ifndef DRIZZLED
		case MYSQL_TYPE_DECIMAL:
		case MYSQL_TYPE_TINY:
		case MYSQL_TYPE_SHORT:
		case MYSQL_TYPE_LONG:
		case MYSQL_TYPE_FLOAT:
		case MYSQL_TYPE_DOUBLE:
		case MYSQL_TYPE_NULL:
		case MYSQL_TYPE_TIMESTAMP:
		case MYSQL_TYPE_LONGLONG:
		case MYSQL_TYPE_INT24:
		case MYSQL_TYPE_DATE:
		case MYSQL_TYPE_TIME:
		case MYSQL_TYPE_DATETIME:
		case MYSQL_TYPE_YEAR:
		case MYSQL_TYPE_NEWDATE:
		case MYSQL_TYPE_BIT:
		case MYSQL_TYPE_NEWDECIMAL:
		case MYSQL_TYPE_ENUM:
		case MYSQL_TYPE_SET:
		case MYSQL_TYPE_GEOMETRY:
#else
		case DRIZZLE_TYPE_TINY:
		case DRIZZLE_TYPE_LONG:
		case DRIZZLE_TYPE_DOUBLE:
		case DRIZZLE_TYPE_NULL:
		case DRIZZLE_TYPE_TIMESTAMP:
		case DRIZZLE_TYPE_LONGLONG:
		case DRIZZLE_TYPE_DATETIME:
		case DRIZZLE_TYPE_DATE:
		case DRIZZLE_TYPE_NEWDECIMAL:
		case DRIZZLE_TYPE_ENUM:
#endif
			break;
	}

	*len = field->pack_length();
	return from;
}

/*
 * Set the length and data value of a field.
 * 
 * If input data is NULL this is a NULL value. In this case
 * we assume the null bit has been set and prepared
 * the field as follows:
 * 
 * According to the InnoDB implementation, we need
 * to zero out the field data...
 * "MySQL seems to assume the field for an SQL NULL
 * value is set to zero or space. Not taking this into
 * account caused seg faults with NULL BLOB fields, and
 * bug number 154 in the MySQL bug database: GROUP BY
 * and DISTINCT could treat NULL values inequal".
 */
static void mx_set_length_and_data(Field *field, char *dest, xtWord4 len, char *data)
{
	char *from;
	
#if MYSQL_VERSION_ID < 50114
	from = dest + field->offset();
#else
	from = dest + field->offset(field->table->record[0]);
#endif
	switch (field->real_type()) {
#ifndef DRIZZLED
		case MYSQL_TYPE_TINY_BLOB:
		case MYSQL_TYPE_MEDIUM_BLOB:
		case MYSQL_TYPE_LONG_BLOB:
#endif
		case MYSQL_TYPE_BLOB: {
			/* GOTCHA: There is no way that this can work.
			 * field is shared, because table is shared!
			char *save = field->ptr;
		 
			field->ptr = (char *) from;
			((Field_blob *) field)->set_ptr(len, data);
			field->ptr = save;					// Restore org row pointer
			*/
			xtWord4 packlength = ((Field_blob *) field)->pack_length() - field->table->s->blob_ptr_size;

			((Field_blob *) field)->store_length((byte *) from, packlength, len);
			memcpy_fixed(((char *) from)+packlength, &data, sizeof(char*));

			if (data)
				mx_set_notnull_in_record(field, dest);
			return;
		}
#ifndef DRIZZLED
		case MYSQL_TYPE_STRING:
			if (data) {
				mx_set_notnull_in_record(field, dest);
				memcpy(from, data, len);
			}
			else
				len = 0;

			/* And I think that fill will do this for me... */
			field->charset()->cset->fill(field->charset(), from + len, field->field_length - len, ' ');
			return;
		case MYSQL_TYPE_VAR_STRING:
			int2store(from, len);
			if (data) {
				mx_set_notnull_in_record(field, dest);
				memcpy(from+HA_KEY_BLOB_LENGTH, data, len);
			}
			return;
#endif
		case MYSQL_TYPE_VARCHAR:
			if (((Field_varstring *) field)->length_bytes == 1)
				*((unsigned char *) from) = (unsigned char) len;
			else
				int2store(from, len);
			if (data) {
				mx_set_notnull_in_record(field, dest);
				memcpy(from+((Field_varstring *) field)->length_bytes, data, len);
			}
			return;
#ifndef DRIZZLED
		case MYSQL_TYPE_DECIMAL:
		case MYSQL_TYPE_TINY:
		case MYSQL_TYPE_SHORT:
		case MYSQL_TYPE_LONG:
		case MYSQL_TYPE_FLOAT:
		case MYSQL_TYPE_DOUBLE:
		case MYSQL_TYPE_NULL:
		case MYSQL_TYPE_TIMESTAMP:
		case MYSQL_TYPE_LONGLONG:
		case MYSQL_TYPE_INT24:
		case MYSQL_TYPE_DATE:
		case MYSQL_TYPE_TIME:
		case MYSQL_TYPE_DATETIME:
		case MYSQL_TYPE_YEAR:
		case MYSQL_TYPE_NEWDATE:
		case MYSQL_TYPE_BIT:
		case MYSQL_TYPE_NEWDECIMAL:
		case MYSQL_TYPE_ENUM:
		case MYSQL_TYPE_SET:
		case MYSQL_TYPE_GEOMETRY:
#else
		case DRIZZLE_TYPE_TINY:
		case DRIZZLE_TYPE_LONG:
		case DRIZZLE_TYPE_DOUBLE:
		case DRIZZLE_TYPE_NULL:
		case DRIZZLE_TYPE_TIMESTAMP:
		case DRIZZLE_TYPE_LONGLONG:
		case DRIZZLE_TYPE_DATETIME:
		case DRIZZLE_TYPE_DATE:
		case DRIZZLE_TYPE_NEWDECIMAL:
		case DRIZZLE_TYPE_ENUM:
#endif
			break;
	}

	if (data) {
		mx_set_notnull_in_record(field, dest);
		memcpy(from, data, len);
	}
	else
		bzero(from, field->pack_length());
}

xtPublic void myxt_set_null_row_from_key(XTOpenTablePtr XT_UNUSED(ot), XTIndexPtr ind, xtWord1 *record)
{
	register XTIndexSegRec *keyseg = ind->mi_seg;

	for (u_int i=0; i<ind->mi_seg_count; i++, keyseg++) {
		ASSERT_NS(keyseg->null_bit);
		record[keyseg->null_pos] |= keyseg->null_bit;
	}
}

xtPublic void myxt_set_default_row_from_key(XTOpenTablePtr ot, XTIndexPtr ind, xtWord1 *record)
{
	XTTableHPtr		tab = ot->ot_table;
	TABLE			*table = tab->tab_dic.dic_my_table;
	XTIndexSegRec	*keyseg = ind->mi_seg;

	xt_lock_mutex_ns(&tab->tab_dic_field_lock);

	for (u_int i=0; i<ind->mi_seg_count; i++, keyseg++) {
		
		u_int col_idx = keyseg->col_idx;
		Field *field = table->field[col_idx];
		byte  *field_save = field->ptr;

		field->ptr = table->s->default_values + keyseg->start;
		memcpy(record + keyseg->start, field->ptr, field->pack_length());
		record[keyseg->null_pos] &= ~keyseg->null_bit;
		record[keyseg->null_pos] |= table->s->default_values[keyseg->null_pos] & keyseg->null_bit;

		field->ptr = field_save;
	}

	xt_unlock_mutex_ns(&tab->tab_dic_field_lock);
}

/* Derived from _mi_put_key_in_record */
xtPublic xtBool myxt_create_row_from_key(XTOpenTablePtr XT_UNUSED(ot), XTIndexPtr ind, xtWord1 *b_value, u_int key_len, xtWord1 *dest_buff)
{
	byte					*record = (byte *) dest_buff;
	register byte			*key;
	byte					*pos,*key_end;
	register XTIndexSegRec	*keyseg = ind->mi_seg;

	/* GOTCHA: When selecting from multiple
	 * indexes the key values are "merged" into the
	 * same buffer!!
	 * This means that this function must not affect
	 * the value of any other feilds.
	 *
	 * I was setting all to NULL:
	memset(dest_buff, 0xFF, table->s->null_bytes);
	*/
	key = (byte *) b_value;
	key_end = key + key_len;
	for (u_int i=0; i<ind->mi_seg_count; i++, keyseg++) {
		if (keyseg->null_bit) {
			if (!*key++)
			{
				record[keyseg->null_pos] |= keyseg->null_bit;
				continue;
			}
			record[keyseg->null_pos] &= ~keyseg->null_bit;
		}
		if (keyseg->type == HA_KEYTYPE_BIT)
		{
			uint length = keyseg->length;

			if (keyseg->bit_length)
			{
				uchar bits= *key++;
				set_rec_bits(bits, record + keyseg->bit_pos, keyseg->bit_start,
										 keyseg->bit_length);
				length--;
			}
			else
			{
				clr_rec_bits(record + keyseg->bit_pos, keyseg->bit_start,
										 keyseg->bit_length);
			}
			memcpy(record + keyseg->start, (byte*) key, length);
			key+= length;
			continue;
		}
		if (keyseg->flag & HA_SPACE_PACK)
		{
			uint length;
			get_key_length(length,key);
#ifdef CHECK_KEYS
			if (length > keyseg->length || key+length > key_end)
				goto err;
#endif
			pos = record+keyseg->start;
			if (keyseg->type != (int) HA_KEYTYPE_NUM)
			{
				memcpy(pos,key,(size_t) length);
				bfill(pos+length,keyseg->length-length,' ');
			}
			else
			{
				bfill(pos,keyseg->length-length,' ');
				memcpy(pos+keyseg->length-length,key,(size_t) length);
			}
			key+=length;
			continue;
		}

		if (keyseg->flag & HA_VAR_LENGTH_PART)
		{
			uint length;
			get_key_length(length,key);
#ifdef CHECK_KEYS
			if (length > keyseg->length || key+length > key_end)
	goto err;
#endif
			/* Store key length */
			if (keyseg->bit_start == 1)
				*(uchar*) (record+keyseg->start)= (uchar) length;
			else
				int2store(record+keyseg->start, length);
			/* And key data */
			memcpy(record+keyseg->start + keyseg->bit_start, (byte*) key, length);
			key+= length;
		}
		else if (keyseg->flag & HA_BLOB_PART)
		{
			uint length;
			get_key_length(length,key);
#ifdef CHECK_KEYS
			if (length > keyseg->length || key+length > key_end)
				goto err;
#endif
			/* key is a pointer into ot_ind_rbuf, which should be
			 * safe until we move to the next index item!
			 */
			byte *key_ptr = key; // Cannot take the address of a register variable!
			memcpy(record+keyseg->start+keyseg->bit_start,
			 (char*) &key_ptr,sizeof(char*));

			my_store_blob_length(record+keyseg->start,
					(uint) keyseg->bit_start,length);
			key+=length;
		}
		else if (keyseg->flag & HA_SWAP_KEY)
		{
			byte *to=	record+keyseg->start+keyseg->length;
			byte *end= key+keyseg->length;
#ifdef CHECK_KEYS
			if (end > key_end)
				goto err;
#endif
			do {
				*--to= *key++;
			} while (key != end);
			continue;
		}
		else
		{
#ifdef CHECK_KEYS
			if (key+keyseg->length > key_end)
				goto err;
#endif
			memcpy(record+keyseg->start,(byte*) key,
			 (size_t) keyseg->length);
			key+= keyseg->length;
		}
	
	}
	return OK;

#ifdef CHECK_KEYS
	err:
	return FAILED;				/* Crashed row */
#endif
}

/*
 * -----------------------------------------------------------------------
 * Compare keys
 */

static int my_compare_bin(uchar *a, uint a_length, uchar *b, uint b_length,
											 my_bool part_key, my_bool skip_end_space)
{
	uint length= min(a_length,b_length);
	uchar *end= a+ length;
	int flag;

	while (a < end)
		if ((flag= (int) *a++ - (int) *b++))
			return flag;
	if (part_key && b_length < a_length)
		return 0;
	if (skip_end_space && a_length != b_length)
	{
		int swap= 1;
		/*
			We are using space compression. We have to check if longer key
			has next character < ' ', in which case it's less than the shorter
			key that has an implicite space afterwards.

			This code is identical to the one in
			strings/ctype-simple.c:my_strnncollsp_simple
		*/
		if (a_length < b_length)
		{
			/* put shorter key in a */
			a_length= b_length;
			a= b;
			swap= -1;					/* swap sign of result */
		}
		for (end= a + a_length-length; a < end ; a++)
		{
			if (*a != ' ')
				return (*a < ' ') ? -swap : swap;
		}
		return 0;
	}
	return (int) (a_length-b_length);
}

xtPublic u_int myxt_get_key_length(XTIndexPtr ind, xtWord1 *key_buf)
{
	register XTIndexSegRec	*keyseg = ind->mi_seg;
	register uchar			*key_data = (uchar *) key_buf;
	uint					seg_len;
	uint					pack_len;

	for (u_int i=0; i<ind->mi_seg_count; i++, keyseg++) {
		/* Handle NULL part */
		if (keyseg->null_bit) {
			if (!*key_data++)	
				continue;
		}

		switch ((enum ha_base_keytype) keyseg->type) {
			case HA_KEYTYPE_TEXT:											 /* Ascii; Key is converted */
				if (keyseg->flag & HA_SPACE_PACK) {
					get_key_pack_length(seg_len, pack_len, key_data);
				}
				else
					seg_len = keyseg->length;
				key_data += seg_len;
				break;
			case HA_KEYTYPE_BINARY:
				if (keyseg->flag & HA_SPACE_PACK) {
					get_key_pack_length(seg_len, pack_len, key_data);
				}
				else
					seg_len = keyseg->length;
				key_data += seg_len;
				break;
			case HA_KEYTYPE_VARTEXT1:
			case HA_KEYTYPE_VARTEXT2:
				get_key_pack_length(seg_len, pack_len, key_data);
				key_data += seg_len;
				break;
			case HA_KEYTYPE_VARBINARY1:
			case HA_KEYTYPE_VARBINARY2:
				get_key_pack_length(seg_len, pack_len, key_data);
				key_data += seg_len;
				break;
			case HA_KEYTYPE_NUM: {
				/* Numeric key */
				if (keyseg->flag & HA_SPACE_PACK)
					seg_len = *key_data++;
				else
					seg_len = keyseg->length;
				key_data += seg_len;
				break;
			}
			case HA_KEYTYPE_INT8:
			case HA_KEYTYPE_SHORT_INT:
			case HA_KEYTYPE_USHORT_INT:
			case HA_KEYTYPE_LONG_INT:
			case HA_KEYTYPE_ULONG_INT:
			case HA_KEYTYPE_INT24:
			case HA_KEYTYPE_UINT24:
			case HA_KEYTYPE_FLOAT:
			case HA_KEYTYPE_DOUBLE:
			case HA_KEYTYPE_LONGLONG:
			case HA_KEYTYPE_ULONGLONG:
			case HA_KEYTYPE_BIT:
				key_data += keyseg->length;
				break;
			case HA_KEYTYPE_END:
				goto end;
		}
	}

	end:
	return (xtWord1 *) key_data - key_buf;
}

/* Derived from ha_key_cmp */
xtPublic int myxt_compare_key(XTIndexPtr ind, int search_flags, uint key_length, xtWord1 *key_value, xtWord1 *b_value)
{
	register XTIndexSegRec	*keyseg = ind->mi_seg;
	int						flag;
	register uchar			*a = (uchar *) key_value;
	uint					a_length;
	register uchar			*b = (uchar *) b_value;
	uint					b_length;
	uint					next_key_length;
	uchar					*end;
	uint					piks;
	uint					pack_len;

	for (uint i=0; i < ind->mi_seg_count && (int) key_length > 0; key_length = next_key_length, keyseg++, i++) {
		piks = !(keyseg->flag & HA_NO_SORT);

		/* Handle NULL part */
		if (keyseg->null_bit) {
			/* 1 is not null, 0 is null */
			int b_not_null = (int) *b++;

			key_length--;
			if ((int) *a != b_not_null && piks)
			{
				flag = (int) *a - b_not_null;
				return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
			}
			if (!*a++) {		
				/* If key was NULL */
				if (search_flags == (SEARCH_FIND | SEARCH_UPDATE))
					search_flags = SEARCH_SAME;								 /* Allow duplicate keys */
				else if (search_flags & SEARCH_NULL_ARE_NOT_EQUAL)
				{
					/*
					 * This is only used from mi_check() to calculate cardinality.
					 * It can't be used when searching for a key as this would cause
					 * compare of (a,b) and (b,a) to return the same value.
					 */
					return -1;
				}
				/* PMC - I don't know why I had next_key_length = key_length - keyseg->length;
				 * This was my comment: even when null we have the complete length
				 *
				 * The truth is, a NULL only takes up one byte in the key, and this has already
				 * been subtracted.
				 */
				next_key_length = key_length;
				continue;															 /* To next key part */
			}
		}
		
		/* Both components are not null... */
		if (keyseg->length < key_length) {
			end = a + keyseg->length;
			next_key_length = key_length - keyseg->length;
		}
		else {
			end = a + key_length;
			next_key_length = 0;
		}

		switch ((enum ha_base_keytype) keyseg->type) {
			case HA_KEYTYPE_TEXT:											 /* Ascii; Key is converted */
				if (keyseg->flag & HA_SPACE_PACK) {
					get_key_pack_length(a_length, pack_len, a);
					next_key_length = key_length - a_length - pack_len;
					get_key_pack_length(b_length, pack_len, b);

					if (piks && (flag = my_compare_text(keyseg->charset, a, a_length, b, b_length,
									(my_bool) ((search_flags & SEARCH_PREFIX) && next_key_length <= 0),
									(my_bool)!(search_flags & SEARCH_PREFIX))))
						return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
					a += a_length;
				}
				else {
					a_length = (uint) (end - a);
					b_length = keyseg->length;
					if (piks && (flag = my_compare_text(keyseg->charset, a, a_length, b, b_length,
									(my_bool) ((search_flags & SEARCH_PREFIX) && next_key_length <= 0),
									(my_bool)!(search_flags & SEARCH_PREFIX))))
						return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
					a = end;
				}
				b += b_length;
				break;
			case HA_KEYTYPE_BINARY:
				if (keyseg->flag & HA_SPACE_PACK) {
					get_key_pack_length(a_length, pack_len, a);
					next_key_length = key_length - a_length - pack_len;
					get_key_pack_length(b_length, pack_len, b);

					if (piks && (flag = my_compare_bin(a, a_length, b, b_length,
								(my_bool) ((search_flags & SEARCH_PREFIX) && next_key_length <= 0), 1)))
						return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
				}
				else {
					a_length = keyseg->length;
					b_length = keyseg->length;
					if (piks && (flag = my_compare_bin(a, a_length, b, b_length,
									(my_bool) ((search_flags & SEARCH_PREFIX) && next_key_length <= 0), 0)))
						return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
				}
				a += a_length;
				b += b_length;
				break;
			case HA_KEYTYPE_VARTEXT1:
			case HA_KEYTYPE_VARTEXT2:
			{
				get_key_pack_length(a_length, pack_len, a);
				next_key_length = key_length - a_length - pack_len;
				get_key_pack_length(b_length, pack_len, b);

				if (piks && (flag = my_compare_text(keyseg->charset, a, a_length, b, b_length,
								(my_bool) ((search_flags & SEARCH_PREFIX) && next_key_length <= 0),
								(my_bool) ((search_flags & (SEARCH_FIND | SEARCH_UPDATE)) == SEARCH_FIND))))
					return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
				a += a_length;
				b += b_length;
				break;
			}
			case HA_KEYTYPE_VARBINARY1:
			case HA_KEYTYPE_VARBINARY2:
			{				
				get_key_pack_length(a_length, pack_len, a);
				next_key_length = key_length - a_length - pack_len;
				get_key_pack_length(b_length, pack_len, b);

				if (piks && (flag=my_compare_bin(a, a_length, b, b_length,
						(my_bool) ((search_flags & SEARCH_PREFIX) && next_key_length <= 0), 0)))
					return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
				a += a_length;
				b += b_length;
				break;
			}
			case HA_KEYTYPE_INT8:
			{
				int i_1 = (int) *((signed char *) a);
				int i_2 = (int) *((signed char *) b);
				if (piks && (flag = CMP_NUM(i_1,i_2)))
					return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
				a = end;
				b += keyseg->length;
				break;
			}
			case HA_KEYTYPE_SHORT_INT: {
				int16 s_1 = sint2korr(a);
				int16 s_2 = sint2korr(b);
				if (piks && (flag = CMP_NUM(s_1, s_2)))
					return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
				a = end;
				b += keyseg->length;
				break;
			}
			case HA_KEYTYPE_USHORT_INT: {
				uint16 us_1= sint2korr(a);
				uint16 us_2= sint2korr(b);
				if (piks && (flag = CMP_NUM(us_1, us_2)))
					return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
				a =	end;
				b += keyseg->length;
				break;
			}
			case HA_KEYTYPE_LONG_INT: {
				int32 l_1 = sint4korr(a);
				int32 l_2 = sint4korr(b);
				if (piks && (flag = CMP_NUM(l_1, l_2)))
					return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
				a = end;
				b += keyseg->length;
				break;
			}
			case HA_KEYTYPE_ULONG_INT: {
				uint32 u_1 = sint4korr(a);
				uint32 u_2 = sint4korr(b);
				if (piks && (flag = CMP_NUM(u_1, u_2)))
					return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
				a = end;
				b += keyseg->length;
				break;
			}
			case HA_KEYTYPE_INT24: {
				int32 l_1 = sint3korr(a);
				int32 l_2 = sint3korr(b);
				if (piks && (flag = CMP_NUM(l_1, l_2)))
					return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
				a = end;
				b += keyseg->length;
				break;
			}
			case HA_KEYTYPE_UINT24: {
				int32 l_1 = uint3korr(a);
				int32 l_2 = uint3korr(b);
				if (piks && (flag = CMP_NUM(l_1, l_2)))
					return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
				a = end;
				b += keyseg->length;
				break;
			}
			case HA_KEYTYPE_FLOAT: {
				float f_1, f_2;

				float4get(f_1, a);
				float4get(f_2, b);
				/*
				 * The following may give a compiler warning about floating point
				 * comparison not being safe, but this is ok in this context as
				 * we are bascily doing sorting
				 */
				if (piks && (flag = CMP_NUM(f_1, f_2)))
					return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
				a = end;
				b += keyseg->length;
				break;
			}
			case HA_KEYTYPE_DOUBLE: {
				double d_1, d_2;

				float8get(d_1, a);
				float8get(d_2, b);
				/*
				 * The following may give a compiler warning about floating point
				 * comparison not being safe, but this is ok in this context as
				 * we are bascily doing sorting
				 */
				if (piks && (flag = CMP_NUM(d_1, d_2)))
					return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
				a = end;
				b += keyseg->length;
				break;
			}
			case HA_KEYTYPE_NUM: {
				/* Numeric key */
				if (keyseg->flag & HA_SPACE_PACK) {
					a_length = *a++;
					end = a + a_length;
					next_key_length = key_length - a_length - 1;
					b_length = *b++;
				}
				else {
					a_length = (int) (end - a);
					b_length = keyseg->length;
				}

				/* remove pre space from keys */
				for ( ; a_length && *a == ' ' ; a++, a_length--) ;
				for ( ; b_length && *b == ' ' ; b++, b_length--) ;

				if (keyseg->flag & HA_REVERSE_SORT) {
					swap_variables(uchar *, a, b);
					swap_variables(uint, a_length, b_length);
				}
				
				if (piks) {
					if (*a == '-') {
						if (*b != '-')
							return -1;
						a++; b++;
						swap_variables(uchar *, a, b);
						swap_variables(uint, a_length, b_length);
						a_length--; b_length--;
					}
					else if (*b == '-')
						return 1;
					while (a_length && (*a == '+' || *a == '0')) {
						a++; a_length--;
					}
					
					while (b_length && (*b == '+' || *b == '0')) {
						b++; b_length--;
					}
				
					if (a_length != b_length)
						return (a_length < b_length) ? -1 : 1;
					while (b_length) {
						if (*a++ !=	*b++)
							return ((int) a[-1] - (int) b[-1]);
						b_length--;
					}
				}
				a = end;
				b += b_length;
				break;
			}
#ifdef HAVE_LONG_LONG
			case HA_KEYTYPE_LONGLONG: {
				longlong ll_a = sint8korr(a);
				longlong ll_b = sint8korr(b);
				if (piks && (flag = CMP_NUM(ll_a,ll_b)))
					return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
				a = end;
				b += keyseg->length;
				break;
			}
			case HA_KEYTYPE_ULONGLONG: {					
				ulonglong ll_a = uint8korr(a);
				ulonglong ll_b = uint8korr(b);
				if (piks && (flag = CMP_NUM(ll_a,ll_b)))
					return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
				a = end;
				b += keyseg->length;
				break;
			}
#endif
			case HA_KEYTYPE_BIT:
				/* TODO: What here? */
				break;
			case HA_KEYTYPE_END:												/* Ready */
				goto end;
		}
	}

	end:
	return 0;
}

xtPublic u_int myxt_key_seg_length(XTIndexSegRec *keyseg, u_int key_offset, xtWord1 *key_value)
{
	register xtWord1	*a = (xtWord1 *) key_value + key_offset;
	u_int				a_length;
	u_int				has_null = 0;
	u_int				key_length = 0;
	u_int				pack_len;

	/* Handle NULL part */
	if (keyseg->null_bit) {
		has_null++;
		/* If the value is null, then it only requires one byte: */
		if (!*a++)
			return has_null;
	}
	
	key_length = has_null + keyseg->length;

	switch ((enum ha_base_keytype) keyseg->type) {
		case HA_KEYTYPE_TEXT:											 /* Ascii; Key is converted */
			if (keyseg->flag & HA_SPACE_PACK) {
				get_key_pack_length(a_length, pack_len, a);
				key_length = has_null + a_length + pack_len;
			}
			break;
		case HA_KEYTYPE_BINARY:
			if (keyseg->flag & HA_SPACE_PACK) {
				get_key_pack_length(a_length, pack_len, a);
				key_length = has_null + a_length + pack_len;
			}
			break;
		case HA_KEYTYPE_VARTEXT1:
		case HA_KEYTYPE_VARTEXT2:
		case HA_KEYTYPE_VARBINARY1:
		case HA_KEYTYPE_VARBINARY2: {				
			get_key_pack_length(a_length, pack_len, a);
			key_length = has_null + a_length + pack_len;
			break;
		}
		case HA_KEYTYPE_INT8:
		case HA_KEYTYPE_SHORT_INT:
		case HA_KEYTYPE_USHORT_INT:
		case HA_KEYTYPE_LONG_INT:
		case HA_KEYTYPE_ULONG_INT:
		case HA_KEYTYPE_INT24:
		case HA_KEYTYPE_UINT24:
		case HA_KEYTYPE_FLOAT:
		case HA_KEYTYPE_DOUBLE:
			break;
		case HA_KEYTYPE_NUM: {
			/* Numeric key */
			if (keyseg->flag & HA_SPACE_PACK) {
				a_length = *a++;
				key_length = has_null + a_length + 1;
			}
			break;
		}
#ifdef HAVE_LONG_LONG
		case HA_KEYTYPE_LONGLONG:
		case HA_KEYTYPE_ULONGLONG:
			break;
#endif
		case HA_KEYTYPE_BIT:
			/* TODO: What here? */
			break;
		case HA_KEYTYPE_END:												/* Ready */
			break;
	}

	return key_length;
}

/*
 * -----------------------------------------------------------------------
 * Load and store rows
 */

xtPublic xtWord4 myxt_store_row_length(XTOpenTablePtr ot, char *rec_buff)
{
	TABLE	*table = ot->ot_table->tab_dic.dic_my_table;
	char	*sdata;
	xtWord4	dlen;
	xtWord4	item_size;
	xtWord4 row_size = 0;

 	for (Field **field=table->field ; *field ; field++) {
		if ((*field)->is_null_in_record((const uchar *) rec_buff)) {
 			sdata = NULL;
 			dlen = 0;
 			item_size = 1;
 		}
 		else {
			sdata = mx_get_length_and_data(*field, rec_buff, &dlen);
			if (!dlen) {
				/* Empty, but not null (blobs may return NULL, when
				 * length is 0.
				 */
				sdata = rec_buff; // Any valid pointer will do
				item_size = 1 + dlen;
			}
			else if (dlen <= 240)
				item_size = 1 + dlen;
			else if (dlen <= 0xFFFF)
				item_size = 3 + dlen;
			else if (dlen <= 0xFFFFFF)
				item_size = 4 + dlen;
			else
				item_size = 5 + dlen;
		}

		row_size += item_size;
	}
	return row_size;
}

static xtWord4 mx_store_row(XTOpenTablePtr ot, xtWord4 row_size, char *rec_buff)
{
	TABLE	*table = ot->ot_table->tab_dic.dic_my_table;
	char	*sdata;
	xtWord4	dlen;
	xtWord4	item_size;

 	for (Field **field=table->field ; *field ; field++) {
		if ((*field)->is_null_in_record((const uchar *) rec_buff)) {
 			sdata = NULL;
 			dlen = 0;
 			item_size = 1;
 		}
 		else {
			sdata = mx_get_length_and_data(*field, rec_buff, &dlen);
			if (!dlen) {
				/* Empty, but not null (blobs may return NULL, when
				 * length is 0.
				 */
				sdata = rec_buff; // Any valid pointer will do
				item_size = 1 + dlen;
			}
			else if (dlen <= 240)
				item_size = 1 + dlen;
			else if (dlen <= 0xFFFF)
				item_size = 3 + dlen;
			else if (dlen <= 0xFFFFFF)
				item_size = 4 + dlen;
			else
				item_size = 5 + dlen;
		}

		if (row_size + item_size > ot->ot_row_wbuf_size) {
			if (!xt_realloc_ns((void **) &ot->ot_row_wbuffer, row_size + item_size))
				return 0;
			ot->ot_row_wbuf_size = row_size + item_size;
		}

		if (!sdata)
			ot->ot_row_wbuffer[row_size] = 255;
		else if (dlen <= 240) {
			ot->ot_row_wbuffer[row_size] = (unsigned char) dlen;
			memcpy(&ot->ot_row_wbuffer[row_size+1], sdata, dlen);
		}
		else if (dlen <= 0xFFFF) {
			ot->ot_row_wbuffer[row_size] = 254;
			XT_SET_DISK_2(&ot->ot_row_wbuffer[row_size+1], dlen);
			memcpy(&ot->ot_row_wbuffer[row_size+3], sdata, dlen);
		}
		else if (dlen <= 0xFFFFFF) {
			ot->ot_row_wbuffer[row_size] = 253;
			XT_SET_DISK_3(&ot->ot_row_wbuffer[row_size+1], dlen);
			memcpy(&ot->ot_row_wbuffer[row_size+4], sdata, dlen);
		}
		else {
			ot->ot_row_wbuffer[row_size] = 252;
			XT_SET_DISK_4(&ot->ot_row_wbuffer[row_size+1], dlen);
			memcpy(&ot->ot_row_wbuffer[row_size+5], sdata, dlen);
		}

		row_size += item_size;
	}
	return row_size;
}

/* Count the number and size of whole columns in the given buffer. */
xtPublic size_t myxt_load_row_length(XTOpenTablePtr ot, size_t buffer_size, xtWord1 *source_buf, u_int *ret_col_cnt)
{
	u_int	col_cnt;
	xtWord4	len;
	size_t	size = 0;
	u_int	i;

	col_cnt = ot->ot_table->tab_dic.dic_no_of_cols;
	if (ret_col_cnt)
		col_cnt = *ret_col_cnt;
 	for (i=0; i<col_cnt; i++) {
		if (size + 1 > buffer_size)
			goto done;
 		switch (*source_buf) {
			case 255: // Indicate NULL value
				size++;
				source_buf++;
				break;
			case 254: // 2 bytes length
				if (size + 3 > buffer_size)
					goto done;
				len = XT_GET_DISK_2(source_buf + 1);
				if (size + 3 + len > buffer_size)
					goto done;
				size += 3 + len;
				source_buf += 3 + len;
				break;
			case 253: // 3 bytes length
				if (size + 4 > buffer_size)
					goto done;
				len = XT_GET_DISK_3(source_buf + 1);
				if (size + 4 + len > buffer_size)
					goto done;
				size += 4 + len;
				source_buf += 4 + len;
				break;
			case 252: // 4 bytes length
				if (size + 5 > buffer_size)
					goto done;
				len = XT_GET_DISK_4(source_buf + 1);
				if (size + 5 + len > buffer_size)
					goto done;
				size += 5 + len;
				source_buf += 5 + len;
				break;
			default: // Length byte
				len = *source_buf;
				if (size + 1 + len > buffer_size)
					goto done;
				size += 1 + len;
				source_buf += 1 + len;
				break;
 		}
	}
	
	done:
	if (ret_col_cnt)
		*ret_col_cnt = i;
	return size;
}

/* Unload from PBXT variable length format to the MySQL row format. */
xtPublic xtBool myxt_load_row(XTOpenTablePtr ot, xtWord1 *source_buf, xtWord1 *dest_buff, u_int col_cnt)
{
	TABLE	*table;
	xtWord4	len;
	Field	*curr_field;
	xtBool	is_null;
	u_int	i = 0;

	if (!(table = ot->ot_table->tab_dic.dic_my_table)) {
		xt_register_taberr(XT_REG_CONTEXT, XT_ERR_NO_DICTIONARY, ot->ot_table->tab_name);
		return FAILED;
	}

	/* According to the InnoDB implementation:
	 * "MySQL assumes that all columns
	 * have the SQL NULL bit set unless it
	 * is a nullable column with a non-NULL value".
	 */
	memset(dest_buff, 0xFF, table->s->null_bytes);
 	for (Field **field=table->field ; *field && (!col_cnt || i<col_cnt); field++, i++) {
		curr_field = *field;
 		is_null = FALSE;
 		switch (*source_buf) {
			case 255: // Indicate NULL value
				is_null = TRUE;
				len = 0;
				source_buf++;
				break;
			case 254: // 2 bytes length
				len = XT_GET_DISK_2(source_buf + 1);
				source_buf += 3;
				break;
			case 253: // 3 bytes length
				len = XT_GET_DISK_3(source_buf + 1);
				source_buf += 4;
				break;
			case 252: // 4 bytes length
				len = XT_GET_DISK_4(source_buf + 1);
				source_buf += 5;
				break;
			default: // Length byte
				if (*source_buf > 240) {
					xt_register_xterr(XT_REG_CONTEXT, XT_ERR_BAD_RECORD_FORMAT);
					return FAILED;
				}
				len = *source_buf;
				source_buf++;
				break;
 		}

		if (is_null)
			mx_set_length_and_data(curr_field, (char *) dest_buff, 0, NULL);
		else
			mx_set_length_and_data(curr_field, (char *) dest_buff, len, (char *) source_buf);

		source_buf += len;
 	}
	return OK;
}

xtPublic xtBool myxt_find_column(XTOpenTablePtr ot, u_int *col_idx, const char *col_name)
{
	TABLE	*table = ot->ot_table->tab_dic.dic_my_table;
	u_int	i=0;

	for (Field **field=table->field; *field; field++, i++) {
		if (!my_strcasecmp(system_charset_info, (*field)->field_name, col_name)) {
			*col_idx = i;
			return OK;
		}
	}
	return FALSE;
}

xtPublic void myxt_get_column_name(XTOpenTablePtr ot, u_int col_idx, u_int len, char *col_name)
{
	TABLE	*table = ot->ot_table->tab_dic.dic_my_table;
	Field	*field;

	field = table->field[col_idx];
	xt_strcpy(len, col_name, field->field_name);
}

xtPublic void myxt_get_column_as_string(XTOpenTablePtr ot, char *buffer, u_int col_idx, u_int len, char *value)
{
	XTTableHPtr	tab = ot->ot_table;
	XTThreadPtr self = ot->ot_thread;
	TABLE		*table = tab->tab_dic.dic_my_table;
	Field		*field = table->field[col_idx];
	char		buf_val[MAX_FIELD_WIDTH];
	String		val(buf_val, sizeof(buf_val), &my_charset_bin);

	if (mx_is_null_in_record(field, buffer))
		xt_strcpy(len, value, "NULL");
	else {
		byte	*save;

		/* Required by store() - or an assertion will fail: */
		if (table->read_set)
			MX_BIT_SET(table->read_set, col_idx);

		save = field->ptr;
		xt_lock_mutex(self, &tab->tab_dic_field_lock);
		pushr_(xt_unlock_mutex, &tab->tab_dic_field_lock);
#if MYSQL_VERSION_ID < 50114
		field->ptr = (byte *) buffer + field->offset();
#else
		field->ptr = (byte *) buffer + field->offset(field->table->record[0]);
#endif
		field->val_str(&val);
		field->ptr = save;					// Restore org row pointer
		freer_(); // xt_unlock_mutex(&tab->tab_dic_field_lock)
		xt_strcpy(len, value, val.c_ptr());
	}
}

xtPublic xtBool myxt_set_column(XTOpenTablePtr ot, char *buffer, u_int col_idx, const char *value, u_int len)
{
	XTTableHPtr	tab = ot->ot_table;
	XTThreadPtr self = ot->ot_thread;
	TABLE		*table = tab->tab_dic.dic_my_table;
	Field		*field = table->field[col_idx];
	byte		*save;
	int			error;

	/* Required by store() - or an assertion will fail: */
	if (table->write_set)
		MX_BIT_SET(table->write_set, col_idx);

	mx_set_notnull_in_record(field, buffer);

	save = field->ptr;
	xt_lock_mutex(self, &tab->tab_dic_field_lock);
	pushr_(xt_unlock_mutex, &tab->tab_dic_field_lock);
#if MYSQL_VERSION_ID < 50114
	field->ptr = (byte *) buffer + field->offset();
#else
	field->ptr = (byte *) buffer + field->offset(field->table->record[0]);
#endif
	error = field->store(value, len, &my_charset_utf8_general_ci);
	field->ptr = save;					// Restore org row pointer
	freer_(); // xt_unlock_mutex(&tab->tab_dic_field_lock)
	return error ? FAILED : OK;
}

xtPublic void myxt_get_column_data(XTOpenTablePtr ot, char *buffer, u_int col_idx, char **value, size_t *len)
{
	TABLE	*table = ot->ot_table->tab_dic.dic_my_table;
	Field	*field = table->field[col_idx];
	char	*sdata;
	xtWord4	dlen;

	sdata = mx_get_length_and_data(field, buffer, &dlen);
	*value = sdata;
	*len = dlen;
}

xtPublic xtBool myxt_store_row(XTOpenTablePtr ot, XTTabRecInfoPtr rec_info, char *rec_buff)
{
	if (ot->ot_rec_fixed) {
		rec_info->ri_fix_rec_buf = (XTTabRecFixDPtr) ot->ot_row_wbuffer;
		rec_info->ri_rec_buf_size = ot->ot_rec_size;
		rec_info->ri_ext_rec = NULL;

		rec_info->ri_fix_rec_buf->tr_rec_type_1 = XT_TAB_STATUS_FIXED;
		memcpy(rec_info->ri_fix_rec_buf->rf_data, rec_buff, ot->ot_rec_size - XT_REC_FIX_HEADER_SIZE);
	}
	else {
		xtWord4 row_size;

		if (!(row_size = mx_store_row(ot, XT_REC_EXT_HEADER_SIZE, rec_buff)))
			return FAILED;
		if (row_size - XT_REC_FIX_EXT_HEADER_DIFF <= ot->ot_rec_size) {	
			rec_info->ri_fix_rec_buf = (XTTabRecFixDPtr) &ot->ot_row_wbuffer[XT_REC_FIX_EXT_HEADER_DIFF];
			rec_info->ri_rec_buf_size = row_size - XT_REC_FIX_EXT_HEADER_DIFF;
			rec_info->ri_ext_rec = NULL;

			rec_info->ri_fix_rec_buf->tr_rec_type_1 = XT_TAB_STATUS_VARIABLE;
		}
		else {
			rec_info->ri_fix_rec_buf = (XTTabRecFixDPtr) ot->ot_row_wbuffer;
			rec_info->ri_rec_buf_size = ot->ot_rec_size;
			rec_info->ri_ext_rec = (XTTabRecExtDPtr) ot->ot_row_wbuffer;
			rec_info->ri_log_data_size = row_size - ot->ot_rec_size;
			rec_info->ri_log_buf = (XTactExtRecEntryDPtr) &ot->ot_row_wbuffer[ot->ot_rec_size - offsetof(XTactExtRecEntryDRec, er_data)];

			rec_info->ri_ext_rec->tr_rec_type_1 = XT_TAB_STATUS_EXT_DLOG;
			XT_SET_DISK_4(rec_info->ri_ext_rec->re_log_dat_siz_4, rec_info->ri_log_data_size);
		}
	}
	return OK;
}

static void mx_print_string(uchar *s, uint count)
{
	while (count > 0) {
		if (s[count - 1] != ' ')
			break;
		count--;
	}
	printf("\"");
	for (u_int i=0; i<count; i++, s++)
		printf("%c", *s);
	printf("\"");
}

xtPublic void myxt_print_key(XTIndexPtr ind, xtWord1 *key_value)
{
	register XTIndexSegRec	*keyseg = ind->mi_seg;
	register uchar			*b = (uchar *) key_value;
	uint					b_length;
	uint					pack_len;

	for (u_int i = 0; i < ind->mi_seg_count; i++, keyseg++) {
		if (i!=0)
			printf(" ");
		if (keyseg->null_bit) {
			if (!*b++) {
				printf("NULL");
				continue;
			}
		}
		switch ((enum ha_base_keytype) keyseg->type) {
			case HA_KEYTYPE_TEXT:											 /* Ascii; Key is converted */
				if (keyseg->flag & HA_SPACE_PACK) {
					get_key_pack_length(b_length, pack_len, b);
				}
				else
					b_length = keyseg->length;
				mx_print_string(b, b_length);
				b += b_length;
				break;
			case HA_KEYTYPE_LONG_INT: {
				int32 l_2 = sint4korr(b);
				b += keyseg->length;
				printf("%ld", (long) l_2);
				break;
			}
			case HA_KEYTYPE_ULONG_INT: {
				xtWord4 u_2 = sint4korr(b);
				b += keyseg->length;
				printf("%lu", (u_long) u_2);
				break;
			}
			default:
				break;
		}
	}
}

/*
 * -----------------------------------------------------------------------
 * MySQL Data Dictionary
 */

#define TS(x)					(x)->s

static void my_close_table(TABLE *table)
{
#ifdef DRIZZLED
	TABLE_SHARE	*share;

	share = (TABLE_SHARE *) ((char *) table + sizeof(TABLE));
	share->free_table_share();
#else
	closefrm(table, 1);  // TODO: Q, why did Stewart remove this?
#endif
	xt_free_ns(table);
}

/*
 * This function returns NULL if the table cannot be opened 
 * because this is not a MySQL thread.
 */ 
static TABLE *my_open_table(XTThreadPtr self, XTDatabaseHPtr XT_UNUSED(db), XTPathStrPtr tab_path)
{
	THD			*thd = current_thd;
	char		path_buffer[PATH_MAX];
	char		*table_name;
	char		database_name[XT_IDENTIFIER_NAME_SIZE];
	char		*ptr;
	size_t		size;
	char		*buffer, *path, *db_name, *name;
	TABLE_SHARE	*share;
	int			error;
	TABLE		*table;

	/* If we have no MySQL thread, then we cannot open this table!
	 * What this means is the thread is probably the sweeper or the
	 * compactor.
	 */
	if (!thd)
		return NULL;

	/* GOTCHA: Check if the table name is a partitian,
	 * if so we need to remove the partition
	 * extension, in order for this to work!
	 *
	 * Reason: the parts of a partition table do not
	 * have .frm files!!
	 */
	xt_strcpy(PATH_MAX, path_buffer, tab_path->ps_path);
	table_name = xt_last_name_of_path(path_buffer);
	if ((ptr = strstr(table_name, "#P#")))
		*ptr = 0;

	xt_2nd_last_name_of_path(XT_IDENTIFIER_NAME_SIZE, database_name, path_buffer);

	size = sizeof(TABLE) + sizeof(TABLE_SHARE) + 
		strlen(path_buffer) + 1 +
		strlen(database_name) + 1 + strlen(table_name) + 1;
	if (!(buffer = (char *) xt_malloc(self, size)))
		return NULL;
	table = (TABLE *) buffer;
	buffer += sizeof(TABLE);
	share = (TABLE_SHARE *) buffer;
	buffer += sizeof(TABLE_SHARE);

	path = buffer;
	strcpy(path, path_buffer);
	buffer += strlen(path_buffer) + 1;
	db_name = buffer;
	strcpy(db_name, database_name);
	buffer += strlen(database_name) + 1;
	name = buffer;
	strcpy(name, table_name);

	/* Required to call 'open_table_from_share'! */
	LEX *old_lex, new_lex;

	old_lex = thd->lex;
	thd->lex = &new_lex;
	new_lex.current_select= NULL;
	lex_start(thd);

#ifdef DRIZZLED
	share->init(db_name, 0, name, path);
	if ((error = open_table_def(thd, share)) ||
		(error = open_table_from_share(thd, share, "", 0, (uint32_t) READ_ALL, 0, table, OTM_OPEN)))
	{
		xt_free(self, table);
		lex_end(&new_lex);
		thd->lex = old_lex;
		xt_throw_ulxterr(XT_CONTEXT, XT_ERR_LOADING_MYSQL_DIC, (u_long) error);
		return NULL;
	}
#else
#if MYSQL_VERSION_ID < 60000
#if MYSQL_VERSION_ID < 50123
	init_tmp_table_share(share, db_name, 0, name, path);
#else
	init_tmp_table_share(thd, share, db_name, 0, name, path);
#endif
#else
#if MYSQL_VERSION_ID < 60004
	init_tmp_table_share(share, db_name, 0, name, path);
#else
	init_tmp_table_share(thd, share, db_name, 0, name, path);
#endif
#endif

	if ((error = open_table_def(thd, share, 0))) {
		xt_free(self, table);
		lex_end(&new_lex);
		thd->lex = old_lex;
		xt_throw_ulxterr(XT_CONTEXT, XT_ERR_LOADING_MYSQL_DIC, (u_long) error);
		return NULL;
	}

#if MYSQL_VERSION_ID >= 60003
	if ((error = open_table_from_share(thd, share, "", 0, (uint) READ_ALL, 0, table, OTM_OPEN)))
#else
	if ((error = open_table_from_share(thd, share, "", 0, (uint) READ_ALL, 0, table, FALSE)))
#endif
	{
		xt_free(self, table);
		lex_end(&new_lex);
		thd->lex = old_lex;
		xt_throw_ulxterr(XT_CONTEXT, XT_ERR_LOADING_MYSQL_DIC, (u_long) error);
		return NULL;
	}
#endif

	lex_end(&new_lex);
	thd->lex = old_lex;

	/* GOTCHA: I am the plug-in!!! Therefore, I should not hold 
	 * a reference to myself. By holding this reference I prevent
	 * plugin_shutdown() and reap_plugins() in sql_plugin.cc
	 * from doing their job on shutdown!
	 */
#ifndef DRIZZLED
	plugin_unlock(NULL, table->s->db_plugin);
	table->s->db_plugin = NULL;
#endif
	return table;
}

/*
static bool my_match_index(XTDDIndex *ind, KEY *index)
{
	KEY_PART_INFO	*key_part;
	KEY_PART_INFO	*key_part_end;
	u_int			j;
	XTDDColumnRef	*cref;

	if (index->key_parts != ind->co_cols.size())
		return false;

	j=0;
	key_part_end = index->key_part + index->key_parts;
	for (key_part = index->key_part; key_part != key_part_end; key_part++, j++) {
		if (!(cref = ind->co_cols.itemAt(j)))
			return false;
		if (myxt_strcasecmp(cref->cr_col_name, (char *) key_part->field->field_name) != 0)
			return false;
	}

	if (ind->co_type == XT_DD_KEY_PRIMARY) {
		if (!(index->flags & HA_NOSAME))
			return false;
	}
	else {
		if (ind->co_type == XT_DD_INDEX_UNIQUE) {
			if (!(index->flags & HA_NOSAME))
				return false;
		}
		if (ind->co_ind_name) {
			if (myxt_strcasecmp(ind->co_ind_name, index->name) != 0)
				return false;
		}
	}

	return true;
}

static XTDDIndex *my_find_index(XTDDTable *dd_tab, KEY *index)
{
	XTDDIndex *ind;

	for (u_int i=0; i<dd_tab->dt_indexes.size(); i++)
	{
		ind = dd_tab->dt_indexes.itemAt(i);
		if (my_match_index(ind, index))
			return ind;
	}
	return NULL;
}
*/

static void my_deref_index_data(struct XTThread *self, XTIndexPtr mi)
{
	enter_();
	/* The dirty list of cache pages should be empty here! */
	ASSERT(!mi->mi_dirty_list);
	ASSERT(!mi->mi_free_list);

	xt_free_mutex(&mi->mi_flush_lock);
	xt_spinlock_free(self, &mi->mi_dirty_lock);
	XT_INDEX_FREE_LOCK(self, mi);
	myxt_bitmap_free(self, &mi->mi_col_map);
	if (mi->mi_free_list)
		xt_free(self, mi->mi_free_list);

	xt_free(self, mi);
	exit_();
}

static xtBool my_is_not_null_int4(XTIndexSegPtr seg)
{
	return (seg->type == HA_KEYTYPE_LONG_INT && !(seg->flag & HA_NULL_PART));
}

/* MY_BITMAP definition in Drizzle does not like if
 * I use a NULL pointer to calculate the offset!?
 */
#define MX_OFFSETOF(x, y)		((size_t)(&((x *) 8)->y) - 8)

/* Derived from ha_myisam::create and mi_create */
static XTIndexPtr my_create_index(XTThreadPtr self, TABLE *table_arg, u_int idx, KEY *index)
{
	XTIndexPtr				ind;
	KEY_PART_INFO			*key_part;
	KEY_PART_INFO			*key_part_end;
	XTIndexSegRec			*seg;
	Field					*field;
	enum ha_base_keytype	type;
	uint					options = 0;
	u_int					key_length = 0;
	xtBool					partial_field;

	enter_();

	pushsr_(ind, my_deref_index_data, (XTIndexPtr) xt_calloc(self, MX_OFFSETOF(XTIndexRec, mi_seg) + sizeof(XTIndexSegRec) * index->key_parts));

	XT_INDEX_INIT_LOCK(self, ind);
	xt_init_mutex_with_autoname(self, &ind->mi_flush_lock);
	xt_spinlock_init_with_autoname(self, &ind->mi_dirty_lock);
	ind->mi_index_no = idx;
	ind->mi_flags = (index->flags & (HA_NOSAME | HA_NULL_ARE_EQUAL | HA_UNIQUE_CHECK));
	ind->mi_low_byte_first = TS(table_arg)->db_low_byte_first;
	ind->mi_fix_key = TRUE;
	ind->mi_select_total = 0;
	ind->mi_subset_of = 0;
	myxt_bitmap_init(self, &ind->mi_col_map, TS(table_arg)->fields);
	
	ind->mi_seg_count = (uint) index->key_parts;
	key_part_end = index->key_part + index->key_parts;
	seg = ind->mi_seg;
	for (key_part = index->key_part; key_part != key_part_end; key_part++, seg++) {
		partial_field = FALSE;
		field = key_part->field;

		type = field->key_type();
		seg->flag = key_part->key_part_flag;

		if (options & HA_OPTION_PACK_KEYS ||
			(index->flags & (HA_PACK_KEY | HA_BINARY_PACK_KEY | HA_SPACE_PACK_USED)))
		{
			if (key_part->length > 8 && (type == HA_KEYTYPE_TEXT || type == HA_KEYTYPE_NUM ||
				(type == HA_KEYTYPE_BINARY && !field->zero_pack())))
			{
				/* No blobs here */
				if (key_part == index->key_part)
					ind->mi_flags |= HA_PACK_KEY;
#ifndef DRIZZLED
				if (!(field->flags & ZEROFILL_FLAG) &&
					(field->type() == MYSQL_TYPE_STRING ||
					field->type() == MYSQL_TYPE_VAR_STRING ||
					((int) (key_part->length - field->decimals())) >= 4))
	    			seg->flag |= HA_SPACE_PACK;
#endif
			}
		}

		seg->col_idx = field->field_index;
		seg->is_recs_in_range = 1;
		seg->is_selectivity = 1;
		seg->type = (int) type;
		seg->start = key_part->offset;
		seg->length = key_part->length;
		seg->bit_start = seg->bit_end = 0;
		seg->bit_length = seg->bit_pos = 0;
		seg->charset = field->charset();

		if (field->null_ptr) {
			key_length++;
			seg->flag |= HA_NULL_PART;
			seg->null_bit = field->null_bit;
			seg->null_pos = (uint) (field->null_ptr - (uchar*) table_arg->record[0]);
		}
		else {
			seg->null_bit = 0;
			seg->null_pos = 0;
		}

		if (field->real_type() == MYSQL_TYPE_ENUM
#ifndef DRIZZLED
			|| field->real_type() == MYSQL_TYPE_SET
#endif
			) {
			/* This values are not indexed as string!!
			 * The index will not be built correctly if this value is non-NULL.
			 */
			seg->charset = NULL;
		}

		if (field->type() == MYSQL_TYPE_BLOB
#ifndef DRIZZLED
			|| field->type() == MYSQL_TYPE_GEOMETRY
#endif
			) {
			seg->flag |= HA_BLOB_PART;
			/* save number of bytes used to pack length */
			seg->bit_start = (uint) (field->pack_length() - TS(table_arg)->blob_ptr_size);
		}
#ifndef DRIZZLED
		else if (field->type() == MYSQL_TYPE_BIT) {
			seg->bit_length = ((Field_bit *) field)->bit_len;
			seg->bit_start = ((Field_bit *) field)->bit_ofs;
			seg->bit_pos = (uint) (((Field_bit *) field)->bit_ptr - (uchar*) table_arg->record[0]);
		}
#else
		/* Drizzle uses HA_KEYTYPE_ULONG_INT keys for enums > 1 byte, which is not consistent with MySQL, so we fix it here  */
		else if (field->type() == MYSQL_TYPE_ENUM) {
			switch (seg->length) {
				case 2: 
					seg->type = HA_KEYTYPE_USHORT_INT;
					break;
				case 3:
					seg->type = HA_KEYTYPE_UINT24;
					break;
			}
		}
#endif

		switch (seg->type) {
			case HA_KEYTYPE_VARTEXT1:
			case HA_KEYTYPE_VARTEXT2:
			case HA_KEYTYPE_VARBINARY1:
			case HA_KEYTYPE_VARBINARY2:
				if (!(seg->flag & HA_BLOB_PART)) {
					/* Make a flag that this is a VARCHAR */
					seg->flag |= HA_VAR_LENGTH_PART;
					/* Store in bit_start number of bytes used to pack the length */
					seg->bit_start = ((seg->type == HA_KEYTYPE_VARTEXT1 || seg->type == HA_KEYTYPE_VARBINARY1) ? 1 : 2);
				}
				break;
		}

		/* All packed fields start with a length (1 or 3 bytes): */
		if (seg->flag & (HA_VAR_LENGTH_PART | HA_BLOB_PART | HA_SPACE_PACK)) {
			key_length++;				/* At least one length byte */
			if (seg->length >= 255)	/* prefix may be 3 bytes */
	    		key_length +=2;
		}

		key_length += seg->length;
		if (seg->length > 40)
			ind->mi_fix_key = FALSE;

		/* Determine if only part of the field is in the key:
		 * This is important for index coverage!
		 * Note, BLOB fields are never retrieved from
		 * an index!
		 */
		if (field->type() == MYSQL_TYPE_BLOB)
			partial_field = TRUE;
		else if (field->real_type() == MYSQL_TYPE_VARCHAR		// For varbinary type
#ifndef DRIZZLED
			|| field->real_type() == MYSQL_TYPE_VAR_STRING		// For varbinary type
			|| field->real_type() == MYSQL_TYPE_STRING			// For binary type
#endif
			)
		{
			Field	*tab_field = table_arg->field[key_part->fieldnr-1];
			u_int	field_len = tab_field->key_length();

			if (key_part->length != field_len)
				partial_field = TRUE;
		}

		/* NOTE: do not set if the field is only partially in the index!!! */
		if (!partial_field)
			MX_BIT_FAST_TEST_AND_SET(&ind->mi_col_map, field->field_index);
	}

	if (key_length > XT_INDEX_MAX_KEY_SIZE)
		xt_throw_sulxterr(XT_CONTEXT, XT_ERR_KEY_TOO_LARGE, index->name, (u_long) XT_INDEX_MAX_KEY_SIZE);

	/* This is the maximum size of the index on disk: */
	ind->mi_key_size = key_length;
	ind->mi_max_items = (XT_INDEX_PAGE_SIZE-2) / (key_length+XT_RECORD_REF_SIZE);

	if (ind->mi_fix_key) {
		/* Special case for not-NULL 4 byte int value: */
		switch (ind->mi_seg_count) {
			case 1:
				ind->mi_single_type = ind->mi_seg[0].type;
				if (ind->mi_seg[0].type == HA_KEYTYPE_LONG_INT ||
					ind->mi_seg[0].type == HA_KEYTYPE_ULONG_INT) {
					if (!(ind->mi_seg[0].flag & HA_NULL_PART))
						ind->mi_scan_branch = xt_scan_branch_single;
				}
				break;
			case 2:
				if (my_is_not_null_int4(&ind->mi_seg[0]) &&
					my_is_not_null_int4(&ind->mi_seg[1])) {
					ind->mi_scan_branch = xt_scan_branch_fix_simple;
					ind->mi_simple_comp_key = xt_compare_2_int4;
				}
				break;
			case 3:
				if (my_is_not_null_int4(&ind->mi_seg[0]) &&
					my_is_not_null_int4(&ind->mi_seg[1]) &&
					my_is_not_null_int4(&ind->mi_seg[2])) {
					ind->mi_scan_branch = xt_scan_branch_fix_simple;
					ind->mi_simple_comp_key = xt_compare_3_int4;
				}
				break;
		}
		if (!ind->mi_scan_branch)
			ind->mi_scan_branch = xt_scan_branch_fix;
		ind->mi_prev_item = xt_prev_branch_item_fix;
		ind->mi_last_item = xt_last_branch_item_fix;
	}
	else {
		ind->mi_scan_branch = xt_scan_branch_var;
		ind->mi_prev_item = xt_prev_branch_item_var;
		ind->mi_last_item = xt_last_branch_item_var;
	}
	ind->mi_lazy_delete = ind->mi_fix_key && ind->mi_max_items >= 4;

	XT_NODE_ID(ind->mi_root) = 0;

	popr_(); // Discard my_deref_index_data(ind)

	return_(ind);
}

/* We estimate the size of BLOBs depending on the number
 * of BLOBs in the table.
 */
static u_int mx_blob_field_size_total[] = {
	500,	// 1
	400,	// 2
	350,	// 3
	320,	// 4
	300,	// 5
	280,	// 6
	260,	// 7
	240,	// 8
	220,	// 9
	210		// 10
};

static u_int mxvarchar_field_min_ave[] = {
	120,	// 1
	105,	// 2
	90,		// 3
	65,		// 4
	50,		// 5
	40,		// 6
	40,		// 7
	40,		// 8
	40,		// 9
	40		// 10
};

xtPublic void myxt_setup_dictionary(XTThreadPtr self, XTDictionaryPtr dic)
{
	TABLE	*my_tab = dic->dic_my_table;
	u_int	field_count;
	u_int	var_field_count = 0;
	u_int	varchar_field_count = 0;
	u_int	blob_field_count = 0;
	u_int	large_blob_field_count = 0;
	xtWord8 min_data_size = 0;
	xtWord8 max_data_size = 0;
	xtWord8 ave_data_size = 0;
	xtWord8 min_row_size = 0;
	xtWord8 max_row_size = 0;
	xtWord8 ave_row_size = 0;
	xtWord8 min_ave_row_size = 0;
	xtWord8 max_ave_row_size = 0;
	u_int	dic_rec_size;
	xtBool	dic_rec_fixed;
	Field	*curr_field;
	Field	**field;

	/* How many columns are required for all indexes. */
	KEY				*index;
	KEY_PART_INFO	*key_part;
	KEY_PART_INFO	*key_part_end;

#ifndef XT_USE_LAZY_DELETE
	dic->dic_no_lazy_delete = TRUE;
#endif

	dic->dic_ind_cols_req = 0;
	for (uint i=0; i<TS(my_tab)->keys; i++) {
		index = &my_tab->key_info[i];

		key_part_end = index->key_part + index->key_parts;
		for (key_part = index->key_part; key_part != key_part_end; key_part++) {
			curr_field = key_part->field;

			if ((u_int) curr_field->field_index+1 > dic->dic_ind_cols_req)
				dic->dic_ind_cols_req = curr_field->field_index+1;
		}
	}

	/* We will work out how many columns are required for all blobs: */
	dic->dic_blob_cols_req = 0;	
	field_count = 0;
 	for (field=my_tab->field; (curr_field = *field); field++) {
 		field_count++;
 		min_data_size = curr_field->key_length();
 		max_data_size = curr_field->key_length();
		enum_field_types tno = curr_field->type();

		min_ave_row_size = 40;
		max_ave_row_size = 128;
 		if (tno == MYSQL_TYPE_BLOB) {
			blob_field_count++;
			min_data_size = 0;
			max_data_size = ((Field_blob *) curr_field)->max_data_length();
			/* Set the average length higher for BLOBs: */
			if (max_data_size == 0xFFFF ||
				max_data_size == 0xFFFFFF) {
				if (large_blob_field_count < 10)
					max_ave_row_size = mx_blob_field_size_total[large_blob_field_count];
				else
					max_ave_row_size = 200;
				large_blob_field_count++;
			}
			else if (max_data_size == 0xFFFFFFFF) {
				/* Scale the estimated size of the blob depending on how many BLOBs
				 * are in the table!
				 */
				if (large_blob_field_count < 10)
					max_ave_row_size = mx_blob_field_size_total[large_blob_field_count];
				else
					max_ave_row_size = 200;
				large_blob_field_count++;
				if ((u_int) curr_field->field_index+1 > dic->dic_blob_cols_req)
					dic->dic_blob_cols_req = curr_field->field_index+1;
				dic->dic_blob_count++;
				xt_realloc(self, (void **) &dic->dic_blob_cols, sizeof(Field *) * dic->dic_blob_count);
				dic->dic_blob_cols[dic->dic_blob_count-1] = curr_field;
			}
		}
		else if (tno == MYSQL_TYPE_VARCHAR
#ifndef DRIZZLED
			|| tno == MYSQL_TYPE_VAR_STRING
#endif
			) {
			/* GOTCHA: MYSQL_TYPE_VAR_STRING does not exist as MYSQL_TYPE_VARCHAR define, but
			 * is used when creating a table with
			 * VARCHAR()
			 */
			min_data_size = 0;
			if (varchar_field_count < 10)
				min_ave_row_size = mxvarchar_field_min_ave[varchar_field_count];
			else
				min_ave_row_size = 40;
			varchar_field_count++;
		}

 		if (max_data_size == min_data_size)
 			ave_data_size = max_data_size;
 		else {
 			var_field_count++;
			/* Take the average a 25% of the maximum: */
 			ave_data_size = max_data_size / 4;

			/* Set the average based on min and max parameters: */
 			if (ave_data_size < min_ave_row_size)
 				ave_data_size = min_ave_row_size;
 			else if (ave_data_size > max_ave_row_size)
 				ave_data_size = max_ave_row_size;

 			if (ave_data_size > max_data_size)
 				ave_data_size = max_data_size;
		}

		/* Add space for the length indicators: */
		if (min_data_size <= 240)
			min_row_size += 1 + min_data_size;
		else if (min_data_size <= 0xFFFF)
			min_row_size += 3 + min_data_size;
		else if (min_data_size <= 0xFFFFFF)
			min_row_size += 4 + min_data_size;
		else
			min_row_size += 5 + min_data_size;

		if (max_data_size <= 240)
			max_row_size += 1 + max_data_size;
		else if (max_data_size <= 0xFFFF)
			max_row_size += 3 + max_data_size;
		else if (max_data_size <= 0xFFFFFF)
			max_row_size += 4 + max_data_size;
		else
			max_row_size += 5 + max_data_size;

		if (ave_data_size <= 240)
			ave_row_size += 1 + ave_data_size;
		else /* Should not be more than this! */
			ave_row_size += 3 + ave_data_size;

		/* This is the length of the record required for all indexes: */
		if (field_count + 1 == dic->dic_ind_cols_req)
			dic->dic_ind_rec_len = max_data_size;
 	}

	dic->dic_min_row_size = min_row_size;
	dic->dic_max_row_size = max_row_size;
	dic->dic_ave_row_size = ave_row_size;
	dic->dic_no_of_cols = field_count;

	if (dic->dic_def_ave_row_size) {
		/* The average row size has been set: */
		dic_rec_size = offsetof(XTTabRecFix, rf_data) + TS(my_tab)->reclength;

		/* The conditions for a fixed record are: */
		if (dic->dic_def_ave_row_size >= (xtWord8) TS(my_tab)->reclength &&
			dic_rec_size <= XT_TAB_MAX_FIX_REC_LENGTH &&
			!blob_field_count) {
			dic_rec_fixed = TRUE;
		}
		else {
			xtWord8 new_rec_size;

			dic_rec_fixed = FALSE;
			if (dic->dic_def_ave_row_size > max_row_size)
				new_rec_size = offsetof(XTTabRecFix, rf_data) + max_row_size;
			else
				new_rec_size = offsetof(XTTabRecFix, rf_data) + dic->dic_def_ave_row_size;

			/* The maximum record size 64K for explicit AVG_ROW_LENGTH! */
			if (new_rec_size > XT_TAB_MAX_FIX_REC_LENGTH_SPEC)
				new_rec_size = XT_TAB_MAX_FIX_REC_LENGTH_SPEC;

			dic_rec_size = (u_int) new_rec_size;
		}
	}
	else {
		/* If the average size is within 10% if of the maximum size, then we
		 * we handle these rows as fixed size rows.
		 * Fixed size rows use the internal MySQL format.
		 */
		dic_rec_size = offsetof(XTTabRecFix, rf_data) + TS(my_tab)->reclength;
		/* Fixed length records must be less than 16K in size,
		 * have an average size which is very close (20%) to the maximum size or
		 * be less than a minimum size,
		 * and not contain any BLOBs:
		 */
		if (dic_rec_size <= XT_TAB_MAX_FIX_REC_LENGTH &&
			(ave_row_size + ave_row_size / 4 >= max_row_size ||
			dic_rec_size < XT_TAB_MIN_VAR_REC_LENGTH) &&
			!blob_field_count) {
			dic_rec_fixed = TRUE;
		}
		else {
			dic_rec_fixed = FALSE;
			/* Note I add offsetof(XTTabRecFix, rf_data) insteard of
			 * offsetof(XTTabRecExt, re_data) here!
			 * The reason is that, we want to include the average size
			 * record in the fixed data part. To do this we only need to
			 * calculate a fixed header size, because in the cases in which
			 * it fits, we will only be using a fixed header!
			 */
			dic_rec_size = (u_int) (offsetof(XTTabRecFix, rf_data) + ave_row_size);
			/* The maximum record size (16K for autorow sizing)! */
			if (dic_rec_size > XT_TAB_MAX_FIX_REC_LENGTH)
				dic_rec_size = XT_TAB_MAX_FIX_REC_LENGTH;
		}
	}

	if (!dic->dic_rec_size) {
		dic->dic_rec_size = dic_rec_size;
		dic->dic_rec_fixed = dic_rec_fixed;
	}
	else {
		/* This just confirms that our original calculation on
		 * create table agrees with the current calculation.
		 * (i.e. if non-zero values were loaded from the table).
		 *
		 * It may be the criteria for calculating the data record size
		 * and whether to used a fixed or variable record has changed,
		 * but we need to stick to the current physical layout of the
		 * table.
		 *
		 * Note that this can occur in rename table when the
		 * method of calculation has changed.
		 *
		 * On rename, the format of the table does not change, so we
		 * will not take the calculated values.
		 */
		//ASSERT(dic->dic_rec_size == dic_rec_size);
		//ASSERT(dic->dic_rec_fixed == dic_rec_fixed);
	}

	if (dic_rec_fixed) {
		/* Recalculate the length of the required required to address all
		 * index columns!
		 */		 
		if (field_count == dic->dic_ind_cols_req)
			dic->dic_ind_rec_len = TS(my_tab)->reclength;
		else {
			field=my_tab->field;
			
			curr_field = field[dic->dic_ind_cols_req];
#if MYSQL_VERSION_ID < 50114
			dic->dic_ind_rec_len = curr_field->offset();
#else
			dic->dic_ind_rec_len = curr_field->offset(curr_field->table->record[0]);
#endif
		}
	}

	/* We now calculate how many of the first columns in the row
	 * will definitely fit into the buffer, when the record is
	 * of type extended.
	 *
	 * In this way we can figure out if we need to load the extended
	 * record at all.
	 */
	dic->dic_fix_col_count = 0;
	if (!dic_rec_fixed) {
		xtWord8 max_rec_size = offsetof(XTTabRecExt, re_data);

		for (Field **f=my_tab->field; (curr_field = *f); f++) {
			max_data_size = curr_field->key_length();
			enum_field_types tno = curr_field->type();
			if (tno == MYSQL_TYPE_BLOB)
				max_data_size = ((Field_blob *) curr_field)->max_data_length();
			if (max_data_size <= 240)
				max_rec_size += 1 + max_data_size;
			else if (max_data_size <= 0xFFFF)
				max_rec_size += 3 + max_data_size;
			else if (max_data_size <= 0xFFFFFF)
				max_rec_size += 4 + max_data_size;
			else
				max_rec_size += 5 + max_data_size;
			if (max_rec_size > (xtWord8) dic_rec_size)
				break;
			dic->dic_fix_col_count++;
		}		
		ASSERT(dic->dic_fix_col_count < dic->dic_no_of_cols);
	}

 	dic->dic_key_count = TS(my_tab)->keys;
	dic->dic_mysql_buf_size = TS(my_tab)->rec_buff_length;
	dic->dic_mysql_rec_size = TS(my_tab)->reclength;
}

static u_int my_get_best_superset(XTThreadPtr XT_UNUSED(self), XTDictionaryPtr dic, XTIndexPtr ind)
{
	XTIndexPtr	super_ind;
	u_int		super = 0;
	u_int		super_seg_count = ind->mi_seg_count;

	for (u_int i=0; i<dic->dic_key_count; i++) {
		super_ind = dic->dic_keys[i];
		if (ind->mi_index_no != super_ind->mi_index_no &&
			super_seg_count < super_ind->mi_seg_count) {
			for (u_int j=0; j<ind->mi_seg_count; j++) {
				if (ind->mi_seg[j].col_idx != super_ind->mi_seg[j].col_idx)
					goto next;
			}
			super_seg_count = super_ind->mi_seg_count;
			super = i+1;
			next:;
		}
	}
	return super;
}

/*
 * Return FAILED if the MySQL dictionary is not available.
 */
xtPublic xtBool myxt_load_dictionary(XTThreadPtr self, XTDictionaryPtr dic, XTDatabaseHPtr db, XTPathStrPtr tab_path)
{
	TABLE *my_tab;

	if (!(my_tab = my_open_table(self, db, tab_path)))
		return FAILED;
	dic->dic_my_table = my_tab;
	dic->dic_def_ave_row_size = (xtWord8) my_tab->s->avg_row_length;
	myxt_setup_dictionary(self, dic);
	dic->dic_keys = (XTIndexPtr *) xt_calloc(self, sizeof(XTIndexPtr) * TS(my_tab)->keys);
	for (uint i=0; i<TS(my_tab)->keys; i++)
		dic->dic_keys[i] = my_create_index(self, my_tab, i, &my_tab->key_info[i]);

	/* Check if any key is a subset of another: */
	for (u_int i=0; i<dic->dic_key_count; i++)
		dic->dic_keys[i]->mi_subset_of = my_get_best_superset(self, dic, dic->dic_keys[i]);

	return OK;
}

xtPublic void myxt_free_dictionary(XTThreadPtr self, XTDictionaryPtr dic)
{
	if (dic->dic_table) {
		dic->dic_table->release(self);
		dic->dic_table = NULL;
	}

	if (dic->dic_my_table) {
		my_close_table(dic->dic_my_table);
		dic->dic_my_table = NULL;
	}

	if (dic->dic_blob_cols) {
		xt_free(self, dic->dic_blob_cols);
		dic->dic_blob_cols = NULL;
	}
	dic->dic_blob_count = 0;

	/* If we have opened a table, then this data is freed with the dictionary: */
	if (dic->dic_keys) {
		for (uint i=0; i<dic->dic_key_count; i++) {
			if (dic->dic_keys[i])
				my_deref_index_data(self, (XTIndexPtr) dic->dic_keys[i]);
		}
		xt_free(self, dic->dic_keys);
		dic->dic_key_count = 0;
		dic->dic_keys = NULL;
	}
}

xtPublic void myxt_move_dictionary(XTDictionaryPtr dic, XTDictionaryPtr source_dic)
{
	dic->dic_my_table = source_dic->dic_my_table;
	source_dic->dic_my_table = NULL;

	if (!dic->dic_rec_size) {
		dic->dic_rec_size = source_dic->dic_rec_size;
		dic->dic_rec_fixed = source_dic->dic_rec_fixed;
	}
	else {
		/* This just confirms that our original calculation on
		 * create table agrees with the current calculation.
		 * (i.e. if non-zero values were loaded from the table).
		 *
		 * It may be the criteria for calculating the data record size
		 * and whether to used a fixed or variable record has changed,
		 * but we need to stick to the current physical layout of the
		 * table.
		 */
		ASSERT_NS(dic->dic_rec_size == source_dic->dic_rec_size);
		ASSERT_NS(dic->dic_rec_fixed == source_dic->dic_rec_fixed);
	}

	dic->dic_tab_flags = source_dic->dic_tab_flags;
	dic->dic_blob_cols_req = source_dic->dic_blob_cols_req;
	dic->dic_blob_count = source_dic->dic_blob_count;
	dic->dic_blob_cols = source_dic->dic_blob_cols;
	source_dic->dic_blob_cols = NULL;

	dic->dic_mysql_buf_size = source_dic->dic_mysql_buf_size;
	dic->dic_mysql_rec_size = source_dic->dic_mysql_rec_size;
 	dic->dic_key_count = source_dic->dic_key_count;
	dic->dic_keys = source_dic->dic_keys;

	/* Set this to zero, bcause later xt_flush_tables() may be called. 
	 * This can occur when using the BLOB streaming engine,
	 * in command ALTER TABLE x ENGINE = PBXT;
	 */
	source_dic->dic_key_count = 0;
	source_dic->dic_keys = NULL;

	dic->dic_min_row_size = source_dic->dic_min_row_size;
	dic->dic_max_row_size = source_dic->dic_max_row_size;
	dic->dic_ave_row_size = source_dic->dic_ave_row_size;
	dic->dic_def_ave_row_size = source_dic->dic_def_ave_row_size;

	dic->dic_no_of_cols = source_dic->dic_no_of_cols;
 	dic->dic_fix_col_count = source_dic->dic_fix_col_count;
 	dic->dic_ind_cols_req = source_dic->dic_ind_cols_req;
 	dic->dic_ind_rec_len = source_dic->dic_ind_rec_len;
}

static void my_free_dd_table(XTThreadPtr self, XTDDTable *dd_tab)
{
	if (dd_tab)
		dd_tab->release(self);
}

static void ha_create_dd_index(XTThreadPtr self, XTDDIndex *ind, KEY *key)
{
	KEY_PART_INFO	*key_part;
	KEY_PART_INFO	*key_part_end;
	XTDDColumnRef	*cref;

	if (strcmp(key->name, "PRIMARY") == 0)
		ind->co_type = XT_DD_KEY_PRIMARY;
	else if (key->flags & HA_NOSAME)
		ind->co_type = XT_DD_INDEX_UNIQUE;
	else
		ind->co_type = XT_DD_INDEX;

	if (ind->co_type == XT_DD_KEY_PRIMARY)
		ind->co_name = xt_dup_string(self, key->name);
	else
		ind->co_ind_name = xt_dup_string(self, key->name);

	key_part_end = key->key_part + key->key_parts;
	for (key_part = key->key_part; key_part != key_part_end; key_part++) {
		if (!(cref = new XTDDColumnRef()))
			xt_throw_errno(XT_CONTEXT, XT_ENOMEM);
		ind->co_cols.append(self, cref);
		cref->cr_col_name = xt_dup_string(self, (char *) key_part->field->field_name);
	}
}

static char *my_type_to_string(XTThreadPtr self, Field *field, TABLE *XT_UNUSED(my_tab))
{
	char		buffer[MAX_FIELD_WIDTH + 400], *ptr;
	String		type((char *) buffer, sizeof(buffer), system_charset_info);

	/* GOTCHA:
	 * - Above sets the string length to the same as the buffer,
	 *   so we must set the length to zero.
	 * - The result is not necessarilly zero terminated.
	 * - We cannot assume that the input buffer is the one
	 *   we get back (for example text field).
	 */
	type.length(0);
	field->sql_type(type);
	ptr = type.c_ptr();
	if (ptr != buffer)
		xt_strcpy(sizeof(buffer), buffer, ptr);			

	if (field->has_charset()) {
		/* Always include the charset so that we can compare types
		 * for FK/PK releations.
		 */
		xt_strcat(sizeof(buffer), buffer, " CHARACTER SET ");
		xt_strcat(sizeof(buffer), buffer, (char *) field->charset()->csname);

		/* For string types dump collation name only if 
		 * collation is not primary for the given charset
		 */
		if (!(field->charset()->state & MY_CS_PRIMARY)) {
			xt_strcat(sizeof(buffer), buffer, " COLLATE ");
			xt_strcat(sizeof(buffer), buffer, (char *) field->charset()->name);
		}
	}

	return xt_dup_string(self, buffer); // type.length()
}

xtPublic XTDDTable *myxt_create_table_from_table(XTThreadPtr self, TABLE *my_tab)
{
	XTDDTable		*dd_tab;
	Field			*curr_field;
	XTDDColumn		*col;
	XTDDIndex		*ind;

	if (!(dd_tab = new XTDDTable()))
		xt_throw_errno(XT_CONTEXT, XT_ENOMEM);
	dd_tab->init(self);
	pushr_(my_free_dd_table, dd_tab);

 	for (Field **field=my_tab->field; (curr_field = *field); field++) {
		col = XTDDColumnFactory::createFromMySQLField(self, my_tab, curr_field);
		dd_tab->dt_cols.append(self, col);
	}

	for (uint i=0; i<TS(my_tab)->keys; i++) {
		if (!(ind = (XTDDIndex *) new XTDDIndex(XT_DD_UNKNOWN)))
			xt_throw_errno(XT_CONTEXT, XT_ENOMEM);
		dd_tab->dt_indexes.append(self, ind);
		ind->co_table = dd_tab;
		ind->in_index = i;
		ha_create_dd_index(self, ind, &my_tab->key_info[i]);
	}

	popr_(); // my_free_dd_table(dd_tab)
	return dd_tab;
}

/*
 * -----------------------------------------------------------------------
 * MySQL CHARACTER UTILITIES
 */

xtPublic void myxt_static_convert_identifier(XTThreadPtr XT_UNUSED(self), MX_CHARSET_INFO *cs, char *from, char *to, size_t to_len)
{
	uint errors;

	/*
	 * Bug#4417
	 * Check that identifiers and strings are not converted 
	 * when the client character set is binary.
	 */
	if (cs == &my_charset_utf8_general_ci || cs == &my_charset_bin)
		xt_strcpy(to_len, to, from);
	else
		strconvert(cs, from, &my_charset_utf8_general_ci, to, to_len, &errors);
}

// cs == current_thd->charset()
xtPublic char *myxt_convert_identifier(XTThreadPtr self, MX_CHARSET_INFO *cs, char *from)
{
	uint	errors;
	u_int	len;
	char	*to;

	if (cs == &my_charset_utf8_general_ci || cs == &my_charset_bin)
		to = xt_dup_string(self, from);
	else {
		len = strlen(from) * 3 + 1;
		to = (char *) xt_malloc(self, len);
		strconvert(cs, from, &my_charset_utf8_general_ci, to, len, &errors);
	}
	return to;
}

xtPublic char *myxt_convert_table_name(XTThreadPtr self, char *from)
{
	u_int	len;
	char	*to;

	len = strlen(from) * 5 + 1;
	to = (char *) xt_malloc(self, len);
	tablename_to_filename(from, to, len);
	return to;
}

xtPublic void myxt_static_convert_table_name(XTThreadPtr XT_UNUSED(self), char *from, char *to, size_t to_len)
{
	tablename_to_filename(from, to, to_len);
}

xtPublic void myxt_static_convert_file_name(char *from, char *to, size_t to_len)
{
	filename_to_tablename(from, to, to_len);
}

xtPublic int myxt_strcasecmp(char * a, char *b)
{
	return my_strcasecmp(&my_charset_utf8_general_ci, a, b);
}

xtPublic int myxt_isspace(MX_CHARSET_INFO *cs, char a)
{
	return my_isspace(cs, a);
}

xtPublic int myxt_ispunct(MX_CHARSET_INFO *cs, char a)
{
	return my_ispunct(cs, a);
}

xtPublic int myxt_isdigit(MX_CHARSET_INFO *cs, char a)
{
	return my_isdigit(cs, a);
}

xtPublic MX_CHARSET_INFO *myxt_getcharset(bool convert)
{
	if (convert) {
		THD *thd = current_thd;

		if (thd)
			return thd_charset(thd);
	}
	return &my_charset_utf8_general_ci;
}

xtPublic void *myxt_create_thread()
{
#ifdef DRIZZLED
	return (void *) 1;
#else
	THD *new_thd;

	if (my_thread_init()) {
		xt_register_error(XT_REG_CONTEXT, XT_ERR_MYSQL_ERROR, 0, "Unable to initialize MySQL threading");
		return NULL;
	}

	if (!(new_thd = new THD())) {
		my_thread_end();
		xt_register_error(XT_REG_CONTEXT, XT_ERR_MYSQL_ERROR, 0, "Unable to create MySQL thread (THD)");
		return NULL;
	}

	new_thd->thread_stack = (char *) &new_thd;
	new_thd->store_globals();
	lex_start(new_thd);

	return (void *) new_thd;
#endif
}

#ifdef DRIZZLED
xtPublic void myxt_destroy_thread(void *, xtBool)
{
}
#else
xtPublic void myxt_destroy_thread(void *thread, xtBool end_threads)
{
	THD *thd = (THD *) thread;

#if MYSQL_VERSION_ID > 60005
	/* PMC - This is a HACK! It is required because
	 * MySQL shuts down MDL before shutting down the
	 * plug-ins.
	 */
	if (!pbxt_inited)
		mdl_init();
	close_thread_tables(thd);
	if (!pbxt_inited)
		mdl_destroy();
#else
	close_thread_tables(thd);
#endif

	delete thd;

	/* Remember that we don't have a THD */
	my_pthread_setspecific_ptr(THR_THD, 0);

	if (end_threads)
		my_thread_end();
}
#endif

xtPublic XTThreadPtr myxt_get_self()
{
	THD *thd;
	
	if ((thd = current_thd))
		return xt_ha_thd_to_self(thd);
	return NULL;
}

/*
 * -----------------------------------------------------------------------
 * INFORMATION SCHEMA FUNCTIONS
 *
 */

static int mx_put_record(THD *thd, TABLE *table)
{
	return schema_table_store_record(thd, table);
}

#ifdef UNUSED_CODE
static void mx_put_int(TABLE *table, int column, int value)
{
	table->field[column]->store(value, false);
}

static void mx_put_real8(TABLE *table, int column, xtReal8 value)
{
	table->field[column]->store(value);
}

static void mx_put_string(TABLE *table, int column, const char *string, u_int len, charset_info_st *charset)
{
	table->field[column]->store(string, len, charset);
}
#endif

static void mx_put_u_llong(TABLE *table, int column, u_llong value)
{
	table->field[column]->store(value, false);
}

static void mx_put_string(TABLE *table, int column, const char *string, charset_info_st *charset)
{
	table->field[column]->store(string, strlen(string), charset);
}

xtPublic int myxt_statistics_fill_table(XTThreadPtr self, void *th, void *ta, void *, MX_CONST void *ch)
{
	THD				*thd = (THD *) th;
	TABLE_LIST		*tables = (TABLE_LIST *) ta;
	charset_info_st	*charset = (charset_info_st *) ch;
	TABLE			*table = (TABLE *) tables->table;
	int				err = 0;
	int				col;
	const char		*stat_name;
	u_llong			stat_value;
	XTStatisticsRec	statistics;

	xt_gather_statistics(&statistics);
	for (u_int rec_id=0; !err && rec_id<XT_STAT_CURRENT_MAX; rec_id++) {
		stat_name = xt_get_stat_meta_data(rec_id)->sm_name;
		stat_value = xt_get_statistic(&statistics, self->st_database, rec_id);

		col=0;
		mx_put_u_llong(table, col++, rec_id+1);
		mx_put_string(table, col++, stat_name, charset);
		mx_put_u_llong(table, col++, stat_value);
		err = mx_put_record(thd, table);
	}

	return err;
}

xtPublic void myxt_get_status(XTThreadPtr self, XTStringBufferPtr strbuf)
{
	char string[200];

	xt_sb_concat(self, strbuf, "\n");
	xt_get_now(string, 200);
	xt_sb_concat(self, strbuf, string);
	xt_sb_concat(self, strbuf, " PBXT ");
	xt_sb_concat(self, strbuf, xt_get_version());
	xt_sb_concat(self, strbuf, " STATUS OUTPUT");
	xt_sb_concat(self, strbuf, "\n");

	xt_sb_concat(self, strbuf, "Record cache usage: ");
	xt_sb_concat_int8(self, strbuf, xt_tc_get_usage());
	xt_sb_concat(self, strbuf, "\n");
	xt_sb_concat(self, strbuf, "Record cache size:  ");
	xt_sb_concat_int8(self, strbuf, xt_tc_get_size());
	xt_sb_concat(self, strbuf, "\n");
	xt_sb_concat(self, strbuf, "Record cache high:  ");
	xt_sb_concat_int8(self, strbuf, xt_tc_get_high());
	xt_sb_concat(self, strbuf, "\n");
	xt_sb_concat(self, strbuf, "Index cache usage:  ");
	xt_sb_concat_int8(self, strbuf, xt_ind_get_usage());
	xt_sb_concat(self, strbuf, "\n");
	xt_sb_concat(self, strbuf, "Index cache size:   ");
	xt_sb_concat_int8(self, strbuf, xt_ind_get_size());
	xt_sb_concat(self, strbuf, "\n");
	xt_sb_concat(self, strbuf, "Log cache usage:    ");
	xt_sb_concat_int8(self, strbuf, xt_xlog_get_usage());
	xt_sb_concat(self, strbuf, "\n");
	xt_sb_concat(self, strbuf, "Log cache size:     ");
	xt_sb_concat_int8(self, strbuf, xt_xlog_get_size());
	xt_sb_concat(self, strbuf, "\n");

	xt_ht_lock(self, xt_db_open_databases);
	pushr_(xt_ht_unlock, xt_db_open_databases);

	XTDatabaseHPtr	*dbptr;
	size_t len = xt_sl_get_size(xt_db_open_db_by_id);

	if (len > 0) {
		xt_sb_concat(self, strbuf, "Data log files:\n");
		for (u_int i=0; i<len; i++) {
			dbptr = (XTDatabaseHPtr *) xt_sl_item_at(xt_db_open_db_by_id, i);
			
#ifndef XT_USE_GLOBAL_DB
			xt_sb_concat(self, strbuf, "Database: ");
			xt_sb_concat(self, strbuf, (*dbptr)->db_name);
			xt_sb_concat(self, strbuf, "\n");
#endif
			xt_dl_log_status(self, *dbptr, strbuf);
		}
	}
	else
		xt_sb_concat(self, strbuf, "No data logs in use\n");

	freer_(); // xt_ht_unlock(xt_db_open_databases)
}

/*
 * -----------------------------------------------------------------------
 * MySQL Bit Maps
 */

static void myxt_bitmap_init(XTThreadPtr self, MX_BITMAP *map, u_int n_bits)
{
	my_bitmap_map	*buf;
    uint			size_in_bytes = (((n_bits) + 31) / 32) * 4;

    buf = (my_bitmap_map *) xt_malloc(self, size_in_bytes);
	map->bitmap= buf;
	map->n_bits= n_bits;
	create_last_word_mask(map);
	bitmap_clear_all(map);
}

static void myxt_bitmap_free(XTThreadPtr self, MX_BITMAP *map)
{
	if (map->bitmap) {
		xt_free(self, map->bitmap);
		map->bitmap = NULL;
	}
}

/*
 * -----------------------------------------------------------------------
 * XTDDColumnFactory methods
 */

XTDDColumn *XTDDColumnFactory::createFromMySQLField(XTThread *self, TABLE *my_tab, Field *field)
{
	XTDDEnumerableColumn *en_col;
	XTDDColumn *col;
	xtBool is_enum = FALSE;

	switch(field->real_type()) {
		case MYSQL_TYPE_ENUM:
			is_enum = TRUE;
			/* fallthrough */

#ifndef DRIZZLED
		case MYSQL_TYPE_SET:
#endif
			col = en_col = new XTDDEnumerableColumn();
		    if (!col)
				xt_throw_errno(XT_CONTEXT, XT_ENOMEM); 
			col->init(self);
			en_col->enum_size = ((Field_enum *)field)->typelib->count;
			en_col->is_enum = is_enum;
			break;

		default:
			col = new XTDDColumn();
			if (!col)
				xt_throw_errno(XT_CONTEXT, XT_ENOMEM); 
			col->init(self);
	}

	col->dc_name = xt_dup_string(self, (char *) field->field_name);
	col->dc_data_type = my_type_to_string(self, field, my_tab);
	col->dc_null_ok = field->null_ptr != NULL;

	return col;
}

