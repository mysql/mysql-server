/*
   Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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

#ifndef NDB_IMPORT_CSV_HPP
#define NDB_IMPORT_CSV_HPP

#include <ndb_global.h>
#include <stdint.h>
#include <ndb_limits.h>
#include <NdbOut.hpp>
#include "NdbImport.hpp"
#include "NdbImportUtil.hpp"
// STL
#include <algorithm>

/*
 * CSV helper class.  There is one Csv instance attached to the Impl
 * instance.  The Csv instance is not aware of the Impl instance.
 *
 * Input: caller passes buffers of CSV data and gets back parsed and
 * evaluated binary rows.  See struct Input below.
 *
 * Output: caller passes binary row data and gets back buffers of
 * formatted CSV data.  See struct Output below.
 */

class NdbImportCsv {
public:
  typedef NdbImport::Opt Opt;
  typedef NdbImport::OptCsv OptCsv;
  typedef NdbImportUtil::Name Name;
  typedef NdbImportUtil::Lockable Lockable;
  typedef NdbImportUtil::ListEnt ListEnt;
  typedef NdbImportUtil::List List;
  typedef NdbImportUtil::Attr Attr;
  typedef NdbImportUtil::Attrs Attrs;
  typedef NdbImportUtil::Table Table;
  typedef NdbImportUtil::Row Row;
  typedef NdbImportUtil::Blob Blob;
  typedef NdbImportUtil::RowList RowList;
  typedef NdbImportUtil::RowCtl RowCtl;
  typedef NdbImportUtil::Range Range;
  typedef NdbImportUtil::RangeList RangeList;
  typedef NdbImportUtil::RowMap RowMap;
  typedef NdbImportUtil::Buf Buf;
  typedef NdbImportUtil::Stats Stats;

  NdbImportCsv(NdbImportUtil& util);
  ~NdbImportCsv();
  NdbImportUtil& m_util;
  Error& m_error;       // global

  // spec

  struct Spec {
    Spec();
    ~Spec();
    // allocated into uchar* with escapes translated
    const uchar* m_fields_terminated_by;
    const uchar* m_fields_enclosed_by;
    const uchar* m_fields_optionally_enclosed_by;
    const uchar* m_fields_escaped_by;
    const uchar* m_lines_terminated_by;
    uint m_fields_terminated_by_len;
    uint m_fields_enclosed_by_len;
    uint m_fields_optionally_enclosed_by_len;
    uint m_fields_escaped_by_len;
    uint m_lines_terminated_by_len;
  };

  // return allocated translated string and its length
  int translate_escapes(const char* src, const uchar*& dst, uint& dstlen);
  int set_spec(Spec& spec, const OptCsv& optcsv, OptCsv::Mode mode);
  int set_spec(const OptCsv& optcsv, OptCsv::Mode mode);

  // items

  struct Chunk {
    uint m_pos; // start position
    uint m_len; // number of bytes returned starting at m_pos
    uint m_end; // end position (possibly m_end > m_pos + m_len)
  };

  struct Data : ListEnt {
    Data() {
      m_pos = 0;
      m_len = 0;
      m_end = 0;
      m_escape = false;
    }
    Data* next() {
      return static_cast<Data*>(m_next);
    }
    uint m_pos;
    uint m_len;
    uint m_end;
    bool m_escape;
  };

  struct DataList : private List {
    Data* front() {
      return static_cast<Data*>(List::m_front);
    }
    Data* back() {
      return static_cast<Data*>(List::m_back);
    }
    void push_back(Data* data) {
      List::push_back(data);
    }
    Data* pop_front() {
      return static_cast<Data*>(List::pop_front());
    }
    void push_back_from(DataList& src) {
      List::push_back_from(src);
    }
    uint cnt() const {
      return m_cnt;
    }
  };

