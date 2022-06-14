/*
   Copyright (c) 2005, 2022, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef Restore_H
#define Restore_H

#include <SimulatedBlock.hpp>

#include <IntrusiveList.hpp>
#include <KeyTable.hpp>
#include <DLHashTable.hpp>
#include <DataBuffer.hpp>
#include <NodeBitmask.hpp>
#include <backup/BackupFormat.hpp>

#define JAM_FILE_ID 439

#define MAX_LCP_PARTS_SUPPORTED 4096

class Restore : public SimulatedBlock
{
  friend class RestoreProxy;

  Uint32 m_lqh_block;
  bool m_is_query_block;
public:
  Restore(Block_context& ctx,
          Uint32 instanceNumber = 0,
          Uint32 blockNo = RESTORE);
  ~Restore() override;
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
  void execFSREMOVEREF(Signal*);
  void execFSREMOVECONF(Signal*);
  void execFSWRITEREF(Signal*);
  void execFSWRITECONF(Signal*);

  void execLQHKEYREF(Signal*);
  void execLQHKEYCONF(Signal*);
  
  
  typedef ArrayPool<DataBufferSegment<15> > BufferPool;
  typedef DataBuffer<15,BufferPool> List;
  typedef LocalDataBuffer<15,BufferPool> LocalList;

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
      READING_RECORDS = 16,
      READ_CTL_FILES = 32,
      CREATE_CTL_FILE = 64,
      REMOVE_LCP_DATA_FILE = 128,
      REMOVE_LCP_CTL_FILE = 256,
      DROP_OLD_FILES = 512
    };

    enum PartState
    {
      PART_IGNORED = 0,
      PART_ALL_ROWS = 1,
      PART_ALL_CHANGES = 2
    };
    
    Uint32 m_table_id;
    Uint32 m_table_version;
    Uint32 m_fragment_id;
    
    Uint32 m_current_page_ptr_i;
    Uint32 m_current_page_pos; 
    Uint32 m_bytes_left; // Bytes read from FS
    Uint32 m_current_file_page;  // Where in file 
    Uint32 m_outstanding_reads;  // 
    Uint32 m_outstanding_operations;

    Uint64 m_rows_restored;
    Uint64 m_rows_restored_insert;
    Uint64 m_rows_restored_delete;
    Uint64 m_rows_restored_delete_page;
    Uint64 m_rows_restored_write;
    Uint64 m_rows_restored_delete_failed;
    Uint64 m_ignored_rows;
    Uint64 m_row_operations;

    Uint64 m_restore_start_time;
    Uint64 m_rows_in_lcp;
    Uint32 m_lcp_ctl_version;
    Uint32 m_restored_gcp_id;
    Uint32 m_restored_lcp_id;
    Uint32 m_restored_local_lcp_id;
    Uint32 m_max_gci_completed;
    Uint32 m_max_gci_written;
    Uint32 m_create_gci;
    Uint32 m_max_page_cnt;

    Uint32 m_rowid_page_no;
    Uint32 m_rowid_page_idx;
    Uint32 m_error_code;

    Uint32 m_file_id;
    Uint32 m_max_parts;
    Uint32 m_max_files;
    Uint32 m_num_files;
    Uint32 m_current_file_index;
    Uint32 m_dih_lcp_no;
    Uint32 m_used_ctl_file_no;
    Uint32 m_ctl_file_no;
    bool m_upgrade_case;
    bool m_double_lcps_found;
    bool m_found_not_restorable;
    Uint32 m_remove_ctl_file_no;
    Uint32 m_old_max_files;
    Uint32 m_num_remove_data_files;
    Uint32 m_remove_data_file_no;
 
    Uint32 m_current_page_index; // Where in page list are we
    List::Head m_pages;

    PartState m_part_state[MAX_LCP_PARTS_SUPPORTED];

    Uint32 nextHash;
    Uint32 prevHash;
    Uint32 nextList;
    Uint32 prevList;
    Uint32 nextPool;
  };
  typedef Ptr<File> FilePtr;
  typedef ArrayPool<File> File_pool;
  typedef DLList<File_pool> File_list;
  typedef KeyTable<File_pool> File_hash;

  /* Methods to handle UPGRADE from old LCP format to new LCP format. */
  void lcp_create_ctl_open(Signal*, FilePtr);
  void lcp_create_ctl_done_open(Signal*, FilePtr);
  void lcp_create_ctl_done_write(Signal*, FilePtr);
  void lcp_create_ctl_done_close(Signal*, FilePtr);

  /* Methods to remove no longer needed LCP control and data files */
  void lcp_drop_old_files(Signal*, FilePtr);
  void lcp_remove_old_file(Signal*, FilePtr, Uint32, bool);
  void lcp_remove_old_file_done(Signal*, FilePtr);

  void open_ctl_file(Signal*, FilePtr, Uint32);
  void open_ctl_file_done_conf(Signal*, FilePtr);
  void open_ctl_file_done_ref(Signal*, FilePtr);
  void read_ctl_file_done(Signal*, FilePtr, Uint32);
  void close_ctl_file_done(Signal*, FilePtr);

  Uint32 init_file(const struct RestoreLcpReq*, FilePtr);
  void release_file(FilePtr, bool statistics);
  Uint32 seize_file(FilePtr);
  void check_restore_ready(Signal*, FilePtr);

  void step_file_number_forward(FilePtr);
  void step_file_number_back(FilePtr, Uint32);
  void calculate_remove_old_data_files(FilePtr);
  void calculate_remove_new_data_files(FilePtr);
  void prepare_parts_for_execution(Signal*, FilePtr);
  void start_restore_lcp_upgrade(Signal*, FilePtr);
  void start_restore_lcp(Signal*, FilePtr);

  void open_data_file(Signal*, FilePtr);
  void read_data_file(Signal*, FilePtr);

  void restore_next(Signal*, FilePtr);
  void parse_file_header(Signal*, FilePtr, const Uint32*, Uint32 len);
  void parse_table_list(Signal*, FilePtr, const Uint32*, Uint32 len);
  void parse_table_description(Signal*, FilePtr, const Uint32*, Uint32 len);
  void parse_fragment_header(Signal*, FilePtr, const Uint32*, Uint32 len);
  const char* get_state_string(Uint32 state);
  const char* get_header_string(Uint32 state);
  void parse_record(Signal*, FilePtr,
                    const Uint32*,
                    Uint32 len,
                    BackupFormat::RecordType type);
  void handle_return_execute_operation(Signal*,
                                       FilePtr,
                                       const Uint32 *data,
                                       Uint32 len,
                                       Uint32 outstanding);
  void execute_operation(Signal*,
                         FilePtr,
                         Uint32 keyLen,
                         Uint32 attrLen,
                         Uint32 op_type,
                         Uint32 gci_id,
                         Uint32 header_type,
                         Local_key *lkey);
  void parse_fragment_footer(Signal*, FilePtr, const Uint32*, Uint32 len);
  void parse_gcp_entry(Signal*, FilePtr, const Uint32*, Uint32 len);
  void close_file(Signal*, FilePtr, bool remove_flag = false);

  Uint32 calculate_hash(Uint32 tableId, const Uint32 *src);

  void parse_error(Signal*, FilePtr, Uint32 line, Uint32 extra);
  int check_file_version(Signal*, Uint32 file_version);
  void restore_lcp_conf(Signal* signal, FilePtr);
  void restore_lcp_conf_after_execute(Signal* signal, FilePtr);
  void crash_during_restore(FilePtr, Uint32 line, Uint32 errCode);

public:
  void delete_by_rowid_fail(Uint32 op_ptr);
  void delete_by_rowid_succ(Uint32 op_ptr);

private:
  class Dblqh* c_lqh;
  class Dbtup* c_tup;
  class Backup* c_backup;
  File_list m_file_list;
  File_hash m_file_hash;
  File_pool m_file_pool;

  Uint64 m_rows_restored;
  Uint64 m_rows_restored_total;
  Uint64 m_millis_spent;
  Uint32 m_frags_restored;
  
  List::DataBufferPool m_databuffer_pool;
  Uint32 m_table_buf[MAX_WORDS_META_FILE];
  Uint32
    m_lcp_ctl_file_data[2][BackupFormat::LCP_CTL_FILE_BUFFER_SIZE_IN_WORDS];

  bool c_encrypted_filesystem;

 public:
  Uint32 getDBLQH()
  {
    return m_lqh_block;
  }
};

NdbOut& operator << (NdbOut&, const Restore::Column&);


#undef JAM_FILE_ID

#endif
