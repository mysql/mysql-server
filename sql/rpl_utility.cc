/* Copyright (C) 2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include "rpl_utility.h"


/*********************************************************************
 *                   table_def member definitions                    *
 *********************************************************************/

/*
  This function returns the field size in raw bytes based on the type
  and the encoded field data from the master's raw data.
*/
uint32 table_def::calc_field_size(uint col, uchar *master_data)
{
  uint32 length;

  switch (type(col)) {
  case MYSQL_TYPE_NEWDECIMAL:
    length= my_decimal_get_binary_size(m_field_metadata[col] >> 8, 
             m_field_metadata[col] - ((m_field_metadata[col] >> 8) << 8));
    break;
  case MYSQL_TYPE_DECIMAL:
  case MYSQL_TYPE_FLOAT:
  case MYSQL_TYPE_DOUBLE:
    length= m_field_metadata[col];
    break;
  case MYSQL_TYPE_SET:
  case MYSQL_TYPE_ENUM:
  case MYSQL_TYPE_STRING:
  {
    if (((m_field_metadata[col] & 0xff00) == (MYSQL_TYPE_SET << 8)) ||
        ((m_field_metadata[col] & 0xff00) == (MYSQL_TYPE_ENUM << 8)))
      length= m_field_metadata[col] & 0x00ff;
    else
    {
      length= m_field_metadata[col] & 0x00ff;
      if (length > 255)
        length= uint2korr(master_data) + 2;
      else
        length= (uint) *master_data + 1;
    }
    break;
  }
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
  case MYSQL_TYPE_NULL:
    length= 0;
    break;
  case MYSQL_TYPE_NEWDATE:
    length= 3;
    break;
  case MYSQL_TYPE_DATE:
  case MYSQL_TYPE_TIME:
    length= 3;
    break;
  case MYSQL_TYPE_TIMESTAMP:
    length= 4;
    break;
  case MYSQL_TYPE_DATETIME:
    length= 8;
    break;
  case MYSQL_TYPE_BIT:
  {
    uint from_len= (m_field_metadata[col] >> 8U) & 0x00ff;
    uint from_bit_len= m_field_metadata[col] & 0x00ff;
    length= from_len + ((from_bit_len > 0) ? 1 : 0);
    break;
  }
  case MYSQL_TYPE_VARCHAR:
    length= m_field_metadata[col] > 255 ? 2 : 1; // c&p of Field_varstring::data_length()
    length+= length == 1 ? (uint32) *master_data : uint2korr(master_data);
    break;
  case MYSQL_TYPE_TINY_BLOB:
  case MYSQL_TYPE_MEDIUM_BLOB:
  case MYSQL_TYPE_LONG_BLOB:
  case MYSQL_TYPE_BLOB:
  case MYSQL_TYPE_GEOMETRY:
  {
    Field_blob fb(m_field_metadata[col]);
    length= fb.get_packed_size(master_data);
    break;
  }
  default:
    length= -1;
  }
  return length;
}

/*
  Is the definition compatible with a table?

*/
int
table_def::compatible_with(RELAY_LOG_INFO const *rli_arg, TABLE *table)
  const
{
  /*
    We only check the initial columns for the tables.
  */
  uint const cols_to_check= min(table->s->fields, size());
  int error= 0;
  RELAY_LOG_INFO const *rli= const_cast<RELAY_LOG_INFO*>(rli_arg);

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
                    (uint) size(), tsh->db.str, tsh->table_name.str,
                    tsh->fields);
  }

  /*
    We now check for column type and size compatibility.
  */
  for (uint col= 0 ; col < cols_to_check ; ++col)
  {
    /*
      Checking types.
    */
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
