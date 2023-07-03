/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.

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

#include "util/require.h"
#include <ndb_global.h>

#include <cstring>
#include <NdbTCP.h>
#include <NdbOut.hpp>
#include "BackupFormat.hpp"
#include <AttributeHeader.hpp>
#include <SimpleProperties.hpp>
#include <util/version.h>
#include <ndb_version.h>
#include "kernel/signaldata/FsOpenReq.hpp"
#include "portlib/ndb_file.h"
#include "util/ndbxfrm_iterator.h"
#include "util/ndbxfrm_file.h"
#include "util/ndb_openssl_evp.h"
#include "util/ndb_opts.h"

#define JAM_FILE_ID 476

using byte = unsigned char;

static ndb_key_state opt_backup_key_state("backup", nullptr);
static ndb_key_option opt_backup_key(opt_backup_key_state);
static ndb_key_from_stdin_option opt_backup_key_from_stdin(
    opt_backup_key_state);

static ndb_password_state opt_backup_password_state("backup", nullptr);
static ndb_password_option opt_backup_password(opt_backup_password_state);
static ndb_password_from_stdin_option opt_backup_password_from_stdin(
                                          opt_backup_password_state);

/*
 * At most one of opt_backup_key_state and opt_backup_password_state should be
 * set.  pwd_key and pwd_key_len will be set to point to whichever key or
 * password is set.
 */
static const byte* pwd_key = nullptr;
static size_t pwd_key_len;

static void print_utility_help();

static bool opt_print_restored_rows = false;
static int opt_print_restored_rows_ctl_dir = -1;
static int opt_print_restored_rows_fid = -1;
static bool opt_num_data_words = false;
static bool opt_show_ignored_rows = false;
static char* opt_file_input = nullptr;
static bool opt_print_rows_per_page = false;
static int opt_print_restored_rows_table = -1;
static bool opt_print_rows_flag = true;
static int opt_verbose_level = 0;

static struct my_option my_long_options[] =
{
  NdbStdOpt::usage,
  NdbStdOpt::help,
  NdbStdOpt::version,

  // Specific options
  { "backup-key", 'K', "backup key for lcp files",
    nullptr, nullptr, nullptr,
    GET_PASSWORD, OPT_ARG, 0, 0, 0, nullptr, 0, &opt_backup_key},
  { "backup-key-from-stdin", NDB_OPT_NOSHORT, "backup key for lcp files",
    &opt_backup_key_from_stdin.opt_value, nullptr, nullptr,
    GET_BOOL, NO_ARG, 0, 0, 0, nullptr, 0, &opt_backup_key_from_stdin},
  { "backup-password", 'P', "backup password for lcp files",
    nullptr, nullptr, nullptr,
    GET_PASSWORD, OPT_ARG, 0, 0, 0, nullptr, 0, &opt_backup_password},
  { "backup-password-from-stdin", NDB_OPT_NOSHORT, "backup password",
    &opt_backup_password_from_stdin.opt_value, nullptr, nullptr,
    GET_BOOL, NO_ARG, 0, 0, 0, nullptr, 0, &opt_backup_password_from_stdin},
  { "print-restored-rows", NDB_OPT_NOSHORT,
    "print restored rows, uses control file ctr/TtabFfrag.ctl as given by "
    "-c, -f, and, -t.",
    &opt_print_restored_rows, nullptr, nullptr,
    GET_BOOL, NO_ARG, 0, 0, 0, nullptr, 0, nullptr },
  { "control-directory-number", 'c', "control directory number",
    &opt_print_restored_rows_ctl_dir, nullptr, nullptr,
    GET_INT, REQUIRED_ARG, opt_print_restored_rows_ctl_dir,
    opt_print_restored_rows_ctl_dir, 1, nullptr, 0, nullptr },
  { "fragment-id", 'f', "fragment id",
    &opt_print_restored_rows_fid, nullptr, nullptr,
    GET_INT, REQUIRED_ARG, opt_print_restored_rows_fid,
    opt_print_restored_rows_fid, 0, nullptr, 0, nullptr },
  { "print-header-words", 'h', "print header words",
    &opt_num_data_words, nullptr, nullptr,
    GET_BOOL, NO_ARG, 0, 0, 0, nullptr, 0, nullptr },
  { "show-ignored-rows", 'i', "show ignored rows",
    &opt_show_ignored_rows, nullptr, nullptr,
    GET_BOOL, NO_ARG, 0, 0, 0, nullptr, 0, nullptr },
  { "rowid-file", 'n',
    "file with row id to check, each row id is a line with: page number, "
    "space, index in page.",
    &opt_file_input, nullptr, nullptr,
    GET_STR, REQUIRED_ARG, 0, 0, 0, nullptr, 0, nullptr },
  { "print-rows-per-page", 'p', "print rows per page",
    &opt_print_rows_per_page, nullptr, nullptr,
    GET_BOOL, NO_ARG, 0, 0, 0, nullptr, 0, nullptr },
  { "table-id", 't', "table id",
    &opt_print_restored_rows_table, nullptr, nullptr,
    GET_INT, REQUIRED_ARG, opt_print_restored_rows_table,
    opt_print_restored_rows_table, 0, nullptr, 0, nullptr },
  { "print-rows", 'U', "do print rows",
    &opt_print_rows_flag, nullptr, nullptr,
    GET_BOOL, NO_ARG, 1, 0, 0, nullptr, 0, nullptr },
  { "no-print-rows", 'u', "do not print rows",
    nullptr, nullptr, nullptr,
    GET_NO_ARG, NO_ARG, 0, 0, 0, nullptr, 0, nullptr },
  { "verbose", 'v', "verbosity",
    &opt_verbose_level, nullptr, nullptr,
    GET_INT, REQUIRED_ARG, 0, 0, 0, nullptr, 0, nullptr },
  NdbStdOpt::end_of_options
};

static const char* load_defaults_groups[] = { "ndb_print_backup_file",
                                              nullptr };

static const Uint32 MaxReadWords = 32768;

bool readHeader(ndbxfrm_file*, BackupFormat::FileHeader *);
bool readFragHeader(ndbxfrm_file*,
                    BackupFormat::DataFile::FragmentHeader *);
bool readFragFooter(ndbxfrm_file*,
                    BackupFormat::DataFile::FragmentFooter *);
Int32 readRecord(ndbxfrm_file*, Uint32 **, Uint32*, bool print);

NdbOut & operator<<(NdbOut&, const BackupFormat::FileHeader &); 
NdbOut & operator<<(NdbOut&, const BackupFormat::DataFile::FragmentHeader &); 
NdbOut & operator<<(NdbOut&, const BackupFormat::DataFile::FragmentFooter &); 

bool readLCPCtlFile(ndbxfrm_file* f, BackupFormat::LCPCtlFile *ret);
bool readTableList(ndbxfrm_file*, BackupFormat::CtlFile::TableList **);
bool readTableDesc(ndbxfrm_file*,
                   BackupFormat::CtlFile::TableDescription **);
bool readGCPEntry(ndbxfrm_file*, BackupFormat::CtlFile::GCPEntry **);

NdbOut & operator<<(NdbOut&, const BackupFormat::LCPCtlFile &); 
NdbOut & operator<<(NdbOut&, const BackupFormat::CtlFile::TableList &); 
NdbOut & operator<<(NdbOut&, const BackupFormat::CtlFile::TableDescription &);
NdbOut & operator<<(NdbOut&, const BackupFormat::CtlFile::GCPEntry &); 

Int32 readLogEntry(ndbxfrm_file*,
                   Uint32**,
                   Uint32 file_type,
                   Uint32 version);

