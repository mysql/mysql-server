/*
   Copyright (c) 2003, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

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

int ndb_error_get_next(int index,
                       int* err_no,
                       const char** status_msg,
                       const char** class_msg,
                       const char** error_msg);

#endif /* doxygen skip internal*/

#ifdef __cplusplus
}
#endif

#endif
