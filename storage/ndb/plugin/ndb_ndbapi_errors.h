/*
   Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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

#ifndef NDB_NDBAPI_ERRORS_H
#define NDB_NDBAPI_ERRORS_H

// Named constants for NdbApi and NDB error codes that are handled in more than
// one place throughout the ndbcluster plugin are defined below.
//
// NOTE! error codes which are handled in specific places, can be declared local
// to that scope.
// For example:
//   int function() {
//      constexpr int NDB_ERR_SPECIAL_ERROR = 37;
//      if (ndb_err.code == NDB_ERR_SPECIAL_ERROR)
//        <handle the special case>
//   }
//
//  class Ndb_super_duper {
//     constexpr int ERROR_THAT_NEED_TO_BE_HANDLED_BY_CLASS = 37;
//     <snip>
//  }
//
// So please don't spread "knowledge" around more than necessary!
//
constexpr int NDB_ERR_CLUSTER_FAILURE = 4009;
constexpr int NDB_INVALID_SCHEMA_OBJECT = 241;

#endif
