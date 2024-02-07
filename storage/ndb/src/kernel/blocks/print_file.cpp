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

#include <ndb_global.h>

#include <ndb_limits.h>
#include <NdbOut.hpp>
#include <UtilBuffer.hpp>
#include <dbtup/tuppage.hpp>
#include "diskpage.hpp"
#include "kernel/signaldata/FsOpenReq.hpp"
#include "portlib/ndb_file.h"
#include "util/ndb_openssl_evp.h"
#include "util/ndb_opts.h"
#include "util/ndbxfrm_file.h"

using byte = unsigned char;

static bool g_v2;

static ndb_key_state opt_file_key_state("file", nullptr);
static ndb_key_option opt_file_key(opt_file_key_state);
static ndb_key_from_stdin_option opt_file_key_from_stdin(opt_file_key_state);

static int opt_verbose_level = 0;
static int opt_quiet_level = 0;

static struct my_option my_long_options[] = {
    NdbStdOpt::usage,
    NdbStdOpt::help,
    NdbStdOpt::version,

    // Specific options
    {"file-key", 'K', "File encryption key", nullptr, nullptr, nullptr,
     GET_PASSWORD, OPT_ARG, 0, 0, 0, nullptr, 0, &opt_file_key},
    {"file-key-from-stdin", NDB_OPT_NOSHORT, "File encryption key",
     &opt_file_key_from_stdin.opt_value, nullptr, nullptr, GET_BOOL, NO_ARG, 0,
     0, 0, nullptr, 0, &opt_file_key_from_stdin},
    {"quiet", 'q', "Reduce verbosity", nullptr, nullptr, nullptr, GET_NO_ARG,
     NO_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"verbose", 'v', "Write more log messages", nullptr, nullptr, nullptr,
     GET_NO_ARG, NO_ARG, 0, 0, 0, nullptr, 0, nullptr},
    NdbStdOpt::end_of_options};

static const char *load_defaults_groups[] = {"ndb_print_file", nullptr};

static bool get_one_option(int optid, const struct my_option *opt,
                           char *argument) {
  switch (optid) {
    case 'q':  // nocheck
      if (argument == disabled_my_option)
        opt_quiet_level = 0;
      else
        opt_quiet_level++;
      break;
    case 'v':  // noprint
      if (argument == disabled_my_option)
        opt_verbose_level = 0;
      else
        opt_verbose_level++;
      break;
    default:
      return ndb_std_get_one_option(optid, opt, argument);
  }
  return false;
}

static void print_utility_help() {}
static int print_zero_page(int, void *, Uint32 sz);
static int print_extent_page(int, void *, Uint32 sz);
static int print_undo_page(int, void *, Uint32 sz);
static int print_data_page(int, void *, Uint32 sz);
static bool print_page(int page_no [[maybe_unused]]) { return false; }

[[noreturn]] inline void ndb_end_and_exit(int exitcode) {
  ndb_end(0);
  ndb_openssl_evp::library_end();
  exit(exitcode);
}

int g_verbosity = 1;
unsigned g_page_size = File_formats::NDB_PAGE_SIZE;
int (*g_print_page)(int count, void *, Uint32 sz) = print_zero_page;

File_formats::Undofile::Zero_page_v2 g_uf_zero_v2;
File_formats::Undofile::Zero_page g_uf_zero;

File_formats::Datafile::Zero_page_v2 g_df_zero_v2;
File_formats::Datafile::Zero_page g_df_zero;

