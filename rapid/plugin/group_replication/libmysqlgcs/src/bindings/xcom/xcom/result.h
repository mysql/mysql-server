/* Copyright (c) 2012, 2016, Oracle and/or its affiliates. All rights reserved.

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



#ifndef RESULT_H
#define RESULT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "x_platform.h"

/* Combined return value and error code */
	struct result
	{
		int val;
		int funerr;
	};
	typedef struct result result;

	enum err_limits{
		errno_max = 1000000,
		ssl_zero = 2000000
	};

	static inline int to_errno(int err)
	{
		return err;
	}

	static inline int to_ssl_err(int err)
	{
		return err + ssl_zero;
	}

	static inline int from_errno(int err)
	{
		return err;
	}

	static inline int from_ssl_err(int err)
	{
		return err - ssl_zero;
	}

	static inline int is_ssl_err(int err)
	{
		return err > errno_max;
	}

#ifdef __cplusplus
}
#endif

#endif
