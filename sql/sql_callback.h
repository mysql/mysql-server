/*
  Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef SQL_CALLBACK_INCLUDED
#define SQL_CALLBACK_INCLUDED

/**
   Macro used for an internal callback.

   The macro will check that the object exists and that the function
   is defined. If that is the case, it will call the function with the
   given parameters.

   If the object or the function is not defined, the callback will be
   considered successful (nothing needed to be done) and will
   therefore return no error.
 */

#define MYSQL_CALLBACK(OBJ, FUNC, PARAMS)         \
  do {                                            \
    if ((OBJ) && ((OBJ)->FUNC))                   \
      (OBJ)->FUNC PARAMS;                         \
  } while (0)

#define MYSQL_CALLBACK_ELSE(OBJ, FUNC, PARAMS, ELSE)    \
  (((OBJ) && ((OBJ)->FUNC)) ? (OBJ)->FUNC PARAMS : (ELSE))


#endif /* SQL_CALLBACK_INCLUDED */
