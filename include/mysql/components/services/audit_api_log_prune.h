/* Copyright (c) 2018, 2026, Oracle and/or its affiliates.

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

#ifndef AUDIT_API_LOG_PRUNE_H
#define AUDIT_API_LOG_PRUNE_H

#include <mysql/components/service.h>

/*
  Version 1.
  Introduced in MySQL 9.4.0
  Status: Active.
*/
BEGIN_SERVICE_DEFINITION(mysql_audit_api_log_prune)

/**
  Method that removes rotated-out audit log file
  from internal audit log list and optionally deletes
  the same file from disk.

  @param file_path            Local audit log file to be pruned.
  @param delete_file          Delete the file.

  @retval false: success
  @retval true: failure
*/
bool (*prune)(const char *file_path, bool delete_file);

END_SERVICE_DEFINITION(mysql_audit_api_log_prune)

#endif /* AUDIT_API_LOG_PRUNE_H */
