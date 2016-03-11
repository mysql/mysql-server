/*
   Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD_ROUTINE_INCLUDED
#define DD_ROUTINE_INCLUDED

#include "my_global.h"

#include "sp.h"               // enum_sp_return_code

namespace dd {
  class Schema;

namespace cache
{
  class Dictionary_client;
}

/**
  Find routine in DD tables.

  @param[in]  dd_client   Dictionary_client object.
  @param[in]  name        Name of the routine.
  @param[in]  type        SP_TYPE_FUNCTION or SP_TYPE_PROCEDURE.
  @param[out] routine     dd::Routine object of the routine.

  @retval SP_OK      ON SUCCESS
  @retval non-SP_OK  ON FAILURE
*/

enum_sp_return_code find_routine(cache::Dictionary_client *dd_client,
                                 sp_name *name,
                                 enum_sp_type type,
                                 const Routine **routine);


/**
  Prepares dd:Routine object from sp_head and updates DD tables
  accordingly.

  @param[in]  thd      Thread handle.
  @param[in]  schema   Schema to create the routine in.
  @param[in]  sp       Stored routine object to store.

  @retval SP_OK      ON SUCCESS
  @retval non-SP_OK  ON FAILURE
*/

enum_sp_return_code create_routine(THD *thd, const Schema *schema, sp_head *sp);


/**
  Removes routine from the DD tables.

  @param[in]  thd     Thread handle.
  @param[in]  routine Procedure or Function to drop.

  @retval SP_OK      ON SUCCESS
  @retval non-SP_OK  ON FAILURE
*/

enum_sp_return_code remove_routine(THD *thd, const Routine *routine);


/**
  Alters routine characteristics in the DD table.

  @param[in]  thd       Thread handle.
  @param[in]  routine   Procedure or Function to alter.
  @param[in]  chistics  New values of stored routine attributes to write.

  @retval SP_OK      ON SUCCESS
  @retval non-SP_OK  ON FAILURE
*/

enum_sp_return_code alter_routine(THD *thd, const Routine *routine,
                                  st_sp_chistics *chistics);
} //namespace dd
#endif //DD_ROUTINE_INCLUDED
