/* Copyright (C) Yuri Dario & 2000 MySQL AB
   All the above parties has a full, independent copyright to
   the following code, including the right to use the code in
   any manner without any demands from the other parties.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

/* Win32 directory search emulation */

#ifndef __MY_OS2DIRSRCH2_H__
#define __MY_OS2DIRSRCH2_H__

#ifdef __cplusplus_00
extern "C" {
#endif

struct _finddata_t
{
	unsigned	attrib;
	//unsigned long	time_create;	/* -1 for FAT file systems */
	//unsigned long	time_access;	/* -1 for FAT file systems */
	//unsigned long	time_write;
	unsigned long	size;
	char		name[260];
	//uint16		wr_date;
	//uint16		wr_time;
};

struct dirent
{
	//unsigned	attrib;
	//unsigned long	time_create;	/* -1 for FAT file systems */
	//unsigned long	time_access;	/* -1 for FAT file systems */
	//unsigned long	time_write;
	//unsigned long	size;
	char		d_name[260];
	//uint16		wr_date;
	//uint16		wr_time;
};

struct DIR
{
   HDIR  hdir;
   FILEFINDBUF3   buf3;
   struct dirent  ent;
};

DIR *opendir ( char *);
struct dirent *readdir (DIR *);
int closedir (DIR *);

//#define _A_NORMAL	FILE_NORMAL
//#define _A_SUBDIR	FILE_DIRECTORY
//#define _A_RDONLY	FILE_READONLY

//long  _findfirst( char*, struct _finddata_t*);
//long  _findnext( long, struct _finddata_t*);
//void  _findclose( long);

#ifdef __cplusplus_00
}
#endif

#endif // __MY_OS2DIRSRCH2_H__
