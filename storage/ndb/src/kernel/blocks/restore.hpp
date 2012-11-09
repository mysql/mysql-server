/*
   Copyright (C) 2005-2008 MySQL AB, 2010 Sun Microsystems, Inc.
    All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef Restore_H
#define Restore_H

#include <SimulatedBlock.hpp>

#include <SLList.hpp>
#include <DLList.hpp>
#include <KeyTable.hpp>
#include <DLHashTable.hpp>
#include <DataBuffer.hpp>
#include <NodeBitmask.hpp>
#include <backup/BackupFormat.hpp>

class Restore : public SimulatedBlock
{
  friend class RestoreProxy;

public:
  Restore(Block_context& ctx, Uint32 instanceNumber = 0);
  virtual ~Restore();
  BLOCK_DEFINES(Restore);
  
protected:
  
  void execSTTOR(Signal* signal);
  void sendSTTORRY(Signal*);
  void execREAD_CONFIG_REQ(Signal*);
  void execDUMP_STATE_ORD(Signal* signal);
  void execCONTINUEB(Signal* signal);
  void execRESTORE_LCP_REQ(Signal* signal);
  
  void execFSOPENREF(Signal*);
  void execFSOPENCONF(Signal*);
  void execFSREADREF(Signal*);
  void execFSREADCONF(Signal*);
  void execFSCLOSEREF(Signal*);
  void execFSCLOSECONF(Signal*);

  void execLQHKEYREF(Signal*);
  void execLQHKEYCONF(Signal*);
  
  typedef DataBuffer<15> List;

public:  
  struct Column
  {
    Uint16 m_id;
    Uint16 m_size;
    Uint16 m_unused;
    Uint16 m_flags; 

    enum Flags 
    { 
      COL_KEY  = 0x1,
      COL_VAR  = 0x2,
      COL_DISK = 0x4,
      COL_NULL = 0x8
    };
  };
private:

  struct File; // CC
  friend struct File;

  struct File 
  {
    File() {}
    Uint32 m_sender_ref;
    Uint32 m_sender_data;

    Uint32 m_fd;          // File pointer
    Uint32 m_file_type;   // File type
    Uint32 m_status;
    Uint32 m_lcp_version;

    enum StatusFlags 
    {
      FILE_EOF = 1,
      FILE_THREAD_RUNNING = 2,
      RESTORE_THREAD_RUNNING = 4,
      FIRST_READ = 8,
      READING_RECORDS = 16
    };
    
    Uint32 m_table_id;
    Uint32 m_table_version;
    Uint32 m_fragment_id;
    List::Head m_columns;
    
    Uint32 m_current_page_ptr_i;
    Uint32 m_current_page_pos; 
    Uint32 m_bytes_left; // Bytes read from FS
    Uint32 m_current_file_page;  // Where in file 
    Uint32 m_outstanding_reads;  // 
    Uint32 m_outstanding_operations;
    Uint64 m_rows_restored;
    
    Uint32 m_current_page_index; // Where in page list are we
    List::Head m_pages;
    
    Uint32 nextHash;
    Uint32 prevHash;
    Uint32 nextList;
    Uint32 prevList;
    Uint32 nextPool;
    Uint32 m_lcp_no;

    bool is_lcp() const { return m_file_type == BackupFormat::LCP_FILE;}
  };
  typedef Ptr<File> FilePtr;
  
  Uint32 init_file(const struct RestoreLcpReq*, FilePtr);
  void release_file(FilePtr);
  
  void open_file(Signal*, FilePtr);
  void read_file(Signal*, FilePtr);
  void restore_next(Signal*, FilePtr);
  void parse_file_header(Signal*, FilePtr, const Uint32*, Uint32 len);
  void parse_table_list(Signal*, FilePtr, const Uint32*, Uint32 len);
  void parse_table_description(Signal*, FilePtr, const Uint32*, Uint32 len);
  void parse_fragment_header(Signal*, FilePtr, const Uint32*, Uint32 len);
  void parse_record(Signal*, FilePtr, const Uint32*, Uint32 len);
  void parse_fragment_footer(Signal*, FilePtr, const Uint32*, Uint32 len);
  void parse_gcp_entry(Signal*, FilePtr, const Uint32*, Uint32 len);
  void close_file(Signal*, FilePtr);

  void reorder_key(const struct KeyDescriptor*, Uint32* data, Uint32 len);
  Uint32 calulate_hash(Uint32 tableId, const Uint32 *src);

  void parse_error(Signal*, FilePtr, Uint32 line, Uint32 extra);
  int check_file_version(Signal*, Uint32 file_version);
  void restore_lcp_conf(Signal* signal, FilePtr);
  void crash_during_restore(FilePtr, Uint32 line, Uint32 errCode);
public:
  
private:
  class Dblqh* c_lqh;
  class Dbtup* c_tup;
  DLList<File> m_file_list;
  KeyTable<File> m_file_hash;
  ArrayPool<File> m_file_pool;
  
  List::DataBufferPool m_databuffer_pool;
  Uint32 m_table_buf[MAX_WORDS_META_FILE];
};

NdbOut& operator << (NdbOut&, const Restore::Column&);

#endif
