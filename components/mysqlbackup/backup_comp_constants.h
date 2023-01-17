/* Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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

// Backup constants
constexpr const char *mysqlbackup{"mysqlbackup"};
constexpr const char *backupid{"backupid"};
constexpr const char *reqd_priv_str{"SUPER or BACKUP_ADMIN"};
constexpr const char *backup_component_version{"mysqlbackup.component_version"};

// Page-track constants
constexpr const char *page_track{"page_track"};
constexpr const char *backupdir{"backupdir"};
constexpr const char *udf_set_page_tracking{"mysqlbackup_page_track_set"};
constexpr const char *udf_get_start_lsn{"mysqlbackup_page_track_get_start_lsn"};
constexpr const char *udf_get_changed_pages{
    "mysqlbackup_page_track_get_changed_pages"};
constexpr const char *udf_get_changed_page_count{
    "mysqlbackup_page_track_get_changed_page_count"};
constexpr const char *udf_page_track_purge_up_to{
    "mysqlbackup_page_track_purge_up_to"};
// Changed pages file path
constexpr const char *backup_scratch_dir{"#meb"};
// Changed pages file extension
constexpr const char *change_file_extension{".idx"};
// 4 bytes for space id + 4 bytes for the page number
constexpr size_t page_number_size{8};

}  // namespace Backup_comp_constants

#endif  // BACKUP_COMP_CONSTANTS_H
