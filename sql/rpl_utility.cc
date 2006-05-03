/* Copyright 2006 MySQL AB. All rights reserved.

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

#include "rpl_utility.h"

uint32
field_length_from_packed(enum_field_types const field_type, 
                         byte const *const data)
{
  uint32 length;

  switch (field_type) {
  case MYSQL_TYPE_DECIMAL:
  case MYSQL_TYPE_NEWDECIMAL:
    length= ~0UL;
    break;
  case MYSQL_TYPE_YEAR:
  case MYSQL_TYPE_TINY:
    length= 1;
    break;
  case MYSQL_TYPE_SHORT:
    length= 2;
    break;
  case MYSQL_TYPE_INT24:
    length= 3;
    break;
  case MYSQL_TYPE_LONG:
    length= 4;
    break;
#ifdef HAVE_LONG_LONG
  case MYSQL_TYPE_LONGLONG:
    length= 8;
    break;
#endif
  case MYSQL_TYPE_FLOAT:
    length= sizeof(float);
    break;
  case MYSQL_TYPE_DOUBLE:
    length= sizeof(double);
    break;
  case MYSQL_TYPE_NULL:
    length= 0;
    break;
  case MYSQL_TYPE_NEWDATE:
    length= 3;
    break;
  case MYSQL_TYPE_DATE:
    length= 4;
    break;
  case MYSQL_TYPE_TIME:
    length= 3;
    break;
  case MYSQL_TYPE_TIMESTAMP:
    length= 4;
    break;
  case MYSQL_TYPE_DATETIME:
    length= 8;
    break;
    break;
  case MYSQL_TYPE_BIT:
    length= ~0UL;
    break;
  default:
    /* This case should never be chosen */
    DBUG_ASSERT(0);
    /* If something goes awfully wrong, it's better to get a string than die */
  case MYSQL_TYPE_STRING:
    length= uint2korr(data);
    break;

  case MYSQL_TYPE_ENUM:
  case MYSQL_TYPE_SET:
  case MYSQL_TYPE_VAR_STRING:
  case MYSQL_TYPE_VARCHAR:
    length= ~0UL;                               // NYI
    break;

  case MYSQL_TYPE_TINY_BLOB:
  case MYSQL_TYPE_MEDIUM_BLOB:
  case MYSQL_TYPE_LONG_BLOB:
  case MYSQL_TYPE_BLOB:
  case MYSQL_TYPE_GEOMETRY:
    length= ~0UL;                               // NYI
    break;
  }
}

/*********************************************************************
 *                   table_def member definitions                    *
 *********************************************************************/

/*
  Is the definition compatible with a table?

  Compare the definition with a table to see if it is compatible with
  it.  A table definition is compatible with a table if
  - the columns types of the table definition is a (not necessarily
    proper) prefix of the column type of the table, or
  - the other way around
*/
int
table_def::compatible_with(RELAY_LOG_INFO *rli, TABLE *table)
  const
{
  /*
    We only check the initial columns for the tables.
  */
  uint const cols_to_check= min(table->s->fields, size());
  int error= 0;

  TABLE_SHARE const *const tsh= table->s;

  /*
    To get proper error reporting for all columns of the table, we
    both check the width and iterate over all columns.
  */
  if (tsh->fields < size())
  {
    DBUG_ASSERT(tsh->db.str && tsh->table_name.str);
    error= 1;
    slave_print_msg(ERROR_LEVEL, rli, ER_BINLOG_ROW_WRONG_TABLE_DEF,
                    "Table width mismatch - "
                    "received %u columns, %s.%s has %u columns",
                    size(), tsh->db.str, tsh->table_name.str, tsh->fields);
  }

  for (uint col= 0 ; col < cols_to_check ; ++col)
  {
    if (table->field[col]->type() != type(col))
    {
      DBUG_ASSERT(col < size() && col < tsh->fields);
      DBUG_ASSERT(tsh->db.str && tsh->table_name.str);
      error= 1;
      slave_print_msg(ERROR_LEVEL, rli, ER_BINLOG_ROW_WRONG_TABLE_DEF,
                      "Column %d type mismatch - "
                      "received type %d, %s.%s has type %d",
                      col, type(col), tsh->db.str, tsh->table_name.str,
                      table->field[col]->type());
    }
  }

  return error;
}
