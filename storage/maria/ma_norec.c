/* Copyright (C) 2010 Monty Program Ab

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
  Functions to handle tables with no row data (only index)
  This is useful when you just want to do key reads or want to use
  the index to check against duplicates.
*/

#include "maria_def.h"

my_bool _ma_write_no_record(MARIA_HA *info __attribute__((unused)),
                            const uchar *record __attribute__((unused)))
{
  return 0;
}

my_bool _ma_update_no_record(MARIA_HA *info __attribute__((unused)),
                             MARIA_RECORD_POS pos __attribute__((unused)),
                             const uchar *oldrec __attribute__((unused)),
                             const uchar *record __attribute__((unused)))
{
  return HA_ERR_WRONG_COMMAND;
}


my_bool _ma_delete_no_record(MARIA_HA *info __attribute__((unused)),
                             const uchar *record __attribute__((unused)))
{
  return HA_ERR_WRONG_COMMAND;
}


int _ma_read_no_record(MARIA_HA *info  __attribute__((unused)),
                       uchar *record  __attribute__((unused)),
                       MARIA_RECORD_POS pos __attribute__((unused)))
{
  return HA_ERR_WRONG_COMMAND;
}


int _ma_read_rnd_no_record(MARIA_HA *info __attribute__((unused)),
                           uchar *buf  __attribute__((unused)),
                           MARIA_RECORD_POS filepos __attribute__((unused)),
                           my_bool skip_deleted_blocks __attribute__((unused)))
{
  return HA_ERR_WRONG_COMMAND;
}

my_off_t _ma_no_keypos_to_recpos(MARIA_SHARE *share __attribute__ ((unused)),
                                 my_off_t pos __attribute__ ((unused)))
{
  return 0;
}
