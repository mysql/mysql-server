/* Copyright (c) 2007, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql_priv.h"
#include "unireg.h"                      // REQUIRED by other includes
#include "rpl_rli.h"
#include "rpl_record_old.h"
#include "log_event.h"                          // Log_event_type

size_t
pack_row_old(TABLE *table, MY_BITMAP const* cols,
             uchar *row_data, const uchar *record)
{
  Field **p_field= table->field, *field;
  int n_null_bytes= table->s->null_bytes;
  uchar *ptr;
  uint i;
  my_ptrdiff_t const rec_offset= record - table->record[0];
  my_ptrdiff_t const def_offset= table->default_values_offset();
  memcpy(row_data, record, n_null_bytes);
  ptr= row_data+n_null_bytes;

  for (i= 0 ; (field= *p_field) ; i++, p_field++)
  {
    if (bitmap_is_set(cols,i))
    {
      my_ptrdiff_t const offset=
        field->is_null(rec_offset) ? def_offset : rec_offset;
      field->move_field_offset(offset);
      ptr= field->pack(ptr, field->ptr);
      field->move_field_offset(-offset);
    }
  }
  return (static_cast<size_t>(ptr - row_data));
}


/*
  Unpack a row into a record.

  SYNOPSIS
    unpack_row()
    rli     Relay log info
    table   Table to unpack into
    colcnt  Number of columns to read from record
    record  Record where the data should be unpacked
    row     Packed row data
    cols    Pointer to columns data to fill in
    row_end Pointer to variable that will hold the value of the
            one-after-end position for the row
    master_reclength
            Pointer to variable that will be set to the length of the
            record on the master side
    rw_set  Pointer to bitmap that holds either the read_set or the
            write_set of the table

  DESCRIPTION

      The row is assumed to only consist of the fields for which the
      bitset represented by 'arr' and 'bits'; the other parts of the
      record are left alone.

      At most 'colcnt' columns are read: if the table is larger than
      that, the remaining fields are not filled in.

  RETURN VALUE

      Error code, or zero if no error. The following error codes can
      be returned:

      ER_NO_DEFAULT_FOR_FIELD
        Returned if one of the fields existing on the slave but not on
        the master does not have a default value (and isn't nullable)
 */
#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)
int
unpack_row_old(Relay_log_info *rli,
               TABLE *table, uint const colcnt, uchar *record,
               uchar const *row, MY_BITMAP const *cols,
               uchar const **row_end, ulong *master_reclength,
               MY_BITMAP* const rw_set, Log_event_type const event_type)
{
  DBUG_ASSERT(record && row);
  my_ptrdiff_t const offset= record - (uchar*) table->record[0];
  size_t master_null_bytes= table->s->null_bytes;

  if (colcnt != table->s->fields)
  {
    Field **fptr= &table->field[colcnt-1];
    do
      master_null_bytes= (*fptr)->last_null_byte();
    while (master_null_bytes == Field::LAST_NULL_BYTE_UNDEF &&
           fptr-- > table->field);

    /*
      If master_null_bytes is LAST_NULL_BYTE_UNDEF (0) at this time,
      there were no nullable fields nor BIT fields at all in the
      columns that are common to the master and the slave. In that
      case, there is only one null byte holding the X bit.

      OBSERVE! There might still be nullable columns following the
      common columns, so table->s->null_bytes might be greater than 1.
     */
    if (master_null_bytes == Field::LAST_NULL_BYTE_UNDEF)
      master_null_bytes= 1;
  }

  DBUG_ASSERT(master_null_bytes <= table->s->null_bytes);
  memcpy(record, row, master_null_bytes);            // [1]
  int error= 0;

  bitmap_set_all(rw_set);

  Field **const begin_ptr = table->field;
  Field **field_ptr;
  uchar const *ptr= row + master_null_bytes;
  Field **const end_ptr= begin_ptr + colcnt;
  for (field_ptr= begin_ptr ; field_ptr < end_ptr ; ++field_ptr)
  {
    Field *const f= *field_ptr;

    if (bitmap_is_set(cols, field_ptr -  begin_ptr))
    {
      f->move_field_offset(offset);
      ptr= f->unpack(f->ptr, ptr);
      f->move_field_offset(-offset);
      /* Field...::unpack() cannot return 0 */
      DBUG_ASSERT(ptr != NULL);
    }
    else
      bitmap_clear_bit(rw_set, field_ptr - begin_ptr);
  }

  *row_end = ptr;
  if (master_reclength)
  {
    if (*field_ptr)
      *master_reclength = (*field_ptr)->ptr - table->record[0];
    else
      *master_reclength = table->s->reclength;
  }

  /*
    Set properties for remaining columns, if there are any. We let the
    corresponding bit in the write_set be set, to write the value if
    it was not there already. We iterate over all remaining columns,
    even if there were an error, to get as many error messages as
    possible.  We are still able to return a pointer to the next row,
    so redo that.

    This generation of error messages is only relevant when inserting
    new rows.
   */
  for ( ; *field_ptr ; ++field_ptr)
  {
    uint32 const mask= NOT_NULL_FLAG | NO_DEFAULT_VALUE_FLAG;

    DBUG_PRINT("debug", ("flags = 0x%x, mask = 0x%x, flags & mask = 0x%x",
                         (*field_ptr)->flags, mask,
                         (*field_ptr)->flags & mask));

    if (event_type == WRITE_ROWS_EVENT &&
        ((*field_ptr)->flags & mask) == mask)
    {
      rli->report(ERROR_LEVEL, ER_NO_DEFAULT_FOR_FIELD,
                  "Field `%s` of table `%s`.`%s` "
                  "has no default value and cannot be NULL",
                  (*field_ptr)->field_name, table->s->db.str,
                  table->s->table_name.str);
      error = ER_NO_DEFAULT_FOR_FIELD;
    }
    else
      (*field_ptr)->set_default();
  }

  return error;
}
#endif
