/* Copyright (C) 2007 Michael Widenius

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*
  When we have finished the write/update/delete of a row, we have cleanups to
  do. For now it is signalling to Checkpoint that all dirtied pages have
  their rec_lsn set and page LSN set (_ma_unpin_all_pages() has been called),
  and that bitmap pages are correct (_ma_bitmap_release_unused() has been
  called).
*/

/* Function definitions for some redo functions */

my_bool _ma_write_clr(MARIA_HA *info, LSN undo_lsn,
                      enum translog_record_type undo_type,
                      my_bool store_checksum, ha_checksum checksum,
                      LSN *res_lsn);
void _ma_unpin_all_pages(MARIA_HA *info, LSN undo_lsn);

uint _ma_apply_redo_index_new_page(MARIA_HA *info, LSN lsn,
                                   const uchar *header, uint length);
uint _ma_apply_redo_index_free_page(MARIA_HA *info, LSN lsn,
                                    const uchar *header);
uint _ma_apply_redo_index(MARIA_HA *info,
                          LSN lsn, const uchar *header, uint length);

my_bool _ma_apply_undo_key_insert(MARIA_HA *info, LSN undo_lsn,
                                  const uchar *header, uint length);
my_bool _ma_apply_undo_key_delete(MARIA_HA *info, LSN undo_lsn,
                                  const uchar *header, uint length);

static inline void _ma_finalize_row(MARIA_HA *info)
{
  info->trn->rec_lsn= LSN_IMPOSSIBLE;
}

/* unpinning is often the last operation before finalizing */

static inline void _ma_unpin_all_pages_and_finalize_row(MARIA_HA *info,
                                                        LSN undo_lsn)
{
  _ma_unpin_all_pages(info, undo_lsn);
  _ma_finalize_row(info);
}

extern my_bool _ma_lock_key_del(MARIA_HA *info, my_bool insert_at_end);
extern void _ma_unlock_key_del(MARIA_HA *info);
static inline void _ma_fast_unlock_key_del(MARIA_HA *info)
{
  if (info->used_key_del)
    _ma_unlock_key_del(info);
}
