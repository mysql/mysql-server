/*
 Copyright (c) 2013, Oracle and/or its affiliates. All rights
 reserved.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2 of
 the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
 */

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <my_global.h>
#include <mysql.h>
#include <my_sys.h>

#include <NdbApi.hpp> 

#include "EncoderCharset.h"

/** 
 * V8 can work with two kinds of external strings: Strict ASCII and UTF-16.
 * 
 * (A) For any strict ASCII string, even if its character set is latin1 or
 *     UTF-8 (i.e. it could have non-ascii characters, but it doesn't), we 
 *     present it to V8 as ASCII.
 * (B) All other strings we present to V8 as UTF-16.  These are in two groups:
 *   (B.1) those already encoded as UTF-16
 *   (B.2) those that must be recoded, such as latin1 or big5 strings
 *
 * Strings in groups (A) and (B.1) are presented directly to JavaScript.
 *
 * For a string in group (B.2), which requires recoding, we have to calculate 
 * the buffer requirement of that string recoded into UTF-16. 
 *
 * Suppose the user creates a VARCHAR(20) size with character set Y.
 * col->getLength() will be 20 * charset_info[Y]->mbmaxlen
 *
 * If mbmaxlen is 4, we will return a buffer requirement of col->getLength().
 * But if mbmaxlen is less than 4, we will return:
 *  2 * (col->getLength() / mbmaxlen)
 * 
 * So, we assert that there are no 2- or 3-byte character sets that encode
 * characters outside the BMP (particularly, that ujis does not).
 * And, if they do, we would still support them "occasionally" (i.e. as long
 * as they don't cause buffer overflow).  BUT IF YOU REALLY REQUIRE EXTRA-BMP
 * SUPPORT, YOU MUST USE A 4-BYTE CHARACTER SET: utf8mb4, utf16, or utf32.
 */


/* strsz = 0 for char; actual size in bytes for varchar.
   Returns needed size in characters
*/
size_t getUtf16BufferSize(const NdbDictionary::Column *col, size_t strsz) {
  const int & sz = col->getLength();
  const int & minlen = col->getCharset()->mbminlen;
  const int & maxlen = col->getCharset()->mbmaxlen;
  size_t bufsz;
  
  /* see note above re. group (B.2) */
  bufsz = (maxlen == 4 ? sz : sz / maxlen);

  /* Alternate length for VARCHAR: max plausible length of string */
  strsz /= minlen;
  if(strsz > 0 && strsz < bufsz) bufsz = strsz;

  return bufsz;
}


inline bool stringIsAscii(const unsigned char *str, size_t len) {
  for(unsigned int i = 0 ; i < len ; i++) 
    if(str[i] & 128) 
      return false;
  return true;
}


/* It may be a good optimization to do these lookups once per 
   charset and store them somewhere.
*/
bool colIsUtf16(const NdbDictionary::Column *col) {
// we let "utf16le" pass as "utf16"
  return ( (strncmp("utf16", col->getCharset()->csname, 5) == 0) ||
           (strncmp("ucs2",  col->getCharset()->csname, 4) == 0));
}

bool colIsUtf8(const NdbDictionary::Column *col) {
  return (strncmp("utf8", col->getCharset()->csname, 4) == 0);
}

bool colIsLatin1(const NdbDictionary::Column *col) {
  return (strncmp("latin1", col->getCharset()->csname, 6) == 0);
}

bool colIsAscii(const NdbDictionary::Column *col) {
  return (strncmp("ascii", col->getCharset()->csname, 5) == 0);
}


bool encoderShouldRecode(const NdbDictionary::Column *col, 
                         char * buffer, size_t len) 
{
  return ! (colIsUtf16(col) || 
            stringIsAscii((const unsigned char *) buffer, len));
}
