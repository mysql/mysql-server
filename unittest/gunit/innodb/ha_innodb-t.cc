/* Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved.

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

/* See http://code.google.com/p/googletest/wiki/Primer */

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"

#include <gtest/gtest.h>

#include "univ.i"

#include "ha_prototypes.h"

namespace innodb_ha_innodb_unittest {

/* test innobase_convert_name() */
TEST(hainnodb, innobaseconvertname)
{
	char	buf[64];
	struct {
		const char*	in;
		ulint		in_len;
		ulint		buf_size;
		const char*	buf_expected;
	} test_data[] = {
		/* the commented tests below fail, please fix
		innobase_convert_name() */
		{"abcd", 4, sizeof(buf), "`abcd`"},
		{"abcd", 4, 7, "`abcd`"},
		{"abcd", 4, 6, "`abcd`"},
		//{"abcd", 4, 5, "`abc`"},
		//{"abcd", 4, 4, "`ab`"},

		{"ab@0060cd", 9, sizeof(buf), "`ab``cd`"},
		{"ab@0060cd", 9, 9, "`ab``cd`"},
		{"ab@0060cd", 9, 8, "`ab``cd`"},
		{"ab@0060cd", 9, 7, "`ab``cd"},
		//{"ab@0060cd", 9, 6, "`ab``c"},
		//{"ab@0060cd", 9, 5, "`ab``"},
		//{"ab@0060cd", 9, 4, "`ab`"},

		//{"ab`cd", 5, sizeof(buf), "`#mysql50#ab``cd`"},
		//{"ab`cd", 5, 17, "`#mysql50#ab``cd`"},
		//{"ab`cd", 5, 16, "`#mysql50#ab``c`"},
		//{"ab`cd", 5, 15, "`#mysql50#ab```"},
		//{"ab`cd", 5, 14, "`#mysql50#ab`"},
		//{"ab`cd", 5, 13, "`#mysql50#ab`"},
		//{"ab`cd", 5, 12, "`#mysql50#a`"},
		//{"ab`cd", 5, 11, "`#mysql50#`"},
		//{"ab`cd", 5, 10, "`#mysql50`"},

		{"ab/cd", 5, sizeof(buf), "`ab`.`cd`"},
		{"ab/cd", 5, 9, "`ab`.`cd`"},
		//{"ab/cd", 5, 8, "`ab`.`c`"},
		//{"ab/cd", 5, 7, "`ab`.``"},
		//{"ab/cd", 5, 6, "`ab`."},
		//{"ab/cd", 5, 5, "`ab`."},
		{"ab/cd", 5, 4, "`ab`"},
		//{"ab/cd", 5, 3, "`a`"},
		//{"ab/cd", 5, 2, "``"},
		//{"ab/cd", 5, 1, "."},
		{"ab/cd", 5, 0, ""},
	};

	for (ulint i = 0; i < UT_ARR_SIZE(test_data); i++) {

		char*	end;
		size_t	res_len;

		memset(buf, 0, sizeof(buf));

		end = innobase_convert_name(
			buf,
			test_data[i].buf_size,
			test_data[i].in,
			test_data[i].in_len,
			NULL);

		res_len = (size_t) (end - buf);

		/* notice that buf is not '\0'-terminated */
		EXPECT_EQ(strlen(test_data[i].buf_expected), res_len);
		EXPECT_STREQ(test_data[i].buf_expected, buf);
	}
}

}