struct RowEntry
{
  Uint32 page_id;
  Uint32 page_idx;
  RowEntry *prev_ptr;
  RowEntry *next_ptr;
};

static Uint32 recNo;
static Uint32 recInsert;
static Uint32 recWrite;
static Uint32 recDeleteByRowId;
static Uint32 recDeleteByPageId;
static Uint32 logEntryNo;
static int parts_array[BackupFormat::NDB_MAX_LCP_PARTS];
static Uint32 max_pages = 0;
static int already_inserted_count = 0;
static int ignored_rows = 0;
static Uint32 all_rows_count = 0;
static RowEntry **row_entries = NULL;
static RowEntry **row_all_entries = NULL;

#define IGNORE_PART 1
#define ALL_PART 2
#define CHANGE_PART 3

static bool get_one_option(int optid, const struct my_option *opt, char *arg)
{
  switch (optid) {
    case 'u':
      opt_print_rows_flag = false;
      break;
    case '?':
      print_utility_help();
      ndb_std_get_one_option(optid, opt, arg);
      break;
    default:
      return ndb_std_get_one_option(optid, opt, arg);
  }
  return false;
}

[[noreturn]] inline void ndb_end_and_exit(int exitcode)
{
  ndb_end(0);
  ndb_openssl_evp::library_end();
  exit(exitcode);
}

Uint32
get_part_id(Uint32 page_id)
{
  static Uint32 reverse_3bits_array[8] = { 0, 4, 2, 6, 1, 5, 3, 7 };
  const Uint32 lowest_3bits_page_id = page_id & 7;
  const Uint32 low_3bits_page_id = (page_id >> 3) & 7;
  const Uint32 high_3bits_page_id = (page_id >> 6) & 7;
  const Uint32 highest_3bits_page_id = (page_id >> 9) & 3;
  Uint32 part_id =
    reverse_3bits_array[highest_3bits_page_id] +
    (reverse_3bits_array[high_3bits_page_id] << 3) +
    (reverse_3bits_array[low_3bits_page_id] << 6) +
    (reverse_3bits_array[lowest_3bits_page_id] << 9);
  part_id >>= 1;
  return part_id;
}

const char*
get_part_type_string(Uint32 part_type)
{
  if (part_type == IGNORE_PART)
    return "IGNORE_PART";
  if (part_type == ALL_PART)
    return "ALL_PART";
  if (part_type == CHANGE_PART)
    return "CHANGE_PART";
  assert(false);
  return "UNKNOWN";
}

const char*
get_header_string(Uint32 header_type)
{
  if (header_type == BackupFormat::INSERT_TYPE)
    return "INSERT";
  if (header_type == BackupFormat::WRITE_TYPE)
    return "WRITE";
  if (header_type == BackupFormat::DELETE_BY_ROWID_TYPE)
    return "DELETE_BY_ROWID";
  if (header_type == BackupFormat::DELETE_BY_PAGEID_TYPE)
    return "DELETE_BY_PAGEID";
  assert(false);
  return "UNKNOWN";
}

Uint32 move_file_back(Uint32 file, Uint32 num_back)
{
  if (file >= num_back)
    return (file - num_back);
  return (file + BackupFormat::NDB_MAX_LCP_FILES - num_back);
}

Uint32 move_file_forward(Uint32 file, Uint32 num_forward)
{
  if ((file + num_forward) >= BackupFormat::NDB_MAX_LCP_FILES)
    return (file + num_forward - BackupFormat::NDB_MAX_LCP_FILES);
  return (file + num_forward);
}

Uint32 move_part_forward(Uint32 file, Uint32 num_forward)
{
  if ((file + num_forward) >= BackupFormat::NDB_MAX_LCP_PARTS)
    return (file + num_forward - BackupFormat::NDB_MAX_LCP_PARTS);
  return (file + num_forward);
}

RowEntry* find_row(Uint32 page_id,
                   Uint32 page_idx,
                   bool is_all)
{
  RowEntry **loc_entries;
  if (is_all)
    loc_entries = row_all_entries;
  else
    loc_entries = row_entries;

  if (page_id >= max_pages)
    return NULL;
  if (loc_entries[page_id] == NULL)
    return NULL;
  RowEntry *current = loc_entries[page_id];
  do
  {
    if (current->page_id != page_id)
    {
      ndbout_c("Inconsistent hash table");
      ndb_end_and_exit(1);
    }
    if (current->page_idx == page_idx)
      return current;
    current = current->next_ptr;
  } while (current != NULL);
  return NULL;
}

void insert_row(Uint32 page_id,
                Uint32 page_idx,
                bool is_insert,
                bool is_all)
{
  if (page_id >= max_pages)
  {
    ndbout_c("Trying to insert row(%u,%u) beyond max_pages: %u",
             page_id,
             page_idx,
             max_pages);
    return;
  }
  RowEntry *found_row_entry = find_row(page_id, page_idx, is_all);
  if (found_row_entry != NULL)
  {
    if (is_insert && !is_all)
    {
      /* Entry already existed */
      ndbout_c("row(%u,%u) already existed", page_id, page_idx);
      already_inserted_count++;
    }
    return;
  }
  RowEntry *new_row_entry = (RowEntry*)malloc(sizeof(struct RowEntry));
  if (new_row_entry == NULL)
  {
    ndbout_c("Malloc failure in insert_row");
    ndb_end_and_exit(1);
  }
  new_row_entry->page_id = page_id;
  new_row_entry->page_idx = page_idx;
  new_row_entry->prev_ptr = NULL;

  RowEntry **loc_entries;
  if (is_all)
    loc_entries = row_all_entries;
  else
    loc_entries = row_entries;

  new_row_entry->next_ptr = loc_entries[page_id];
  if (loc_entries[page_id] != NULL)
  {
    loc_entries[page_id]->prev_ptr = new_row_entry;
  }
  loc_entries[page_id] = new_row_entry;
  if (is_all)
  {
    all_rows_count++;
  }
}

void delete_row(Uint32 page_id, Uint32 page_idx)
{
  if (page_id >= max_pages)
  {
    ndbout_c("Trying to delete row(%u,%u) beyond max_pages: %u",
             page_id,
             page_idx,
             max_pages);
    return;
  }
  RowEntry *found_row_entry = find_row(page_id, page_idx, false);
  if (found_row_entry == NULL)
  {
    ndbout_c("Trying to delete row(%u,%u) NOT FOUND",
             page_id,
             page_idx);
    return;
  }
  if (found_row_entry->prev_ptr == NULL)
  {
    /* First entry in linked list */
    row_entries[page_id] = found_row_entry->next_ptr;
  }
  else
  {
    found_row_entry->prev_ptr->next_ptr =
      found_row_entry->next_ptr;
  }
  if (found_row_entry->next_ptr != NULL)
  {
    found_row_entry->next_ptr->prev_ptr =
      found_row_entry->prev_ptr;
  }
  free(found_row_entry);
}

void delete_page(Uint32 page_id)
{
  if (page_id >= max_pages)
  {
    ndbout_c("Trying to delete page(%u) beyond max_pages: %u",
             page_id,
             max_pages);
    return;
  }
  if (row_entries[page_id] == NULL)
    return;
  RowEntry *current = row_entries[page_id];
  row_entries[page_id] = NULL;
  do
  {
    RowEntry *free_row = current;
    current = current->next_ptr;
    free(free_row);
  } while (current != NULL);
}

