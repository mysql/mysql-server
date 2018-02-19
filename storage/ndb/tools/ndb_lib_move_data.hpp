/* Copyright (c) 2013, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include <NdbDictionary.hpp>

#include "my_compiler.h"

/*
 * Move data between "compatible" tables.
 *
 * Uses batches of insert-into-target / delete-from-source.
 * Compared to copying alter table, the advantages are
 * 1) does not need double storage 2) can be restarted
 * after temporary failure.
 *
 * Use init() to define source / target and then move_data().
 * Methods return -1 on error and 0 on success.  On temporary
 * error call move_data() again to continue.
 *
 * Use get_error() for details.  Negative error code means
 * non-recoverable error.  Positive error code is ndb error
 * which may be temporary.
 *
 * Like ndb_restore, remaps columns based on name.
 *
 * Used from ndb_restore for char<->text conversion.  Here
 * user pre-creates the new table and then loads data into it
 * via ndb_restore -r.  This first loads data into a temporary
 * "staging table" which has same structure as table in backup.
 * Then move_data() is called to move data into the new table.
 *
 * Current version handles data conversions between all char,
 * binary, text, blob types.
 *
 * The conversion methods should be unified with ndb_restore.
 * Missing cases (date and numeric types) should be added.
 */

struct Ndb_move_data {
  struct Opts {
    enum {
      MD_ABORT_ON_ERROR = 0x1,
      MD_EXCLUDE_MISSING_COLUMNS = 0x2,
      MD_ATTRIBUTE_PROMOTION = 0x4,
      MD_ATTRIBUTE_DEMOTION = 0x8
    };
    int flags;
    // for parsing --staging-tries (but caller handles retries)
    struct Tries {
      int maxtries; // 0 = no limit
      int mindelay;
      int maxdelay;
      Tries() : maxtries(0), mindelay(1000), maxdelay(60*1000) {}
    };
    Opts();
  };
  struct Stat {
    Uint64 rows_moved; // in current move_data() call
    Uint64 rows_total; // total moved so far
    Uint64 truncated; // truncated attributes so far
    Stat();
  };
  struct Error {
    enum {
      InvalidState = -101,
      InvalidSource = -102,
      InvalidTarget = -103,
      UnsupportedConversion = -201,
      NoExcludeMissingFlag = -202,
      NoPromotionFlag = -203,
      NoDemotionFlag = -204,
      DataTruncated = -301
    };
    int line;
    int code;
    char message[512];
    NdbError ndberror; // valid if code > 0
    bool is_temporary() const;
    Error();
  };
  Ndb_move_data();
  ~Ndb_move_data();
  int init(const NdbDictionary::Table* source,
           const NdbDictionary::Table* target);
  void set_opts_flags(int flags);
  static void unparse_opts_tries(char* opt, const Opts::Tries& ot);
  static int parse_opts_tries(const char* opt, Opts::Tries& retry);
  int move_data(Ndb* ndb);
  void error_insert(); // insert random temporary error
  const Stat& get_stat();
  const Error& get_error();

private:
  const NdbDictionary::Table* m_source; // source rows moved from
  const NdbDictionary::Table* m_target; // target rows moved to
  struct Attr {
    enum {
      TypeNone = 0,
      TypeArray,
      TypeBlob,
      TypeOther
    };
    const NdbDictionary::Column* column;
    const char* name;
    int id; // own id (array index)
    int map_id; // column id in other table
    int type;
    Uint32 size_in_bytes;
    Uint32 length_bytes;
    Uint32 data_size; // size_in_bytes - length_bytes
    int pad_char;
    bool equal; // attr1,attr2 equal non-blobs
    Attr();
  };
  Attr* m_sourceattr;
  Attr* m_targetattr;
  void set_type(Attr& attr, const NdbDictionary::Column* c);
  Uint32 calc_str_len_truncated(CHARSET_INFO *cs, char *data, uint32 maxlen);
  int check_nopk(const Attr& attr1, const Attr& attr2);
  int check_promotion(const Attr& attr1, const Attr& attr2);
  int check_demotion(const Attr& attr1, const Attr& attr2);
  int check_sizes(const Attr& attr1, const Attr& attr2);
  int check_unsupported(const Attr& attr1, const Attr& attr2);
  int check_tables();
  struct Data {
    char* data;
    Data* next;
    Data();
    ~Data();
  };
  Data* m_data; // blob data for the batch, used for blob2blob
  char* alloc_data(Uint32 n);
  void release_data();
  struct Op {
    Ndb* ndb;
    NdbTransaction* scantrans;
    NdbScanOperation* scanop;
    NdbTransaction* updatetrans;
    NdbOperation* updateop;
    union Value {
      NdbRecAttr* ra;
      NdbBlob* bh;
    };
    Value* values;
    int buflen;
    char* buf1;
    char* buf2;
    Uint32 rows_in_batch;
    Uint32 truncated_in_batch;
    bool end_of_scan;
    Op();
    ~Op();
  };
  Op m_op;
  int start_scan();
  int copy_other_to_other(const Attr& attr1, const Attr& attr2);
  int copy_data_to_array(const char* data1, const Attr& attr2,
                         Uint32 length1, Uint32 length1x);
  int copy_array_to_array(const Attr& attr1, const Attr& attr2);
  int copy_array_to_blob(const Attr& attr1, const Attr& attr2);
  int copy_blob_to_array(const Attr& attr1, const Attr& attr2);
  int copy_blob_to_blob(const Attr& attr1, const Attr& attr2);
  int copy_attr(const Attr& attr1, const Attr& attr2);
  int move_row();
  int move_batch();
  void close_op(Ndb* ndb, int ret);
  Opts m_opts;
  Stat m_stat;
  Error m_error;
  void set_error_line(int line);
  void set_error_code(int code, const char* fmt, ...)
    MY_ATTRIBUTE((format(printf, 3, 4)));
  void set_error_code(const NdbError& ndberror);
  void reset_error();
  friend class NdbOut& operator<<(NdbOut&, const Error&);
  bool m_error_insert;
  void invoke_error_insert();
  void abort_on_error();
};
