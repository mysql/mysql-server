/*
 Copyright (c) 2010, 2018, Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is also distributed with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have included with MySQL.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifdef TEST_MYSQL_UTILS_TEST

#include <string.h> // not using namespaces yet
#include <stdio.h> // not using namespaces yet
#include <stdlib.h> // not using namespaces yet

#include <util/NdbTap.hpp>
#include <util/BaseString.hpp>

#include "dbug_utils.hpp"
#include "decimal_utils.hpp"
#include "CharsetMap.hpp"

#include "my_sys.h"
#include "mysql.h"

// need two levels of macro substitution
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

// return non-zero value on failed condition
// C99's __func__ not supported by some C++ compilers yet (Solaris)
#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            fflush(stdout);                                             \
            fprintf(stderr, "\n!!! failed check: " TOSTRING(cond)       \
                    ", file: " __FILE__                                 \
                    ", line: " TOSTRING(__LINE__)                       \
                    ".\n");                                             \
            fflush(stderr);                                             \
            return 1;                                                   \
        }                                                               \
    } while (0)

int test_dbug_utils()
{
    printf("\n==== DBUG Utilities ====\n");
    const int DBUG_BUF_SIZE = 1024;
    char buffer[DBUG_BUF_SIZE];

    const char * s = "some initial string";
    const char * const s0 = "";
    s = dbugExplain(buffer, DBUG_BUF_SIZE);
    CHECK(!s || !strcmp(s, s0));

    s = dbugExplain(NULL, DBUG_BUF_SIZE);
    CHECK(!s);

    s = dbugExplain(buffer, 0);
    CHECK(!s);

    const char * const s1 = "t";
    dbugSet(s1);
    s = dbugExplain(buffer, DBUG_BUF_SIZE);
    CHECK(!s || !strcmp(s, s1));

    dbugSet(NULL);
    s = dbugExplain(buffer, DBUG_BUF_SIZE);
    CHECK(!s || !strcmp(s, s1));

    /* Build dbug string honoring setting of TMPDIR
       using the format "d,somename:o,<TMPDIR>/somepath"
     */
    BaseString tmp("d,somename:o,");
    const char* tmpd = getenv("TMPDIR");
    if(tmpd)
      tmp.append(tmpd);
    else
      tmp.append("/tmp");
    tmp.append("/somepath");
    const char * const s2 = tmp.c_str();
    dbugPush(s2);
    s = dbugExplain(buffer, DBUG_BUF_SIZE);
    CHECK(!s || !strcmp(s, s2));

    dbugPush(NULL);
    s = dbugExplain(buffer, DBUG_BUF_SIZE);
    CHECK(!s || !strcmp(s, s2));

    const char * const s3 = "d,a,b,c,x,y,z";
    dbugPush(s3);
    s = dbugExplain(buffer, DBUG_BUF_SIZE);
    CHECK(!s || (strspn(s, s3) == strlen(s3))); // allow for different order

    dbugPop();
    s = dbugExplain(buffer, DBUG_BUF_SIZE);
    CHECK(!s || !strcmp(s, s2));

    dbugPop();
    s = dbugExplain(buffer, DBUG_BUF_SIZE);
    CHECK(!s || !strcmp(s, s1));

    dbugPush(NULL);
    s = dbugExplain(buffer, DBUG_BUF_SIZE);
    CHECK(!s || !strcmp(s, s1));

    dbugPop();
    s = dbugExplain(buffer, DBUG_BUF_SIZE);
    CHECK(!s || !strcmp(s, s0));

    return 0;
}

int test_decimal(const char *s, int prec, int scale, int expected_rv)
{
    char bin_buff[128], str_buff[128];
    int r1, r2 = 0;

    str_buff[0] = 0;

    // cast: decimal_str2bin expects 'int' for size_t strlen()
    r1 = decimal_str2bin(s, (int)strlen(s), prec, scale, bin_buff, 128);
    if(r1 <= E_DEC_OVERFLOW) {
        r2 = decimal_bin2str(bin_buff, 128, prec, scale, str_buff, 128);
        CHECK(r2 == E_DEC_OK);
    }
    printf("[%-2d,%-2d] %-29s => res=%d,%d     %s\n",
           prec, scale, s, r1, r2, str_buff);

    if(r1 != expected_rv)
        printf("decimal_str2bin returned %d when %d was expected.\n",
               r1, expected_rv);
    CHECK(r1 == expected_rv);

    return 0;
}

