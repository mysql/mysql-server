/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef TABLEINFO_PS_HPP
#define TABLEINFO_PS_HPP

#include <Vector.hpp>
#include <ndb_types.h>
#include <string.h>
#include <NdbMem.h>

struct TableInfo {
  Uint32 tableId;
  char* tableName;
};

/**
 * @class TableInfoPS
 * @brief Meta information about tables stored on PS
 */
class TableInfoPs {
public:
  inline void    insert(const Uint32 tableId, const char * tableName);

  inline bool    del(const Uint32 tableId);

  inline char *  getTableName(const Uint32 tableId) const;

private:
  Vector<struct TableInfo*> tableInfo;  

  inline TableInfo * lookup(const Uint32 tableId) const;
  inline TableInfo * lookup(const Uint32 tableId , Uint32 * pos) const;
};

inline 
TableInfo *
TableInfoPs::lookup(const Uint32 tableId) const{
  TableInfo * table;
  Uint32 i=0;
  
  while(i<tableInfo.size()) {
    table=tableInfo[i];
    if(table->tableId == tableId)
      return table;
    i++;
  }
  return 0;
}

inline 
TableInfo *
TableInfoPs::lookup(const Uint32 tableId, Uint32 * pos ) const{
  TableInfo * table;
  Uint32 i=0;  
  while(i<tableInfo.size()) {
    table=tableInfo[i];
    if(table->tableId == tableId) {
      *pos=i;
      return table;
    }
    i++;
  }
  return 0;
}


inline 
char * 
TableInfoPs::getTableName(const Uint32 tableId) const{
  TableInfo * table;
  table=lookup(tableId);
  if(table!=0)
    return table->tableName;
  return 0;
}


inline 
void
TableInfoPs::insert(const Uint32 tableId, const char * tableName) {
  TableInfo * table = new TableInfo;
  table->tableId=tableId;
  table->tableName=strdup(tableName);  
  tableInfo.push_back(table);
}

inline 
bool
TableInfoPs::del(const Uint32 tableId) {

  TableInfo * table;
  Uint32 i=0;
  table =  lookup(tableId, &i);
  
  if(table!=0) {
    NdbMem_Free(table->tableName);
    delete table;
    tableInfo.erase(i);
    return true;
  }
  return false;
}

#endif
