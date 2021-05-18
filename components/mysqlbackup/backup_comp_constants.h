/* Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.

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

#ifndef BACKUP_COMP_CONSTANTS_H
#define BACKUP_COMP_CONSTANTS_H
namespace Backup_comp_constants {
// backup consts
const std::string mysqlbackup("mysqlbackup");
const std::string backupid("backupid");
const std::string reqd_priv_str("SUPER or BACKUP_ADMIN");
const std::string backup_component_version("mysqlbackup.component_version");
// backup consts

// page-track constants
const std::string page_track("page_track");
const std::string backupdir("backupdir");
const std::string udf_set_page_tracking("mysqlbackup_page_track_set");
const std::string udf_get_start_lsn("mysqlbackup_page_track_get_start_lsn");
const std::string udf_get_changed_pages(
    "mysqlbackup_page_track_get_changed_pages");
const std::string udf_get_changed_page_count(
    "mysqlbackup_page_track_get_changed_page_count");
const std::string backup_scratch_dir("#meb");     // changed pages file path
const std::string change_file_extension(".idx");  // changed page file extn
// 4 bytes for space id + 4 bytes for the page number
const int page_number_size(8);
// page-track constants
}  // namespace Backup_comp_constants
#endif /* BACKUP_COMP_CONSTANTS_H */
