/* Copyright (C) 2006-2008 MySQL AB

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

#include <m_string.h>
#include "../ma_pagecache.h"
#ifdef _WIN32
#include <direct.h>
#endif
/*
  File content descriptor
*/
struct file_desc
{
  unsigned int length;
  unsigned char content;
};

int test_file(PAGECACHE_FILE file, char *file_name,
              off_t size, size_t buff_size, struct file_desc *desc);