int main(int argc, char **argv) {
  NDB_INIT(argv[0]);
  ndb_openssl_evp::library_init();
  Ndb_opts opts(argc, argv, my_long_options, load_defaults_groups);
  if (opts.handle_options(&get_one_option)) {
    print_utility_help();
    opts.usage();
    ndb_end_and_exit(1);
  }

  if (ndb_option::post_process_options()) {
    BaseString err_msg = opt_file_key_state.get_error_message();
    if (!err_msg.empty()) {
      fprintf(stderr, "Error: file key: %s\n", err_msg.c_str());
    }
    print_utility_help();
    opts.usage();
    ndb_end_and_exit(1);
  }

  if (opt_file_key_state.get_key() != nullptr &&
      !ndb_openssl_evp::is_aeskw256_supported()) {
    fprintf(stderr,
            "Error: file key options requires OpenSSL 1.0.2 or newer.\n");
    return 2;
  }

  g_verbosity = opt_verbose_level - opt_quiet_level;

  if (argc == 0) {
    ndbout << "Filename not given" << endl;
    ndb_end_and_exit(1);
  }

  for (int i = 0; i < argc; i++) {
    const char *filename = argv[i];

    struct stat sbuf;
    if (stat(filename, &sbuf) != 0) {
      ndbout << "Could not find file: \"" << filename << "\"" << endl;
      continue;
    }
    // const Uint32 bytes = sbuf.st_size;

    UtilBuffer buffer;

    ndb_file file;
    ndbxfrm_file xfrm;
    int r;
    r = file.open(filename, FsOpenReq::OM_READONLY);
    if (r == -1) {
      ndbout << "Failed to open file" << endl;
      continue;
    }
    const byte *key = opt_file_key_state.get_key();
    const size_t key_len = opt_file_key_state.get_key_length();
    r = xfrm.open(file, key, key_len);
    if (r != 0) {
      if (r == -2) xfrm.close(true);
      ndbout << "Failed to open file" << endl;
      file.close();
      continue;
    }

    Uint32 sz;
    Uint32 j = 0;
    bool eof = false;
    do {
      buffer.grow(g_page_size);
      byte *buffer_data = (byte *)buffer.get_data();
      ndbxfrm_output_iterator it = {buffer_data, buffer_data + g_page_size,
                                    false};
      r = xfrm.read_forward(&it);
      if (r == -1) break;
      eof = it.last();
      sz = it.begin() - buffer_data;
      if ((*g_print_page)(j++, buffer.get_data(), sz)) break;
    } while (sz == g_page_size);
    xfrm.close(false);
    file.close();
    if (!eof) {
      ndbout << "Failed to read file" << endl;
    }
    continue;
  }
  ndb_end_and_exit(0);
}

int print_zero_page(int i, void *ptr, Uint32 sz) {
  File_formats::Zero_page_header *page = (File_formats::Zero_page_header *)ptr;
  if (memcmp(page->m_magic, "NDBDISK", 8) != 0) {
    ndbout << "Invalid magic: file is not ndb disk data file" << endl;
    return 1;
  }

  if (page->m_byte_order != 0x12345678) {
    ndbout << "Unhandled byteorder" << endl;
    return 1;
  }

  g_v2 = page->m_ndb_version >= NDB_DISK_V2;
  const char *v2_str = g_v2 ? "true" : "false";
  ndbout << "Version v2 is " << v2_str << endl;
  switch (page->m_file_type) {
    case File_formats::FT_Datafile: {
      if (g_v2) {
        g_df_zero_v2 = (*(File_formats::Datafile::Zero_page_v2 *)ptr);
        ndbout << "-- Datafile -- " << endl;
        ndbout << g_df_zero_v2 << endl;
      } else {
        g_df_zero = (*(File_formats::Datafile::Zero_page *)ptr);
        ndbout << "-- Datafile -- " << endl;
        ndbout << g_df_zero << endl;
      }
      g_print_page = print_extent_page;
      return 0;
    } break;
    case File_formats::FT_Undofile: {
      if (g_v2) {
        g_uf_zero_v2 = (*(File_formats::Undofile::Zero_page_v2 *)ptr);
        ndbout << "-- Undofile -- " << endl;
        ndbout << g_uf_zero_v2 << endl;
      } else {
        g_uf_zero = (*(File_formats::Undofile::Zero_page *)ptr);
        ndbout << "-- Undofile -- " << endl;
        ndbout << g_uf_zero << endl;
      }
      g_print_page = print_undo_page;
      return 0;
    } break;
    default:
      ndbout << "Unhandled file type: " << page->m_file_type << endl;
      return 1;
  }

  if (page->m_page_size != g_page_size) {
    /**
     * Todo
     * lseek
     * g_page_size = page->m_page_size;
     */
    ndbout << "Unhandled page size: " << page->m_page_size << endl;
    return 1;
  }

  return 0;
}

NdbOut &operator<<(NdbOut &out,
                   const File_formats::Datafile::Extent_data &obj) {
  Uint32 extent_size =
      g_v2 ? g_df_zero_v2.m_extent_size : g_df_zero.m_extent_size;
  for (Uint32 i = 0; i < extent_size; i++) {
    char t[2];
    BaseString::snprintf(t, sizeof(t), "%x", obj.get_free_bits(i));
    out << t;
  }
  return out;
}

