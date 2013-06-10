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

/* CharsetMapImpl
 *
 * The private singleton implementation behind the public CharsetMap class
 *
 */

#include "NdbMutex.h"

#define CHARSET_MAP_HASH_TABLE_SIZE 256

class MapTableItem 
{
public:
    MapTableItem() : name(0), value(0), next(0) {};
    const char *name;
    const char *value;
    MapTableItem *next;
};


class CharsetMapImpl : public NdbLockable 
{
public:    
    CharsetMapImpl() : NdbLockable() , ready(0) , collisions(0) {};
       
    /** getName() returns a character set name that in most cases 
     will be a preferred name from 
     http://www.iana.org/assignments/character-sets and will be recognized 
     and usable by Java (e.g. java.nio, java.io, and java.lang).  
     However it may return "binary" if a column is BLOB / BINARY / VARBINARY,
     or it may return the name of an uncommon, rarely-used MySQL character set
     such as "keybcs2" or "dec8".
     */
    const char * getName(int);
            
    const char * mysql_charset_name[256];

    int UTF16Charset;
    int UTF8Charset;

    void build_map();
    int ready;
    int collisions;
    int n_items;

private:
    void put(const char *, const char *);
    const char * get(const char *) const;
    int hash(const char *) const;
    MapTableItem map[CHARSET_MAP_HASH_TABLE_SIZE];
};
