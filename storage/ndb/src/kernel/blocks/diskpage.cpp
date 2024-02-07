/*
   Copyright (c) 2005, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "diskpage.hpp"
#include <time.h>
#include <version.h>
#include <NdbOut.hpp>
#include <signaldata/SignalData.hpp>

#define JAM_FILE_ID 364

void File_formats::Zero_page_header::init(File_type ft, Uint32 node_id,
                                          Uint32 version, Uint32 now) {
  memcpy(m_magic, "NDBDISK", 8);
  m_byte_order = 0x12345678;
  m_page_size = File_formats::NDB_PAGE_SIZE;
  m_ndb_version = version;
  m_node_id = node_id;
  m_file_type = ft;
  m_time = now;
}

int File_formats::Zero_page_header::validate(File_type ft, Uint32 node_id,
                                             Uint32 version, Uint32 now) {
  return 0;  // TODO Check header
}

NdbOut &operator<<(NdbOut &out, const File_formats::Zero_page_header &obj) {
  char buf[256];
  out << "page size:   " << obj.m_page_size << endl;
  out << "ndb version: " << obj.m_ndb_version << ", "
      << ndbGetVersionString(obj.m_ndb_version, 0, 0, buf, sizeof(buf)) << endl;
  out << "ndb node id: " << obj.m_node_id << endl;
  out << "file type:   " << obj.m_file_type << endl;

  /**
   *  m_time is 32bit, time_t may be bigger (64bit).
   */
  const time_t tm = obj.m_time;
  out << "time:        " << obj.m_time << ", " << ctime(&tm) << endl;
  return out;
}

NdbOut &operator<<(NdbOut &out, const File_formats::Datafile::Zero_page &obj) {
  out << obj.m_page_header << endl;
  out << "m_file_no: " << obj.m_file_no << endl;
  out << "m_tablespace_id: " << obj.m_tablespace_id << endl;
  out << "m_tablespace_version: " << obj.m_tablespace_version << endl;
  out << "m_data_pages: " << obj.m_data_pages << endl;
  out << "m_extent_pages: " << obj.m_extent_pages << endl;
  out << "m_extent_size: " << obj.m_extent_size << endl;
  out << "m_extent_count: " << obj.m_extent_count << endl;
  out << "m_extent_headers_per_page: " << obj.m_extent_headers_per_page << endl;
  out << "m_extent_header_words: " << obj.m_extent_header_words << endl;
  out << "m_extent_header_bits_per_page: " << obj.m_extent_header_bits_per_page
      << endl;

  return out;
}

NdbOut &operator<<(NdbOut &out,
                   const File_formats::Datafile::Zero_page_v2 &obj) {
  out << obj.m_page_header << endl;
  out << "m_file_no: " << obj.m_file_no << endl;
  out << "m_tablespace_id: " << obj.m_tablespace_id << endl;
  out << "m_tablespace_version: " << obj.m_tablespace_version << endl;
  out << "m_data_pages: " << obj.m_data_pages << endl;
  out << "m_extent_pages: " << obj.m_extent_pages << endl;
  out << "m_extent_size: " << obj.m_extent_size << endl;
  out << "m_extent_count: " << obj.m_extent_count << endl;
  out << "m_extent_headers_per_page: " << obj.m_extent_headers_per_page << endl;
  out << "m_extent_header_words: " << obj.m_extent_header_words << endl;
  out << "m_extent_header_bits_per_page: " << obj.m_extent_header_bits_per_page
      << endl;
  out << "m_checksum: " << obj.m_checksum << endl;

  return out;
}

NdbOut &operator<<(NdbOut &out, const File_formats::Undofile::Zero_page &obj) {
  out << obj.m_page_header << endl;
  out << "m_file_id: " << obj.m_file_id << endl;
  out << "m_logfile_group_id: " << obj.m_logfile_group_id << endl;
  out << "m_logfile_group_version: " << obj.m_logfile_group_version << endl;
  out << "m_undo_pages: " << obj.m_undo_pages << endl;

  return out;
}

NdbOut &operator<<(NdbOut &out,
                   const File_formats::Undofile::Zero_page_v2 &obj) {
  out << obj.m_page_header << endl;
  out << "m_file_id: " << obj.m_file_id << endl;
  out << "m_logfile_group_id: " << obj.m_logfile_group_id << endl;
  out << "m_logfile_group_version: " << obj.m_logfile_group_version << endl;
  out << "m_undo_pages: " << obj.m_undo_pages << endl;
  out << "m_checksum: " << obj.m_checksum << endl;

  return out;
}
