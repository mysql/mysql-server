/* Copyright (C) 2008 MySQL AB

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


typedef struct st_seq_storage
{
  uint pos;
  DYNAMIC_ARRAY seq;
} SEQ_STORAGE;

extern my_bool seq_storage_reader_init(SEQ_STORAGE *seq, const char *file);
extern ulong seq_storage_next(SEQ_STORAGE *seq);
extern void seq_storage_destroy(SEQ_STORAGE *seq);
extern void seq_storage_rewind(SEQ_STORAGE *seq);
extern my_bool seq_storage_write(const char *file, ulong num);

