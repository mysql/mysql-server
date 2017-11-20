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

#ifndef DD__DD_INCLUDED
#define DD__DD_INCLUDED

#ifndef LOG_SUBSYSTEM_TAG
#define LOG_SUBSYSTEM_TAG "data_dictionary"
#endif

#include "sql/dd/types/object_type.h" // IWYU pragma: keep

namespace dd {

///////////////////////////////////////////////////////////////////////////

// enum type to pass to init() function.
enum class enum_dd_init_type
{
  DD_INITIALIZE= 1,
  DD_INITIALIZE_SYSTEM_VIEWS,
  DD_RESTART_OR_UPGRADE,
  DD_POPULATE_UPGRADE,
  DD_DELETE,
  DD_UPDATE_I_S_METADATA
};


/**
  Initialize data dictionary upon server startup, server startup on old
  data directory or install data dictionary for the first time.

  @param dd_init - Option for initialization, population or deletion
                   of data dictionary.

  @return false - On success
  @return true - On error
*/
bool init(enum_dd_init_type dd_init);


/**
  Shuts down the data dictionary instance by deleting
  the instance of dd::Dictionary_impl* upon server shutdown.

  @return false - On success
  @return true - If invoked when data dictionary instance
                 is not yet initialized.
*/
bool shutdown();


/**
  Get the data dictionary instance.

  @returns 'Dictionary*' pointer to data dictionary instance.
           Else returns NULL if data dictionary is not
           initialized.
*/
class Dictionary *get_dictionary();


/**
  Create a instance of data dictionary object of type X.
  E.g., X could be 'dd::Table', 'dd::View' and etc.

  @returns Pointer to the newly allocated dictionary object.
*/
template <typename X>
inline X *create_object()
{ return dynamic_cast<X *> (X::TYPE().create_object()); }

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__DD_INCLUDED