  struct Field : ListEnt {
    Field() {
      m_fieldno = 0;
      m_pos = 0;
      m_end = 0;
      m_pack_pos = 0;
      m_pack_end = 0;
      m_null = false;
    }
    Field* next() {
      return static_cast<Field*>(m_next);
    }
    bool is_empty() const {
      return (m_pos == m_end);
    }
    uint m_fieldno;
    uint m_pos;
    uint m_end;
    uint m_pack_pos;
    uint m_pack_end;
    bool m_null;
    DataList m_data_list;
  };

  struct FieldList : private List {
    Field* front() {
      return static_cast<Field*>(List::m_front);
    }
    void push_back(Field* field) {
      List::push_back(field);
    }
    Field* pop_front() {
      return static_cast<Field*>(List::pop_front());
    }
    void push_back_from(FieldList& src) {
      List::push_back_from(src);
    }
    uint cnt() const {
      return m_cnt;
    }
    bool final_field_is_empty() const {
      return (static_cast<Field*>(m_back))->is_empty();
    }
    Field * pop_back() {
      return static_cast<Field*>(List::pop_back());
    }
  };

  struct Line : ListEnt {
    Line() {
      m_lineno = 0;
      m_pos = 0;
      m_end = 0;
      m_reject = false;
    }
    Line* next() {
      return static_cast<Line*>(m_next);
    }
    uint m_lineno;
    uint m_pos;
    uint m_end;
    bool m_reject;
    FieldList m_field_list;
  };

  struct LineList : private List {
    Line* front() {
      return static_cast<Line*>(List::m_front);
    }
    Line* back() {
      return static_cast<Line*>(List::m_back);
    }
    void push_back(Line* line) {
      List::push_back(line);
    }
    Line* pop_front() {
      return static_cast<Line*>(List::pop_front());
    }
    void push_back_from(LineList& src) {
      List::push_back_from(src);
    }
    uint cnt() const {
      return m_cnt;
    }
  };

  struct Alloc {
    Alloc();
    Data* alloc_data();
    Field* alloc_field();
    Line* alloc_line();
    void free_data_list(DataList& data_list);
    void free_field_list(FieldList& field_list);
    void free_field(Field *);
    void free_line_list(LineList& line_list);
    bool balanced();
    DataList m_data_free;
    FieldList m_field_free;
    LineList m_line_free;
    uint m_alloc_data_cnt;
    uint m_alloc_field_cnt;
    uint m_alloc_line_cnt;
    uint m_free_data_cnt;
    uint m_free_field_cnt;
    uint m_free_line_cnt;
  };

  void free_data_list(Data*& data);
  void free_field_list(Field*& field);
  void free_line_list(Line*& line);

  // input

  /*
   * CSV input.
   *
   * Each CSV input worker has its own Input instance and buffer.
   * The input buffer is "split" i.e. has upper and lower halves.
   *
   * The input file is always owned by some CSV input worker.  The
   * worker reads a block of data into its buffer lower half.  File
   * ownership is passed immediately to the next worker so it can
   * read next file block.  And so on.
   *
   * Meanwhile current worker does parse to find lines and fields.
   * The last line is usually partial, causing parse error, but if
   * the last token was end-of-data we can assume that no real error
   * occurred.  The partial line ("tail") is copied to the upper
   * half of next input worker buffer just above the lower half.
   * The next worker can then do its own parse.
   *
   * Meanwhile current worker proceeds with evaluation of the lines
   * and fields found.  The resulting rows are stored locally until
   * a separate send step pipes them to relay rows (rows_out).
   *
   * Parsing uses bison.  The CSV delimiters are not fixed so the
   * lex part is hand-coded with lookup tables.  We require that
   * each non-empty delimiter starts with a different special char.
   * Also a strict format with field separators and line terminators
   * is required.
   */

  struct Parse;
  struct Eval;

