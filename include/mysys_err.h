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

#ifndef _mysys_err_h
#define _mysys_err_h
#ifdef	__cplusplus
extern "C" {
#endif

#define GLOB		0	/* Error maps */
#define GLOBERRS	29	/* Max number of error messages in map's */
#define EE(X)	globerrs[ X ]	/* Defines to add error to right map */

extern const char * NEAR globerrs[];	/* my_error_messages is here */

/* Error message numbers in global map */
#define EE_FILENOTFOUND		0
#define EE_CANTCREATEFILE	1
#define EE_READ			2
#define EE_WRITE		3
#define EE_BADCLOSE		4
#define EE_OUTOFMEMORY		5
#define EE_DELETE		6
#define EE_LINK			7
#define EE_EOFERR		9
#define EE_CANTLOCK		10
#define EE_CANTUNLOCK		11
#define EE_DIR			12
#define EE_STAT			13
#define EE_CANT_CHSIZE		14
#define EE_CANT_OPEN_STREAM	15
#define EE_GETWD		16
#define EE_SETWD		17
#define EE_LINK_WARNING		18
#define EE_OPEN_WARNING		19
#define EE_DISK_FULL		20
#define EE_CANT_MKDIR		21
#define EE_UNKNOWN_CHARSET	22
#define EE_OUT_OF_FILERESOURCES	23
#define EE_CANT_READLINK	24
#define EE_CANT_SYMLINK		25
#define EE_REALPATH		26
#define EE_SYNC			27
#define EE_UNKNOWN_COLLATION	28

  /* exit codes for all MySQL programs */

#define EXIT_UNSPECIFIED_ERROR		1
#define EXIT_UNKNOWN_OPTION		2
#define EXIT_AMBIGUOUS_OPTION		3
#define EXIT_NO_ARGUMENT_ALLOWED	4
#define EXIT_ARGUMENT_REQUIRED		5
#define EXIT_VAR_PREFIX_NOT_UNIQUE	6
#define EXIT_UNKNOWN_VARIABLE		7
#define EXIT_OUT_OF_MEMORY		8
#define EXIT_UNKNOWN_SUFFIX		9
#define EXIT_NO_PTR_TO_VARIABLE		10
#define EXIT_CANNOT_CONNECT_TO_SERVICE	11
#define EXIT_OPTION_DISABLED            12


#ifdef	__cplusplus
}
#endif
#endif

