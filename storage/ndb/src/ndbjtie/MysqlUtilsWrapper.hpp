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
 * MysqlUtilsWrapper.hpp
 */

#ifndef MysqlUtilsWrapper_hpp
#define MysqlUtilsWrapper_hpp

// API to wrap
#include "CharsetMap.hpp"
#include "decimal_utils.hpp"

struct MysqlUtilsWrapper {

// ---------------------------------------------------------------------------

// mapped by "com_mysql_ndbjtie_mysql_CharsetMap.h"

    static const char *
    CharsetMap__getName
    ( const CharsetMap & obj, int p0 )
    {
        return obj.getName(p0);
    }
    
    static const char *
    CharsetMap__getMysqlName
    ( const CharsetMap & obj, int p0 )
    {
        return obj.getMysqlName(p0);
    }
    
    static int
    CharsetMap__getCharsetNumber
    ( const CharsetMap & obj, const char * p0 )
    {
        return obj.getCharsetNumber(p0);
    }
    
    static int
    CharsetMap__getUTF8CharsetNumber
    ( const CharsetMap & obj )
    {
        return obj.getUTF8CharsetNumber();
    }
    
    static int
    CharsetMap__getUTF16CharsetNumber
    ( const CharsetMap & obj )
    {
        return obj.getUTF16CharsetNumber();
    }
    
    static const bool *  
    CharsetMap__isMultibyte
    ( const CharsetMap & obj, int p0 )
    {
       return obj.isMultibyte(p0);
    }
    
    static CharsetMap::RecodeStatus
    CharsetMap__recode
    ( const CharsetMap & obj, int32_t * p0, int p1, int p2, const void * p3, void * p4 )
    {
        return obj.recode(p0, p1, p2, p3, p4);
    }
    
// ---------------------------------------------------------------------------

// mapped by "com_mysql_ndbjtie_mysql_Utils.h"

    static int
    decimal_str2bin
    ( const char * p0, int p1, int p2, int p3, void * p4, int p5 )
    {
        return ::decimal_str2bin(p0, p1, p2, p3, p4, p5);
    }
    
    static int
    decimal_bin2str
    ( const void * p0, int p1, int p2, int p3, char * p4, int p5 )
    {
        return ::decimal_bin2str(p0, p1, p2, p3, p4, p5);
    }
    
// ---------------------------------------------------------------------------

};

#endif // MysqlUtilsWrapper_hpp