void check_data(const char *file_input)
{
  FILE *file = fopen(file_input, "r");
  if (file != NULL)
  {
    char line [100];
    while (fgets(line,sizeof(line),file)!= NULL) /* read a line from a file */
    {
      Uint32 page_id, page_idx;
      int ret = sscanf(line, "%d %d", &page_id, &page_idx);
      if (ret != 2)
      {
        ndbout_c("-n file expects a file with two numbers page_id space "
                 "page_idx");
        ndb_end_and_exit(1);
      }
      RowEntry *found_entry = find_row(page_id, page_idx, false);
      if (found_entry != NULL)
      {
        ndbout_c("Found deleted row in hash: row_id(%u,%u)",
                 found_entry->page_id,
                 found_entry->page_idx);
      }
    }
    fclose(file);
  } else {
    ndbout_c("check_data: Failed to open file '%s'", file_input);
  }
}

void print_rows()
{
  Uint32 row_count = 0;
  for (Uint32 page_id = 0; page_id < max_pages; page_id++)
  {
    Uint32 rows_page = 0;
    if (row_entries[page_id] != NULL)
    {
      RowEntry *current = row_entries[page_id];
      do
      {
        if (opt_print_rows_flag)
          ndbout_c("Found row(%u,%u)", page_id, current->page_idx);
        current = current->next_ptr;
        row_count++;
        rows_page++;
      } while (current != NULL);
      if (opt_print_rows_per_page && rows_page != 3)
      {
        ndbout_c("Rows on page: %u is %u", page_id, rows_page);
      }
    }
  }
  ndbout_c("Found a total of %u rows after restore", row_count);
  if (already_inserted_count != 0)
  {
    ndbout_c("Found a total of %u rows already existing",
             already_inserted_count);
  }
}

void print_ignored_rows()
{
  ndbout_c("Printing ignored rows");
  for (Uint32 page_id = 0; page_id < max_pages; page_id++)
  {
    if (row_all_entries[page_id] != NULL)
    {
      RowEntry *current = row_all_entries[page_id];
      do
      {
        if (!find_row(current->page_id,
                      current->page_idx,
                      false))
        {
          ndbout_c("Found ignored rowid(%u,%u)",
                   current->page_id,
                   current->page_idx);
        }
        current = current->next_ptr;
      } while (current != NULL);
    }
  }
}
void delete_all()
{
  for (Uint32 page_id = 0; page_id < max_pages; page_id++)
  {
    delete_page(page_id);
  }
}

void handle_print_restored_rows()
{
  ndbout_c("Print restored rows for T%uF%u",
           opt_print_restored_rows_table,
           opt_print_restored_rows_fid);

  char buf[255];
  ndb_file file;
  ndbxfrm_file fo;

  BaseString::snprintf(buf, sizeof(buf),
                       "%u/T%uF%u.ctl",
                       opt_print_restored_rows_ctl_dir,
                       opt_print_restored_rows_table,
                       opt_print_restored_rows_fid);
  int r = file.open(buf, FsOpenReq::OM_READONLY);
  if (r != -1)
  {
    r = fo.open(file, pwd_key, pwd_key_len);
  }
  if(r != 0)
  {
    // -2 is special failure return for ndbxfrm tool
    if (r == -2) fo.close(true);
    ndbout_c("Failed to open ctl file '%s', error: %d", buf, r);
    ndb_end_and_exit(1);
  }
  ndbxfrm_file* f = &fo;
  BackupFormat::FileHeader fileHeader;
  if (!readHeader(f, &fileHeader))
  {
    ndbout << "Invalid ctl file!" << endl;
    ndb_end_and_exit(1);
  }
  if (fileHeader.FileType != BackupFormat::LCP_CTL_FILE)
  {
    ndbout << "Invalid ctl file header!" << endl;
    ndb_end_and_exit(1);
  }
  union
  {
    BackupFormat::LCPCtlFile lcpCtlFilePtr;
    char extra_space[4 * BackupFormat::NDB_MAX_LCP_PARTS];
  };
  (void)extra_space;
  if (!readLCPCtlFile(f, &lcpCtlFilePtr))
  {
    ndbout << "Invalid LCP Control file!" << endl;
    ndb_end_and_exit(1);
  }
  fo.close(false);
  file.close();

  /**
   * Allocate the array of linked list first pointers.
   */
  max_pages = lcpCtlFilePtr.MaxPageCount;
  row_entries = (RowEntry**)malloc(sizeof(RowEntry*) * max_pages);
  if (row_entries == NULL)
  {
    ndbout << "Malloc failure" << endl;
    ndb_end_and_exit(1);
  }
  std::memset(row_entries, 0, sizeof(RowEntry*) * max_pages);

  row_all_entries = (RowEntry**)malloc(sizeof(RowEntry*) * max_pages);
  if (row_all_entries == NULL)
  {
    ndbout << "Malloc failure" << endl;
    ndb_end_and_exit(1);
  }
  std::memset(row_all_entries, 0, sizeof(RowEntry*) * max_pages);

  Uint32 last_file = lcpCtlFilePtr.LastDataFileNumber;
  Uint32 num_parts = lcpCtlFilePtr.NumPartPairs;
  Uint32 current_file = move_file_back(last_file, (num_parts - 1));
  Uint32 inx = 0;
  Uint32 first_change = lcpCtlFilePtr.partPairs[inx].startPart;
  for (Uint32 i = current_file; i <= last_file; i = move_file_forward(i,1))
  {
    Uint32 first_all = lcpCtlFilePtr.partPairs[inx].startPart;
    Uint32 first_ignore =
      move_part_forward(first_all, lcpCtlFilePtr.partPairs[inx].numParts);
    inx++;
    std::memset(&parts_array[0], 0, sizeof(parts_array));
    for (Uint32 j = first_change; j != first_all; j = move_part_forward(j,1))
    {
      parts_array[j] = CHANGE_PART;
      assert(j < BackupFormat::NDB_MAX_LCP_PARTS);
    }
    /**
     * Fill in all parts, including non-partial case where
     * PartPair covers all parts, such that first_all == first_ignore
     */
    assert(num_parts > 0);
    Uint32 all_part_count = 0;
    for (Uint32 j = first_all; ((all_part_count++) == 0) || (j != first_ignore);
         j = move_part_forward(j, 1)) {
      parts_array[j] = ALL_PART;
      assert(j < BackupFormat::NDB_MAX_LCP_PARTS);
    }
    for (Uint32 j = first_ignore;
         j != first_change;
         j = move_part_forward(j,1))
    {
      parts_array[j] = IGNORE_PART;
      assert(j < BackupFormat::NDB_MAX_LCP_PARTS);
    }
    for (Uint32 j = 0; j < BackupFormat::NDB_MAX_LCP_PARTS; j++)
    {
      assert(parts_array[0] != 0);
    }
    ndbout_c("Processing %u/T%uF%u.Data",
             i,
             opt_print_restored_rows_table,
             opt_print_restored_rows_fid);
    BaseString::snprintf(buf, sizeof(buf),
                         "%u/T%uF%u.Data",
                         i,
                         opt_print_restored_rows_table,
                         opt_print_restored_rows_fid);
    r = file.open(buf, FsOpenReq::OM_READONLY);
    if (r != -1)
    {
      r = fo.open(file, pwd_key, pwd_key_len);
    }
    if(r != 0)
    {
      // -2 is special failure return for ndbxfrm tool
      if (r == -2) fo.close(true);
      ndbout_c("Failed to open data file '%s', error: %d", buf, r);
      //ndb_end_and_exit(1);
      continue;
    }

    if (!readHeader(f, &fileHeader))
    {
      ndbout << "Invalid file!" << endl;
      ndb_end_and_exit(1);
    }	
    BackupFormat::CtlFile::TableList * tabList;
    if (!readTableList(f, &tabList))
    {
      ndbout << "Invalid file! No table list" << endl;
      break;
    }
    BackupFormat::DataFile::FragmentHeader fragHeader;
    if (!readFragHeader(f, &fragHeader))
    {
      ndbout << "Invalid file! No table list" << endl;
      break;
    }
    Uint32 len, * data, header_type;
    while((len = readRecord(f,
                            &data,
                            &header_type,
                            (opt_verbose_level > 0))) > 0)
    {
      Uint32 page_id = data[0];
      Uint32 page_idx = data[1];
      Uint32 part_id = get_part_id(page_id);
      const char *header_string = get_header_string(header_type);
      const char *part_string = get_part_type_string(parts_array[part_id]);
      if (parts_array[part_id] == IGNORE_PART)
      {
        if (header_type == BackupFormat::INSERT_TYPE ||
            header_type == BackupFormat::WRITE_TYPE)
        {
          insert_row(page_id, page_idx, true, true);
          ndbout_c("IGNORE: rowid(%u,%u)", page_id, page_idx);
          ignored_rows++;
        }
        continue;
      }
      else if (parts_array[part_id] == ALL_PART)
      {
        if (header_type != BackupFormat::INSERT_TYPE)
        {
          ndbout_c("NOT INSERT_TYPE when expected");
          ndb_end_and_exit(1);
        }
        if (opt_verbose_level > 0)
        ndbout_c("%s: page(%u,%u), len: %u, part_id: %u, part_type: %s",
                 header_string,
                 page_id,
                 page_idx,
                 len,
                 part_id,
                 part_string);
        insert_row(page_id, page_idx, true, true);
        insert_row(page_id, page_idx, true, false);
      }
      else if (parts_array[part_id] != CHANGE_PART)
      {
        ndbout_c("NOT CHANGE_PART when expected");
        ndb_end_and_exit(1);
      }
      else
      {
        if (header_type == BackupFormat::INSERT_TYPE)
        {
          ndbout_c("INSERT_TYPE in CHANGE_PART");
          ndb_end_and_exit(1);
        }
        if (header_type == BackupFormat::DELETE_BY_PAGEID_TYPE)
        {
          if (opt_verbose_level > 0)
          ndbout_c("%s: page(%u), len: %u, part_id: %u, part_type: %s",
                   header_string,
                   page_id,
                   len,
                   part_id,
                   part_string);
          delete_page(page_id);
        }
        else if (header_type == BackupFormat::WRITE_TYPE)
        {
          if (opt_verbose_level > 0)
          ndbout_c("%s: page(%u,%u), len: %u, part_id: %u, part_type: %s",
                   header_string,
                   page_id,
                   page_idx,
                   len,
                   part_id,
                   part_string);
          insert_row(page_id, page_idx, false, false);
          insert_row(page_id, page_idx, true, true);
        }
        else if (header_type == BackupFormat::DELETE_BY_ROWID_TYPE)
        {
          if (opt_verbose_level > 0)
          ndbout_c("%s: page(%u,%u), len: %u, part_id: %u, part_type: %s",
                   header_string,
                   page_id,
                   page_idx,
                   len,
                   part_id,
                   part_string);
          delete_row(page_id, page_idx);
        }
        else
        {
          ndbout_c("Wrong header_type: %u in CHANGE_PART", header_type);
          ndb_end_and_exit(1);
        }
      }
    }
    fo.close(false);
    file.close();
    ndbout_c("Number of all rows currently are: %u", all_rows_count);
  }
  print_rows();
  if (opt_show_ignored_rows)
    print_ignored_rows();
  if (opt_file_input)
  {
    check_data(opt_file_input);
  }
  delete_all();
  exit(0);
}

