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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * 2004-01-03	Paul McCullagh
 *
 * H&G2JCtL
 */

#include "xt_config.h"

#include <stdio.h>
#include <time.h>
#include <ctype.h>
#ifndef XT_WIN
#include <sys/param.h>
#endif

#include "util_xt.h"
#include "strutil_xt.h"
#include "memory_xt.h"

xtPublic int xt_comp_log_pos(xtLogID id1, off_t off1, xtLogID id2, off_t off2)
{
	if (id1 < id2)
		return -1;
	if (id1 > id2)
		return 1;
	if (off1 < off2)
		return -1;
	if (off1 > off2)
		return 1;
	return 0;
}

/*
 * This function returns the current time in micorsonds since
 * 00:00:00 UTC, January 1, 1970.
 * Currently it is accurate to the second :(
 */
xtPublic xtWord8 xt_time_now(void)
{
	xtWord8 ms;

	ms = (xtWord8) time(NULL);
	ms *= 1000000;
	return ms;
}

xtPublic void xt_free_nothing(struct XTThread *XT_UNUSED(thread), void *XT_UNUSED(x))
{
}

/*
 * A file name has the form:
 * <text>-<number>[.<ext>]
 * This function return the number part as a
 * u_long.
 */
xtPublic xtWord4 xt_file_name_to_id(char *file_name)
{
	u_long value = 0;

	if (file_name) {
		char	*num = file_name +  strlen(file_name) - 1;
		
		while (num >= file_name && *num != '-')
			num--;
		num++;
		if (isdigit(*num))
			sscanf(num, "%lu", &value);
	}
	return (xtWord4) value;
}

/*
 * now is moving forward. then is a static time in the
 * future. What is the time difference?
 *
 * These variables can overflow.
 */ 
xtPublic int xt_time_difference(register xtWord4 now, register xtWord4 then)
{
	/* now is after then, so the now time has passed 
	 * then. So we return a negative difference.
	 */
	if (now >= then) {
		/* now has gone past then. If the difference is
		 * great, then we assume an overflow, and reverse!
		 */
		if ((now - then) > (xtWord4) 0xFFFFFFFF/2)
			return (int) (0xFFFFFFFF - (now - then));

		return (int) now - (int) then;
	}
	/* If now is before then, we check the difference.
	 * If the difference is very large, then we assume
	 * that now has gone past then, and overflowed.
	 */
	if ((then - now) > (xtWord4) 0xFFFFFFFF/2)
		return - (int) (0xFFFFFFFF - (then - now));
	return then - now;
}

xtPublic xtWord2 xt_get_checksum(xtWord1 *data, size_t len, u_int interval)
{
	register xtWord4	sum = 0, g;
	xtWord1				*chk;

	chk = data + len - 1;
	while (chk > data) {
		sum = (sum << 4) + *chk;
		if ((g = sum & 0xF0000000)) {
			sum = sum ^ (g >> 24);
			sum = sum ^ g;
		}
		chk -= interval;
	}
	return (xtWord2) (sum ^ (sum >> 16));
}

xtPublic xtWord1 xt_get_checksum1(xtWord1 *data, size_t len)
{
	register xtWord4	sum = 0, g;
	xtWord1				*chk;

	chk = data + len - 1;
	while (chk > data) {
		sum = (sum << 4) + *chk;
		if ((g = sum & 0xF0000000)) {
			sum = sum ^ (g >> 24);
			sum = sum ^ g;
		}
		chk--;
	}
	return (xtWord1) (sum ^ (sum >> 24) ^ (sum >> 16) ^ (sum >> 8));
}

xtPublic xtWord4 xt_get_checksum4(xtWord1 *data, size_t len)
{
	register xtWord4	sum = 0, g;
	xtWord1				*chk;

	chk = data + len - 1;
	while (chk > data) {
		sum = (sum << 4) + *chk;
		if ((g = sum & 0xF0000000)) {
			sum = sum ^ (g >> 24);
			sum = sum ^ g;
		}
		chk--;
	}
	return sum;
}

/*
 * --------------- Data Buffer ------------------
 */

xtPublic xtBool xt_db_set_size(struct XTThread *self, XTDataBufferPtr dbuf, size_t size)
{
	if (dbuf->db_size < size) {
		if (!xt_realloc(self, (void **) &dbuf->db_data, size))
			return FAILED;
		dbuf->db_size = size;
	}
	else if (!size) {
		if (dbuf->db_data)
			xt_free(self, dbuf->db_data);
		dbuf->db_data = NULL;
		dbuf->db_size = 0;
	}
	return OK;
}

