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
If this program compiles and returns 0, then GCC atomic funcions are available.

Created September 12, 2009 Vasil Dimov
*****************************************************************************/

int
main(int argc, char** argv)
{
	long	x;
	long	y;
	long	res;
	char	c;

	x = 10;
	y = 123;
	res = __sync_bool_compare_and_swap(&x, x, y);
	if (!res || x != y) {
		return(1);
	}

	x = 10;
	y = 123;
	res = __sync_bool_compare_and_swap(&x, x + 1, y);
	if (res || x != 10) {
		return(1);
	}

	x = 10;
	y = 123;
	res = __sync_add_and_fetch(&x, y);
	if (res != 123 + 10 || x != 123 + 10) {
		return(1);
	}

	c = 10;
	res = __sync_lock_test_and_set(&c, 123);
	if (res != 10 || c != 123) {
		return(1);
	}

	return(0);
}
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
If this program compiles and returns 0, then GCC atomic funcions are available.

Created September 12, 2009 Vasil Dimov
*****************************************************************************/

int
main(int argc, char** argv)
{
	long	x;
	long	y;
	long	res;
	char	c;

	x = 10;
	y = 123;
	res = __sync_bool_compare_and_swap(&x, x, y);
	if (!res || x != y) {
		return(1);
	}

	x = 10;
	y = 123;
	res = __sync_bool_compare_and_swap(&x, x + 1, y);
	if (res || x != 10) {
		return(1);
	}

	x = 10;
	y = 123;
	res = __sync_add_and_fetch(&x, y);
	if (res != 123 + 10 || x != 123 + 10) {
		return(1);
	}

	c = 10;
	res = __sync_lock_test_and_set(&c, 123);
	if (res != 10 || c != 123) {
		return(1);
	}

	return(0);
}