const char * ndb_basename(const char *path);

int
main(int argc, char * argv[])
{
  NDB_INIT(argv[0]);
  ndb_openssl_evp::library_init();
  Ndb_opts opts(argc, argv, my_long_options, load_defaults_groups);
  if (opts.handle_options(&get_one_option))
  {
    print_utility_help();
    opts.usage();
    ndb_end_and_exit(1);
  }

  // TODO check valid option combinations
  if (ndb_option::post_process_options())
  {
    BaseString err_msg = opt_backup_key_state.get_error_message();
    if (!err_msg.empty())
    {
      fprintf(stderr, "Error: backup key: %s\n", err_msg.c_str());
    }
    err_msg = opt_backup_password_state.get_error_message();
    if (!err_msg.empty())
    {
      fprintf(stderr, "Error: backup password: %s\n", err_msg.c_str());
    }
    print_utility_help();
    opts.usage();
    ndb_end_and_exit(1);
  }
  if (opt_backup_key_state.get_key() != nullptr &&
      opt_backup_password_state.get_password() != nullptr)
  {
    fprintf(stderr, "Error: Both backup key and backup password are set.\n");
    return 2;
  }

  if (opt_backup_key_state.get_key() != nullptr)
  {
    if (!ndb_openssl_evp::is_aeskw256_supported())
    {
      fprintf(stderr,
              "Error: Backup key options requires OpenSSL 1.0.2 or newer.\n");
      return 2;
    }
    pwd_key = opt_backup_key_state.get_key();
    pwd_key_len = opt_backup_key_state.get_key_length();
  }
  else
  {
    pwd_key = reinterpret_cast<const byte*>(
                 opt_backup_password_state.get_password());
    pwd_key_len = opt_backup_password_state.get_password_length();
  }

  if (opt_print_restored_rows) {
    if (opt_print_restored_rows_table == -1 ||
        opt_print_restored_rows_ctl_dir == -1 ||
        opt_print_restored_rows_fid == -1) {
      ndbout_c(
          "Missing table -t or fragment -f or control-directory -c argument");
      print_utility_help();
      opts.usage();
      ndb_end_and_exit(1);
    }
    if (argc > 0) {
      fprintf(stderr, "Ignoring %s for print-restored-rows\n", argv[0]);
    }
    handle_print_restored_rows();
  }

  if (argc != 1) {
    fprintf(stderr, "Error: Need one filename for a backup file to print.\n");
    ndb_end_and_exit(1);
  }

  const char *file_name = argv[0];
  ndb_file file;
  ndbxfrm_file fo;

  int r = file.open(file_name, FsOpenReq::OM_READONLY);
  r = fo.open(file, pwd_key, pwd_key_len);
  if(r != 0)
  {
    // -2 is special failure return for ndbxfrm tool
    if (r == -2) fo.close(true);
    ndbout_c("Failed to open file '%s', error: %d",
             argv[1], r);
    ndb_end_and_exit(1);
  }

  ndbxfrm_file* f = &fo;

  BackupFormat::FileHeader fileHeader;
  if(!readHeader(f, &fileHeader)){
    ndbout << "Invalid file!" << endl;
    ndb_end_and_exit(1);
  }	
  ndbout << fileHeader << endl;

  switch(fileHeader.FileType){
  case BackupFormat::DATA_FILE:
  {
    BackupFormat::DataFile::FragmentHeader fragHeader;
    while (readFragHeader(f, &fragHeader))
    {
      ndbout << fragHeader << endl;
      
      Uint32 len, * data, header_type;
      while((len = readRecord(f, &data, &header_type, true)) > 0)
      {
        if (opt_verbose_level > 0)
        {
	  ndbout << "-> " << hex;
	  for(Uint32 i = 0; i < len; i++)
          {
	    ndbout << data[i] << " ";
          }
	  ndbout << endl;
        }
      }

      BackupFormat::DataFile::FragmentFooter fragFooter;
      if(!readFragFooter(f, &fragFooter))
      {
	break;
      }
      ndbout << fragFooter << endl;
    }
  }
  break;
  case BackupFormat::CTL_FILE:{
    BackupFormat::CtlFile::TableList * tabList;
    if(!readTableList(f, &tabList)){
      ndbout << "Invalid file! No table list" << endl;
      break;
    }
    ndbout << (* tabList) << endl;

    const Uint32 noOfTables = tabList->SectionLength - 2;
    for(Uint32 i = 0; i<noOfTables; i++){
      BackupFormat::CtlFile::TableDescription * tabDesc;
      if(!readTableDesc(f, &tabDesc)){
	ndbout << "Invalid file missing table description" << endl;
	break;
      }
      ndbout << (* tabDesc) << endl;
    }

    BackupFormat::CtlFile::GCPEntry * gcpE;
    if(!readGCPEntry(f, &gcpE)){
      ndbout << "Invalid file! GCP ENtry" << endl;
      break;
    }
    ndbout << (* gcpE) << endl;
    
    break;
  }
  case BackupFormat::LOG_FILE:
  case BackupFormat::UNDO_FILE:
  {
    logEntryNo = 0;

    const Uint32 log_entry_version =
        (likely(ndbd_backup_file_fragid(fileHeader.BackupVersion)) ? 2 : 1);

    typedef BackupFormat::LogFile::LogEntry LogEntry;

    Int32 dataLen;
    Uint32 * data;
    while ((dataLen = readLogEntry(f,
                                   &data,
                                   fileHeader.FileType,
                                   log_entry_version)) > 0)
    {
      LogEntry * logEntry = (LogEntry *) data;
      /**
       * Log Entry
       */
      Uint32 event = ntohl(logEntry->TriggerEvent);
      bool gcp = (event & 0x10000) != 0;
      event &= 0xFFFF;
      if(gcp)
	dataLen--;
      
      ndbout << "LogEntry Table: " << (Uint32)ntohl(logEntry->TableId) 
	     << " Event: " << event
	     << " Length: " << dataLen;
      
      if(gcp)
	  ndbout << " GCP: " << (Uint32)ntohl(logEntry->Data[dataLen]);
      ndbout << endl;
      if (opt_verbose_level > 0)
      {
        Int32 pos = 0;
        while (pos < dataLen)
        {
          AttributeHeader * ah = (AttributeHeader*)&logEntry->Data[pos];
          ndbout_c(" Attribut: %d Size: %d",
                   ah->getAttributeId(),
                   ah->getDataSize());
          pos += ah->getDataSize() + 1;
        }
        require(pos == dataLen);
      }
    }
    require(dataLen == 0);
    break;
  }
  case BackupFormat::LCP_FILE:
  {
    BackupFormat::CtlFile::TableList * tabList;
    if(!readTableList(f, &tabList)){
      ndbout << "Invalid file! No table list" << endl;
      break;
    }
    ndbout << (* tabList) << endl;
    
    if (fileHeader.BackupVersion < NDB_MAKE_VERSION(7,6,4))
    {
      const Uint32 noOfTables = tabList->SectionLength - 2;
      for(Uint32 i = 0; i<noOfTables; i++){
        BackupFormat::CtlFile::TableDescription * tabDesc;
        if(!readTableDesc(f, &tabDesc)){
	  ndbout << "Invalid file missing table description" << endl;
	  break;
        }
        ndbout << (* tabDesc) << endl;
      }
    }
    {
      BackupFormat::DataFile::FragmentHeader fragHeader;
      if(!readFragHeader(f, &fragHeader))
	break;
      ndbout << fragHeader << endl;
      
      Uint32 len, * data, header_type;
      while((len = readRecord(f, &data, &header_type, true)) > 0)
      {
        if (opt_verbose_level > 0)
        {
	  ndbout << "-> " << hex;
	  for(Uint32 i = 0; i < len; i++)
          {
	    ndbout << data[i] << " ";
          }
          ndbout << endl;
        }
      }
      
      BackupFormat::DataFile::FragmentFooter fragFooter;
      if(!readFragFooter(f, &fragFooter))
	break;
      ndbout << fragFooter << endl;
    }
    break;
  }
  case BackupFormat::LCP_CTL_FILE:
  {
    union
    {
      BackupFormat::LCPCtlFile lcpCtlFilePtr;
      char extra_space[4 * BackupFormat::NDB_MAX_LCP_PARTS];
    };
    (void)extra_space;
    if (!readLCPCtlFile(f, &lcpCtlFilePtr))
    {
      ndbout << "Invalid LCP Control file!" << endl;
      break;
    }
    ndbout << lcpCtlFilePtr << endl;
    break;
  }
  default:
    ndbout << "Unsupported file type for printer: " 
	   << fileHeader.FileType << endl;
    break;
  }
  /*
   * Above error handling is weak, one can not decide if whole file have been
   * consumed or not.
   * Closing in abort mode to avoid relax error checking in close (such as
   * verifying at end of file and checksum is correct).
   */
  fo.close(true);
  file.close();
  ndb_end_and_exit(0);
}

