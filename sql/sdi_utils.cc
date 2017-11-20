/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "sql/sdi_utils.h"

#include <string.h>

#include "my_compiler.h"
#include "my_sys.h"
#include "mysql/udf_registration_types.h"
#include "mysqld_error.h"


/* purecov: begin deadcode */
bool create_serialized_meta_data(const char *schema_name MY_ATTRIBUTE((unused)),
                                 const char *table_name MY_ATTRIBUTE((unused)),
                                 uchar **meta_data MY_ATTRIBUTE((unused)),
                                 size_t *meta_data_length MY_ATTRIBUTE((unused)))
{
  /*
    TODO: This function is currently not implemented. The procedure here
    will be along the following lines:

     1. Assert that we have a shared MDL for the given schema, table,
        tablespace and tablespace files (if relevant). These will all be
        serialized into the same SDI blob.
     2. Retrieve the dd::Schema object.
     3. Retrieve the dd::Table object.
    [4. Optional: Retrieve the dd::Tablespace object.]
    [5. Optional: Retrieve the dd::Tablespace_files objects.]
     6. Create a top level SDI object.
     7. Fill inn top level object details, e.g. dictionary version etc.
     8. Fore each relevant object retrieved, serialize the object,
        including preparation of references to loosely coupled objects,
        e.g. collations.
     9. For each serialized object, add it to the top level object as an
        array element.
    10. Generate a string representation of the serialized meta data.
    11. Assign output parameters and return.
  */
  *meta_data_length= 0;
  *meta_data= NULL;
  return false;
}


bool import_serialized_meta_data(const uchar *meta_data MY_ATTRIBUTE((unused)),
                                 size_t meta_data_length MY_ATTRIBUTE((unused)),
                                 bool readonly MY_ATTRIBUTE((unused)))
{
  // TODO: This function is currently not implemented. Return error so
  // that client code will not attempt to open a non-existent table
  my_error(ER_FEATURE_DISABLED, MYF(0), "Serialized metadata import",
           "WL#7069");
  return true;
}


bool different_serialized_meta_data(const uchar *a_meta_data,
                                    size_t a_meta_data_length,
                                    const uchar *b_meta_data,
                                    size_t b_meta_data_length)
{
  if ((a_meta_data_length != b_meta_data_length) ||
      (memcmp(a_meta_data, b_meta_data, a_meta_data_length)))
    return true;
  return false;
}
/* purecov: end */
