/* Copyright (C) 2000 MySQL AB

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

#include "mysys_priv.h"

#ifdef __WIN__

/*
 * Check a file or path for accessability.
 *
 * SYNOPSIS
 * 	file_access()
 * 	pathpath to check
 * 	amodemode to check
 *
 * 	DESCRIPTION
 * 	This function wraps the normal access method because the access 
 * 	available in MSVCRT> +reports that filenames such as LPT1 and 
 * 	COM1 are valid (they are but should not be so for us).
 *
 * 	RETURN VALUES
 * 	0     ok
 * 	-1    error
 */
int my_access(const char *path, int amode) 
{ 
	WIN32_FILE_ATTRIBUTE_DATA fileinfo;
	BOOL result;
	
	result = GetFileAttributesEx(path, GetFileExInfoStandard, 
			&fileinfo);
	if (! result) 
		return -1;
	if ((fileinfo.dwFileAttributes & FILE_ATTRIBUTE_READONLY) &&
			(amode & 2))
		return -1;
	return 0;
}

#endif
