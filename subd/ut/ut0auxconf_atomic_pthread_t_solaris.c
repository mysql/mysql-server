/*****************************************************************************

Copyright (c) 2009, Innobase Oy. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

/*****************************************************************************
If this program compiles and returns 0, then pthread_t objects can be used as
arguments to Solaris libc atomic functions.

Created April 18, 2009 Vasil Dimov
*****************************************************************************/

#include <pthread.h>
#include <string.h>

int
main(int argc, char** argv)
{
	pthread_t	x1;
	pthread_t	x2;
	pthread_t	x3;

	memset(&x1, 0x0, sizeof(x1));
	memset(&x2, 0x0, sizeof(x2));
	memset(&x3, 0x0, sizeof(x3));

	if (sizeof(pthread_t) == 4) {

		atomic_cas_32(&x1, x2, x3);

	} else if (sizeof(pthread_t) == 8) {

		atomic_cas_64(&x1, x2, x3);

	} else {

		return(1);
	}

	return(0);
}
