/* Copyright 2007 MySQL AB. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "mysql_priv.h"
#include "rpl_rli.h"
#include "rpl_record.h"
#include "slave.h"                  // Need to pull in slave_print_msg
#include "rpl_utility.h"
#include "rpl_rli.h"

/**
   Pack a record of data for a table into a format suitable for
   transfer via the binary log.

   The format for a row in transfer with N fields is the following:

   ceil(N/8) null bytes:
       One null bit for every column *regardless of whether it can be
       null or not*. This simplifies the decoding. Observe that the
       number of null bits is equal to the number of set bits in the
       @c cols bitmap. The number of null bytes is the smallest number
       of bytes necessary to store the null bits.

       Padding bits are 1.

   N packets:
       Each field is stored in packed format.


   @param table    Table describing the format of the record

   @param cols     Bitmap with a set bit for each column that should
                   be stored in the row

   @param row_data Pointer to memory where row will be written

   @param record   Pointer to record that should be packed. It is
                   assumed that the pointer refers to either @c
                   record[0] or @c record[1], but no such check is
                   made since the code does not rely on that.

   @return The number of bytes written at @c row_data.
 */
#if !defined(MYSQL_CLIENT)
size_t
pack_row(TABLE *table, MY_BITMAP const* cols,
         uchar *row_data, const uchar *record)
{
  Field **p_field= table->field, *field;
  int const null_byte_count= (bitmap_bits_set(cols) + 7) / 8;
  uchar *pack_ptr = row_data + null_byte_count;
  uchar *null_ptr = row_data;
  my_ptrdiff_t const rec_offset= record - table->record[0];
  my_ptrdiff_t const def_offset= table->s->default_values - table->record[0];

  DBUG_ENTER("pack_row");

  /*
    We write the null bits and the packed records using one pass
    through all the fields. The null bytes are written little-endian,
    i.e., the first fields are in the first byte.
   */
  unsigned int null_bits= (1U << 8) - 1;
  // Mask to mask out the correct but among the null bits
  unsigned int null_mask= 1U;
  for ( ; (field= *p_field) ; p_field++)
  {
    DBUG_PRINT("debug", ("null_mask=%d; null_ptr=%p; row_data=%p; null_byte_count=%d",
                         null_mask, null_ptr, row_data, null_byte_count));
    if (bitmap_is_set(cols, p_field - table->field))
    {
      my_ptrdiff_t offset;
      if (field->is_null(rec_offset))
      {
        offset= def_offset;
        null_bits |= null_mask;
      }
      else
      {
        offset= rec_offset;
        null_bits &= ~null_mask;

        /*
          We only store the data of the field if it is non-null

          For big-endian machines, we have to make sure that the
          length is stored in little-endian format, since this is the
          format used for the binlog.
        */
#ifndef DBUG_OFF
        const uchar *old_pack_ptr= pack_ptr;
#endif
        pack_ptr= field->pack(pack_ptr, field->ptr + offset,
                              field->max_data_length(), TRUE);
        DBUG_PRINT("debug", ("field: %s; pack_ptr: 0x%lx;"
                             " pack_ptr':0x%lx; bytes: %d",
                             field->field_name, (ulong) old_pack_ptr,
                             (ulong) pack_ptr,
                             (int) (pack_ptr - old_pack_ptr)));
      }

      null_mask <<= 1;
      if ((null_mask & 0xFF) == 0)
      {
        DBUG_ASSERT(null_ptr < row_data + null_byte_count);
        null_mask = 1U;
        *null_ptr++ = null_bits;
        null_bits= (1U << 8) - 1;
      }
    }
  }

  /*
    Write the last (partial) byte, if there is one
  */
  if ((null_mask & 0xFF) > 1)
  {
    DBUG_ASSERT(null_ptr < row_data + null_byte_count);
    *null_ptr++ = null_bits;
  }

  /*
    The null pointer should now point to the first byte of the
    packed data. If it doesn't, something is very wrong.
  */
  DBUG_ASSERT(null_ptr == row_data + null_byte_count);
  DBUG_DUMP("row_data", row_data, pack_ptr - row_data);
  DBUG_RETURN(static_cast<size_t>(pack_ptr - row_data));
}
#endif


/**
   Unpack a row into @c table->record[0].

   The function will always unpack into the @c table->record[0]
   record.  This is because there are too many dependencies on where
   the various member functions of Field and subclasses expect to
   write.

   The row is assumed to only consist of the fields for which the corresponding
   bit in bitset @c cols is set; the other parts of the record are left alone.

   At most @c colcnt columns are read: if the table is larger than
   that, the remaining fields are not filled in.

   @param rli     Relay log info
   @param table   Table to unpack into
   @param colcnt  Number of columns to read from record
   @param row_data
                  Packed row data
   @param cols    Pointer to bitset describing columns to fill in
   @param row_end Pointer to variable that will hold the value of the
                  one-after-end position for the row
   @param master_reclength
                  Pointer to variable that will be set to the length of the
                  record on the master side

   @retval 0 No error

   @retval ER_NO_DEFAULT_FOR_FIELD
   Returned if one of the fields existing on the slave but not on the
   master does not have a default value (and isn't nullable)

 */
