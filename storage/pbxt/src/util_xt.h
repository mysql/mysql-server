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

#ifndef __xt_xtutil_h__
#define __xt_xtutil_h__

#include <stddef.h>

#include "xt_defs.h"

#define XT_CHECKSUM_1(sum)		((xtWord1) ((sum) ^ ((sum) >> 24) ^ ((sum) >> 16) ^ ((sum) >> 8)))
#define XT_CHECKSUM_2(sum)		((xtWord2) ((sum) ^ ((sum) >> 16)))
#define XT_CHECKSUM4_8(sum)		((xtWord4) (sum) ^ (xtWord4) ((sum) >> 32))

int		xt_comp_log_pos(xtLogID id1, off_t off1, xtLogID id2, off_t off2);
xtWord8	xt_time_now(void);
void	xt_free_nothing(struct XTThread *self, void *x);
xtWord4	xt_file_name_to_id(char *file_name);
xtBool	xt_time_difference(register xtWord4 now, register xtWord4 then);
xtWord2	xt_get_checksum(xtWord1 *data, size_t len, u_int interval);
xtWord1 xt_get_checksum1(xtWord1 *data, size_t len);
xtWord4 xt_get_checksum4(xtWord1 *data, size_t len);

typedef struct XTDataBuffer {
	size_t			db_size;
	xtWord1			*db_data;
} XTDataBufferRec, *XTDataBufferPtr;

xtBool xt_db_set_size(struct XTThread *self, XTDataBufferPtr db, size_t size);

#define XT_IB_DEFAULT_SIZE			512

typedef struct XTInfoBuffer {
	xtBool			ib_free;
	XTDataBufferRec	ib_db;
	xtWord1			ib_data[XT_IB_DEFAULT_SIZE];
} XTInfoBufferRec, *XTInfoBufferPtr;

xtBool	xt_ib_alloc(struct XTThread *self, XTInfoBufferPtr ib, size_t size);
void	xt_ib_free(struct XTThread *self, XTInfoBufferPtr ib);

typedef struct XTBasicList {
	u_int			bl_item_size;
	u_int			bl_size;
	u_int			bl_count;
	xtWord1			*bl_data;
} XTBasicListRec, *XTBasicListPtr;

xtBool	xt_bl_set_size(struct XTThread *self, XTBasicListPtr wl, size_t size);
xtBool	xt_bl_dup(struct XTThread *self, XTBasicListPtr from_bl, XTBasicListPtr to_bl);
xtBool	xt_bl_append(struct XTThread *self, XTBasicListPtr wl, void *value);
void	*xt_bl_last_item(XTBasicListPtr wl);
void	*xt_bl_item_at(XTBasicListPtr wl, u_int i);
void	xt_bl_free(struct XTThread *self, XTBasicListPtr wl);

typedef struct XTBasicQueue {
	u_int			bq_item_size;
	u_int			bq_max_waste;
	u_int			bq_item_inc;
	u_int			bq_size;
	u_int			bq_front;
	u_int			bq_back;
	xtWord1			*bq_data;
} XTBasicQueueRec, *XTBasicQueuePtr;

xtBool	xt_bq_set_size(struct XTThread *self, XTBasicQueuePtr wq, size_t size);
void	*xt_bq_get(XTBasicQueuePtr wq);
void	xt_bq_next(XTBasicQueuePtr wq);
xtBool	xt_bq_add(struct XTThread *self, XTBasicQueuePtr wl, void *value);

typedef struct XTStringBuffer {
	size_t			sb_size;
	size_t			sb_len;
	char			*sb_cstring;
} XTStringBufferRec, *XTStringBufferPtr;

void	xt_sb_free(struct XTThread *self, XTStringBufferPtr db);
xtBool	xt_sb_set_size(struct XTThread *self, XTStringBufferPtr db, size_t size);
xtBool	xt_sb_concat_len(struct XTThread *self, XTStringBufferPtr dbuf, c_char *str, size_t len);
xtBool	xt_sb_concat(struct XTThread *self, XTStringBufferPtr dbuf, c_char *str);
xtBool	xt_sb_concat_char(struct XTThread *self, XTStringBufferPtr dbuf, int ch);
xtBool	xt_sb_concat_int8(struct XTThread *self, XTStringBufferPtr dbuf, xtInt8 val);
char	*xt_sb_take_cstring(XTStringBufferPtr dbuf);
xtBool	xt_sb_concat_url_len(struct XTThread *self, XTStringBufferPtr dbuf, c_char *str, size_t len);

static inline size_t xt_align_size(size_t size, size_t align)
{
	register size_t diff = size % align;
	
	if (diff)
		return size + align - diff;
	return size;
}

static inline off_t xt_align_offset(off_t size, size_t align)
{
	register off_t diff = size % (off_t) align;
	
	if (diff)
		return size + align - diff;
	return size;
}

#endif
