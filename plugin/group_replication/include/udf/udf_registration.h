/* Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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

#ifndef PLUGIN_GR_INCLUDE_UDF_REGISTRATION_H
#define PLUGIN_GR_INCLUDE_UDF_REGISTRATION_H

/**
 * Registers the plugin's UDFs.
 * If there is an error registering any UDF, all installed UDFs are
 * unregistered.
 *
 * @retval true if there was an error
 * @retval false if all UDFs were registered
 */
bool register_udfs();

/**
 * Unregisters the plugin's UDFs.
 *
 * @retval true if there was an error
 * @retval false if all UDFs were unregistered
 */
bool unregister_udfs();

#endif /* PLUGIN_GR_INCLUDE_UDF_REGISTRATION_H */
