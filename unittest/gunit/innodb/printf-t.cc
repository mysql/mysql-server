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

namespace innodb_printf_unittest {

static
void
test_snprintf(const char* res, char* buf, size_t bufsz, const char* fmt, ...)
{
	va_list args;
	size_t len;

	EXPECT_STREQ(buf, res);
	va_start(args,fmt);
	len= my_vsnprintf(buf, bufsz-1, fmt, args);
	va_end(args);
	EXPECT_EQ(len, strlen(res));
	EXPECT_STREQ(buf, res);
}

TEST(hainnodb, UtMySnprintf)
{
	char	buf[72];
	size_t	bufsz;

	bufsz = sizeof buf;
#define ARGS buf, bufsz, "foo %u " IB_ID_FMT ".", 1, (trx_id_t) -1
	ut_snprintf(ARGS);
	test_snprintf("foo 1 18446744073709551615.", ARGS);

	bufsz = 25;
	ut_snprintf(ARGS); EXPECT_STREQ(buf, "foo 1 184467440737095516");
	my_snprintf(ARGS); EXPECT_STREQ(buf, "foo 1 .");
	bufsz = sizeof buf;
#undef ARGS

	ib_uint32_t	a;
	ib_uint64_t	b;
	int64_t		c;

#define ARGS buf, bufsz, UINT32PF "/" UINT64PF "/%" PRId64 "/" UINT64PFx "*", \
	a, b, c, b

	a = 0, b = 1, c = 2;
	ut_snprintf(ARGS);
	test_snprintf(
		"0/1/2/0000000000000001*", ARGS);
	a = -1, b = -2, c = -3;
	ut_snprintf(ARGS);
	test_snprintf(
		"4294967295/18446744073709551614/-3/fffffffffffffffe*", ARGS);

	a = 1234567890, b = 12345678901234567890ULL, c = static_cast<int64_t>(b);
	ut_snprintf(ARGS);
	test_snprintf(
		"1234567890/12345678901234567890/-6101065172474983726/"
		"ab54a98ceb1f0ad2*", ARGS);
	a = -1234567890, c = -8765432109876543210LL, b = ib_uint64_t (c);
	ut_snprintf(ARGS);
	test_snprintf(
		"3060399406/9681311963833008406/-8765432109876543210/"
		"865aedeff4018116*", ARGS);
#undef ARGS
}

}