/*
 * --------------- Data Buffer ------------------
 */

xtPublic xtBool xt_ib_alloc(struct XTThread *self, XTInfoBufferPtr ib, size_t size)
{
	if (!ib->ib_free) {
		ib->ib_db.db_size = 0;
		ib->ib_db.db_data = NULL;
	}
	if (size <= ib->ib_db.db_size)
		return OK;

	if (size <= XT_IB_DEFAULT_SIZE) {
		ib->ib_db.db_size = XT_IB_DEFAULT_SIZE;
		ib->ib_db.db_data = ib->ib_data;
		return OK;
	}

	if (ib->ib_db.db_data == ib->ib_data) {
		ib->ib_db.db_size = 0;
		ib->ib_db.db_data = NULL;
	}

	ib->ib_free = TRUE;
	return xt_db_set_size(self, &ib->ib_db, size);
}

void xt_ib_free(struct XTThread *self, XTInfoBufferPtr ib)
{
	if (ib->ib_free) {
		xt_db_set_size(self, &ib->ib_db, 0);
		ib->ib_free = FALSE;
	}
}

/*
 * --------------- Basic List ------------------
 */

xtPublic xtBool xt_bl_set_size(struct XTThread *self, XTBasicListPtr bl, size_t size)
{
	if (bl->bl_size < size) {
		if (!xt_realloc(self, (void **) &bl->bl_data, size * bl->bl_item_size))
			return FAILED;
		bl->bl_size = size;
	}
	else if (!size) {
		if (bl->bl_data)
			xt_free(self, bl->bl_data);
		bl->bl_data = NULL;
		bl->bl_size = 0;
		bl->bl_count = 0;
	}
	return OK;
}

xtPublic xtBool xt_bl_dup(struct XTThread *self, XTBasicListPtr from_bl, XTBasicListPtr to_bl)
{
	to_bl->bl_item_size = from_bl->bl_item_size;
	to_bl->bl_size = 0;
	to_bl->bl_count = from_bl->bl_count;
	to_bl->bl_data = NULL;
	if (!xt_bl_set_size(self, to_bl, from_bl->bl_count))
		return FAILED;
	memcpy(to_bl->bl_data, from_bl->bl_data, to_bl->bl_count * to_bl->bl_item_size);
	return OK;
}

xtPublic xtBool xt_bl_append(struct XTThread *self, XTBasicListPtr bl, void *value)
{
	if (bl->bl_count == bl->bl_size) {
		if (!xt_bl_set_size(self, bl, bl->bl_count+1))
			return FAILED;
	}
	memcpy(&bl->bl_data[bl->bl_count * bl->bl_item_size], value, bl->bl_item_size);
	bl->bl_count++;
	return OK;
}

xtPublic void *xt_bl_last_item(XTBasicListPtr bl)
{
	if (!bl->bl_count)
		return NULL;
	return &bl->bl_data[(bl->bl_count-1) * bl->bl_item_size];
}

xtPublic void *xt_bl_item_at(XTBasicListPtr bl, u_int i)
{
	if (i >= bl->bl_count)
		return NULL;
	return &bl->bl_data[i * bl->bl_item_size];
}

xtPublic void xt_bl_free(struct XTThread *self, XTBasicListPtr wl)
{
	xt_bl_set_size(self, wl, 0);
}

/*
 * --------------- Basic Queue ------------------
 */

xtPublic xtBool xt_bq_set_size(struct XTThread *self, XTBasicQueuePtr bq, size_t size)
{
	if (bq->bq_size < size) {
		if (!xt_realloc(self, (void **) &bq->bq_data, size * bq->bq_item_size))
			return FAILED;
		bq->bq_size = size;
	}
	else if (!size) {
		if (bq->bq_data)
			xt_free(self, bq->bq_data);
		bq->bq_data = NULL;
		bq->bq_size = 0;
		bq->bq_front = 0;
		bq->bq_back = 0;
	}
	return OK;
}

xtPublic void *xt_bq_get(XTBasicQueuePtr bq)
{
	if (bq->bq_back == bq->bq_front)
		return NULL;
	return &bq->bq_data[bq->bq_back * bq->bq_item_size];
}