int print_extent_page(int count, void *ptr, Uint32 sz) {
  Uint32 extent_pages =
      g_v2 ? g_df_zero_v2.m_extent_pages : g_df_zero.m_extent_pages;
  int extent_count =
      g_v2 ? g_df_zero_v2.m_extent_count : g_df_zero.m_extent_count;
  Uint32 extent_size =
      g_v2 ? g_df_zero_v2.m_extent_size : g_df_zero.m_extent_size;
  if ((unsigned)count == extent_pages) {
    g_print_page = print_data_page;
  }
  Uint32 header_words =
      File_formats::Datafile::extent_header_words(extent_size, g_v2);
  Uint32 per_page =
      File_formats::Datafile::extent_page_words(g_v2) / header_words;

  int no = count * per_page;
  const int max = count < int(extent_pages)
                      ? per_page
                      : extent_count - (extent_count - 1) * per_page;

  File_formats::Datafile::Extent_page *page =
      (File_formats::Datafile::Extent_page *)ptr;

  ndbout << "Extent page: " << count << ", lsn = [ "
         << page->m_page_header.m_page_lsn_hi << " "
         << page->m_page_header.m_page_lsn_lo << "] " << endl;
  for (int i = 0; i < max; i++) {
    Uint32 size = g_v2 ? g_df_zero_v2.m_extent_size : g_df_zero.m_extent_size;
    Uint32 *ext_table_id = page->get_table_id(no + i, size, g_v2);
    Uint32 *ext_fragment_id = page->get_table_id(no + i, size, g_v2);
    Uint32 *ext_next_free_extent =
        page->get_next_free_extent(no + i, size, g_v2);

    ndbout << "  extent " << no + i << ": ";
    if ((*ext_table_id) == RNIL) {
      if ((*ext_next_free_extent) != RNIL) {
        ndbout << " FREE, next free: " << (*ext_next_free_extent);
      } else {
        ndbout << " FREE, next free: RNIL";
      }
    } else {
      ndbout << " table_id = " << *ext_table_id;
      ndbout << " fragment_id = " << *ext_fragment_id;
      if (g_v2) {
        Uint32 *ext_create_table_version =
            page->get_create_table_version(no + i, size, g_v2);
        ndbout << " create_table_version = " << *ext_create_table_version;
      }
      ndbout << (*page->get_extent_data(i, size, g_v2)) << endl;
    }
  }
  return 0;
}

int print_data_page(int count, void *ptr, Uint32 sz) {
  File_formats::Datafile::Data_page *page =
      (File_formats::Datafile::Data_page *)ptr;

  ndbout << "Data page: " << count << ", lsn = [ "
         << page->m_page_header.m_page_lsn_hi << " "
         << page->m_page_header.m_page_lsn_lo << "]";

  if (g_verbosity > 1 || print_page(count)) {
    switch (page->m_page_header.m_page_type) {
      case File_formats::PT_Unallocated:
        break;
      case File_formats::PT_Tup_fixsize_page:
        ndbout << " fix ";
        if (g_verbosity > 2 || print_page(count))
          ndbout << (*(Tup_fixsize_page *)page);
        break;
      case File_formats::PT_Tup_varsize_page:
        ndbout << " var ";
        if (g_verbosity > 2 || print_page(count))
          ndbout << endl << (*(Tup_varsize_page *)page);
        break;
      default:
        ndbout << " unknown page type: %d" << page->m_page_header.m_page_type;
    }
  }
  ndbout << endl;
  return 0;
}

#define DBTUP_C
#include "dbtup/Dbtup.hpp"

#define JAM_FILE_ID 431

