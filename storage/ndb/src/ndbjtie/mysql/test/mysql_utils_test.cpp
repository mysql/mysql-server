/*
 Copyright (c) 2010, 2012, Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/
/*
 * mysql_utils_test.cpp
 */

#include <string.h> // not using namespaces yet
#include <stdio.h> // not using namespaces yet
#include <stdlib.h> // not using namespaces yet
#include <assert.h> // not using namespaces yet

#include "decimal_utils.hpp"
#include "CharsetMap.hpp"

#include "my_global.h"
#include "my_sys.h"
#include "mysql.h"

void test_decimal(const char *s, int prec, int scale, int expected_rv) 
{
    char bin_buff[128], str_buff[128];
    int r1, r2 = 0;
    
    str_buff[0] = 0;

    r1 = decimal_str2bin(s, strlen(s), prec, scale, bin_buff, 128);
    if(r1 <= E_DEC_OVERFLOW) 
        r2 = decimal_bin2str(bin_buff, 128, prec, scale, str_buff, 128);
    
    printf("[%-2d,%-2d] %-29s => res=%d,%d     %s\n",
           prec, scale, s, r1, r2, str_buff);

    if(r1 != expected_rv) {
        printf("decimal_str2bin returned %d when %d was expected.\n",
               r1, expected_rv);
        exit(1);
    }
}


