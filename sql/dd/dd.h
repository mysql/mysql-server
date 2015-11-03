/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

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

namespace dd {

///////////////////////////////////////////////////////////////////////////

/**
  Initialize data dictionary upon server startup or install data
  dictionary for the first time.

  @param install - If true, creates data dictionary tables. Else
                   it initializes data dictionary.

  @return false - On success
  @return true - On error
*/
bool init(bool install);


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


/**
  Create dd::Schema object representing 'mysql' schema where data
  dictionary tables reside. The object id for 'mysql' schema will
  be 1.

  @returns Pointer to the newly created Schema*.
*/
class Schema *create_dd_schema();

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__DD_INCLUDED
