/* Copyright (C) 2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include <signaldata/SignalData.hpp>
#include "diskpage.hpp"
#include <NdbOut.hpp>
#include <version.h>
#include <time.h>

void
File_formats::Zero_page_header::init(File_type ft, 
				     Uint32 node_id, 
				     Uint32 version, 
				     Uint32 now)
{
  memcpy(m_magic, "NDBDISK", 8);
  m_byte_order = 0x12345678;
  m_page_size = File_formats::NDB_PAGE_SIZE;
  m_ndb_version = version;
  m_node_id = node_id;
  m_file_type = ft;
  m_time = now;
}

int
File_formats::Zero_page_header::validate(File_type ft, 
					 Uint32 node_id, 
					 Uint32 version, 
					 Uint32 now)
{
  return 0; // TODO Check header
}

NdbOut&
operator<<(NdbOut& out, const File_formats::Zero_page_header& obj)
{
  char buf[256];
  out << "page size:   " << obj.m_page_size << endl;
  out << "ndb version: " << obj.m_ndb_version << ", " <<
    ndbGetVersionString(obj.m_ndb_version, 0, 0, buf, sizeof(buf)) << endl;
  out << "ndb node id: " << obj.m_node_id << endl;
  out << "file type:   " << obj.m_file_type << endl;
  out << "time:        " << obj.m_time << ", " 
      << ctime((time_t*)&obj.m_time)<< endl;
  return out;
}

NdbOut&
operator<<(NdbOut& out, const File_formats::Datafile::Zero_page& obj)
{
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
  out << "m_extent_header_bits_per_page: " << obj.m_extent_header_bits_per_page << endl;

  return out;
}

NdbOut&
operator<<(NdbOut& out, const File_formats::Undofile::Zero_page& obj)
{
  out << obj.m_page_header << endl;
  out << "m_file_id: " << obj.m_file_id << endl;
  out << "m_logfile_group_id: " << obj.m_logfile_group_id << endl;
  out << "m_logfile_group_version: " << obj.m_logfile_group_version << endl;
  out << "m_undo_pages: " << obj.m_undo_pages << endl;
  
  return out;
}