xtPublic void xt_bq_next(XTBasicQueuePtr bq)
{
	if (bq->bq_back < bq->bq_front) {
		bq->bq_back++;
		if (bq->bq_front == bq->bq_back) {
			bq->bq_front = 0;
			bq->bq_back = 0;
		}
	}
}

xtPublic xtBool xt_bq_add(struct XTThread *self, XTBasicQueuePtr bq, void *value)
{
	if (bq->bq_front == bq->bq_size) {
		if (bq->bq_back >= bq->bq_max_waste) {
			bq->bq_front -= bq->bq_back;
			memmove(bq->bq_data, &bq->bq_data[bq->bq_back * bq->bq_item_size], bq->bq_front * bq->bq_item_size);
			bq->bq_back = 0;
		}
		else {
			if (!xt_bq_set_size(self, bq, bq->bq_front+bq->bq_item_inc))
				return FAILED;
		}
	}
	memcpy(&bq->bq_data[bq->bq_front * bq->bq_item_size], value, bq->bq_item_size);
	bq->bq_front++;
	return OK;
}

xtPublic void xt_sb_free(struct XTThread *self, XTStringBufferPtr dbuf)
{
	xt_sb_set_size(self, dbuf, 0);
}

xtPublic xtBool xt_sb_set_size(struct XTThread *self, XTStringBufferPtr dbuf, size_t size)
{
	if (dbuf->sb_size < size) {
		if (!xt_realloc(self, (void **) &dbuf->sb_cstring, size))
			return FAILED;
		dbuf->sb_size = size;
	}
	else if (!size) {
		if (dbuf->sb_cstring)
			xt_free(self, dbuf->sb_cstring);
		dbuf->sb_cstring = NULL;
		dbuf->sb_size = 0;
		dbuf->sb_len = 0;
	}
	return OK;
}

xtPublic xtBool xt_sb_concat_len(struct XTThread *self, XTStringBufferPtr dbuf, c_char *str, size_t len)
{
	if (!xt_sb_set_size(self, dbuf, dbuf->sb_len + len + 1))
		return FAILED;
	memcpy(dbuf->sb_cstring + dbuf->sb_len, str, len);
	dbuf->sb_len += len;
	dbuf->sb_cstring[dbuf->sb_len] = 0;
	return OK;
}

xtPublic xtBool xt_sb_concat(struct XTThread *self, XTStringBufferPtr dbuf, c_char *str)
{
	return xt_sb_concat_len(self, dbuf, str, strlen(str));
}

xtPublic xtBool xt_sb_concat_char(struct XTThread *self, XTStringBufferPtr dbuf, int ch)
{
	if (!xt_sb_set_size(self, dbuf, dbuf->sb_len + 1 + 1))
		return FAILED;
	dbuf->sb_cstring[dbuf->sb_len] = (char) ch;
	dbuf->sb_len++;
	dbuf->sb_cstring[dbuf->sb_len] = 0;
	return OK;
}

xtPublic xtBool xt_sb_concat_int8(struct XTThread *self, XTStringBufferPtr dbuf, xtInt8 val)
{
	char buffer[200];

	sprintf(buffer, "%"PRId64, val);
	return xt_sb_concat(self, dbuf, buffer);
}

xtPublic char *xt_sb_take_cstring(XTStringBufferPtr sbuf)
{
	char *str = sbuf->sb_cstring;
	
	sbuf->sb_cstring = NULL;
	sbuf->sb_size = 0; 
	sbuf->sb_len = 0; 
	return str;
}

xtPublic xtBool xt_sb_concat_url_len(struct XTThread *self, XTStringBufferPtr dbuf, c_char *from, size_t len_from)
{
	if (!xt_sb_set_size(self, dbuf, dbuf->sb_len + len_from + 1))
		return FAILED;
	while (len_from--) {
		if (*from == '%' && len_from >= 2 && isxdigit(*(from+1)) && isxdigit(*(from+2))) {
			unsigned char a = xt_hex_digit(*(from+1));
			unsigned char b = xt_hex_digit(*(from+2));
			dbuf->sb_cstring[dbuf->sb_len] = a << 4 | b;
			from += 3;
		}
		else
			dbuf->sb_cstring[dbuf->sb_len] = *from++;
		dbuf->sb_len++;
	}
	dbuf->sb_cstring[dbuf->sb_len] = 0;
	return OK;
}