  struct Input : Alloc {
    Input(NdbImportCsv& csv,
          const char* name,
          const Spec& spec,
          const Table& table,
          Buf& buf,
          RowList& rows_out,
          RowList& rows_reject,
          RowMap& rowmap_in,
          Stats& stats);
    ~Input();
    void do_init();
    void do_resume(Range range_in);
    void do_parse();
    void do_eval();
    void do_send(uint& curr, uint& left);
    void do_movetail(Input& input2);
    void reject_line(const Line* line,
                     const Field* field,
                     const Error& error);
    void print(NdbOut& out);
    NdbImportCsv& m_csv;
    NdbImportUtil& m_util;
    Name m_name;
    const Spec& m_spec;
    const Table& m_table;
    Buf& m_buf;
    RowList& m_rows_out;
    RowList& m_rows_reject;
    RowMap& m_rowmap_in;
    Error m_error;      // local csv error
    bool has_error() {
      return m_util.has_error(m_error);
    }
    LineList m_line_list;
    RowList m_rows;     // lines eval'd to rows
    Parse* m_parse;
    Eval* m_eval;
    uint64 m_startpos;
    uint64 m_startlineno;
    uint64 m_ignore_lines;
    bool m_missing_ai_col;
  };

  // parse

  static const uint g_bytecnt = 256;

  struct Parse {
    enum State {
      State_plain = 0,
      State_quote = 1,
      State_escape = 2,
      State_cr = 3
    };
    static const int g_statecnt = State_cr + 1;
    Parse(Input& input);
    void do_init();
    void push_state(State state);
    void pop_state();
    void do_parse();
    int do_lex(union YYSTYPE* lvalp);
    void do_error(const char* msg);
    void pack_field(Field* field);
    Input& m_input;
    NdbImportCsv& m_csv;
    NdbImportUtil& m_util;
    Error& m_error;     // team level
    int m_trans[g_statecnt][g_bytecnt];
    static const uint g_stackmax = 10;
    uint m_stacktop;
    State m_state[g_stackmax];
    uint m_escapes[g_bytecnt];
    int m_last_token;
    // parse temporaries
    LineList m_line_list;
    FieldList m_field_list;
    DataList m_data_list;
  };

  static const char* g_str_state(Parse::State state);

  // eval

  struct Eval {
    Eval(Input& input);
    ~Eval();
    void do_init();
    void do_eval();
    void eval_line(Row* row, Line* line, const uint expect_attrcnt);
    void eval_auto_inc_field(Row* row, Line* line, Field* field, const uint attr_id);
    void eval_field(Row* row, Line* line, Field* field, const uint attr_id);
    void eval_null(Row* row, Line* line, Field* field, const uint attr_id);
    Input& m_input;
    NdbImportCsv& m_csv;
    NdbImportUtil& m_util;
    Error& m_error;     // team level
  };

  // output

  /*
   * CSV output.
   *
   * Currently used only by the diagnostics worker to write results
   * etc into CSV files.  The worker adds one row at a time and gets
   * back formatted CSV data in the buffer, which it then writes
   * immediately to the associated file.
   *
   * A high-performance multi-threaded CSV output team might appear
   * in the future (ndb_export).
   */

  struct Output {
    Output(NdbImportCsv& csv,
           const Spec& spec,
           const Table& table,
           Buf& buf);
    void do_init();
    void add_header();
    void add_line(const Row* row);
    void add_field(const Attr& attr, const Row* row);
    void add_char(const uchar* data, uint len);
    void add_quote();
    void add_fieldsep();
    void add_lineend();
    NdbImportCsv& m_csv;
    NdbImportUtil& m_util;
    const Spec& m_spec;
    const Table& m_table;
    Buf& m_buf;
    uchar m_escapes[g_bytecnt];
  };
};

NdbOut& operator<<(NdbOut& out, const NdbImportCsv::Input& input);
NdbOut& operator<<(NdbOut& out, const NdbImportCsv::Parse& parse);
NdbOut& operator<<(NdbOut& out, const NdbImportCsv::Eval& eval);
NdbOut& operator<<(NdbOut& out, const NdbImportCsv::Output& output);

#endif