int main()
{
    printf("==== init MySQL lib ====\n");
    my_init();
    CharsetMap::init();

    printf("==== decimal_str2bin() / decimal_bin2str() ====\n");
    
    test_decimal("100", 3, -1, E_DEC_BAD_SCALE); 
    test_decimal("3.3", 2, 1, E_DEC_OK);
    test_decimal("124.000", 20, 4, E_DEC_OK);
    test_decimal("-11", 14, 1, E_DEC_OK);
    test_decimal("1.123456000000000", 20, 16, E_DEC_OK);
    test_decimal("-20.333", 4, 2, E_DEC_TRUNCATED);
    test_decimal("0", 20, 10, E_DEC_OK);
    test_decimal("1 ", 20, 10, E_DEC_OK);
    test_decimal("1,35", 20, 10, E_DEC_OK);
    test_decimal("text", 20, 10, E_DEC_BAD_NUM);
    
    /* CharsetMap */
    printf("\n==== CharsetMap ==== \n");

    CharsetMap csmap;
    int utf8_num = csmap.getUTF8CharsetNumber();
    int utf16_num = csmap.getUTF16CharsetNumber();
    
    /* If this mysql build does not include UTF-8 and either UCS-2 or UTF-16 
       then the test suite must fail. 
    */    
    printf("UTF-8 charset num: %d     UTF-16 or UCS-2 charset num:  %d\n",
           utf8_num, utf16_num);
    if((utf8_num == 0) || (utf16_num == 0)) exit(1);

    /* test csmap.getName()
    */
    const char *utf8 = csmap.getName(utf8_num);
    if(strcmp(utf8,"UTF-8")) exit(1);
   
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
    
    const char my_word_latin1[6]    = { 0xFC, 'l', 'k', 'e', 'r', 0}; 
    const char my_word_utf8[7]      = { 0xC3, 0xBC, 'l', 'k', 'e', 'r', 0}; 
    const char my_word_truncated[5] = { 0xC3, 0xBC, 'l', 'k', 0};
    const unsigned char my_bad_utf8[5]       = { 'l' , 0xBC, 'a', 'd', 0};
    char result_buff_1[32];
    char result_buff_2[32];
    char result_buff_too_small[4];
    int lengths[2];
    
    /* latin1 must be available to run the recode test */
    int latin1_num = csmap.getCharsetNumber("latin1");    
    printf("latin1 charset number: %d  standard name: \"%s\" \n", 
           latin1_num, csmap.getName(latin1_num));
    assert(latin1_num != 0);
    assert(! strcmp(csmap.getName(latin1_num) , "windows-1252"));
    
    printf("Latin1: \"%s\"                       UTF8:  \"%s\" \n", 
           my_word_latin1, my_word_utf8);
    
    /* RECODE TEST 1: recode from UTF-8 to Latin 1 */
    lengths[0] = 7;
    lengths[1] = 32;
    CharsetMap::RecodeStatus rr1 = csmap.recode(lengths, utf8_num, latin1_num, 
                                                my_word_utf8, result_buff_1);
    printf("Recode Test 1 - UTF-8 to Latin-1: %d %ld %ld \"%s\" => \"%s\" \n", 
           rr1, lengths[0], lengths[1], my_word_utf8, result_buff_1);
    assert(rr1 == CharsetMap::RECODE_OK);
    assert(lengths[0] == 7);
    assert(lengths[1] == 6);
    assert(!strcmp(result_buff_1, my_word_latin1));
    
    /* RECODE TEST 2: recode from Latin1 to to UTF-8 */
    lengths[0] = 6;
    lengths[1] = 32;
    CharsetMap::RecodeStatus rr2 = csmap.recode(lengths, latin1_num, utf8_num,
                                                my_word_latin1, result_buff_2);
    printf("Recode Test 2 - Latin-1 to UTF-8: %d %ld %ld \"%s\" => \"%s\" \n", 
           rr2, lengths[0], lengths[1], my_word_latin1, result_buff_2);
    assert(rr2 == CharsetMap::RECODE_OK);
    assert(lengths[0] == 6);
    assert(lengths[1] == 7);
    assert(!(strcmp(result_buff_2, my_word_utf8)));
    
    /* RECODE TEST 3: recode with a too-small result buffer */
    lengths[0] = 6;
    lengths[1] = 4;
    CharsetMap::RecodeStatus rr3 = csmap.recode(lengths, latin1_num, utf8_num,
                                                my_word_latin1, result_buff_too_small);
    printf("Recode Test 3 - too-small buffer: %d %ld %ld \"%s\" => \"%s\" \n", 
           rr3, lengths[0], lengths[1], my_word_latin1, result_buff_too_small);
    assert(rr3 == CharsetMap::RECODE_BUFF_TOO_SMALL);
    assert(lengths[0] == 3);
    assert(lengths[1] == 4);
    /* Confirm that the first four characters were indeed recoded: */
    assert(!(strncmp(result_buff_too_small, my_word_truncated, 4)));
    
    /* RECODE TEST 4: recode with an invalid character set */
    CharsetMap::RecodeStatus rr4 = csmap.recode(lengths, 0, 999, my_word_latin1, result_buff_2);
    printf("Recode Test 4 - invalid charset: %d \n", rr4);
    assert(rr4 == CharsetMap::RECODE_BAD_CHARSET);

    /* RECODE TEST 5: source string is ill-formed UTF-8 */
    lengths[0] = 5;
    lengths[1] = 32;
    int rr5 = csmap.recode(lengths, utf8_num, latin1_num, 
                           my_bad_utf8, result_buff_2);
    printf("Recode Test 5 - ill-formed source string: %d \n", rr5);
    assert(rr5 == CharsetMap::RECODE_BAD_SRC);

  
    printf("isMultibyte TEST: ");
    const bool * result1, * result2, * result3;
    result1 = csmap.isMultibyte(latin1_num);
    result2 = csmap.isMultibyte(utf16_num);
    result3 = csmap.isMultibyte(utf8_num);
    printf("latin 1: %s      UTF16: %s       UTF8: %s\n",
           *result1 ? "Yes" : "No" , 
           *result2 ? "Yes" : "No" ,
           *result3 ? "Yes" : "No");
    assert(! *result1);
    assert(*result2);
    assert(*result3);

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
    assert(nNull && nSingle && nMulti);
  
    
    
    CharsetMap::unload();
}
