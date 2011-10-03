/*
 Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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
 * CharsetMap.hpp
 */

#ifndef CharsetMap_hpp
#define CharsetMap_hpp

#include "ndb_types.h"

/**
 * Handles encoding issues for character data
 * while keeping MySQL's CHARSET_INFO structure hidden.
 */
class CharsetMap {
public:

    CharsetMap();
    /* The compiler-generated destructor is OK. */
    /* The compiler-generated copy constructor is OK. */
    /* The compiler-generated assignment operator is OK. */

    /**
     * Initializes any global CharsetMap resources.
     * Should be called exactly once before any other CharsetMap function
     * from a single thread.
     */
    static void init();

    /**
     * Releases all global CharsetMap resources.
     * Also not thread-safe
     */
    static void unload();

    /**
     * Returns a standard character set name.
     *
     * The cs_number argument in getName(), getMysqlName(), and recode()
     * can be obtained from NdbDictionary::Column::getCharsetNumber().
     *
     * getName() returns a name that in most cases will be a preferred name
     * from http://www.iana.org/assignments/character-sets and will
     * be recognized and usable by Java (e.g. java.nio, java.io, and java.lang).
     * However it may return "binary" if a column is BLOB / BINARY / VARBINARY,
     * or it may return the name of an obscure MySQL character set such as
     * "keybcs2" or "dec8". 
     */
    const char * getName(int cs_number) const;

    /**
     * Returns just the internal mysql name of the charset.
     */
    const char * getMysqlName(int cs_number) const;

    /**
     * Takes the mysql name (not the standardized name) and returns a
     * character set number.
     */
    int getCharsetNumber(const char *mysql_name) const;

    /**
     * Convenience function for UTF-8.
     */
    int getUTF8CharsetNumber() const;

    /**
     * Convenience function for UTF-16.
     */
    int getUTF16CharsetNumber() const;

    /**
     * The return status of a buffer recode operation.
     */
    enum RecodeStatus {
        RECODE_OK ,
        RECODE_BAD_CHARSET ,
        RECODE_BAD_SRC ,
        RECODE_BUFF_TOO_SMALL
    };
    
    /**
     * Returns true if this charset number refers to a multibyte charset;
     * otherwise false.
     */
  const bool * isMultibyte(int cs_number) const;

    /**
     * Recodes the content of a source buffer into destination buffer.
     *
     * Takes five arguments:
     * lengths is an array of two ints: first the source
     *  buffer length, then the destination buffer length.
     * From and To are character set numbers.
     *  src and dest are buffers.
     *
     * On return, lengths[0] is set to the number of bytes consumed from src
     * and lengths[1] to the number of bytes written to dest.
     *
     * The string in src will be recoded from charset cs_from to charset cs_to.
     * If the conversion is successful we return RECODE_OK.
     * Other return values are noted above.
     */
    RecodeStatus recode(Int32 *lengths /* IN/OUT */,
                        int cs_from, int cs_to, const void *src,
                        void *dest) const;
                        
};

#endif // CharsetMap_hpp
