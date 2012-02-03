#ifndef DELAYABLE_INSERT_OPERATION_H
#define DELAYABLE_INSERT_OPERATION_H

/*
   Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

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
#include "sql_class.h"

/**
   An insert operation that can be delayed.
*/
class Delayable_insert_operation : public COPY_INFO
{
public:

  /*
    This is an INSERT, and as function defaults have been set by the client
    thread already, they needn't be set again:
  */
  Delayable_insert_operation() :
    COPY_INFO(COPY_INFO::INSERT_OPERATION,
              NULL,      // inserted_columns
              false,     // manage_defaults
              DUP_ERROR, // duplicate_handling
              false)     // ignore_errors
  {}

  /**
     Does nothing. A Delayable_insert_operation expects to have its function
     defaults evaluated by the client thread, prior to handing over the row to
     the delayed insert thread.
  */
  virtual void set_function_defaults(TABLE *table) { }


  /**
     This is a backdoor interface for supporting legacy code for delayed
     inserts. The delayed insert handler abuses COPY_INFO by having a shared
     instance for all delayed insert operations, and keeping a copy of the
     state that is local to the current insert operation inside each delayed
     row. Then these two members must be altered before writing each record.
  */
  void set_dup_and_ignore(enum enum_duplicates d, bool i)
  {
    handle_duplicates= d;
    ignore= i;
  }
};

#endif // DELAYABLE_INSERT_OPERATION_H