int test_decimal_conv()
{
    printf("\n==== decimal_str2bin() / decimal_bin2str() ====\n");
    CHECK(test_decimal("100", 3, -1, E_DEC_BAD_SCALE) == 0);
    CHECK(test_decimal("3.3", 2, 1, E_DEC_OK) == 0);
    CHECK(test_decimal("124.000", 20, 4, E_DEC_OK) == 0);
    CHECK(test_decimal("-11", 14, 1, E_DEC_OK) == 0);
    CHECK(test_decimal("1.123456000000000", 20, 16, E_DEC_OK) == 0);
    CHECK(test_decimal("-20.333", 4, 2, E_DEC_TRUNCATED) == 0);
    CHECK(test_decimal("0", 20, 10, E_DEC_OK) == 0);
    CHECK(test_decimal("1 ", 20, 10, E_DEC_OK) == 0);
    CHECK(test_decimal("1,35", 20, 10, E_DEC_OK) == 0);
    CHECK(test_decimal("text", 20, 10, E_DEC_BAD_NUM) == 0);

    return 0;
}

int test_charset_map()
{
    printf("\n==== CharsetMap ====\n");
    printf("init MySQL lib, CharsetMap...\n");
    my_init();
    CharsetMap::init();

    /* CharsetMap */
    CharsetMap csmap;
    int utf8_num = csmap.getUTF8CharsetNumber();
    int utf16_num = csmap.getUTF16CharsetNumber();

    /* If this mysql build does not include UTF-8 and either UCS-2 or UTF-16
       then the test suite must fail.
    */
    printf("UTF-8 charset num: %d     UTF-16 or UCS-2 charset num:  %d\n",
           utf8_num, utf16_num);
    CHECK(utf8_num != 0);
    CHECK(utf16_num != 0);

    /* test csmap.getName()
     */
    const char *utf8 = csmap.getName(utf8_num);
    CHECK(!strcmp(utf8,"UTF-8"));

    /* MySQL 5.1 and earlier will have UCS-2 but later versions may have true
       UTF-16.  For information, print whether UTF-16 or UCS-2 is being used.
    */
    const char *utf16 = csmap.getMysqlName(csmap.getUTF16CharsetNumber());
    printf("Using mysql's %s for UTF-16.\n", utf16);

    /* Now we're going to recode.
       We test with the string "ülker", which begins with the character
       LATIN SMALL LETTER U WITH DIARESIS - unicode code point U+00FC.
       In the latin1 encoding this is a literal 0xFC,
       but in the UTF-8 representation it is 0xC3 0xBC.
    */
    // use numeric escape sequencesx.. (or downcast integer literal to char)
    // to avoid narrowing conversion compile warnings
    const char my_word_latin1[6]    = { '\xFC', 'l', 'k', 'e', 'r', 0};
    const char my_word_utf8[7]      = { '\xC3', '\xBC', 'l', 'k', 'e', 'r', 0};
    const char my_word_truncated[5] = { '\xC3', '\xBC', 'l', 'k', 0};
    // no need for 'unsigned char[]'
    const char my_bad_utf8[5]       = { 'l', '\xBC', 'a', 'd', 0};
    char result_buff_1[32];
    char result_buff_2[32];
    char result_buff_too_small[4];
    int lengths[2];

    /* latin1 must be available to run the recode test */
    int latin1_num = csmap.getCharsetNumber("latin1");
    printf("latin1 charset number: %d  standard name: \"%s\" \n",
           latin1_num, csmap.getName(latin1_num));
    CHECK(latin1_num != 0);
    CHECK(! strcmp(csmap.getName(latin1_num), "windows-1252"));

    printf("Latin1: \"%s\"                       UTF8:  \"%s\" \n",
           my_word_latin1, my_word_utf8);

    /* RECODE TEST 1: recode from UTF-8 to Latin 1 */
    lengths[0] = 7;
    lengths[1] = 32;
    CharsetMap::RecodeStatus rr1 = csmap.recode(lengths, utf8_num, latin1_num,
                                                my_word_utf8, result_buff_1);
    printf("Recode Test 1 - UTF-8 to Latin-1: %d %d %d \"%s\" => \"%s\" \n",
           rr1, lengths[0], lengths[1], my_word_utf8, result_buff_1);
    CHECK(rr1 == CharsetMap::RECODE_OK);
    CHECK(lengths[0] == 7);
    CHECK(lengths[1] == 6);
    CHECK(!strcmp(result_buff_1, my_word_latin1));

    /* RECODE TEST 2: recode from Latin1 to to UTF-8 */
    lengths[0] = 6;
    lengths[1] = 32;
    CharsetMap::RecodeStatus rr2 = csmap.recode(lengths, latin1_num, utf8_num,
                                                my_word_latin1, result_buff_2);
    printf("Recode Test 2 - Latin-1 to UTF-8: %d %d %d \"%s\" => \"%s\" \n",
           rr2, lengths[0], lengths[1], my_word_latin1, result_buff_2);
    CHECK(rr2 == CharsetMap::RECODE_OK);
    CHECK(lengths[0] == 6);
    CHECK(lengths[1] == 7);
    CHECK(!(strcmp(result_buff_2, my_word_utf8)));

    /* RECODE TEST 3: recode with a too-small result buffer */
    lengths[0] = 6;
    lengths[1] = 4;
    CharsetMap::RecodeStatus rr3 = csmap.recode(lengths, latin1_num, utf8_num,
                                                my_word_latin1, result_buff_too_small);
    printf("Recode Test 3 - too-small buffer: %d %d %d \"%s\" => \"%.4s\" \n",
           rr3, lengths[0], lengths[1], my_word_latin1, result_buff_too_small);
    CHECK(rr3 == CharsetMap::RECODE_BUFF_TOO_SMALL);
    CHECK(lengths[0] == 3);
    CHECK(lengths[1] == 4);
    /* Confirm that the first four characters were indeed recoded: */
    CHECK(!(strncmp(result_buff_too_small, my_word_truncated, 4)));

    /* RECODE TEST 4: recode with an invalid character set */
    CharsetMap::RecodeStatus rr4 = csmap.recode(lengths, 0, 999, my_word_latin1, result_buff_2);
    printf("Recode Test 4 - invalid charset: %d \n", rr4);
    CHECK(rr4 == CharsetMap::RECODE_BAD_CHARSET);

    /* RECODE TEST 5: source string is ill-formed UTF-8 */
    lengths[0] = 5;
    lengths[1] = 32;
    int rr5 = csmap.recode(lengths, utf8_num, latin1_num,
                           my_bad_utf8, result_buff_2);
    printf("Recode Test 5 - ill-formed source string: %d \n", rr5);
    CHECK(rr5 == CharsetMap::RECODE_BAD_SRC);


    printf("isMultibyte TEST: ");
    const bool * result1, * result2, * result3;
    result1 = csmap.isMultibyte(latin1_num);
    result2 = csmap.isMultibyte(utf16_num);
    result3 = csmap.isMultibyte(utf8_num);
    printf("latin 1: %s      UTF16: %s       UTF8: %s\n",
           *result1 ? "Yes" : "No",
           *result2 ? "Yes" : "No",
           *result3 ? "Yes" : "No");
    CHECK(! *result1);
    CHECK(*result2);
    CHECK(*result3);

    int nNull = 0, nSingle = 0, nMulti = 0;
    for(int i = 0 ; i < 256 ; i++) {
      const bool *r = csmap.isMultibyte(i);
      if(r) {
        if(*r) nMulti++;
        else nSingle++;
      }
      else nNull++;
    }
    printf("Charset stats:  %d unused, %d single-byte, %d multi-byte\n",
           nNull, nSingle, nMulti);
    // If there is not at least one of each, then something is probably wrong
    CHECK(nNull && nSingle && nMulti);

    printf("unload CharsetMap...\n");
    CharsetMap::unload();

    return 0;
}

int main(int argc, const char** argv)
{
    // TAP: print number of tests to run
    plan(3);

    // init MySQL lib
    if (my_init())
        BAIL_OUT("my_init() failed");

    // TAP: report test result: ok(passed, non-null format string)
    ok(test_dbug_utils() == 0, "subtest: dbug_utils");
    ok(test_decimal_conv() == 0, "subtest: decimal_conv");
    ok(test_charset_map() == 0, "subtest: charset_map");

    // TAP: print summary report and return exit status
    return exit_status();
}

#endif
