/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD__DD_VERSION_INCLUDED
#define DD__DD_VERSION_INCLUDED

/**
  @file sql/dd/dd_version.h
  Data dictionary version.
*/

/**
  The version of the current data dictionary table definitions.

  This version number is stored on disk in the data dictionary. Every time
  the data dictionary schema structure changes, this version number must
  change. The table that stores the data dictionary version number is never
  allowed to change.

  The data dictionary version number is the MySQL server version number
  of the first MySQL server version that published a given database schema.
  The format is Mmmdd with M=Major, m=minor, d=dot, so that MySQL 8.0.4 is
  encoded as 80004. This is the same version numbering scheme as the
  information schema and performance schema are using.

  When a data dictionary version is made public, the next change to a
  dictionary table will be associated with the next available MySQL server
  version number. So if DD version 80004 is made available in MySQL 8.0.4,
  and 8.0.5 is an MRU with no changes to the DD tables, then the DD version
  will stay 80004 also in MySQL 8.0.5. If MySQL 9.0.4 is the first GA of
  9.0, and if there are no changes to the DD tables compared to 8.0.4, then
  the DD version number will stay 80004 also in MySQL 9.0.4. Then, if there
  are changes to the DD tables after MySQL 9.0.4, then the new DD version will
  be 90005. In day to day builds internally, changes to the DD tables may be
  done incrementally, so there may be different builds having the same DD
  version number, yet with different DD table definitions.

  Historical version number published in the data dictionary:

  1:

  Introduced in MySQL 8.0.0 by WL#6378.
  Never published in a GA version, abandoned.

  80004 (current):

  There are so far no changes in the dictionary table definitions compared
  to the DD version 1 that was used in MySQL 8.0.3 (RC1).

  If a new DD version is published in a MRU, that version may or may not
  be possible to downgrade to previous MRUs within the same GA. If
  downgrade is supported, the constant DD_VERSION_MINOR_DOWNGRADE_THRESHOLD
  should be set to the lowest DD_VERSION that we may downgrade to. If minor
  downgrade is not supported at all, DD_VERSION_MINOR_DOWNGRADE_THRESHOLD
  should be set to DD_VERSION.
*/
namespace dd {

  static const uint DD_VERSION= 80004;

  static const uint DD_VERSION_MINOR_DOWNGRADE_THRESHOLD= 80004;

}

#endif /* DD__DD_VERSION_INCLUDED */