#define RETURN_FALSE() { \
  ndbout_c("false: %d", __LINE__); \
  /*abort();*/ \
  return false; \
}

static bool endian = false;

static inline Uint32 Twiddle32(Uint32 x)
{
  if (!endian) return x;
  return ((x & 0x000000ff) << 24) |
         ((x & 0x0000ff00) << 8) |
         ((x & 0x00ff0000) >> 8) |
         ((x & 0xff000000) >> 24);
}

static
inline
size_t
aread(void * buf, size_t sz, size_t n, ndbxfrm_file* f)
{
  byte* byte_buf = static_cast<byte*>(buf);
  ndbxfrm_output_iterator it(byte_buf, byte_buf + sz * n, false);
  int r = f->read_forward(&it);
  if (r == -1)
  {
    return 0; // TODO: fail somehow
  }
  r = it.begin() - byte_buf;
  if (r % sz != 0)
  {
    abort();
    exit(1);
  }
  return r / sz;
}

bool 
readHeader(ndbxfrm_file* f, BackupFormat::FileHeader * dst){
  if(aread(dst, 4, 3, f) != 3)
    RETURN_FALSE();

  if(memcmp(dst->Magic, BACKUP_MAGIC, sizeof(BACKUP_MAGIC)) != 0)
  {
    ndbout_c("Incorrect file-header!");
    printf("Found:  ");
    for (unsigned i = 0; i<sizeof(BACKUP_MAGIC); i++)
      printf("0x%.2x ", (Uint32)(Uint8)dst->Magic[i]);
    printf("\n");
    printf("Expect: ");
    for (unsigned i = 0; i<sizeof(BACKUP_MAGIC); i++)
      printf("0x%.2x ", (Uint32)(Uint8)BACKUP_MAGIC[i]);
    printf("\n");
    
    RETURN_FALSE();
  }

  dst->BackupVersion = ntohl(dst->BackupVersion);
  if(dst->BackupVersion > NDB_VERSION)
  {
    printf("incorrect versions, file: 0x%x expect: 0x%x\n",
           dst->BackupVersion,
           NDB_VERSION);
    RETURN_FALSE();
  }

  if(aread(&dst->SectionType, 4, 2, f) != 2)
    RETURN_FALSE();
  dst->SectionType = ntohl(dst->SectionType);
  dst->SectionLength = ntohl(dst->SectionLength);

  if(dst->SectionType != BackupFormat::FILE_HEADER)
    RETURN_FALSE();

  const Uint32 file_header_section_length =
    ((dst->BackupVersion < NDBD_RAW_LCP)
     ? sizeof(BackupFormat::FileHeader_pre_backup_version)
     : sizeof(BackupFormat::FileHeader))/4 - 3;

  if(dst->SectionLength != file_header_section_length)
    RETURN_FALSE();

  if(aread(&dst->FileType, 4, dst->SectionLength - 2, f) !=
     (dst->SectionLength - 2))
    RETURN_FALSE();

  dst->FileType = ntohl(dst->FileType);
  dst->BackupId = ntohl(dst->BackupId);
  dst->BackupKey_0 = ntohl(dst->BackupKey_0);
  dst->BackupKey_1 = ntohl(dst->BackupKey_1);
  
  if (dst->BackupVersion < NDBD_RAW_LCP)
  {
    dst->NdbVersion = dst->BackupVersion;
    dst->MySQLVersion = 0;
  }

  if(dst->ByteOrder != 0x12345678)
    endian = true;
  
  return true;
}

