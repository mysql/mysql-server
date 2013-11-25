/*
Copyright (c) 2003, 2011, 2013, Oracle and/or its affiliates. All rights
reserved.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of
the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
02110-1301  USA
*/

#include "binary_log.h"
#include <list>

using namespace binary_log;

namespace binary_log
{
/**
 *Errors you can get from the API
 */
const char *bapi_error_messages[]=
{
  "All OK",
  "End of File",
  "Unexpected failure",
  "binlog_checksum is enabled on the master. Set them to NONE.",
  "Could not notify master about checksum awareness.\n"
  "Master returned no rows for the query\n"
  "SHOW GLOBAL VARIABLES LIKE 'BINLOG_CHECKSUM.",
  "Unable to set up connection",
  "Binlog Version not supported",
  "Error in packet length. Binlog checksums may be enabled on the master.\n"
  "Please set it to NONE.",
  "Error in executing MySQL Query on the server",
  ""
};

/*
  Get a string describing an error from BAPI.

  @param  error_no   the error number

  @retval buf        buffer containing the error message
*/
const char* str_error(int error_no)
{
  char *msg= NULL;
  if (error_no != ERR_OK)
  {
    if ((error_no > ERR_OK) && (error_no < ERROR_CODE_COUNT))
      msg= (char*)bapi_error_messages[error_no];
    else
      msg= (char*)"Unknown error";
   }
   return msg;
}
}
