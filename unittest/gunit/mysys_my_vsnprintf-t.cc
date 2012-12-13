/* Copyright (c) 2003, 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"
#include <gtest/gtest.h>

#include <my_global.h>
#include <m_string.h>

namespace mysys_my_vsnprintf_unittest {

char buf[1024]; /* let's hope that's enough */

void test1(const char *res, const char *fmt, ...)
{
  va_list args;
  size_t len;
  va_start(args,fmt);
  len= my_vsnprintf(buf, sizeof(buf)-1, fmt, args);
  va_end(args);
  EXPECT_EQ(len, strlen(res));
  EXPECT_STREQ(buf, res);
}


TEST(Mysys, Vsnprintf)
{
  test1("Constant string",
        "Constant string");

  test1("Format specifier s works",
        "Format specifier s %s", "works");
  test1("Format specifier b works (mysql extension)",
        "Format specifier b %.5b (mysql extension)", "works!!!");
  test1("Format specifier c !",
        "Format specifier c %c", '!');
  test1("Format specifier d 1",
        "Format specifier d %d", 1);
  test1("Format specifier i 1",
        "Format specifier i %i", 1);
  test1("Format specifier u 2",
        "Format specifier u %u", 2);
  test1("Format specifier o 375",
        "Format specifier o %o", 0375);
  test1("Format specifier x a",
        "Format specifier x %x", 10);
  test1("Format specifier X B",
        "Format specifier X %X", 11);
  test1("Format specifier p 0x5",
        "Format specifier p %p", 5);
  test1("Format specifier f 3.141593",
        "Format specifier f %f", 3.1415926);
  test1("Format specifier g 3.1416",
        "Format specifier g %g", 3.1415926);

  test1("Flag '-' is ignored <   1>",
        "Flag '-' is ignored <%-4d>", 1);
  test1("Flag '0' works <0006>",
        "Flag '0' works <%04d>", 6);

  test1("Width is ignored for strings <x> <y>",
        "Width is ignored for strings <%04s> <%5s>", "x", "y");

  test1("Precision works for strings <abcde>",
        "Precision works for strings <%.5s>", "abcdef!");

  test1("Flag '`' (backtick) works: `abcd` `op``q` (mysql extension)",
        "Flag '`' (backtick) works: %`s %`.4s (mysql extension)",
        "abcd", "op`qrst");

  test1("Length modifiers work: 1 * -1 * 2 * 3",
        "Length modifiers work: %d * %ld * %lld * %zd", 1, -1L, 2LL, (size_t)3);

  test1("Length modifiers work: 1 * -1 * 2 * 3",
        "Length modifiers work: %i * %li * %lli * %zd", 1, -1L, 2LL, (size_t)3);

  test1("long long X: 123456789abcdef0",
        "long long X: %llx", 0x123456789abcdef0LL);

  test1("(null) pointer is fine",
        "%s pointer is fine", NULL);

  test1("Positional arguments work: on the dark side they are",
        "Positional arguments work: %3$s %1$s %2$s",
        "they", "are", "on the dark side");

  test1("Asterisk '*' as a width works: <    4>",
        "Asterisk '*' as a width works: <%*d>", 5, 4);

  test1("Asterisk '*' as a precision works: <qwerty>",
        "Asterisk '*' as a precision works: <%.*s>", 6, "qwertyuiop");

  test1("Positional arguments for a width: <    4>",
        "Positional arguments for a width: <%1$*2$d>", 4, 5);

  test1("Positional arguments for a precision: <qwerty>",
        "Positional arguments for a precision: <%1$.*2$s>", "qwertyuiop", 6);

  test1("Positional arguments and a width: <0000ab>",
        "Positional arguments and a width: <%1$06x>", 0xab);

  test1("Positional arguments octal: <7777>",
        "Positional arguments octal: <%1$o>", 07777);

  /* Can't use int arguments, as they may be different size from pointers */

  test1("Padding and %p <0x12> <0x034> <0x0000ab> <    0xcd>",
        "Padding and %%p <%04p> <%05p> <%08p> <%8p>",
        (void*) 0x12, (void*) 0x34, (void*) 0xab, (void*) 0xcd);

  test1("F with a width (ignored) and precision: <12.34568>",
        "F with a width (ignored) and precision: <%10.5f>", 12.3456789);
  test1("G with a width (ignored) and precision: <12.35>",
        "G with a width (ignored) and precision: <%10.5g>", 12.3456789);

  test1("Hello",
        "Hello");
  test1("Hello int, 1",
        "Hello int, %d", 1);
  test1("Hello int, -1",
        "Hello int, %d", -1);
  test1("Hello int, 1",
        "Hello int, %i", 1);
  test1("Hello int, -1",
        "Hello int, %i", -1);
  test1("Hello string 'I am a string'",
        "Hello string '%s'", "I am a string");
  test1("Hello hack hack hack hack hack hack hack 1",
        "Hello hack hack hack hack hack hack hack %d", 1);
  test1("Hello 1 hack 4",
        "Hello %d hack %d", 1, 4);
  test1("Hello 1 hack hack hack hack hack 4",
        "Hello %d hack hack hack hack hack %d", 1, 4);
  test1("Hello 'hack' hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh",
        "Hello '%s' hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh", "hack");
  test1("Hello hhhhhhhhhhhhhh 1 sssssssssssssss",
        "Hello hhhhhhhhhhhhhh %d sssssssssssssss", 1);
  test1("Hello 1",
        "Hello %u", 1);
  test1("Hello 4294967295",
        "Hello %u", -1);
  test1("Hex:   20  '    41'",
        "Hex:   %lx  '%6lx'", 32, 65);
  test1("conn 1 to: '(null)' user: '(null)' host: '(null)' ((null))",
        "conn %ld to: '%-.64s' user: '%-.32s' host: '%-.64s' (%-.64s)",
                   1L,     NULL,          NULL,          NULL,    NULL);
  test1("Hello string `I am a string`",
        "Hello string %`s", "I am a string");
  test1("Hello TEST",
        "Hello %05s", "TEST");
  test1("My `Q` test",
        "My %1$`-.1s test", "QQQQ");
  test1("My AAAA test done DDDD",
        "My %2$s test done %1$s", "DDDD", "AAAA");
  test1("My DDDD test CCCC, DDD",
        "My %1$s test %2$s, %1$-.3s", "DDDD", "CCCC");
  test1("My QQQQ test",
        "My %1$`-.4b test", "QQQQ");
  test1("My X test",
        "My %1$c test", 'X');
  test1("My <0000000010> test1 <   a> test2 <   A>",
        "My <%010d> test1 <%4x> test2 <%4X>", 10, 10, 10);
  test1("My <0000000010> test1 <   a> test2 <   a>",
        "My <%1$010d> test1 <%2$4x> test2 <%2$4x>", 10, 10);
  test1("My 00010 test",
        "My %1$*02$d test", 10, 5);
  test1("My `DDDD` test CCCC, `DDD`",
        "My %1$`s test %2$s, %1$`-.3s", "DDDD", "CCCC");

}

}