bool 
readFragHeader(ndbxfrm_file* f,
               BackupFormat::DataFile::FragmentHeader * dst)
{
  if(aread(dst, 1, sizeof(* dst), f) != sizeof(* dst))
    return false;

  dst->SectionType = ntohl(dst->SectionType);
  dst->SectionLength = ntohl(dst->SectionLength);
  dst->TableId = ntohl(dst->TableId);
  dst->FragmentNo = ntohl(dst->FragmentNo);
  dst->ChecksumType = ntohl(dst->ChecksumType);

  if(dst->SectionLength != (sizeof(* dst) >> 2))
    RETURN_FALSE();
  
  if(dst->SectionType != BackupFormat::FRAGMENT_HEADER)
    RETURN_FALSE();

  recNo = 0;
  recInsert = 0;
  recWrite = 0;
  recDeleteByRowId = 0;
  recDeleteByPageId = 0;

  return true;
}

bool 
readFragFooter(ndbxfrm_file* f,
               BackupFormat::DataFile::FragmentFooter * dst)
{ 
  if(aread(dst, 1, sizeof(* dst), f) != sizeof(* dst))
    RETURN_FALSE();
  
  dst->SectionType = ntohl(dst->SectionType);
  dst->SectionLength = ntohl(dst->SectionLength);
  dst->TableId = ntohl(dst->TableId);
  dst->FragmentNo = ntohl(dst->FragmentNo);
  dst->NoOfRecords = ntohl(dst->NoOfRecords);
  dst->Checksum = ntohl(dst->Checksum);
  
  if(dst->SectionLength != (sizeof(* dst) >> 2))
    RETURN_FALSE();
  
  if(dst->SectionType != BackupFormat::FRAGMENT_FOOTER)
    RETURN_FALSE();
  return true;
}


static union {
  Uint32 buf[MaxReadWords];
  BackupFormat::CtlFile::TableList TableList;
  BackupFormat::CtlFile::GCPEntry GcpEntry;
  BackupFormat::CtlFile::TableDescription TableDescription;
  BackupFormat::LogFile::LogEntry LogEntry;
  BackupFormat::LCPCtlFile LCPCtlFile;
  char extra_space[4 * BackupFormat::NDB_MAX_LCP_PARTS];
} theData;

Int32
readRecord(ndbxfrm_file* f,
           Uint32 **dst,
           Uint32 *ext_header_type,
           bool print)
{
  Uint32 len;
  if(aread(&len, 1, 4, f) != 4)
    RETURN_FALSE();

  Uint32 header = ntohl(len);
  len = header & 0xFFFF;
  
  if(aread(theData.buf, 4, len, f) != len)
  {
    return -1;
  }

  if(len > 0)
  {
    Uint32 header_type = header >> 16;
    *ext_header_type = header_type;
    if (header_type == BackupFormat::INSERT_TYPE)
    {
      if (print)
      {
        ndbout_c("INSERT: RecNo: %u: Len: %x, page(%u,%u)",
                 recNo,
                 len,
                 Twiddle32(theData.buf[0]),
                 Twiddle32(theData.buf[1]));
        if (opt_num_data_words)
        {
          ndbout_c("Header_words[Header:%x,GCI:%u,Checksum: %x, X: %x]",
                   theData.buf[2],
                   theData.buf[3],
                   theData.buf[4],
                   theData.buf[5]);
        }
      }
      recNo++;
      recInsert++;
    }
    else if (header_type == BackupFormat::WRITE_TYPE)
    {
      if (print)
      {
        ndbout_c("WRITE: RecNo: %u: Len: %x, page(%u,%u)",
                 recNo,
                 len,
                 Twiddle32(theData.buf[0]),
                 Twiddle32(theData.buf[1]));
        if (opt_num_data_words)
        {
          ndbout_c("Header_words[Header:%x,GCI:%u,Checksum: %x, X: %x]",
                   theData.buf[2],
                   theData.buf[3],
                   theData.buf[4],
                   theData.buf[5]);
        }
      }
      recNo++;
      recWrite++;
    }
    else if (header_type == BackupFormat::DELETE_BY_ROWID_TYPE)
    {
      if (print)
        ndbout_c("DELETE_BY_ROWID: RecNo: %u: Len: %x, page(%u,%u)",
                 recNo,
                 len,
                 Twiddle32(theData.buf[0]),
                 Twiddle32(theData.buf[1]));
      recNo++;
      recDeleteByRowId++;
    }
    else if (header_type == BackupFormat::DELETE_BY_PAGEID_TYPE)
    {
      if (print)
        ndbout_c("DELETE_BY_PAGEID: RecNo: %u: Len: %x, page(%u)",
                 recNo, len, Twiddle32(theData.buf[0]));
      recNo++;
      recDeleteByPageId++;
    }
    else
    {
      ndbout_c("Wrong header type %u", header_type);
    }
  }
  else
  {
    ndbout_c("Found %d INSERT records", recInsert);
    ndbout_c("Found %d WRITE records", recWrite);
    ndbout_c("Found %d DELETE BY ROWID records", recDeleteByRowId);
    ndbout_c("Found %d DELETE BY PAGEID records", recDeleteByPageId);
    ndbout_c("Found %d IGNOREd records", ignored_rows);
    ndbout_c("Found %d records", recNo);
    ignored_rows = 0;
  }
  
  * dst = &theData.buf[0];

  
  return len;
}

Int32
readLogEntry(ndbxfrm_file* f,
             Uint32 **dst,
             Uint32 file_type,
             Uint32 version)
{
  static_assert(MaxReadWords >= BackupFormat::LogFile::LogEntry::MAX_SIZE);
  constexpr Uint32 word_size = sizeof(Uint32);

  Uint32 len;
  if(aread(&len, word_size, 1, f) != 1)
    RETURN_FALSE();

  len = ntohl(len);

  if (len == 0)
    return 0;
  
  Uint32 data_len;
  if (likely(version == 2))
  {
    constexpr Uint32 header_len =
        BackupFormat::LogFile::LogEntry::HEADER_LENGTH_WORDS;
    if (len < header_len)
    {
      return -1;
    }
    if (1 + len > MaxReadWords)
    {
      return -1;
    }
    data_len = len - header_len;
    if (aread(&theData.buf[1], word_size, len, f) != len)
    {
      return -1;
    }
  }
  else
  {
    assert(version == 1);
    constexpr Uint32 header_len =
        BackupFormat::LogFile::LogEntry_no_fragid::HEADER_LENGTH_WORDS;
    if (len < header_len)
    {
      return -1;
    }
    static_assert(header_len <=
                    BackupFormat::LogFile::LogEntry::HEADER_LENGTH_WORDS);
    constexpr Uint32 header_len_diff =
        BackupFormat::LogFile::LogEntry::HEADER_LENGTH_WORDS - header_len;
    if (1 + len + header_len_diff > MaxReadWords)
    {
      return -1;
    }
    data_len = len - header_len;
    if (aread(&theData.buf[1], word_size, header_len, f) != header_len)
    {
      return -1;
    }
    // No fragment id in log event, set it to zero.
    theData.buf[BackupFormat::LogFile::LogEntry::FRAGID_OFFSET] = 0;
    if (aread(&theData.buf[BackupFormat::LogFile::LogEntry::DATA_OFFSET],
              word_size,
              data_len,
              f) != data_len)
    {
      return -1;
    }
  }
  
  theData.buf[0] = len;
  
  if(len > 0)
    logEntryNo++;
  
  * dst = &theData.buf[0];
  
  if (file_type == BackupFormat::UNDO_FILE)
  {
    Uint32 tail_length;
    if (aread(&tail_length, word_size, 1, f) != 1)
    {
      return -1;
    }
    tail_length = ntohl(tail_length);

    if (len != tail_length)
    {
      return -1;
    }
  }

  return data_len;
}

