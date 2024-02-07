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

#ifndef _TERMINOLOGY_USE_PREVIOUS_H_
#define _TERMINOLOGY_USE_PREVIOUS_H_

#include "storage/perfschema/pfs_instr_class.h"  // PFS_class_type

namespace terminology_use_previous {

/**
  Encapsulates a <name, version> pair, holding an instrumentation
  name, and the version before which it was in use by the server.
*/
struct compatible_name_t {
  /**
    The old name, for an instrumentation name that was changed in some
    server release.
  */
  const char *old_name;
  /**
    The version where the name was changed.
  */
  enum_compatibility_version version;
};

/**
  For a given PFS_class_type, and a name within that class, return the
  version-dependent alias for that name.

  This is used when registering performance_schema names, to check if
  there are any alternative names. If there are, those are stored in
  the PFS_instr_class object. Later, when the name is required
  (e.g. during the execution of SELECT * FROM
  performance_schema.threads statement), it decides which name to use
  based on the value of @@session.terminology_use_previous
  and the fields that were stored in PFS_instr_class.

  This framework is extensible, so in future versions we can rename
  more names, and user will be able to choose exactly which version's
  names will be used.  However, note that the framework currently does
  not support successive changes of *the same* identifier.  This
  limitation allows us to return just a singleton compatible_name_t
  from this function.  If, in the future, we need to make successive
  changes to the same identifier, this function needs to be changed so
  that it returns something like a std::map<ulong, char*>, for a given
  instrumented object mapping versions to alternative names.

  @param class_type The PFS_class_type of 'str', indicating whether it
  is a mutex/rwlock/condition variable/memory allocation/thread
  name/thread stage/thread command/etc.

  @param str The object name to check.

  @param use_prefix If true, 'str' is expected to begin with the
  prefix for 'class_type', and the return value will include the
  prefix.  If false, 'str' is not expected to begin with the prefix
  and the return value will not include the prefix.

  @retval A compatible_name_t object. If there is an alternative name,
  'old_name' points to a static buffer containing that name, and
  'version' represents the enum_compatibility_version where that name
  was introduced.  If there is no alternative name, 'old_name' is
  nullptr and version is 0.
*/
compatible_name_t lookup(PFS_class_type class_type, std::string str,
                         bool use_prefix = true);

/**
  Checks the session variable
  @@session.terminology_use_previous, to determine whether
  an instrumented object that was renamed in the given version should
  use the old name.

  @param version The version where the instrumentation name was
  renamed.

  @retval true The old instrumentation name should be used.

  @retval false The new instrumentation name should be used.
*/
bool is_older_required(enum_compatibility_version version);

}  // namespace terminology_use_previous

#endif  // ifndef _TERMINOLOGY_USE_PREVIOUS_H_
