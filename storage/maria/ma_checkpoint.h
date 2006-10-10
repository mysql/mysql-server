/* Copyright (C) 2006 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*
  WL#3071 Maria checkpoint
  First version written by Guilhem Bichot on 2006-04-27.
  Does not compile yet.
*/

/* This is the interface of this module. */

typedef enum enum_checkpoint_level {
  NONE=-1,
  INDIRECT, /* just write dirty_pages, transactions table and sync files */
  MEDIUM, /* also flush all dirty pages which were already dirty at prev checkpoint*/
  FULL /* also flush all dirty pages */
} CHECKPOINT_LEVEL;

void request_asynchronous_checkpoint(CHECKPOINT_LEVEL level);
my_bool execute_synchronous_checkpoint(CHECKPOINT_LEVEL level);
my_bool execute_asynchronous_checkpoint_if_any();
/* that's all that's needed in the interface */