NdbOut & 
operator<<(NdbOut& ndbout, const BackupFormat::FileHeader & hf){
  
  char buf[9];
  memcpy(buf, hf.Magic, sizeof(hf.Magic));
  buf[8] = 0;

  ndbout << "-- FileHeader:" << endl;
  ndbout << "Magic: " << buf << endl;
  ndbout << "BackupVersion: " << hex << hf.BackupVersion << endl;
  ndbout << "SectionType: " << hf.SectionType << endl;
  ndbout << "SectionLength: " << hf.SectionLength << endl;
  ndbout << "FileType: " << hf.FileType << endl;
  ndbout << "BackupId: " << hf.BackupId << endl;
  ndbout << "BackupKey: [ " << hex << hf.BackupKey_0 
	 << " "<< hf.BackupKey_1 << " ]" << endl;
  ndbout << "ByteOrder: " << hex << hf.ByteOrder << endl;
  return ndbout;
} 

NdbOut & operator<<(NdbOut& ndbout, 
		    const BackupFormat::DataFile::FragmentHeader & hf){

  ndbout << "-- Fragment header:" << endl;
  ndbout << "SectionType: " << hf.SectionType << endl;
  ndbout << "SectionLength: " << hf.SectionLength << endl;
  ndbout << "TableId: " << hf.TableId << endl;
  ndbout << "FragmentNo: " << hf.FragmentNo << endl;
  ndbout << "ChecksumType: " << hf.ChecksumType << endl;
  
  return ndbout;
}

NdbOut & operator<<(NdbOut& ndbout,
                    const BackupFormat::DataFile::FragmentFooter & hf){

  ndbout << "-- Fragment footer:" << endl;
  ndbout << "SectionType: " << hf.SectionType << endl;
  ndbout << "SectionLength: " << hf.SectionLength << endl;
  ndbout << "TableId: " << hf.TableId << endl;
  ndbout << "FragmentNo: " << hf.FragmentNo << endl;
  ndbout << "NoOfRecords: " << hf.NoOfRecords << endl;
  ndbout << "Checksum: " << hf.Checksum << endl;

  return ndbout;
}

NdbOut & operator<<(NdbOut& ndbout, 
                   const BackupFormat::LCPCtlFile & lcf)
{
  ndbout << "-- LCP Control file part:" << endl;
  ndbout << "Checksum: " << hex << lcf.Checksum << endl;
  ndbout << "ValidFlag: " << lcf.ValidFlag << endl;
  ndbout << "TableId: " << lcf.TableId << endl;
  ndbout << "FragmentId: " << lcf.FragmentId << endl;
  ndbout << "CreateTableVersion: " << lcf.CreateTableVersion << endl;
  ndbout << "CreateGci: " << lcf.CreateGci << endl;
  ndbout << "MaxGciCompleted: " << lcf.MaxGciCompleted << endl;
  ndbout << "MaxGciWritten: " << lcf.MaxGciWritten << endl;
  ndbout << "LcpId: " << lcf.LcpId << endl;
  ndbout << "LocalLcpId: " << lcf.LocalLcpId << endl;
  ndbout << "MaxPageCount: " << lcf.MaxPageCount << endl;
  ndbout << "MaxNumberDataFiles: " << lcf.MaxNumberDataFiles << endl;
  ndbout << "LastDataFileNumber: " << lcf.LastDataFileNumber << endl;
  ndbout << "RowCount: " <<
            Uint64(Uint64(lcf.RowCountLow) +
                   (Uint64(lcf.RowCountHigh) << 32)) << endl;
  ndbout << "MaxPartPairs: " << lcf.MaxPartPairs << endl;
  ndbout << "NumPartPairs: " << lcf.NumPartPairs << endl;
  if (lcf.NumPartPairs > BackupFormat::NDB_MAX_LCP_PARTS)
  {
    ndbout_c("Too many parts");
    abort();
  }
  for (Uint32 i = 0; i < lcf.NumPartPairs; i++)
  {
    ndbout << "Pair[" << i << "]: StartPart: "
           << lcf.partPairs[i].startPart << " NumParts: "
           << lcf.partPairs[i].numParts << endl;
  }
  return ndbout;
} 

Uint32 decompress_part_pairs(
  struct BackupFormat::LCPCtlFile *lcpCtlFilePtr,
  Uint32 num_parts)
{
  static unsigned char c_part_array[BackupFormat::NDB_MAX_LCP_PARTS * 4];
  Uint32 total_parts = 0;
  unsigned char *part_array =
      (unsigned char*)&lcpCtlFilePtr->partPairs[0].startPart;
  memcpy(c_part_array, part_array, 3 * num_parts);
  Uint32 j = 0;
  for (Uint32 part = 0; part < num_parts; part++)
  {
    Uint32 part_0 = c_part_array[j+0];
    Uint32 part_1 = c_part_array[j+1];
    Uint32 part_2 = c_part_array[j+2];
    Uint32 startPart = ((part_1 & 0xF) + (part_0 << 4));
    Uint32 numParts = (((part_1 >> 4) & 0xF)) + (part_2 << 4);
    lcpCtlFilePtr->partPairs[part].startPart = startPart;
    lcpCtlFilePtr->partPairs[part].numParts = numParts;
    total_parts += numParts;
    j += 3;
  }
  return total_parts;
}

bool 
readLCPCtlFile(ndbxfrm_file* f, BackupFormat::LCPCtlFile *ret)
{
  char * struct_dst = (char*)&theData.LCPCtlFile.Checksum;
  size_t struct_sz = sizeof(BackupFormat::LCPCtlFile) -
              sizeof(BackupFormat::FileHeader);

  if(aread(struct_dst, (struct_sz - 4), 1, f) != 1)
    RETURN_FALSE();

  theData.LCPCtlFile.Checksum = ntohl(theData.LCPCtlFile.Checksum);
  theData.LCPCtlFile.ValidFlag = ntohl(theData.LCPCtlFile.ValidFlag);
  theData.LCPCtlFile.TableId = ntohl(theData.LCPCtlFile.TableId);
  theData.LCPCtlFile.FragmentId = ntohl(theData.LCPCtlFile.FragmentId);
  theData.LCPCtlFile.CreateTableVersion =
    ntohl(theData.LCPCtlFile.CreateTableVersion);
  theData.LCPCtlFile.CreateGci = ntohl(theData.LCPCtlFile.CreateGci);
  theData.LCPCtlFile.MaxGciCompleted =
    ntohl(theData.LCPCtlFile.MaxGciCompleted);
  theData.LCPCtlFile.MaxGciWritten =
    ntohl(theData.LCPCtlFile.MaxGciWritten);
  theData.LCPCtlFile.LcpId = ntohl(theData.LCPCtlFile.LcpId);
  theData.LCPCtlFile.LocalLcpId = ntohl(theData.LCPCtlFile.LocalLcpId);
  theData.LCPCtlFile.MaxPageCount = ntohl(theData.LCPCtlFile.MaxPageCount);
  theData.LCPCtlFile.MaxNumberDataFiles =
    ntohl(theData.LCPCtlFile.MaxNumberDataFiles);
  theData.LCPCtlFile.LastDataFileNumber =
    ntohl(theData.LCPCtlFile.LastDataFileNumber);
  theData.LCPCtlFile.RowCountLow = ntohl(theData.LCPCtlFile.RowCountLow);
  theData.LCPCtlFile.RowCountHigh = ntohl(theData.LCPCtlFile.RowCountHigh);
  theData.LCPCtlFile.MaxPartPairs = ntohl(theData.LCPCtlFile.MaxPartPairs);
  theData.LCPCtlFile.NumPartPairs = ntohl(theData.LCPCtlFile.NumPartPairs);

  size_t parts = theData.LCPCtlFile.NumPartPairs;
  char * part_dst = (char*)&theData.LCPCtlFile.partPairs[0];
  if(aread(part_dst, 3 * parts, 1, f) != 1)
    RETURN_FALSE();

  decompress_part_pairs(&theData.LCPCtlFile, theData.LCPCtlFile.NumPartPairs);
  size_t file_header_sz = sizeof(BackupFormat::FileHeader);
  size_t copy_sz = struct_sz + (4 * parts) + file_header_sz;
  memcpy((char*)ret, &theData.LCPCtlFile, copy_sz);
  return true;
}

