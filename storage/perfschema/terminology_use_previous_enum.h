/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#ifndef _TERMINOLOGY_USE_PREVIOUS_ENUM_H_
#define _TERMINOLOGY_USE_PREVIOUS_ENUM_H_

/**
  In respect to the organization of modules, this really belongs in
  terminology_use_previous.h.  However, that would create a
  cyclic dependency between header files:

  - enum_compatibility_version is needed in pfs_instr_class.h

  - PFS_class_type is defined in pfs_instr_class.h and needed in
    instrumentation_class_compatibility.h

  So we keep enum_compatibility_version in its own header, included
  from both the other headers, to avoid the cyclicity.
*/

namespace terminology_use_previous {

/**
  Enumeration holding the possible values for
  @@terminology_use_previous.  Each element corresponds to a
  server release where some instrumentation name was changed.
*/
enum enum_compatibility_version {
  /// Use new names; do not provide backward compatibility
  NONE,
  /// Use names that were in use up to 8.0.25, inclusive.
  BEFORE_8_0_26,
  /// Use names that were in use before 8.2.0.
  BEFORE_8_2_0
};

}  // namespace terminology_use_previous

#endif  // _TERMINOLOGY_USE_PREVIOUS_ENUM_H_
