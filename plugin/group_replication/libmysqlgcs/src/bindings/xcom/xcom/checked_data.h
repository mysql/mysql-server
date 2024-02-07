/* Copyright (c) 2018, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef CHECKED_DATA_H
#define CHECKED_DATA_H

#include "xdr_gen/xcom_vp.h" /* checked_data */

/**
 Creates a copy of the given checked_data.

 @param[in,out] to Where to copy to
 @param[in] from Where to copy from
 @retval true If the copy was successful
 @retval false If there was an error allocating memory for the copy
 */
bool_t copy_checked_data(checked_data *const to,
                         checked_data const *const from);

#endif /* CHECKED_DATA_H */
