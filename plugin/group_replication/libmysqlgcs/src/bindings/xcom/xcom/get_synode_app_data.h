/* Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef GET_SYNODE_APP_DATA_H
#define GET_SYNODE_APP_DATA_H

#include "plugin/group_replication/libmysqlgcs/xdr_gen/xcom_vp.h"  // synode_no_array, synode_app_data_array

#ifdef __cplusplus
extern "C" {
#endif

/*
 Error code for the xcom_get_synode_app_data command.
 */
typedef enum {
  XCOM_GET_SYNODE_APP_DATA_OK,
  XCOM_GET_SYNODE_APP_DATA_NOT_CACHED,
  XCOM_GET_SYNODE_APP_DATA_NOT_DECIDED,
  XCOM_GET_SYNODE_APP_DATA_NO_MEMORY,
  XCOM_GET_SYNODE_APP_DATA_ERROR
} xcom_get_synode_app_data_result;

/**
 Retrieves the application payloads decided on the given synodes.

 @param[in] synodes The desired synodes
 @param[out] reply The application payloads of the requested synodes
 @retval XCOM_GET_SYNODE_APP_DATA_OK If successful, and @c reply was written to
 @retval XCOM_GET_SYNODE_APP_DATA_NOT_CACHED If we do not have some requested
 synode's application payload
 @retval XCOM_GET_SYNODE_APP_DATA_NOT_DECIDED If we haven't yet reached
 consensus on some requested synode
 @retval XCOM_GET_SYNODE_APP_DATA_NO_MEMORY If there was an error allocating
 memory
 @retval XCOM_GET_SYNODE_APP_DATA_ERROR If there was some unspecified error
 */
xcom_get_synode_app_data_result xcom_get_synode_app_data(
    synode_no_array const *const synodes, synode_app_data_array *const reply);

#ifdef __cplusplus
}
#endif

#endif /* GET_SYNODE_APP_DATA_H */
