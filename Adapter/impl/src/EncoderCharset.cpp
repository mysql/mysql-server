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
 * But working with external UTF-16 depends on MySQL's UTF-16-LE charset, 
 * which is available only in MySQL 5.6 and higher. 
 * 
 * (A) For any strict ASCII string, even if its character set is latin1 or
 *     UTF-8 (i.e. it could have non-ascii characters, but it doesn't), we 
 *     present it to V8 as external ASCII.
 * (B) If a string is UTF16LE, we present it as external UTF-16.
 * (C) If a string is UTF-8, we create a new JS string (one copy operation)
 * (D) All others must be recoded.  There are two possibilities:
 *   (D.1) Recode to UTF16LE and present as external string (one copy operation)
 *   (D.2) Recode to UTF8 and create a new JS string (two copy operations)
 *
 * For a string in group (D.1), which requires recoding, we have to calculate 
 * the buffer requirement of that string recoded into UTF-16. 
 * If mbmaxlen is 4, this should be col->getLength().
 * But if mbmaxlen is less than 4, it should be:
 *  2 * (col->getLength() / mbmaxlen)
 *
 * Some support for D.1 was implemented (around bzr revno 440) and then
 * rolled back.  
 * 
 */

// move the comment back into type enocders?
// reimagine this as a file that takes a charsetinfo * and returns
// a usable struct

/* It may be a good optimization to do these lookups once per 
   charset and store them somewhere.
*/
bool colIsUtf16le(const NdbDictionary::Column *col) {
  return (strncmp("utf16le", col->getCharset()->csname, 7) == 0);
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

bool colIsMultibyte(const NdbDictionary::Column *col) { 
  return (col->getCharset()->mbminlen > 1);
}

unsigned int colSizeInCharacters(const NdbDictionary::Column *col) {
  return col->getLength() / col->getCharset()->mbmaxlen;
}

