/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.

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

#ifndef NDBT_RETURNCODES_H
#define NDBT_RETURNCODES_H

#ifdef	__cplusplus
extern "C" {
#endif

#define NDBT_OK 0
#define NDBT_FAILED 1
#define NDBT_WRONGARGS 2
#define NDBT_TEMPORARY 3
#define NDBT_SKIPPED 4

/**
 * NDBT_ProgramExit
 * This function will print the returncode together with a prefix on 
 * the screen and then exit the test program. 
 * Call this function when exiting the main function in your test programs
 * Returns the return code
 */
int NDBT_ProgramExit(int rcode);


#ifdef	__cplusplus
}
#endif

#endif
