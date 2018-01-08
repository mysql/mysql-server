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

/**
  @page CPP_CODING_GUIDELINES_FOR_NDB_SE C++ Coding Guidelines for the NDB Storage Engine

  The mysqld handler part of NDB (ha_ndbcluster.cc,
  ha_ndbcluster_binlog.cc, etc.) uses the same coding style as
  the rest of the mysqld code.

  The non-mysqld part of NDB code has a long history, and uses
  many coding styles. When modifying and extending
  existing source files or modules, the coding style already
  used in that code should be followed in terms of
  indentation, naming conventions, etc. For completely new
  code, the mysqld conventions (with exceptions below) should
  probably be followed.

  Do not make changes to NDB code purely for the sake of
  changing from one formatting style to another. It just causes
  merge annoyances and makes patches harder to read, and we do
  not expect the style to ever become 100% consistent across
  all of the source code. However, it is okay to fix inconsistent
  style in lines that are changed for other reasons.

  One convention that should be followed for all new or
  modified code, in both mysqld and non-mysqld parts of the
  code, is that class member variables should be named with
  lowercase words separated by underscores '_', and prefixed
  with 'm_'. Like this:
  ~~~~~~~~~~~~~~~~
  const char *m_my_class_member;
  ~~~~~~~~~~~~~~~~

  - @subpage BRACES
  - @subpage ASSIGNMENT
  - @subpage USE_OF_NDBREQUIRE
*/


/**
  @page BRACES Braces

  <p>if, while, etc. *must* always have braces, and each brace
  should be on a separate line.</p>

  Good example:
  ~~~~~~~~~~~~~~~~
  if (a == b)
      {
         dosomething();
      }
  ~~~~~~~~~~~~~~~~

  Bad example:
  ~~~~~~~~~~~~~~~~
  if (a == b) {
         dosomething();
      }
  ~~~~~~~~~~~~~~~~

  Inline methods inside class (struct) are okay to write as shown
  below (i.e., opening brace is on the same line as the function
  declaration).
  ~~~~~~~~~~~~~~~~
  struct A
      {
         A() {
         }
      }
  ~~~~~~~~~~~~~~~~
*/


/**
  @page ASSIGNMENT Assignment
  ~~~~~~~~~~~~~~~~
  a = 3; // ok
  a= 3; // not ok
  ~~~~~~~~~~~~~~~~
*/


/**
  @page USE_OF_NDBREQUIRE Use of ndbrequire

  In the NDB kernel code, the ndbrequire() facility has
  historically been widely used. However, most of this is now
  considered misuse, and use of ndbrequire() should generally be
  avoided. Over time, we want to remove most or all
  ndbrequire() instances.

  There are three different classes of ndbrequire() usage, with
  corresponding replacement as follows:

  - Verification of code logic: Hitting this is a real bug,
    and the error message should be written accordingly. For this use, one
    option is ndbassert() (only enabled in debug builds), or
    we might need to add ndbchecklogic() or similar.

  - Hitting a configurable limit, which cannot be handled
    gracefully: For this, use ndbrequireErr(). The
    error message should suggest a configuration change to correct the
    problem, or refer to a section in the manual for
    more information.

  - Hitting hardcoded limits: We should really try to avoid
    this, but if it is unavoidable, or if it is a limit we
    think we will never hit, use ndbrequireErr() and add an
    appropriate error message.
*/
