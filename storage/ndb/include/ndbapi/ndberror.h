/*
   Copyright (C) 2004-2006 MySQL AB
    All rights reserved. Use is subject to license terms.

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

#ifndef NDBERROR_H
#define NDBERROR_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL

typedef enum
{
  ndberror_st_success = 0,
  ndberror_st_temporary = 1,
  ndberror_st_permanent = 2,
  ndberror_st_unknown = 3
} ndberror_status_enum;

typedef enum
{
  ndberror_cl_none = 0,  
  ndberror_cl_application = 1,  
  ndberror_cl_no_data_found = 2,
  ndberror_cl_constraint_violation = 3,
  ndberror_cl_schema_error = 4,
  ndberror_cl_user_defined = 5,
  ndberror_cl_insufficient_space = 6,
  ndberror_cl_temporary_resource = 7,
  ndberror_cl_node_recovery = 8,
  ndberror_cl_overload = 9,
  ndberror_cl_timeout_expired = 10,
  ndberror_cl_unknown_result = 11,
  ndberror_cl_internal_error = 12,
  ndberror_cl_function_not_implemented = 13,
  ndberror_cl_unknown_error_code = 14,
  ndberror_cl_node_shutdown = 15,
  ndberror_cl_configuration = 16,
  ndberror_cl_schema_object_already_exists = 17,
  ndberror_cl_internal_temporary = 18
} ndberror_classification_enum;


typedef struct {

  /**
   * Error status.  
   */
  ndberror_status_enum status;

  /**
   * Error type
   */
  ndberror_classification_enum classification;
  
  /**
   * Error code
   */
  int code;

  /**
   * Mysql error code
   */
  int mysql_code;

  /**
   * Error message
   */
  const char * message;

  /**
   * The detailed description.  This is extra information regarding the 
   * error which is not included in the error message.
   *
   * @note Is NULL when no details specified
   */
  char * details;

} ndberror_struct;


typedef ndberror_status_enum ndberror_status;
typedef  ndberror_classification_enum ndberror_classification;

const char *ndberror_status_message(ndberror_status);
const char *ndberror_classification_message(ndberror_classification);
void ndberror_update(ndberror_struct *);
int ndb_error_string(int err_no, char *str, int size);

#endif /* doxygen skip internal*/

#ifdef __cplusplus
}
#endif

#endif
