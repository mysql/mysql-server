/*
 Copyright (c) 2010, 2024, Oracle and/or its affiliates.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is designed to work with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have either included with
 the program or referenced in the documentation.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/*
 *  CharsetMap.cpp
 */

#include "CharsetMap.hpp"
#include "CharsetMapImpl.h"
#include "my_sys.h"
#include "mysql.h"
#include "mysql/strings/m_ctype.h"

bool m_false_result = false;
bool m_true_result = true;

/* _map is a static pointer visible only within the scope of this file.
   A singleton CharsetMapImpl serves every instance of CharsetMap.
*/
static CharsetMapImpl *_map = nullptr;

/* Initialization allocates the CharsetMapImpl and initializes its mutex.
 But we defer building the map of charset names, so as not to create
 any sort of ordering dependency that would require mysql_init() to
 be run first.
 */

void CharsetMap::init() {
  if (_map == nullptr) _map = new CharsetMapImpl;
}

/* Free the CharsetMapImpl at shutdown time.
 */
void CharsetMap::unload() {
  delete _map;
  _map = nullptr;
}

/* On the first invocation of the CharsetMap constructor, it completes the
   initialization of the CharsetMapImpl by building the map of character set
   names.
*/
CharsetMap::CharsetMap() {
  _map->lock();
  if (_map->ready == 0) _map->build_map();
  _map->unlock();
}

const char *CharsetMap::getName(int csnum) const {
  return _map->getName(csnum);
}

const char *CharsetMap::getMysqlName(int csnum) const {
  CHARSET_INFO *cs = get_charset(csnum, MYF(0));
  return cs ? cs->csname : nullptr;
}

int CharsetMap::getUTF8CharsetNumber() const { return _map->UTF8Charset; }

int CharsetMap::getUTF16CharsetNumber() const { return _map->UTF16Charset; }

int CharsetMap::getCharsetNumber(const char *name) const {
  return get_charset_number(name, MY_CS_PRIMARY);
}

const bool *CharsetMap::isMultibyte(int cs_number) const {
  CHARSET_INFO *cset = get_charset(cs_number, MYF(0));
  if (cset == nullptr) return nullptr;
  return use_mb(cset) ? &m_true_result : &m_false_result;
}

CharsetMap::RecodeStatus CharsetMap::recode(Int32 *lengths, int From, int To,
                                            const void *void_src,
                                            void *void_dest) const {
  Int32 &total_read = lengths[0];     // IN/OUT
  Int32 &total_written = lengths[1];  // IN/OUT
  my_wc_t wide;
  my_wc_t mystery_char = '?';  // used in place of unmappable characters
  const unsigned char *src = (const unsigned char *)void_src;
  unsigned char *dest = (unsigned char *)void_dest;
  CHARSET_INFO *csFrom = get_charset(From, MYF(0));
  CHARSET_INFO *csTo = get_charset(To, MYF(0));

  if (!(csTo && csFrom)) return RECODE_BAD_CHARSET;

  Int32 src_len = lengths[0];
  Int32 dest_len = lengths[1];
  const unsigned char *src_end = src + src_len;
  unsigned char *dest_end = dest + dest_len;
  total_read = 0;     // i.e. lengths[0] = 0;
  total_written = 0;  // i.e. lengths[1] = 0;

  while (src < src_end) {
    /* First recode from source character to 32-bit wide character */
    int nread = csFrom->cset->mb_wc(csFrom, &wide, src, src_end);
    if (nread < 0) return RECODE_BUFF_TOO_SMALL;
    if (nread == 0) return RECODE_BAD_SRC;

    /* Then recode from wide character to target character */
    int nwritten = csTo->cset->wc_mb(csTo, wide, dest, dest_end);
    if (nwritten == MY_CS_ILUNI) {
      /* Character does not exist in target charset */
      nwritten = csTo->cset->wc_mb(csTo, mystery_char, dest, dest_end);
    }
    if (nwritten < 0) return RECODE_BUFF_TOO_SMALL;

    total_read += nread;
    src += nread;
    total_written += nwritten;
    dest += nwritten;
  }

  return RECODE_OK;
}
