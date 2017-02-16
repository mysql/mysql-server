/* Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/*
  Header to remove use of my_functions in functions where we need speed and
  where calls to posix functions should work
*/
#ifndef _my_nosys_h
#define _my_nosys_h
#ifdef	__cplusplus
extern "C" {
#endif

#ifndef __MY_NOSYS__
#define __MY_NOSYS__

#ifndef HAVE_STDLIB_H
#include <malloc.h>
#endif

#undef my_read
#undef my_write
#undef my_seek
#define my_read(a,b,c,d) my_quick_read(a,b,c,d)
#define my_write(a,b,c,d) my_quick_write(a,b,c)
extern size_t my_quick_read(File Filedes,uchar *Buffer,size_t Count,
                            myf myFlags);
extern size_t my_quick_write(File Filedes,const uchar *Buffer,size_t Count);

#endif /* __MY_NOSYS__ */

#ifdef	__cplusplus
}
#endif
#endif
