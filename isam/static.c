/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
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
  Static variables for pisam library. All definied here for easy making of
  a shared library
*/

#ifndef _global_h
#include "isamdef.h"
#endif

LIST	*nisam_open_list=0;
uchar	NEAR nisam_file_magic[]=
{ (uchar) 254, (uchar) 254,'\005', '\002', };
uchar	NEAR nisam_pack_file_magic[]=
{ (uchar) 254, (uchar) 254,'\006', '\001', };
my_string    nisam_log_filename= (char*) "isam.log";
File	nisam_log_file= -1;
uint	nisam_quick_table_bits=9;
uint	nisam_block_size=1024;			/* Best by test */
my_bool nisam_flush=0;

/* read_vec[] is used for converting between P_READ_KEY.. and SEARCH_ */
/* Position is , == , >= , <= , > , < */

uint NEAR nisam_read_vec[]=
{
  SEARCH_FIND, SEARCH_FIND | SEARCH_BIGGER, SEARCH_FIND | SEARCH_SMALLER,
  SEARCH_NO_FIND | SEARCH_BIGGER, SEARCH_NO_FIND | SEARCH_SMALLER,
  SEARCH_FIND, SEARCH_LAST
};