bool 
readTableList(ndbxfrm_file* f, BackupFormat::CtlFile::TableList **ret){
  BackupFormat::CtlFile::TableList * dst = &theData.TableList;
  
  if(aread(dst, 4, 2, f) != 2)
    RETURN_FALSE();

  dst->SectionType = ntohl(dst->SectionType);
  dst->SectionLength = ntohl(dst->SectionLength);
  
  if(dst->SectionType != BackupFormat::TABLE_LIST)
    RETURN_FALSE();
  
  const Uint32 len = dst->SectionLength - 2;
  if(aread(&dst->TableIds[0], 4, len, f) != len)
    RETURN_FALSE();

  for(Uint32 i = 0; i<len; i++){
    dst->TableIds[i] = ntohl(dst->TableIds[i]);
  }

  * ret = dst;

  return true;
}

bool 
readTableDesc(ndbxfrm_file* f,
              BackupFormat::CtlFile::TableDescription **ret)
{
  BackupFormat::CtlFile::TableDescription * dst = &theData.TableDescription;
  
  if(aread(dst, 4, 3, f) != 3)
    RETURN_FALSE();

  dst->SectionType = ntohl(dst->SectionType);
  dst->SectionLength = ntohl(dst->SectionLength);
  dst->TableType = ntohl(dst->TableType);

  if(dst->SectionType != BackupFormat::TABLE_DESCRIPTION)
    RETURN_FALSE();
  
  const Uint32 len = dst->SectionLength - 3;
  if(aread(&dst->DictTabInfo[0], 4, len, f) != len)
    RETURN_FALSE();
  
  * ret = dst;
  
  return true;
}

bool 
readGCPEntry(ndbxfrm_file* f, BackupFormat::CtlFile::GCPEntry **ret){
  BackupFormat::CtlFile::GCPEntry * dst = &theData.GcpEntry;
  
  if(aread(dst, 4, 4, f) != 4)
    RETURN_FALSE();

  dst->SectionType = ntohl(dst->SectionType);
  dst->SectionLength = ntohl(dst->SectionLength);
  
  if(dst->SectionType != BackupFormat::GCP_ENTRY)
    RETURN_FALSE();
  
  dst->StartGCP = ntohl(dst->StartGCP);
  dst->StopGCP = ntohl(dst->StopGCP);

  * ret = dst;
  
  return true;
}


NdbOut & 
operator<<(NdbOut& ndbout, const BackupFormat::CtlFile::TableList & hf) {
  ndbout << "-- Table List:" << endl;
  ndbout << "SectionType: " << hf.SectionType << endl;
  ndbout << "SectionLength: " << hf.SectionLength << endl;
  ndbout << "Tables: ";
  for(Uint32 i = 0; i < hf.SectionLength - 2; i++){
    ndbout << hf.TableIds[i] << " ";
    if((i + 1) % 16 == 0)
      ndbout << endl;
  }
  ndbout << endl;
  return ndbout;
}

NdbOut & 
operator<<(NdbOut& ndbout, const BackupFormat::CtlFile::TableDescription & hf)
{
  ndbout << "-- Table Description:" << endl;
  ndbout << "SectionType: " << hf.SectionType << endl;
  ndbout << "SectionLength: " << hf.SectionLength << endl;
  ndbout << "TableType: " << hf.TableType << endl;

  SimplePropertiesLinearReader it(&hf.DictTabInfo[0],  hf.SectionLength - 3);
  char buf[1024];
  for(it.first(); it.valid(); it.next()){
    switch(it.getValueType()){
    case SimpleProperties::Uint32Value:
      ndbout << "Key: " << it.getKey()
	     << " value(" << it.getValueLen() << ") : " 
	     << it.getUint32() << endl;
      break;
    case SimpleProperties::StringValue:
      if(it.getValueLen() < sizeof(buf)){
	it.getString(buf);
	ndbout << "Key: " << it.getKey()
	       << " value(" << it.getValueLen() << ") : " 
	       << "\"" << buf << "\"" << endl;
      } else {
	ndbout << "Key: " << it.getKey()
	       << " value(" << it.getValueLen() << ") : " 
	       << "\"" << "<TOO LONG>" << "\"" << endl;
	
      }
      break;
    case SimpleProperties::BinaryValue:
      if(it.getValueLen() < sizeof(buf))
      {
	ndbout << "Key: " << it.getKey()
	       << " binary value len = " << it.getValueLen() << endl;

      }
      else
      {
	ndbout << "Key: " << it.getKey()
	       << " value(" << it.getValueLen() << ") : " 
	       << "\"" << "<TOO LONG>" << "\"" << endl;
      }
      break;
    default:
      ndbout << "Unknown type for key: " << it.getKey() 
	     << " type: " << it.getValueType() << endl;
    }
  }
  
  return ndbout;
} 

NdbOut & 
operator<<(NdbOut& ndbout, const BackupFormat::CtlFile::GCPEntry & hf) {
  ndbout << "-- GCP Entry:" << endl;
  ndbout << "SectionType: " << hf.SectionType << endl;
  ndbout << "SectionLength: " << hf.SectionLength << endl;
  ndbout << "Start GCP: " << hf.StartGCP << endl;
  ndbout << "Stop GCP: " << hf.StopGCP << endl;
  
  return ndbout;
}

void print_utility_help() {
  fprintf(stdout,
          "\n"
          "There are 3 ways ndb_print_backup_file could be used:\n"
          "1. Can be used to check backup generated files individually (CTL, "
          "DATA, LOG)\n"
          "   Usage: ndb_print_backup_file BACKUP-x.x.ctl | BACKUP-x-x.x.Data "
          "| BACKUP-x.x.log [OPTIONS]\n"
          "2. Can be used to check LCP Data and CTL files individually\n"
          "   Usage: ndb_print_backup_file LCP/c/TtFf.ctl | LCP/c/TtFf.Data "
          "[OPTIONS]\n"
          "3. Can be used to check a set of LCP CTL + DATA files that will be "
          "used to restore a fragment\n"
          "   (Should be run from 'data-dir/ndb_x_fs/LCP' directory)\n"
          "   Usage: ndb_print_backup_file -c <control-directory-number> -t "
          "<table-id> -f <fragment-id> --print-restored-rows\n"
          "\n");
}