int print_undo_page(int count, void *ptr, Uint32 sz) {
  Uint32 undo_pages = g_v2 ? g_uf_zero_v2.m_undo_pages : g_uf_zero.m_undo_pages;
  if (count > int(undo_pages + 1)) {
    ndbout_c(" ERROR to many pages in file!!");
    return 1;
  }

  File_formats::Undofile::Undo_page *page =
      (File_formats::Undofile::Undo_page *)ptr;

  Uint32 *data = &page->m_data[0];
  Uint32 words_used = page->m_words_used;
  if (g_v2) {
    File_formats::Undofile::Undo_page_v2 *page_v2 =
        (File_formats::Undofile::Undo_page_v2 *)page;
    words_used = page_v2->m_words_used;
    data = &page_v2->m_data[0];
  }
  if (page->m_page_header.m_page_lsn_hi != 0 ||
      page->m_page_header.m_page_lsn_lo != 0) {
    ndbout << "Undo page: " << count << ", lsn = [ "
           << page->m_page_header.m_page_lsn_hi << " "
           << page->m_page_header.m_page_lsn_lo << "] "
           << "words used: " << words_used << endl;

    Uint64 lsn = 0;
    lsn += page->m_page_header.m_page_lsn_hi;
    lsn <<= 32;
    lsn += page->m_page_header.m_page_lsn_lo;
    lsn++;

    if (g_verbosity >= 3) {
      Uint32 pos = words_used - 1;
      while (pos + 1 != 0) {
        Uint32 len = data[pos] & 0xFFFF;
        Uint32 type = data[pos] >> 16;
        const Uint32 *src = data + pos - len + 1;
        Uint32 next_pos = pos - len;
        if (type & File_formats::Undofile::UNDO_NEXT_LSN) {
          type &= ~(Uint32)File_formats::Undofile::UNDO_NEXT_LSN;
          lsn--;
        } else {
          lsn = 0;
          lsn += *(src - 2);
          lsn <<= 32;
          lsn += *(src - 1);
          next_pos -= 2;
        }
        if (g_verbosity > 3) printf(" %.4d - %.4d : ", pos - len + 1, pos);
        switch (type) {
          case File_formats::Undofile::UNDO_LCP_FIRST:
            printf("[ %lld LCP First %d tab: %d frag: %d ]", lsn, src[0],
                   src[1] >> 16, src[1] & 0xFFFF);
            if (g_verbosity <= 3) printf("\n");
            break;
          case File_formats::Undofile::UNDO_LCP:
            printf("[ %lld LCP %d tab: %d frag: %d ]", lsn, src[0],
                   src[1] >> 16, src[1] & 0xFFFF);
            if (g_verbosity <= 3) printf("\n");
            break;
          case File_formats::Undofile::UNDO_LOCAL_LCP_FIRST:
            printf("[ %lld Local LCP First %d,%d tab: %d frag: %d ]", lsn,
                   src[0], src[1], src[2] >> 16, src[2] & 0xFFFF);
            if (g_verbosity <= 3) printf("\n");
            break;
          case File_formats::Undofile::UNDO_LOCAL_LCP:
            printf("[ %lld Local LCP %d,%d tab: %d frag: %d ]", lsn, src[0],
                   src[1], src[2] >> 16, src[2] & 0xFFFF);
            if (g_verbosity <= 3) printf("\n");
            break;
          case File_formats::Undofile::UNDO_TUP_ALLOC:
            if (g_verbosity > 3) {
              Dbtup::Disk_undo::Alloc *req = (Dbtup::Disk_undo::Alloc *)src;
              printf("[ %lld A %d %d %d ]", lsn, req->m_file_no_page_idx >> 16,
                     req->m_file_no_page_idx & 0xFFFF, req->m_page_no);
            }
            break;
          case File_formats::Undofile::UNDO_TUP_UPDATE:
            if (g_verbosity > 3) {
              Dbtup::Disk_undo::Update *req = (Dbtup::Disk_undo::Update *)src;
              printf("[ %lld U %d %d %d gci: %d ]", lsn,
                     req->m_file_no_page_idx >> 16,
                     req->m_file_no_page_idx & 0xFFFF, req->m_page_no,
                     req->m_gci);
            }
            break;
          case File_formats::Undofile::UNDO_TUP_FREE:
            if (g_verbosity > 3) {
              Dbtup::Disk_undo::Free *req = (Dbtup::Disk_undo::Free *)src;
              printf("[ %lld F %d %d %d gci: %d, row(%u,%u) ]", lsn,
                     req->m_file_no_page_idx >> 16,
                     req->m_file_no_page_idx & 0xFFFF, req->m_page_no,
                     req->m_gci, src[3], src[4]);
            }
            break;
          case File_formats::Undofile::UNDO_TUP_DROP: {
            Dbtup::Disk_undo::Drop *req = (Dbtup::Disk_undo::Drop *)src;
            printf("[ %lld Drop %d ]", lsn, req->m_table);
            if (g_verbosity <= 3) printf("\n");
            break;
          }
          default:
            ndbout_c("[ Unknown type %d len: %d, pos: %d ]", type, len, pos);
            if (!(len && type)) {
              pos = 0;
              while (pos < words_used) {
                printf("%.8x ", data[pos]);
                if ((pos + 1) % 7 == 0) ndbout_c("%s", "");
                pos++;
              }
            }
            assert(len && type);
        }
        pos = next_pos;
        if (g_verbosity > 3) printf("\n");
      }
    }
  }
  return 0;
}

// Dummy implementations
Signal::Signal() {}
SimulatedBlock::Callback SimulatedBlock::TheEmptyCallback = {0, 0};