#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)
int
unpack_row(Relay_log_info const *rli,
           TABLE *table, uint const colcnt,
           uchar const *const row_data, MY_BITMAP const *cols,
           uchar const **const row_end, ulong *const master_reclength)
{
  DBUG_ENTER("unpack_row");
  DBUG_ASSERT(row_data);
  size_t const master_null_byte_count= (bitmap_bits_set(cols) + 7) / 8;
  int error= 0;

  uchar const *null_ptr= row_data;
  uchar const *pack_ptr= row_data + master_null_byte_count;

  Field **const begin_ptr = table->field;
  Field **field_ptr;
  Field **const end_ptr= begin_ptr + colcnt;

  DBUG_ASSERT(null_ptr < row_data + master_null_byte_count);

  // Mask to mask out the correct bit among the null bits
  unsigned int null_mask= 1U;
  // The "current" null bits
  unsigned int null_bits= *null_ptr++;
  uint i= 0;
  table_def *tabledef= ((Relay_log_info*)rli)->get_tabledef(table);
  for (field_ptr= begin_ptr ; field_ptr < end_ptr && *field_ptr ; ++field_ptr)
  {
    Field *const f= *field_ptr;

    /*
      No need to bother about columns that does not exist: they have
      gotten default values when being emptied above.
     */
    if (bitmap_is_set(cols, field_ptr -  begin_ptr))
    {
      if ((null_mask & 0xFF) == 0)
      {
        DBUG_ASSERT(null_ptr < row_data + master_null_byte_count);
        null_mask= 1U;
        null_bits= *null_ptr++;
      }

      DBUG_ASSERT(null_mask & 0xFF); // One of the 8 LSB should be set

      /* Field...::unpack() cannot return 0 */
      DBUG_ASSERT(pack_ptr != NULL);

      if ((null_bits & null_mask) && f->maybe_null())
        f->set_null();
      else
      {
        f->set_notnull();

        /*
          We only unpack the field if it was non-null.
          Use the master's size information if available else call
          normal unpack operation.
        */
        uint16 const metadata= tabledef->field_metadata(i);
#ifndef DBUG_OFF
        uchar const *const old_pack_ptr= pack_ptr;
#endif
        pack_ptr= f->unpack(f->ptr, pack_ptr, metadata, TRUE);
	DBUG_PRINT("debug", ("field: %s; metadata: 0x%x;"
                             " pack_ptr: 0x%lx; pack_ptr': 0x%lx; bytes: %d",
                             f->field_name, metadata,
                             (ulong) old_pack_ptr, (ulong) pack_ptr,
                             (int) (pack_ptr - old_pack_ptr)));
      }

      null_mask <<= 1;
    }
    i++;
  }

  /*
    throw away master's extra fields
  */
  uint max_cols= min(tabledef->size(), cols->n_bits);
  for (; i < max_cols; i++)
  {
    if (bitmap_is_set(cols, i))
    {
      if ((null_mask & 0xFF) == 0)
      {
        DBUG_ASSERT(null_ptr < row_data + master_null_byte_count);
        null_mask= 1U;
        null_bits= *null_ptr++;
      }
      DBUG_ASSERT(null_mask & 0xFF); // One of the 8 LSB should be set

      if (!((null_bits & null_mask) && tabledef->maybe_null(i)))
        pack_ptr+= tabledef->calc_field_size(i, (uchar *) pack_ptr);
      null_mask <<= 1;
    }
  }

  /*
    We should now have read all the null bytes, otherwise something is
    really wrong.
   */
  DBUG_ASSERT(null_ptr == row_data + master_null_byte_count);

  DBUG_DUMP("row_data", row_data, pack_ptr - row_data);

  *row_end = pack_ptr;
  if (master_reclength)
  {
    if (*field_ptr)
      *master_reclength = (*field_ptr)->ptr - table->record[0];
    else
      *master_reclength = table->s->reclength;
  }
  
  DBUG_RETURN(error);
}

/**
  Fills @c table->record[0] with default values.

  First @c empty_record() is called and then, additionally, fields are
  initialized explicitly with a call to @c set_default().

  For optimization reasons, the explicit initialization can be skipped for
  first @c skip fields. This is useful if later we are going to fill these 
  fields from other source (e.g. from a Rows replication event).

  If @c check is true, fields are explicitly initialized only if they have
  default value or can be NULL. Otherwise error is reported.
 
  @param log    Used to report errors.
  @param table  Table whose record[0] buffer is prepared. 
  @param skip   Number of columns for which default value initialization 
                should be skipped.
  @param check  Indicates if errors should be checked when setting default
                values.
                
  @returns 0 on success. 
 */ 
int prepare_record(const Slave_reporting_capability *const log, 
                   TABLE *const table, 
                   const uint skip, const bool check)
{
  DBUG_ENTER("prepare_record");

  int error= 0;
  empty_record(table);

  if (skip >= table->s->fields)  // nothing to do
    DBUG_RETURN(0);

  /* Explicit initialization of fields */

  for (Field **field_ptr= table->field+skip ; *field_ptr ; ++field_ptr)
  {
    uint32 const mask= NOT_NULL_FLAG | NO_DEFAULT_VALUE_FLAG;
    Field *const f= *field_ptr;

    if (check && ((f->flags & mask) == mask))
    {
      DBUG_ASSERT(log);
      error= ER_NO_DEFAULT_FOR_FIELD;
      log->report(ERROR_LEVEL, error,
                  "Field `%s` of table `%s`.`%s` "
                  "has no default value and cannot be NULL",
                  f->field_name, table->s->db.str,
                  table->s->table_name.str);
      my_error(error, MYF(0), f->field_name);
    }
    else
      f->set_default();
  }

  DBUG_RETURN(error);
}

#endif // HAVE_REPLICATION
