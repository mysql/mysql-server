/*
   Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <my_sys.h>
#include <NdbSqlUtil.hpp>
#include <decimal_utils.hpp>
#include "NdbImportCsv.hpp"
#include "NdbImportCsvGram.hpp"
// legacy
#include <BaseString.hpp>

#define snprintf BaseString::snprintf

extern int NdbImportCsv_yyparse(NdbImportCsv::Parse& csvparse);
#ifdef VM_TRACE
extern int NdbImportCsv_yydebug;
#endif

NdbImportCsv::NdbImportCsv(NdbImportUtil& util) :
  m_util(util),
  m_error(m_util.c_error)
{
#ifdef VM_TRACE
  NdbImportCsv_yydebug = 0;
#endif
}

NdbImportCsv::~NdbImportCsv()
{
}

// spec

NdbImportCsv::Spec::Spec()
{
  m_fields_terminated_by = 0;
  m_fields_enclosed_by = 0;
  m_fields_optionally_enclosed_by = 0;
  m_fields_escaped_by = 0;
  m_lines_terminated_by = 0;
  m_fields_terminated_by_len = Inval_uint;
  m_fields_enclosed_by_len = Inval_uint;
  m_fields_optionally_enclosed_by_len = Inval_uint;
  m_fields_escaped_by_len = Inval_uint;
  m_lines_terminated_by_len = Inval_uint;
}

NdbImportCsv::Spec::~Spec()
{
  delete [] m_fields_terminated_by;
  delete [] m_fields_enclosed_by;
  delete [] m_fields_optionally_enclosed_by;
  delete [] m_fields_escaped_by;
  delete [] m_lines_terminated_by;
}

int
NdbImportCsv::translate_escapes(const char* src,
                                const uchar*& dst,
                                uint& dstlen)
{
  dst = 0;
  dstlen = Inval_uint;
  if (src != 0)
  {
    uint n = strlen(src);
    uchar* tmpdst = new uchar [n + 1];  // cannot be longer than src
    const char* p = src;
    uchar* q = tmpdst;
    while (*p != 0)
    {
      if (*p != '\\')
      {
        *q++ = (uchar)*p++;
      }
      else
      {
        // XXX check what mysqlimport translates
        char c = *++p;
        switch (c) {
        case '\\':
          *q++ = '\\';
          break;
        case 'n':
          *q++ = '\n';
          break;
        case 'r':
          *q++ = '\r';
          break;
        case 't':
          *q++ = '\t';
          break;
        default:
          m_util.set_error_usage(m_error, __LINE__,
                                 "unknown escape '\\%c' (0x%x) in CSV option",
                                 c, (uint)(unsigned char)c);
          return -1;
        }
        p++;
      }
    }
    // null-terminate for use as char*
    *q = 0;
    dst = tmpdst;
    dstlen = q - tmpdst;
  }
  return 0;
}

int
NdbImportCsv::set_spec(Spec& spec, const OptCsv& optcsv, OptCsv::Mode mode)
{
  if (translate_escapes(optcsv.m_fields_terminated_by,
                        spec.m_fields_terminated_by,
                        spec.m_fields_terminated_by_len) == -1)
    return -1;
  if (translate_escapes(optcsv.m_fields_enclosed_by,
                        spec.m_fields_enclosed_by,
                        spec.m_fields_enclosed_by_len) == -1)
    return -1;
  if (translate_escapes(optcsv.m_fields_optionally_enclosed_by,
                        spec.m_fields_optionally_enclosed_by,
                        spec.m_fields_optionally_enclosed_by_len) == -1)
    return -1;
  if (translate_escapes(optcsv.m_fields_escaped_by,
                        spec.m_fields_escaped_by,
                        spec.m_fields_escaped_by_len) == -1)
    return -1;
  if (translate_escapes(optcsv.m_lines_terminated_by,
                        spec.m_lines_terminated_by,
                        spec.m_lines_terminated_by_len) == -1)
    return -1;
  int used[256];
  for (uint i = 0; i < 256; i++)
    used[i] = 0;
  do {
    // fields-terminated-by
    {
      if (spec.m_fields_terminated_by == 0 ||
          spec.m_fields_terminated_by_len == 0)
      {
        const char* msg =
          "fields-terminated-by cannot be empty";
        m_util.set_error_usage(m_error, __LINE__, "%s", msg);
        break;
      }
      uchar u = spec.m_fields_terminated_by[0];
      if (used[u])
      {
        const char* msg =
          "fields-terminated-by re-uses previous special char";
        m_util.set_error_usage(m_error, __LINE__, "%s", msg);
        break;
      }
      used[u] = T_FIELDSEP;
    }
    // fields-enclosed-by
    {
      if (spec.m_fields_enclosed_by != 0)
      {
        if (spec.m_fields_enclosed_by_len != 1)
        {
          const char* msg =
            "fields-enclosed-by must be a single char";
          m_util.set_error_usage(m_error, __LINE__, "%s", msg);
          break;
        }
        uchar u = spec.m_fields_enclosed_by[0];
        if (used[u])
        {
          const char* msg =
            "fields-enclosed-by re-uses previous special char";
          m_util.set_error_usage(m_error, __LINE__, "%s", msg);
          break;
        }
        used[u] = T_QUOTE;
      }
    }
    // fields-optionally-enclosed-by
    {
      if (spec.m_fields_optionally_enclosed_by != 0)
      {
        if (spec.m_fields_optionally_enclosed_by_len != 1)
        {
          const char* msg =
            "fields-optionally-enclosed-by must be a single char";
          m_util.set_error_usage(m_error, __LINE__, "%s", msg);
          break;
        }
        uchar u = spec.m_fields_optionally_enclosed_by[0];
        if (used[u] && used[u] != T_QUOTE)
        {
          const char* msg =
            "fields-optionally-enclosed-by re-uses previous special char";
          m_util.set_error_usage(m_error, __LINE__, "%s", msg);
          break;
        }
        used[u] = T_QUOTE;
      }
    }
    // fields-escaped-by
    {
      require(spec.m_fields_escaped_by != 0);
      if (spec.m_fields_escaped_by_len != 1)
      {
        const char* msg =
          "fields-escaped-by must be empty or a single char";
        m_util.set_error_usage(m_error, __LINE__, "%s", msg);
        break;
      }
      uchar u = spec.m_fields_escaped_by[0];
      if (used[u])
      {
        const char* msg =
          "fields-escaped-by re-uses previous special char";
        m_util.set_error_usage(m_error, __LINE__, "%s", msg);
        break;
      }
      used[u] = T_ESCAPE;
    }
    // lines terminated-by
    {
      require(spec.m_lines_terminated_by != 0);
      if (spec.m_lines_terminated_by_len == 0)
      {
        const char* msg =
          "lines-terminated-by cannot be empty";
        m_util.set_error_usage(m_error, __LINE__, "%s", msg);
        break;
      }
      uchar u = spec.m_lines_terminated_by[0];
      if (used[u])
      {
        const char* msg =
          "lines-terminated-by re-uses previous special char";
        m_util.set_error_usage(m_error, __LINE__, "%s", msg);
        break;
      }
      used[u] = T_LINEEND;
    }
    // adjust
    if (mode == OptCsv::ModeInput)
    {
      /*
       * fields-enclosed-by and fields-optionally-enclosed-by
       * have exact same meaning
       */
      if (spec.m_fields_enclosed_by != 0 &&
          spec.m_fields_optionally_enclosed_by != 0)
      {
        if (spec.m_fields_enclosed_by_len !=
            spec.m_fields_optionally_enclosed_by_len ||
            memcmp(spec.m_fields_enclosed_by,
                   spec.m_fields_optionally_enclosed_by,
                   spec.m_fields_enclosed_by_len) != 0)
        {
          const char* msg =
            "conflicting fields-enclosed-by options";
          m_util.set_error_usage(m_error, __LINE__, "%s", msg);
          break;
        }
      }
      else if (spec.m_fields_enclosed_by != 0)
      {
        // for completeness - will not be used
        uchar* fields_optionally_enclosed_by =
          new uchar [spec.m_fields_enclosed_by_len + 1];
        memcpy(fields_optionally_enclosed_by,
               spec.m_fields_enclosed_by,
               spec.m_fields_enclosed_by_len + 1);
        spec.m_fields_optionally_enclosed_by =
          fields_optionally_enclosed_by;
        spec.m_fields_optionally_enclosed_by_len =
          spec.m_fields_enclosed_by_len;
      }
      else if (spec.m_fields_optionally_enclosed_by != 0)
      {
        uchar* fields_enclosed_by =
          new uchar [spec.m_fields_optionally_enclosed_by_len + 1];
        memcpy(fields_enclosed_by,
               spec.m_fields_optionally_enclosed_by,
               spec.m_fields_optionally_enclosed_by_len + 1);
        spec.m_fields_enclosed_by =
          fields_enclosed_by;
        spec.m_fields_enclosed_by_len =
          spec.m_fields_optionally_enclosed_by_len;
      }
    }
    if (mode == OptCsv::ModeOutput)
    {
      // XXX later
    }
    return 0;
  } while (0);
  return -1;
}

// alloc

NdbImportCsv::Alloc::Alloc()
{
  m_alloc_data_cnt = 0;
  m_alloc_field_cnt = 0;
  m_alloc_line_cnt = 0;
  m_free_data_cnt = 0;
  m_free_field_cnt = 0;
  m_free_line_cnt = 0;
}

NdbImportCsv::Data*
NdbImportCsv::Alloc::alloc_data()
{
  Data* data =  m_data_free.pop_front();
  if (data == 0)
    data = new Data;
  else
    new (data) Data;
  m_alloc_data_cnt++;
  return data;
}

void
NdbImportCsv::Alloc::free_data_list(DataList& data_list)
{
  m_free_data_cnt += data_list.cnt();
  m_data_free.push_back_from(data_list);
}

NdbImportCsv::Field*
NdbImportCsv::Alloc::alloc_field()
{
  Field* field = m_field_free.pop_front();
  if (field == 0)
    field = new Field;
  else
    new (field) Field;
  m_alloc_field_cnt++;
  return field;
}

void
NdbImportCsv::Alloc::free_field_list(FieldList& field_list)
{
  Field* field = field_list.front();
  while (field != 0)
  {
    free_data_list(field->m_data_list);
    field = field->next();
  }
  m_free_field_cnt += field_list.cnt();
  m_field_free.push_back_from(field_list);
}

NdbImportCsv::Line*
NdbImportCsv::Alloc::alloc_line()
{
  Line* line = m_line_free.pop_front();
  if (line == 0)
    line = new Line;
  else
    new (line) Line;
  m_alloc_line_cnt++;
  return line;
}

void
NdbImportCsv::Alloc::free_line_list(LineList& line_list)
{
  Line* line = line_list.front();
  while (line != 0)
  {
    free_field_list(line->m_field_list);
    line = line->next();
  }
  m_free_line_cnt += line_list.cnt();
  m_line_free.push_back_from(line_list);
}

bool
NdbImportCsv::Alloc::balanced()
{
  return
    m_alloc_data_cnt == m_free_data_cnt &&
    m_alloc_field_cnt == m_free_field_cnt &&
    m_alloc_line_cnt == m_free_line_cnt;
}

// input

NdbImportCsv::Input::Input(NdbImportCsv& csv,
                           const char* name,
                           const Spec& spec,
                           const Table& table,
                           Buf& buf,
                           RowList& rows_out,
                           RowList& rows_reject,
                           RowMap& rowmap_in,
                           Stats& stats) :
  m_csv(csv),
  m_util(m_csv.m_util),
  m_name(name),
  m_spec(spec),
  m_table(table),
  m_buf(buf),
  m_rows_out(rows_out),
  m_rows_reject(rows_reject),
  m_rowmap_in(rowmap_in)
{
  m_parse = new Parse(*this);
  m_eval = new Eval(*this);
  m_rows.set_stats(m_util.c_stats, Name(m_name, "rows"));
  m_startpos = 0;
  m_startlineno = 0;
  m_ignore_lines = 0;
}

NdbImportCsv::Input::~Input()
{
  delete m_parse;
  delete m_eval;
}

void
NdbImportCsv::Input::do_init()
{
  const Opt& opt = m_util.c_opt;
  m_ignore_lines = opt.m_ignore_lines;
  m_parse->do_init();
  m_eval->do_init();
}

/*
 * Adjust counters at resume.  Argument is first range in old
 * rowmap.  Input file seek is done by caller.
 */
void
NdbImportCsv::Input::do_resume(Range range_in)
{
  m_startpos = range_in.m_endpos;
  m_startlineno = range_in.m_end + m_ignore_lines;
}

void
NdbImportCsv::Input::do_parse()
{
#ifdef VM_TRACE
  NdbImportCsv_yydebug = (m_util.c_opt.m_log_level >= 4);
#endif
  m_parse->do_parse();
#ifdef VM_TRACE
  NdbImportCsv_yydebug = 0;
#endif
}

void
NdbImportCsv::Input::do_eval()
{
  m_eval->do_eval();
}

void
NdbImportCsv::Input::do_send(uint& curr, uint& left)
{
  const Opt& opt = m_util.c_opt;
  RowList& rows_out = m_rows_out;       // shared
  rows_out.lock();
  curr = m_rows.cnt();
  RowCtl ctl(opt.m_rowswait);
  m_rows.pop_front_to(rows_out, ctl);
  left = m_rows.cnt();
  if (rows_out.m_foe)
  {
    log1("consumer has stopped");
    m_util.set_error_gen(m_error, __LINE__, "consumer has stopped");
  }
  rows_out.unlock();
}

void
NdbImportCsv::Input::do_movetail(Input& input2)
{
  Buf& buf1 = m_buf;
  Buf& buf2 = input2.m_buf;
  require(buf1.movetail(buf2) == 0);
  buf1.m_pos = buf1.m_len;      // keep pos within new len
  input2.m_startpos = m_startpos + buf1.m_len;
  input2.m_startlineno = m_startlineno + m_line_list.cnt();
  log1("movetail " <<
      " src: " << buf1 <<
      " dst: " << buf2 <<
      " startpos: " << m_startpos << "->" << input2.m_startpos <<
      " startline: " << m_startlineno << "->" << input2.m_startlineno);
}

void
NdbImportCsv::Input::reject_line(const Line* line,
                                 const Field* field,
                                 const Error& error)
{
  const Opt& opt = m_util.c_opt;
  RowList& rows_reject = m_rows_reject;
  rows_reject.lock();
  // write reject row first
  const Table& table = m_util.c_reject_table;
  Row* rejectrow = m_util.alloc_row(table);
  rejectrow->m_rowid = m_startlineno + line->m_lineno - m_ignore_lines;
  rejectrow->m_linenr = 1 + m_startlineno + line->m_lineno;
  rejectrow->m_startpos = m_startpos + line->m_pos;
  rejectrow->m_endpos = m_startpos + line->m_end;
  const Buf& buf = m_buf;
  const uchar* bufdata = &buf.m_data[buf.m_start];
  const char* bufdatac = (const char*)bufdata;
  const char* reject = &bufdatac[line->m_pos];
  uint32 rejectlen = line->m_end - line->m_pos;
  m_util.set_reject_row(rejectrow, Inval_uint32, error, reject, rejectlen);
  require(rows_reject.push_back(rejectrow));
  // error if rejects exceeded
  if (rows_reject.totcnt() > opt.m_rejects)
  {
    m_util.set_error_data(m_error, __LINE__, 0,
                          "reject limit %u exceeded", opt.m_rejects);
  }
  rows_reject.unlock();
}

void
NdbImportCsv::Input::print(NdbOut& out)
{
  typedef NdbImportCsv::Line Line;
  typedef NdbImportCsv::Field Field;
  const NdbImportCsv::Buf& buf = m_buf;
  const uchar* bufdata = &buf.m_data[buf.m_start];
  const char* bufdatac = (const char*)bufdata;
  LineList& line_list = m_line_list;
  out << "input:" << endl;
  out << "len=" << m_buf.m_len << endl;
  uint n = strlen(bufdatac);
  if (n != 0 && bufdatac[n-1] == '\n')
    out << bufdatac;
  else
    out << bufdatac << "\\c" << endl;
  out << "linecnt=" << line_list.cnt();
  Line* line = line_list.front();
  while (line != 0)
  {
    out << endl;
    out << "lineno=" << line->m_lineno;
    out << " pos=" << line->m_pos;
    out << " length=" << line->m_end - line->m_pos;
    out << " fieldcnt=" << line->m_field_list.cnt();
    Field* field = line->m_field_list.front();
    while (field != 0)
    {
      out << endl;
      uint pos = field->m_pos;
      uint end = field->m_end;
      uint pack_pos = field->m_pack_pos;
      uint pack_end = field->m_pack_end;
      char b[4096];
      snprintf(b, sizeof(b), "%.*s", pack_end - pack_pos, &bufdatac[pack_pos]);
      out << "fieldno=" << field->m_fieldno;
      out << " pos=" << pos;
      out << " length=" << end - pos;
      out << " pack_pos=" << pack_pos;
      out << " pack_length=" << pack_end - pack_pos;
      out << " null=" << field->m_null;
      out << " data=" << b;
      field = field->next();
    }
    line = line->next();
  }
  out << endl;
  require(false);
}

NdbOut&
operator<<(NdbOut& out, const NdbImportCsv::Input& input)
{
  out << input.m_name;
  out << " len=" << input.m_buf.m_len;
  out << " linecnt=" << input.m_line_list.cnt();
  return out;
}

// parse

NdbImportCsv::Parse::Parse(Input& input) :
  m_input(input),
  m_csv(m_input.m_csv),
  m_util(m_input.m_util),
  m_error(m_input.m_error)
{
  m_stacktop = 0;
  m_state[m_stacktop] = State_plain;
  m_last_token = 0;
}

void
NdbImportCsv::Parse::do_init()
{
  log1("do_init");
  const Spec& spec = m_input.m_spec;
  for (int s = 0; s < g_statecnt; s++)
  {
    /*
     * NUL byte 0x00 can be represented as NUL, \NUL, or \0
     * where the first two contain a literal NUL byte 0x00.
     * The T_NUL token is used to avoid branching in the normal
     * case where the third printable format is used.
     */
    m_trans[s][0] = T_NUL;
  }
  for (uint u = 1; u < g_bytecnt; u++)
  {
    m_trans[State_plain][u] = T_DATA;
    m_trans[State_quote][u] = T_DATA;
    m_trans[State_escape][u] = T_BYTE;
  }
  {
    const uchar* p = spec.m_fields_terminated_by;
    const uint len = spec.m_fields_terminated_by_len;
    require(p != 0 && p[0] != 0 && len == strlen((const char*)p));
    uint u = p[0];
    // avoid parse-time branch in the common case
    m_trans[State_plain][u] = len == 1 ? T_FIELDSEP : T_FIELDSEP2;
    m_trans[State_quote][u] = T_DATA;
    m_trans[State_escape][u] = T_BYTE;
  }
  {
    const uchar* p = spec.m_fields_optionally_enclosed_by;
    if (p != 0 && p[0] != 0)
    {
      require(p[1] == 0);
      uint u = p[0];
      m_trans[State_plain][u] = T_QUOTE;
      m_trans[State_quote][u] = T_QUOTEQUOTE;
      m_trans[State_escape][u] = T_BYTE;
    }
  }
  {
    const uchar* p = spec.m_fields_escaped_by;
    require(p != 0);
    if (p[0] != 0)
    {
      require(p[1] == 0);
      uint u = p[0];
      m_trans[State_plain][u] = T_ESCAPE;
      m_trans[State_quote][u] = T_ESCAPE;
      m_trans[State_escape][u] = T_BYTE;
    }
  }
  {
    const uchar* p = spec.m_lines_terminated_by;
    const uint len = spec.m_lines_terminated_by_len;
    require(p != 0 && p[0] != 0 && len == strlen((const char*)p));
    uint u = p[0];
    // avoid parse-time branch in the common case
    m_trans[State_plain][u] = len == 1 ? T_LINEEND : T_LINEEND2;
    m_trans[State_quote][u] = T_DATA;
    m_trans[State_escape][u] = T_BYTE;
  }
  // escape (\N is special)
  {
    const uchar* p = spec.m_fields_escaped_by;
    for (uint u = 0; u < g_bytecnt; u++)
      m_escapes[u] = u;
    require(p != 0);
    if (p[0] != 0)
    {
      m_escapes[(int)'0'] = 000;  // NUL
      m_escapes[(int)'b'] = 010;  // BS
      m_escapes[(int)'n'] = 012;  // NL
      m_escapes[(int)'r'] = 015;  // CR
      m_escapes[(int)'t'] = 011;  // TAB
      m_escapes[(int)'Z'] = 032;  // ^Z
    }
  }
}

void
NdbImportCsv::Parse::push_state(State state)
{
  require(m_stacktop + 1 < g_stackmax);
  m_state[++m_stacktop] = state;
  log3("push " << g_str_state(m_state[m_stacktop-1])
       << "->" << g_str_state(m_state[m_stacktop]));
}

void
NdbImportCsv::Parse::pop_state()
{
  require(m_stacktop > 0);
  m_stacktop--;
  log3("pop " << g_str_state(m_state[m_stacktop])
       << "<-" << g_str_state(m_state[m_stacktop+1]));
}

void
NdbImportCsv::Parse::do_parse()
{
  log2("do_parse");
  m_input.free_line_list(m_input.m_line_list);
  m_input.free_line_list(m_line_list);
  m_input.free_field_list(m_field_list);
  m_input.free_data_list(m_data_list);
  m_stacktop = 0;
  m_state[m_stacktop] = State_plain;
  Buf& buf = m_input.m_buf;
  buf.m_pos = 0;
  int ret = 0;
  if (buf.m_len != 0)
    ret = NdbImportCsv_yyparse(*this);
  log1("parse ret=" << ret);
  if (ret == 0)
  {
    require(m_last_token == 0);
    buf.m_tail = buf.m_len;
  }
  else if (!m_util.has_error())
  {
    // last parsed line
    Line* line = m_line_list.back();
    if (line != 0)
    {
      buf.m_tail = line->m_end;
      m_input.m_line_list.push_back_from(m_line_list);
      m_input.free_field_list(m_field_list);
      m_input.free_data_list(m_data_list);
    }
    else
    {
      uint64 abspos = m_input.m_startpos;
      uint64 abslineno = 1 + m_input.m_startlineno;
      m_util.set_error_data(m_error, __LINE__, 0,
                            "parse error at line=%llu: pos=%llu:"
                            " CSV page contains no complete record"
                            " (buffer too small"
                            " or missing last line terminator)",
                            abslineno, abspos);
      return;
    }
  }
  /*
   * Pack data parts into fields.  Modifies buf data and cannot
   * be done before accepted lines and fields are known.  Otherwise
   * movetail() passes garbage to next worker.
   */
  {
    Line* line = m_input.m_line_list.front();
    while (line != 0)
    {
      Field* field = line->m_field_list.front();
      while (field != 0)
      {
        if (field->m_data_list.cnt() != 0)
          pack_field(field);
        field = field->next();
      }
      line = line->next();
    }
  }
}

int
NdbImportCsv::Parse::do_lex(YYSTYPE* lvalp)
{
  log3("do_lex");
  const Spec& spec = m_input.m_spec;
  Buf& buf = m_input.m_buf;
  const uchar* bufdata = &buf.m_data[buf.m_start];
  State state = m_state[m_stacktop];
  const int* trans = m_trans[state];
  const uint pos = buf.m_pos;
  uint len = 0;
  uint end = pos;
  uint u = bufdata[pos];
  int token = trans[u];
  switch (token) {
  case T_FIELDSEP:
    len = 1;
    end += len;
    break;
  case T_FIELDSEP2:
    len = spec.m_fields_terminated_by_len;
    if (len <= buf.m_len - buf.m_pos &&
        memcmp(&bufdata[pos], spec.m_fields_terminated_by, len) == 0)
    {
      end += len;
      token = T_FIELDSEP;
      break;
    }
    len = 1;
    end += len;
    token = T_DATA;
    break;
  case T_QUOTE:
    push_state(State_quote);
    require(spec.m_fields_enclosed_by_len == 1);
    len = 1;
    end += len;
    break;
  case T_QUOTEQUOTE:
    require(spec.m_fields_enclosed_by_len == 1);
    if (bufdata[pos + 1] == u)
    {
      token = T_DATA;
      len = 1;
      end += 2;
      break;
    }
    token = T_QUOTE;
    len = 1;
    end += len;
    pop_state();
    break;
  case T_ESCAPE:
    push_state(State_escape);
    require(spec.m_fields_escaped_by_len == 1);
    len = 1;
    end += len;
    break;
  case T_LINEEND:
    len = 1;
    end += len;
    break;
  case T_LINEEND2:
    len = spec.m_lines_terminated_by_len;
    if (len <= buf.m_len - buf.m_pos &&
        memcmp(&bufdata[pos], spec.m_lines_terminated_by, len) == 0)
    {
      end += len;
      token = T_LINEEND;
      break;
    }
    len = 1;
    end += len;
    token = T_DATA;
    break;
  case T_DATA:
    do
    {
      len++;
      u = bufdata[pos + len];
    } while (trans[u] == T_DATA);
    end += len;
    break;
  case T_BYTE:
    len = 1;
    end += len;
    pop_state();
    break;
  case T_NUL:
    if (buf.m_pos == buf.m_len)
    {
      token = 0;
      break;
    }
    if (m_state[m_stacktop] != State_escape)
      token = T_DATA;
    else
    {
      token = T_BYTE;
      pop_state();
    }
    len = 1;
    end += len;
    break;
  }
  Chunk chunk;
  chunk.m_pos = pos;
  chunk.m_len = len;
  chunk.m_end = end;
  log3("do_lex: token=" << token <<
        " pos=" << chunk.m_pos << " len=" << len << " end=" << end);
  buf.m_pos = end;
  lvalp->m_chunk = chunk;
  m_last_token = token;
  return token;
}

void
NdbImportCsv::Parse::do_error(const char* msg)
{
  if (m_last_token != 0)
  {
    const Buf& buf = m_input.m_buf;
    log2("parse error at buf:" << buf);
    uint64 abspos = m_input.m_startpos + buf.m_pos;
    uint64 abslineno = m_input.m_startlineno + m_line_list.cnt();
    m_util.set_error_data(m_error, __LINE__, 0,
                          "parse error at line=%llu: pos=%llu: %s",
                          abslineno, abspos, msg);
  }
}

void
NdbImportCsv::Parse::pack_field(Field* field)
{
  Buf& buf = m_input.m_buf;
  uchar* bufdata = &buf.m_data[buf.m_start];
  DataList& data_list = field->m_data_list;
  Data* data = data_list.front();
  require(data != 0);
  // if field is exactly "\N" then it becomes NULL
  if (data->next() == 0 &&
      data->m_escape &&
      bufdata[data->m_pos] == 'N')
  {
    field->m_pack_pos = Inval_uint;
    field->m_pack_end = Inval_uint;
    field->m_null = true;
    return;
  }
  // handle multiple pieces and normal escapes
  uint pack_pos = data->m_pos;
  uint pack_end = pack_pos;
  while (data != 0)
  {
    uint len = data->m_len;
    memmove(&bufdata[pack_end], &bufdata[data->m_pos], len);
    if (data->m_escape)
    {
      require(len == 1);
      bufdata[pack_end] = m_escapes[bufdata[pack_end]];
    }
    pack_end += len;
    data = data->next();
  }
  field->m_pack_pos = pack_pos;
  field->m_pack_end = pack_end;
  field->m_null = false;
}

NdbOut&
operator<<(NdbOut& out, const NdbImportCsv::Parse& parse)
{
  const NdbImportCsv::Buf& buf = parse.m_input.m_buf;
  out << "parse " << parse.m_input.m_name;
  NdbImportCsv::Parse::State state = parse.m_state[parse.m_stacktop];
  out << " [" << NdbImportCsv::g_str_state(state) << "]";
  if (buf.m_len != 0)
  {
    const uchar* bufdata = &buf.m_data[buf.m_start];
    char chr[20];
    int c = bufdata[buf.m_pos];
    if (isascii(c) && isprint(c))
      sprintf(chr, "%c", c);
    else if (c == '\n')
      sprintf(chr, "%s", "\\n");
    else
      sprintf(chr, "0x%02x", c);
    out << " len=" << buf.m_len << " pos=" << buf.m_pos << " chr=" << chr;
  }
  return out;
}

const char*
NdbImportCsv::g_str_state(Parse::State state)
{
  const char* str = 0;
  switch (state) {
  case Parse::State_plain:
    str = "plain";
    break;
  case Parse::State_quote:
    str = "quote";
    break;
  case Parse::State_escape:
    str = "escape";
    break;
  }
  require(str != 0);
  return str;
}

// eval

NdbImportCsv::Regex::Regex(NdbImportUtil& util,
                           const char* pattern,
                           uint nsub) :
  m_util(util),
  m_pattern(pattern),
  m_nsub(nsub)
{
  const CHARSET_INFO* cs = get_charset_by_name("latin1_bin", MYF(0));
  require(cs != 0);
  int cflags = MY_REG_EXTENDED;
  int ret = my_regcomp(&m_regex, m_pattern, cflags, cs);
  if (ret != 0)
  {
    char msg[256];
    my_regerror(ret, &m_regex, msg, sizeof(msg));
    m_util.c_opt.m_log_level = 1;
    log1("abort: regcomp error " << ret << ": " << msg);
    require(false);
  }
  require(m_regex.re_nsub == m_nsub);
  m_subs = new my_regmatch_t[1 + m_nsub];
}

NdbImportCsv::Regex::~Regex()
{
  my_regfree(&m_regex);
  delete [] m_subs;
}

bool
NdbImportCsv::Regex::match(const char* string)
{
  int eflags = 0;
  int ret = my_regexec(&m_regex, string, 1 + m_nsub, m_subs, eflags);
  if (ret != 0)
  {
    if (ret != MY_REG_NOMATCH)
    {
      char msg[256];
      my_regerror(ret, &m_regex, msg, sizeof(msg));
      m_util.c_opt.m_log_level = 1;
      log1("abort: regexec error " << ret << ": " << msg);
      require(false);
    }
  }
  return (ret == 0);
}

NdbImportCsv::Eval::Eval(Input& input) :
  m_input(input),
  m_csv(m_input.m_csv),
  m_util(m_input.m_util),
  m_error(m_input.m_error)
{
}

NdbImportCsv::Eval::~Eval()
{
}

void
NdbImportCsv::Eval::do_init()
{
}

void
NdbImportCsv::Eval::do_eval()
{
  const Opt& opt = m_util.c_opt;
  const Table& table = m_input.m_table;
  LineList& line_list = m_input.m_line_list;
  Line* line = line_list.front();
  RowList rows_chunk;
  while (line != 0)
  {
    const uint64 ignore_lines = m_input.m_ignore_lines;
    const uint64 lineno = m_input.m_startlineno + line->m_lineno;
    if (lineno < ignore_lines)
    {
      line = line->next();
      continue;
    }
    if (opt.m_resume)
    {
      RowMap& rowmap_in = m_input.m_rowmap_in;
      const uint64 rowid = lineno - ignore_lines;
      if (!rowmap_in.empty())
      {
        bool found = rowmap_in.remove(rowid);
        if (found)
        {
          line = line->next();
          log1("skip old rowid: " << rowid);
          continue;
        }
      }
    }
    if (rows_chunk.cnt() == 0)
    {
      require(line->m_lineno < line_list.cnt());
      uint cnt = line_list.cnt() - line->m_lineno;
      if (cnt > opt.m_alloc_chunk)
        cnt = opt.m_alloc_chunk;
      m_util.alloc_rows(table, cnt, rows_chunk);
    }
    Row* row = rows_chunk.pop_front();
    eval_line(row, line);
    // stop loading if error
    if (m_input.has_error())
    {
      break;
    }
    line = line->next();
  }
  m_input.free_line_list(m_input.m_line_list);
}

void
NdbImportCsv::Eval::eval_line(Row* row, Line* line)
{
  const Table& table = m_input.m_table;
  const Attrs& attrs = table.m_attrs;
  const uint attrcnt = attrs.size();
  const uint64 lineno = m_input.m_startlineno + line->m_lineno;
  const uint64 linenr = 1 + lineno;
  row->m_rowid = lineno - m_input.m_ignore_lines;
  row->m_linenr = linenr;
  row->m_startpos = m_input.m_startpos + line->m_pos;
  row->m_endpos = m_input.m_startpos + line->m_end;
  const uint fieldcnt = line->m_field_list.cnt();
  const uint has_hidden_pk = (uint)table.m_has_hidden_pk;
  const uint expect_attrcnt = attrcnt - has_hidden_pk;
  Error error;  // local error
  do
  {
    if (fieldcnt < expect_attrcnt)
    {
      m_util.set_error_data(
        error, __LINE__, 0,
        "line %llu: too few fields (%u < %u)",
        linenr, fieldcnt, attrcnt);
      break;
    }
    if (fieldcnt > expect_attrcnt)
    {
      m_util.set_error_data(
        error, __LINE__, 0,
        "line %llu: too many fields (%u > %u)",
        linenr, fieldcnt, attrcnt);
      break;
    }
  } while (0);
  if (m_util.has_error(error))
  {
    m_input.reject_line(line, (Field*)0, error);
    line->m_reject = true;
  }
  Field* field = line->m_field_list.front();
  for (uint n = 0; n < fieldcnt; n++)
  {
    if (line->m_reject) // wrong field count or eval error
      break;
    require(field != 0);
    require(field->m_fieldno == n);
    if (!field->m_null)
      eval_field(row, line, field);
    else
      eval_null(row, line, field);
    field = field->next();
  }
  if (!line->m_reject)
  {
    require(field == 0);
  }
  if (has_hidden_pk)
  {
    /*
     * CSV has no access to Ndb (in fact there may not be any Ndb
     * object e.g. in CSV input -> CSV output).  Any autoincrement
     * value for hidden pk is set later in RelayOpWorker.  Fill in
     * some dummy value to not leave uninitialized data.
     */
    const Attr& attr = attrs[attrcnt - 1];
    require(attr.m_type == NdbDictionary::Column::Bigunsigned);
    uint64 val = Inval_uint64;
    attr.set_value(row, &val, 8);
  }
  if (!line->m_reject)
    m_input.m_rows.push_back(row);
}

/*
 * Parse some fields.  Using my_regex was impossibly slow so here
 * we do a CS101 "turn string into number".  Digits must be ascii
 * digits.  Bengalese numbers are not supported.
 */

struct Ndb_import_csv_error {
  enum Error_code {
    No_error = 0,
    Format_error = 1,
    Value_error = 2,    // but DBTUP should be final arbiter
    Internal_error = 3
  };
  static const int error_code_count = Internal_error + 1;
  int error_code;
  const char* error_text;
  int error_line;
};

static const Ndb_import_csv_error
ndb_import_csv_error[Ndb_import_csv_error::error_code_count] = {
  { Ndb_import_csv_error::No_error, "no error", 0 },
  { Ndb_import_csv_error::Format_error, "format error", 0 },
  { Ndb_import_csv_error::Value_error, "value error", 0 },
  { Ndb_import_csv_error::Internal_error, "internal error", 0 }
};

static void
ndb_import_csv_decimal_error(int err,
                             Ndb_import_csv_error& csv_error)
{
  switch (err) {
  case E_DEC_OK:
    csv_error = ndb_import_csv_error[Ndb_import_csv_error::No_error];
    break;
  case E_DEC_TRUNCATED:
  case E_DEC_OVERFLOW:
    csv_error = ndb_import_csv_error[Ndb_import_csv_error::Value_error];
    break;
  case E_DEC_BAD_NUM:
    csv_error = ndb_import_csv_error[Ndb_import_csv_error::Format_error];
    break;
  case E_DEC_OOM:
  case E_DEC_BAD_PREC:
  case E_DEC_BAD_SCALE:
  default:
    csv_error = ndb_import_csv_error[Ndb_import_csv_error::Internal_error];
    break;
  }
}

static bool
ndb_import_csv_parse_decimal(const NdbImportCsv::Attr& attr,
                             bool is_unsigned,
                             const char* datac, uint length,
                             uchar* val, uint val_len,
                             Ndb_import_csv_error& csv_error)
{
#if 0
  // [-+]ddd.ff
  "^"
  "([-+])*"                                   // 1:sign
  "([[:digit:]]*)?"                           // 2:ddd
  "(.)?"                                      // 3:.
  "([[:digit:]]*)?"                           // 4:ff
  "$"
#endif
  // sign
  const char* p = datac;
  const char* q = p;
  if (!is_unsigned)
    while (*p == '+' || *p == '-')
      p++;
  else
    while (*p == '+')
      p++;
  q = p;
  // decimal_str2bin does not check string end so parse here
  uint digits = 0;
  while (isdigit(*p))
    p++;
  digits += p - q;
  q = p;
  if (*p == '.')
  {
    q = ++p;
    while (isdigit(*p))
      p++;
    digits += p - q;
  }
  if (*p != 0)
  {
    csv_error = ndb_import_csv_error[Ndb_import_csv_error::Format_error];
    csv_error.error_line = __LINE__;
    return false;
  }
  if (digits == 0)
  {
    // single "." is not valid decimal
    csv_error = ndb_import_csv_error[Ndb_import_csv_error::Format_error];
    csv_error.error_line = __LINE__;
    return false;
  }
  int err;
  err = decimal_str2bin(datac, length,
                        attr.m_precision, attr.m_scale,
                        val, val_len);
  if (err != 0)
  {
    ndb_import_csv_decimal_error(err, csv_error);
    csv_error.error_line = __LINE__;
    return false;
  }
  return true;
}

static bool
ndb_import_csv_parse_year(const NdbImportCsv::Attr& attr,
                          const char* datac,
                          NdbSqlUtil::Year& s,
                          Ndb_import_csv_error& csv_error)
{
#if 0
  // yyyy
  "^"
  "([[:digit:]]{4}|[[:digit:]]{2})"           // 1:yyyy
  "$"
#endif
  csv_error = ndb_import_csv_error[Ndb_import_csv_error::No_error];
  s.year = 0;
  const char* p = datac;
  const char* q = p;
  while (isdigit(*p) && p - q < 4)
    s.year = 10 * s.year + (*p++ - '0');
  if (p - q == 4)
    ;
  else if (p - q == 2)
  {
    if (s.year >= 70)
      s.year += 1900;
    else
      s.year += 2000;
  }
  else
  {
    csv_error = ndb_import_csv_error[Ndb_import_csv_error::Format_error];
    csv_error.error_line = __LINE__;
    return false;
  }
  return true;
}

static bool
ndb_import_csv_parse_date(const NdbImportCsv::Attr& attr,
                          const char* datac,
                          NdbSqlUtil::Date& s,
                          Ndb_import_csv_error& csv_error)
{
#if 0
  // yyyy-mm-dd
  "^"
  "([[:digit:]]{4}|[[:digit:]]{2})"           // 1:yyyy
  "("                                         // 2:
  "[[:punct:]]+"
  "([[:digit:]]{1,2})"                        // 3:mm
  "[[:punct:]]+"
  "([[:digit:]]{1,2})"                        // 4:dd
  "|"
  "([[:digit:]]{2})"                          // 5:mm
  "([[:digit:]]{2})"                          // 6:dd
  ")"
  "$"
#endif
  csv_error = ndb_import_csv_error[Ndb_import_csv_error::No_error];
  s.year = s.month = s.day = 0;
  const char* p = datac;
  const char* q = p;
  while (isdigit(*p) && p - q < 4)
    s.year = 10 * s.year + (*p++ - '0');
  if (p - q == 4)
    ;
  else if (p - q == 2)
  {
    if (s.year >= 70)
      s.year += 1900;
    else
      s.year += 2000;
  }
  else
  {
    csv_error = ndb_import_csv_error[Ndb_import_csv_error::Format_error];
    csv_error.error_line = __LINE__;
    return false;
  }
  q = p;
  // separator vs non-separator variant
  if (ispunct(*p))
  {
    // anything goes
    while (ispunct(*p))
      p++;
    q = p;
    // month
    while (isdigit(*p) && p - q < 2)
      s.month = 10 * s.month + (*p++ - '0');
    if (p - q > 0)
      ;
    else
    {
      csv_error = ndb_import_csv_error[Ndb_import_csv_error::Format_error];
      csv_error.error_line = __LINE__;
      return false;
    }
    q = p;
    if (ispunct(*p))
      ;
    else
    {
      csv_error = ndb_import_csv_error[Ndb_import_csv_error::Format_error];
      csv_error.error_line = __LINE__;
      return false;
    }
    // anything goes
    while (ispunct(*p))
      p++;
    q = p;
    // day
    while (isdigit(*p) && p - q < 2)
      s.day = 10 * s.day + (*p++ - '0');
    if (p - q > 0)
      ;
    else
    {
      csv_error = ndb_import_csv_error[Ndb_import_csv_error::Format_error];
      csv_error.error_line = __LINE__;
      return false;
    }
    q = p;
  }
  else
  {
    // month
    while (isdigit(*p) && p - q < 2)
      s.month = 10 * s.month + (*p++ - '0');
    if (p - q == 2)
      ;
    else
    {
      csv_error = ndb_import_csv_error[Ndb_import_csv_error::Format_error];
      csv_error.error_line = __LINE__;
      return false;
    }
    q = p;
    // day
    while (isdigit(*p) && p - q < 2)
      s.day = 10 * s.day + (*p++ - '0');
    if (p - q == 2)
      ;
    else
    {
      csv_error = ndb_import_csv_error[Ndb_import_csv_error::Format_error];
      csv_error.error_line = __LINE__;
      return false;
    }
    q = p;
  }
  return true;
}

static bool
ndb_import_csv_parse_time2(const NdbImportCsv::Attr& attr,
                           const char* datac,
                           NdbSqlUtil::Time2& s,
                           Ndb_import_csv_error& csv_error)
{
#if 0
  // dd hh:mm:ss.ffffff
  "^"
  "(([[:digit:]]+)[[:space:]]+)?"             // 1:dd 2: ***NOTYET***
  "("                                         // 3:
  "([[:digit:]]{1,2})"                        // 4:hh
  "[:]"
  "([[:digit:]]{1,2})"                        // 5:mm
  "[:]"
  "([[:digit:]]{1,2})"                        // 6:ss
  "|"
  "([[:digit:]]{2})"                          // 7:hh
  "([[:digit:]]{2})"                          // 8:mm
  "([[:digit:]]{2})"                          // 9:ss
  ")"
  "(\\.([[:digit:]]*))?"                      // 10: 11:ffffff
  "$"
#endif
  csv_error = ndb_import_csv_error[Ndb_import_csv_error::No_error];
  s.sign = 1;
  s.interval = 0;
  s.hour = s.minute = s.second = 0;
  s.fraction = 0;
  const char* p = datac;
  const char* q = p;
  // hour
  while (isdigit(*p) && p - q < 2)
    s.hour = 10 * s.hour + (*p++ - '0');
  if (p - q == 1 || p - q == 2)
    ;
  else
  {
    csv_error = ndb_import_csv_error[Ndb_import_csv_error::Format_error];
    csv_error.error_line = __LINE__;
    return false;
  }
  q = p;
  // separator vs non-separator variant
  if (*p == ':')
  {
    q = ++p;
    // minute
    while (isdigit(*p))
      s.minute = 10 * s.minute + (*p++ - '0');
    if (p - q == 1 || p - q == 2)
      ;
    else
    {
      csv_error = ndb_import_csv_error[Ndb_import_csv_error::Format_error];
      csv_error.error_line = __LINE__;
      return false;
    }
    q = p;
    if (*p == ':')
      q = ++p;
    else
    {
      csv_error = ndb_import_csv_error[Ndb_import_csv_error::Format_error];
      csv_error.error_line = __LINE__;
      return false;
    }
    while (isdigit(*p))
      s.second = 10 * s.second + (*p++ - '0');
    if (p - q == 1 || p - q == 2)
      ;
    else
    {
      csv_error = ndb_import_csv_error[Ndb_import_csv_error::Format_error];
      csv_error.error_line = __LINE__;
      return false;
    }
    q = p;
  }
  else
  {
    while (isdigit(*p) && p - q < 2)
      s.minute = 10 * s.minute + (*p++ - '0');
    if (p - q == 2)
      ;
    else
    {
      csv_error = ndb_import_csv_error[Ndb_import_csv_error::Format_error];
      csv_error.error_line = __LINE__;
      return false;
    }
    q = p;
    while (isdigit(*p) && p - q < 2)
      s.second = 10 * s.second + (*p++ - '0');
    if (p - q == 2)
      ;
    else
    {
      csv_error = ndb_import_csv_error[Ndb_import_csv_error::Format_error];
      csv_error.error_line = __LINE__;
      return false;
    }
    q = p;
  }
  // fraction point (optional)
  if (*p != 0)
  {
    if (*p == '.')
      p++;
    if (p - q == 1)
      ;
    else
    {
      csv_error = ndb_import_csv_error[Ndb_import_csv_error::Format_error];
      csv_error.error_line = __LINE__;
      return false;
    }
    q = p;
    // fraction value (optional)
    while (isdigit(*p))
      s.fraction = 10 * s.fraction + (*p++ - '0');
    if (p - q <= 6)
    {
      uint n = p - q;
      while (n++ < attr.m_precision)
        s.fraction *= 10;
    }
    else
    {
      csv_error = ndb_import_csv_error[Ndb_import_csv_error::Format_error];
      csv_error.error_line = __LINE__;
      return false;
    }
  }
  return true;
}

static bool
ndb_import_csv_parse_datetime2(const NdbImportCsv::Attr& attr,
                               const char* datac,
                               NdbSqlUtil::Datetime2& s,
                               Ndb_import_csv_error& csv_error)
{
#if 0
  yyyy-mm-dd/hh:mm:ss.ffffff
  "^"
  "([[:digit:]]{4}|[[:digit:]]{2})"           // 1:yyyy
  "[[:punct:]]+"
  "([[:digit:]]{1,2})"                        // 2:mm
  "[[:punct:]]+"
  "([[:digit:]]{1,2})"                        // 3:dd
  "(T|[[:space:]]+|[[:punct:]]+)"             // 4:
  "([[:digit:]]{1,2})"                        // 5:hh
  "[[:punct:]]+"
  "([[:digit:]]{1,2})"                        // 6:mm
  "[[:punct:]]+"
  "([[:digit:]]{1,2})"                        // 7:ss
  "(\\.([[:digit:]]*))?"                      // 8: 9:ffffff
  "$"
#endif
  csv_error = ndb_import_csv_error[Ndb_import_csv_error::No_error];
  s.sign = 1;
  s.year = s.month = s.day = 0;
  s.hour = s.minute = s.second = 0;
  s.fraction = 0;
  const char* p = datac;
  const char* q = p;
  // year
  while (isdigit(*p))
    s.year = 10 * s.year + (*p++ - '0');
  if (p - q == 4)
    ;
  else if (p - q == 2)
  {
    if (s.year >= 70)
      s.year += 1900;
    else
      s.year += 2000;
  }
  else
  {
    csv_error = ndb_import_csv_error[Ndb_import_csv_error::Format_error];
    csv_error.error_line = __LINE__;
    return false;
  }
  q = p;
  // separator
  while (ispunct(*p))
    p++;
  if (p - q != 0)
    ;
  else
  {
    csv_error = ndb_import_csv_error[Ndb_import_csv_error::Format_error];
    csv_error.error_line = __LINE__;
    return false;
  }
  q = p;
  // month
  while (isdigit(*p))
    s.month = 10 * s.month + (*p++ - '0');
  if (p - q == 1 || p - q == 2)
    ;
  else
  {
    csv_error = ndb_import_csv_error[Ndb_import_csv_error::Format_error];
    csv_error.error_line = __LINE__;
    return false;
  }
  // separator
  while (ispunct(*p))
    p++;
  if (p - q != 0)
    ;
  else
  {
    csv_error = ndb_import_csv_error[Ndb_import_csv_error::Format_error];
    csv_error.error_line = __LINE__;
    return false;
  }
  q = p;
  // day
  while (isdigit(*p))
    s.day = 10 * s.day + (*p++ - '0');
  if (p - q == 1 || p - q == 2)
    ;
  else
  {
    csv_error = ndb_import_csv_error[Ndb_import_csv_error::Format_error];
    csv_error.error_line = __LINE__;
    return false;
  }
  q = p;
  // separator
  if (*p == 'T')
    p++;
  else if (isspace(*p))
  {
    while (isspace(*p))
      p++;
  }
  else if (ispunct(*p))
  {
    while (ispunct(*p))
      p++;
  }
  if (p - q != 0)
    ;
  else
  {
    csv_error = ndb_import_csv_error[Ndb_import_csv_error::Format_error];
    csv_error.error_line = __LINE__;
    return false;
  }
  q = p;
  // hour
  while (isdigit(*p))
    s.hour = 10 * s.hour + (*p++ - '0');
  if (p - q == 1 || p - q == 2)
    ;
  else
  {
    csv_error = ndb_import_csv_error[Ndb_import_csv_error::Format_error];
    csv_error.error_line = __LINE__;
    return false;
  }
  q = p;
  // separator
  while (ispunct(*p))
    p++;
  if (p - q != 0)
    ;
  else
  {
    csv_error = ndb_import_csv_error[Ndb_import_csv_error::Format_error];
    csv_error.error_line = __LINE__;
    return false;
  }
  q = p;
  // minute
  while (isdigit(*p))
    s.minute = 10 * s.minute + (*p++ - '0');
  if (p - q == 1 || p - q == 2)
    ;
  else
  {
    csv_error = ndb_import_csv_error[Ndb_import_csv_error::Format_error];
    csv_error.error_line = __LINE__;
    return false;
  }
  q = p;
  // separator
  while (ispunct(*p))
    p++;
  if (p - q != 0)
    ;
  else
  {
    csv_error = ndb_import_csv_error[Ndb_import_csv_error::Format_error];
    csv_error.error_line = __LINE__;
    return false;
  }
  q = p;
  // second
  while (isdigit(*p))
    s.second = 10 * s.second + (*p++ - '0');
  if (p - q == 1 || p - q == 2)
    ;
  else
  {
    csv_error = ndb_import_csv_error[Ndb_import_csv_error::Format_error];
    csv_error.error_line = __LINE__;
    return false;
  }
  q = p;
  // fraction point (optional)
  if (*p != 0)
  {
    if (*p == '.')
      p++;
    if (p - q == 1)
      ;
    else
    {
      csv_error = ndb_import_csv_error[Ndb_import_csv_error::Format_error];
      csv_error.error_line = __LINE__;
      return false;
    }
    q = p;
    // fraction value (optional)
    while (isdigit(*p))
      s.fraction = 10 * s.fraction + (*p++ - '0');
    if (p - q <= 6)
    {
      uint n = p - q;
      while (n++ < attr.m_precision)
        s.fraction *= 10;
    }
    else
    {
      csv_error = ndb_import_csv_error[Ndb_import_csv_error::Format_error];
      csv_error.error_line = __LINE__;
      return false;
    }
    if (*p == 0)
      ;
    else
    {
      csv_error = ndb_import_csv_error[Ndb_import_csv_error::Format_error];
      csv_error.error_line = __LINE__;
      return false;
    }
  }
  //
  return true;
}

static bool
ndb_import_csv_parse_timestamp2(const NdbImportCsv::Attr& attr,
                                const char* datac,
                                NdbSqlUtil::Timestamp2& s,
                                Ndb_import_csv_error& csv_error)
{
  // parsed as Datetime2
  NdbSqlUtil::Datetime2 s2;
  if (!ndb_import_csv_parse_datetime2(attr, datac, s2, csv_error))
    return false;
  // convert to seconds in localtime
  struct tm tm;
  tm.tm_year = s2.year - 1900;
  tm.tm_mon = s2.month - 1;
  tm.tm_mday = s2.day;
  tm.tm_hour = s2.hour;
  tm.tm_min = s2.minute;
  tm.tm_sec = s2.second;
  tm.tm_isdst = -1;       // mktime() will determine
  s.second = mktime(&tm);
  s.fraction = s2.fraction;
  return true;
}

void
NdbImportCsv::Eval::eval_field(Row* row, Line* line, Field* field)
{
  const Opt& opt = m_util.c_opt;
  const CHARSET_INFO* cs = opt.m_charset;
  const Table& table = m_input.m_table;
  const Attrs& attrs = table.m_attrs;
  Buf& buf = m_input.m_buf;
  uchar* bufdata = &buf.m_data[buf.m_start];
  char* bufdatac = (char*)bufdata;
  // internal counts file lines and fields from 0
  const uint64 lineno = m_input.m_startlineno + line->m_lineno;
  const uint fieldno = field->m_fieldno;
  // user wants the counts from 1
  const uint64 linenr = 1 + lineno;
  const uint fieldnr = 1 + fieldno;
  const Attr& attr = attrs[fieldno];
  uint pos = field->m_pack_pos;
  uint end = field->m_pack_end;
  uint length = end - pos;
  uchar* data = &bufdata[pos];
  char* datac = &bufdatac[pos];
  /*
   * A field is followed by non-empty separator or terminator.
   * We null-terminate the field and restore it at end.
   */
  uchar saveterm = data[length];
  data[length] = 0;
  Error error;  // local error
  /*
   * Lots of repeated code here but it is not worth changing
   * before it moves to some datatypes library.
   */
  switch (attr.m_type) {
  case NdbDictionary::Column::Tinyint:
    {
      int err = 0;
      char* endptr = 0;
      int val = cs->cset->strntol(
                cs, datac, length, 10, &endptr, &err);
      if (err != 0)
      {
        m_util.set_error_data(
          error, __LINE__, err,
          "line %llu field %u: eval %s failed",
          linenr, fieldnr, attr.m_sqltype);
        break;
      }
      if (uint(endptr - datac) != length)
      {
        m_util.set_error_data(
          error, __LINE__, 0,
          "line %llu field %u: eval %s failed: bad format",
           linenr, fieldnr, attr.m_sqltype);
        break;
      }
      const int minval = -128;
      const int maxval = +127;
      if (val < minval || val > maxval)
      {
        m_util.set_error_data(
          error, __LINE__, 0,
          "line %llu field %u: eval %s failed: "
          "value %d out of range",
           linenr, fieldnr, attr.m_sqltype, val);
        break;
      }
      int8 byteval = val;
      attr.set_value(row, &byteval, 1);
    }
    break;
  case NdbDictionary::Column::Smallint:
    {
      int err = 0;
      char* endptr = 0;
      int val = cs->cset->strntol(
                cs, datac, length, 10, &endptr, &err);
      if (err != 0)
      {
        m_util.set_error_data(
          error, __LINE__, err,
          "line %llu field %u: eval %s failed",
          linenr, fieldnr, attr.m_sqltype);
        break;
      }
      if (uint(endptr - datac) != length)
      {
        m_util.set_error_data(
          error, __LINE__, 0,
          "line %llu field %u: eval %s failed: bad format",
           linenr, fieldnr, attr.m_sqltype);
        break;
      }
      const int minval = -32768;
      const int maxval = +32767;
      if (val < minval || val > maxval)
      {
        m_util.set_error_data(
          error, __LINE__, 0,
          "line %llu field %u: eval %s failed: "
          "value %d out of range",
           linenr, fieldnr, attr.m_sqltype, val);
        break;
      }
      int16 shortval = val;
      attr.set_value(row, &shortval, 2);
    }
    break;
  case NdbDictionary::Column::Mediumint:
    {
      int err = 0;
      char* endptr = 0;
      int val = cs->cset->strntol(
                cs, datac, length, 10, &endptr, &err);
      if (err != 0)
      {
        m_util.set_error_data(
          error, __LINE__, err,
          "line %llu field %u: eval %s failed",
          linenr, fieldnr, attr.m_sqltype);
        break;
      }
      if (uint(endptr - datac) != length)
      {
        m_util.set_error_data(
          error, __LINE__, 0,
          "line %llu field %u: eval %s failed: bad format",
           linenr, fieldnr, attr.m_sqltype);
        break;
      }
      const int minval = -8388608;
      const int maxval = +8388607;
      if (val < minval || val > maxval)
      {
        m_util.set_error_data(
          error, __LINE__, 0,
          "line %llu field %u: eval %s failed: "
          "value %d out of range",
           linenr, fieldnr, attr.m_sqltype, val);
        break;
      }
      uchar val3[3];
      int3store(val3, (uint)val);
      attr.set_value(row, val3, 3);
    }
    break;
  case NdbDictionary::Column::Int:
    {
      int err = 0;
      char* endptr = 0;
      int32 val = cs->cset->strntol(
                  cs, datac, length, 10, &endptr, &err);
      if (err != 0)
      {
        m_util.set_error_data(
          error, __LINE__, err,
          "line %llu field %u: eval %s failed",
          linenr, fieldnr, attr.m_sqltype);
        break;
      }
      if (uint(endptr - datac) != length)
      {
        m_util.set_error_data(
          error, __LINE__, 0,
          "line %llu field %u: eval %s failed: bad format",
           linenr, fieldnr, attr.m_sqltype);
        break;
      }
      attr.set_value(row, &val, 4);
    }
    break;
  case NdbDictionary::Column::Bigint:
    {
      int err = 0;
      char* endptr = 0;
      int64 val = cs->cset->strntoll(
                  cs, datac, length, 10, &endptr, &err);
      if (err != 0)
      {
        m_util.set_error_data(
          error, __LINE__, err,
          "line %llu field %u: eval %s failed",
          linenr, fieldnr, attr.m_sqltype);
        break;
      }
      if (uint(endptr - datac) != length)
      {
        m_util.set_error_data(
          error, __LINE__, 0,
         "line %llu field %u: eval %s failed: bad format",
         linenr, fieldnr, attr.m_sqltype);
        break;
      }
      attr.set_value(row, &val, 8);
    }
    break;
  case NdbDictionary::Column::Tinyunsigned:
    {
      int err = 0;
      char* endptr = 0;
      uint val = cs->cset->strntoul(
                 cs, datac, length, 10, &endptr, &err);
      if (err != 0)
      {
        m_util.set_error_data(
          error, __LINE__, err,
         "line %llu field %u: eval %s failed",
         linenr, fieldnr, attr.m_sqltype);
        break;
      }
      if (uint(endptr - datac) != length)
      {
        m_util.set_error_data(
          error, __LINE__, 0,
          "line %llu field %u: eval %s failed: bad format",
          linenr, fieldnr, attr.m_sqltype);
        break;
      }
      const uint maxval = 255;
      if (val > maxval)
      {
        m_util.set_error_data(
          error, __LINE__, 0,
          "line %llu field %u: eval %s failed: "
          "value %u out of range",
           linenr, fieldnr, attr.m_sqltype, val);
        break;
      }
      uint8 byteval = val;
      attr.set_value(row, &byteval, 1);
    }
    break;
  case NdbDictionary::Column::Smallunsigned:
    {
      int err = 0;
      char* endptr = 0;
      uint val = cs->cset->strntoul(
                 cs, datac, length, 10, &endptr, &err);
      if (err != 0)
      {
        m_util.set_error_data(
          error, __LINE__, err,
         "line %llu field %u: eval %s failed",
         linenr, fieldnr, attr.m_sqltype);
        break;
      }
      if (uint(endptr - datac) != length)
      {
        m_util.set_error_data(
          error, __LINE__, 0,
          "line %llu field %u: eval %s failed: bad format",
          linenr, fieldnr, attr.m_sqltype);
        break;
      }
      const uint maxval = 65535;
      if (val > maxval)
      {
        m_util.set_error_data(
          error, __LINE__, 0,
          "line %llu field %u: eval %s failed: "
          "value %u out of range",
           linenr, fieldnr, attr.m_sqltype, val);
        break;
      }
      uint16 shortval = val;
      attr.set_value(row, &shortval, 2);
    }
    break;
  case NdbDictionary::Column::Mediumunsigned:
    {
      int err = 0;
      char* endptr = 0;
      uint val = cs->cset->strntoul(
                 cs, datac, length, 10, &endptr, &err);
      if (err != 0)
      {
        m_util.set_error_data(
          error, __LINE__, err,
         "line %llu field %u: eval %s failed",
         linenr, fieldnr, attr.m_sqltype);
        break;
      }
      if (uint(endptr - datac) != length)
      {
        m_util.set_error_data(
          error, __LINE__, 0,
          "line %llu field %u: eval %s failed: bad format",
          linenr, fieldnr, attr.m_sqltype);
        break;
      }
      const uint maxval = 16777215;
      if (val > maxval)
      {
        m_util.set_error_data(
          error, __LINE__, 0,
          "line %llu field %u: eval %s failed: "
          "value %u out of range",
           linenr, fieldnr, attr.m_sqltype, val);
        break;
      }
      uchar val3[3];
      int3store(val3, val);
      attr.set_value(row, val3, 3);
    }
    break;
  case NdbDictionary::Column::Unsigned:
    {
      int err = 0;
      char* endptr = 0;
      uint32 val = cs->cset->strntoul(
                   cs, datac, length, 10, &endptr, &err);
      if (err != 0)
      {
        m_util.set_error_data(
          error, __LINE__, err,
         "line %llu field %u: eval %s failed",
         linenr, fieldnr, attr.m_sqltype);
        break;
      }
      if (uint(endptr - datac) != length)
      {
        m_util.set_error_data(
          error, __LINE__, 0,
          "line %llu field %u: eval %s failed: bad format",
          linenr, fieldnr, attr.m_sqltype);
        break;
      }
      attr.set_value(row, &val, 4);
    }
    break;
  case NdbDictionary::Column::Bigunsigned:
    {
      int err = 0;
      char* endptr = 0;
      uint64 val = cs->cset->strntoull(
                   cs, datac, length, 10, &endptr, &err);
      if (err != 0)
      {
        m_util.set_error_data(
          error, __LINE__, err,
          "line %llu field %u: eval %s failed",
          linenr, fieldnr, attr.m_sqltype);
        break;
      }
      if (uint(endptr - datac) != length)
      {
        m_util.set_error_data(
          error, __LINE__, 0,
          "line %llu field %u: eval %s failed: bad format",
          linenr, fieldnr, attr.m_sqltype);
        break;
      }
      attr.set_value(row, &val, 8);
    }
    break;
  case NdbDictionary::Column::Decimal:
    {
      uchar val[200];
      Ndb_import_csv_error csv_error;
      if (!ndb_import_csv_parse_decimal(attr,
                                        false,
                                        datac, length,
                                        val, sizeof(val),
                                        csv_error))
      {
        m_util.set_error_data(
          error, __LINE__, 0,
          "line %llu field %u: eval %s failed: %s at %d",
          linenr, fieldnr, attr.m_sqltype,
          csv_error.error_text, csv_error.error_line);
        break;
      }
      attr.set_value(row, val, attr.m_size);
    }
    break;
  case NdbDictionary::Column::Decimalunsigned:
    {
      uchar val[200];
      Ndb_import_csv_error csv_error;
      if (!ndb_import_csv_parse_decimal(attr,
                                        true,
                                        datac, length,
                                        val, sizeof(val),
                                        csv_error))
      {
        m_util.set_error_data(
          error, __LINE__, 0,
          "line %llu field %u: eval %s failed: %s at %d",
          linenr, fieldnr, attr.m_sqltype,
          csv_error.error_text, csv_error.error_line);
        break;
      }
      attr.set_value(row, val, attr.m_size);
    }
    break;
  /*
   * Float and Double.  We use same methods as LOAD DATA but for
   * some reason there are occasional infinitesimal diffs on "el6".
   * Fix by using ::strtod if charset allows (it does).
   */
  case NdbDictionary::Column::Float:
    {
      char* endptr = 0;
      double val = 0.0;
      bool use_os_strtod =
#ifndef _WIN32
        (opt.m_charset == &my_charset_bin);
#else
        false;
#endif
      if (use_os_strtod)
      {
        errno = 0;
        val = ::strtod(datac, &endptr);
        if (errno != 0)
        {
          m_util.set_error_data(
            error, __LINE__, errno,
            "line %llu field %u: eval %s failed",
            linenr, fieldnr, attr.m_sqltype);
          break;
        }
      }
      else
      {
        int err = 0;
        val = cs->cset->strntod(
              cs, datac, length, &endptr, &err);
        if (err != 0)
        {
          m_util.set_error_data(
            error, __LINE__, err,
            "line %llu field %u: eval %s failed",
            linenr, fieldnr, attr.m_sqltype);
          break;
        }
      }
      if (uint(endptr - datac) != length)
      {
        m_util.set_error_data(
          error, __LINE__, 0,
          "line %llu field %u: eval %s failed: bad format",
          linenr, fieldnr, attr.m_sqltype);
        break;
      }
      if (my_isnan(val))
      {
        m_util.set_error_data(
          error, __LINE__, 0,
          "line %llu field %u: eval %s failed: invalid value",
          linenr, fieldnr, attr.m_sqltype);
        break;
      }
      const double max_val = FLT_MAX;
      if (val < -max_val || val > max_val)
      {
        m_util.set_error_data(
          error, __LINE__, 0,
          "line %llu field %u: eval %s failed: value out of range",
          linenr, fieldnr, attr.m_sqltype);
        break;
      }
      float valf = (float)val;
      attr.set_value(row, &valf, 4);
    }
    break;
  case NdbDictionary::Column::Double:
    {
      int err = 0;
      char* endptr = 0;
      double val = 0.0;
      bool use_os_strtod =
#ifndef _WIN32
        (opt.m_charset == &my_charset_bin);
#else
        false;
#endif
      if (use_os_strtod)
      {
        errno = 0;
        val = ::strtod(datac, &endptr);
        if (errno != 0)
        {
          m_util.set_error_data(
            error, __LINE__, errno,
            "line %llu field %u: eval %s failed",
            linenr, fieldnr, attr.m_sqltype);
          break;
        }
      }
      else
      {
        val = cs->cset->strntod(
              cs, datac, length, &endptr, &err);
        if (err != 0)
        {
          m_util.set_error_data(
            error, __LINE__, err,
            "line %llu field %u: eval %s failed",
            linenr, fieldnr, attr.m_sqltype);
          break;
        }
      }
      if (uint(endptr - datac) != length)
      {
        m_util.set_error_data(
          error, __LINE__, 0,
          "line %llu field %u: eval %s failed: bad format",
          linenr, fieldnr, attr.m_sqltype);
        break;
      }
      if (my_isnan(val))
      {
        m_util.set_error_data(
          error, __LINE__, 0,
          "line %llu field %u: eval %s failed: invalid value",
          linenr, fieldnr, attr.m_sqltype);
        break;
      }
      const double max_val = DBL_MAX;
      if (val < -max_val || val > max_val)
      {
        m_util.set_error_data(
          error, __LINE__, 0,
          "line %llu field %u: eval %s failed: value out of range",
          linenr, fieldnr, attr.m_sqltype);
        break;
      }
      attr.set_value(row, &val, 8);
    }
    break;
  case NdbDictionary::Column::Char:
    {
      const char* val = datac;
      if (length > attr.m_length)
      {
        m_util.set_error_data(
          error, __LINE__, 0,
          "line %llu field %u: eval %s failed: "
          "byte length too long (%u > %u)",
          linenr, fieldnr, attr.m_sqltype, length, attr.m_length);
        break;
      }
      attr.set_value(row, val, length);
    }
    break;
  case NdbDictionary::Column::Varchar:
    {
      const char* val = datac;
      if (length > attr.m_length)
      {
        m_util.set_error_data(
          error, __LINE__, 0,
          "line %llu field %u: eval %s failed: "
          "byte length too long (%u > %u)",
          linenr, fieldnr, attr.m_sqltype, length, attr.m_length);
        break;
      }
      attr.set_value(row, val, length);
    }
    break;
  case NdbDictionary::Column::Longvarchar:
    {
      const char* val = datac;
      if (length > attr.m_length)
      {
        m_util.set_error_data(
          error, __LINE__, 0,
          "line %llu field %u: eval %s failed: "
          "byte length too long (%u > %u)",
          linenr, fieldnr, attr.m_sqltype, length, attr.m_length);
        break;
      }
      attr.set_value(row, val, length);
    }
    break;
  case NdbDictionary::Column::Binary:
    {
      const char* val = datac;
      if (length > attr.m_length)
      {
        m_util.set_error_data(
          error, __LINE__, 0,
          "line %llu field %u: eval %s failed: "
          "length too long (%u > %u)",
          linenr, fieldnr, attr.m_sqltype, length, attr.m_length);
        break;
      }
      attr.set_value(row, val, length);
    }
    break;
  case NdbDictionary::Column::Varbinary:
    {
      const char* val = datac;
      if (length > attr.m_length)
      {
        m_util.set_error_data(
          error, __LINE__, 0,
          "line %llu field %u: eval %s failed: "
          "length too long (%u > %u)",
          linenr, fieldnr, attr.m_sqltype, length, attr.m_length);
        break;
      }
      attr.set_value(row, val, length);
    }
    break;
  case NdbDictionary::Column::Longvarbinary:
    {
      const char* val = datac;
      if (length > attr.m_length)
      {
        m_util.set_error_data(
          error, __LINE__, 0,
          "line %llu field %u: eval %s failed: "
          "length too long (%u > %u)",
          linenr, fieldnr, attr.m_sqltype, length, attr.m_length);
        break;
      }
      attr.set_value(row, val, length);
    }
    break;
  case NdbDictionary::Column::Bit:
    {
      require(attr.m_length <= 64);
      uint bytelength = (attr.m_length + 7) / 8;
      require(bytelength <= 8);
      uchar val[8];
      memset(val, 0, sizeof(val));
      uint i = 0;
      uint j = Inval_uint;      // highest non-zero byte
      while (i < length)
      {
        uchar b = data[length - 1 - i];
        if (b != 0)
          j = i;
        if (i < bytelength)
          val[i] = b;
        i++;
      }
      if (j != Inval_uint)
      {
        uint k = 8;             // highest bit at j
        while (k != 0)
        {
          k--;
          if ((data[length - 1 - j] & (1 << k)) != 0)
            break;
        }
        uint hibit = 8 * (length - 1 - j) + k;
        if (hibit >= attr.m_length)
        {
          m_util.set_error_data(
            error, __LINE__, 0,
            "line %llu field %u: eval %s failed: "
            "highest set bit %u out of range",
             linenr, fieldnr, attr.m_sqltype, hibit);
          break;
        }
      }
#if defined(WORDS_BIGENDIAN)
      std::swap(val[0], val[3]);
      std::swap(val[1], val[2]);
      std::swap(val[4], val[7]);
      std::swap(val[5], val[6]);
#endif
      attr.set_value(row, val, attr.m_size);
    }
    break;
  case NdbDictionary::Column::Year:
    {
      NdbSqlUtil::Year s;
      Ndb_import_csv_error csv_error;
      if (!ndb_import_csv_parse_year(attr,
                                     datac,
                                     s,
                                     csv_error))
      {
        m_util.set_error_data(
          error, __LINE__, 0,
          "line %llu field %u: eval %s failed: %s at %d",
          linenr, fieldnr, attr.m_sqltype,
          csv_error.error_text, csv_error.error_line);
        break;
      }
      uchar val[1];
      NdbSqlUtil::pack_year(s, val);
      attr.set_value(row, val, 1);
    }
    break;
  case NdbDictionary::Column::Date:
    {
      NdbSqlUtil::Date s;
      Ndb_import_csv_error csv_error;
      if (!ndb_import_csv_parse_date(attr,
                                     datac,
                                     s,
                                     csv_error))
      {
        m_util.set_error_data(
          error, __LINE__, 0,
          "line %llu field %u: eval %s failed: %s at %d",
          linenr, fieldnr, attr.m_sqltype,
          csv_error.error_text, csv_error.error_line);
        break;
      }
      uchar val[3];
      NdbSqlUtil::pack_date(s, val);
      attr.set_value(row, val, 3);
    }
    break;
  case NdbDictionary::Column::Time2:
    {
      NdbSqlUtil::Time2 s;
      Ndb_import_csv_error csv_error;
      if (!ndb_import_csv_parse_time2(attr,
                                      datac,
                                      s,
                                      csv_error))
      {
        m_util.set_error_data(
          error, __LINE__, 0,
          "line %llu field %u: eval %s failed: %s at %d",
          linenr, fieldnr, attr.m_sqltype,
          csv_error.error_text, csv_error.error_line);
        break;
      }
      uint prec = attr.m_precision;
      require(prec <= 6);
      uint flen = (1 + prec) / 2;
      uint len = 3 + flen;
      require(len <= 6);
      uchar val[6];
      NdbSqlUtil::pack_time2(s, val, prec);
      attr.set_value(row, val, len);
    }
    break;
  case NdbDictionary::Column::Datetime2:
    {
      NdbSqlUtil::Datetime2 s;
      Ndb_import_csv_error csv_error;
      if (!ndb_import_csv_parse_datetime2(attr,
                                          datac,
                                          s,
                                          csv_error))
      {
        m_util.set_error_data(
          error, __LINE__, 0,
          "line %llu field %u: eval %s failed: %s at %d",
          linenr, fieldnr, attr.m_sqltype,
          csv_error.error_text, csv_error.error_line);
        break;
      }
      uint prec = attr.m_precision;
      require(prec <= 6);
      uint flen = (1 + prec) / 2;
      uint len = 5 + flen;
      require(len <= 8);
      uchar val[8];
      NdbSqlUtil::pack_datetime2(s, val, prec);
      attr.set_value(row, val, len);
    }
    break;
  case NdbDictionary::Column::Timestamp2:
    {
      NdbSqlUtil::Timestamp2 s;
      Ndb_import_csv_error csv_error;
      if (!ndb_import_csv_parse_timestamp2(attr,
                                          datac,
                                          s,
                                          csv_error))
      {
        m_util.set_error_data(
          error, __LINE__, 0,
          "line %llu field %u: eval %s failed: %s at %d",
          linenr, fieldnr, attr.m_sqltype,
          csv_error.error_text, csv_error.error_line);
        break;
      }
      uint prec = attr.m_precision;
      require(prec <= 6);
      uint flen = (1 + prec) / 2;
      uint len = 4 + flen;
      require(len <= 7);
      uchar val[7];
      NdbSqlUtil::pack_timestamp2(s, val, prec);
      attr.set_value(row, val, len);
    }
    break;
  case NdbDictionary::Column::Blob:
  case NdbDictionary::Column::Text:
    {
      const char* val = datac;
      attr.set_blob(row, val, length);
    }
    break;
  default:
    require(false);
    break;
  }
  data[length] = saveterm;
  if (m_util.has_error(error))
  {
    m_input.reject_line(line, field, error);
    line->m_reject = true;
  }
}

void
NdbImportCsv::Eval::eval_null(Row* row, Line* line, Field* field)
{
  const Table& table = m_input.m_table;
  const Attrs& attrs = table.m_attrs;
  // internal counts file lines and fields from 0
  const uint64 lineno = m_input.m_startlineno + line->m_lineno;
  const uint fieldno = field->m_fieldno;
  // user wants the counts from 1
  const uint64 linenr = 1 + lineno;
  const uint fieldnr = 1 + fieldno;
  const Attr& attr = attrs[fieldno];
  Error error;  // local error
  do
  {
    if (!attr.m_nullable)
    {
      m_util.set_error_data(
        error, __LINE__, 0,
        "line %llu field %u: setting non-nullable attr to NULL",
        linenr, fieldnr);
      break;
    }
  } while (0);
  if (m_util.has_error(error))
  {
    m_input.reject_line(line, field, error);
    line->m_reject = true;
  }
  attr.set_null(row, true);
}

NdbOut&
operator<<(NdbOut& out, const NdbImportCsv::Eval& eval)
{
  out << "eval";
  return out;
}

NdbOut&
operator<<(NdbOut& out, const NdbImportCsv::Regex& regex)
{
  out << "regex";
  out << " pattern=" << regex.m_pattern;
  out << " nsub=" << regex.m_nsub;
  return out;
}

// output

NdbImportCsv::Output::Output(NdbImportCsv& csv,
                             const Spec& spec,
                             const Table& table,
                             Buf& buf) :
  m_csv(csv),
  m_util(m_csv.m_util),
  m_spec(spec),
  m_table(table),
  m_buf(buf)
{
  for (uint u = 0; u < g_bytecnt; u++)
    m_escapes[u] = 0;
}

void
NdbImportCsv::Output::do_init()
{
  log1("do_init");
  const Spec& spec = m_spec;
  for (uint u = 0; u < g_bytecnt; u++)
    m_escapes[u] = 0;
  if (spec.m_fields_escaped_by != 0)    // should be
  {
    m_escapes[0] = '0';
    m_escapes[010] = 'b';
    m_escapes[012] = 'n';
    m_escapes[015] = 'r';
    m_escapes[011] = 't';
    m_escapes[032] = 'Z';
    if (spec.m_fields_enclosed_by != 0)
    {
      uchar quote = spec.m_fields_enclosed_by[0];
      m_escapes[quote] = quote;
    }
    uchar esc = spec.m_fields_escaped_by[0];
    m_escapes[esc] = esc;
  }
}

void
NdbImportCsv::Output::add_header()
{
  const Table& table = m_table;
  const Attrs& attrs = table.m_attrs;
  const uint attrcnt = attrs.size();
  for (uint i = 0; i < attrcnt; i++)
  {
    const Attr& attr = attrs[i];
    if (i > 0)
    {
      add_fieldsep();
    }
    uchar* bufptr = &m_buf.m_data[m_buf.m_start + m_buf.m_len];
    char* bufptrc = (char*)bufptr;
    strcpy(bufptrc, attr.m_attrname.c_str());
    m_buf.m_len += strlen(bufptrc);
  }
  add_lineend();
}

void
NdbImportCsv::Output::add_line(const Row* row)
{
  const Spec& spec = m_spec;
  const Table& table = m_table;
  const Attrs& attrs = table.m_attrs;
  const uint attrcnt = attrs.size();
  for (uint i = 0; i < attrcnt; i++)
  {
    const Attr& attr = attrs[i];
    if (i > 0)
    {
      add_fieldsep();
    }
    if (attr.m_quotable)
    {
      add_quote();
    }
    add_field(attr, row);
    if (attr.m_quotable && spec.m_fields_enclosed_by != 0)
    {
      add_quote();
    }
  }
  add_lineend();
}

void
NdbImportCsv::Output::add_field(const Attr& attr, const Row* row)
{
  uchar* bufptr = &m_buf.m_data[m_buf.m_start + m_buf.m_len];
  char* bufptrc = (char*)bufptr;
  const uchar* rowptr = &row->m_data[attr.m_offset];
  switch (attr.m_type) {
  case NdbDictionary::Column::Int:
    {
      int32 val;
      require(attr.m_size == sizeof(val));
      memcpy(&val, rowptr, sizeof(val));
      sprintf(bufptrc, "%d", val);
      break;
    }
    break;
  case NdbDictionary::Column::Unsigned:
    {
      uint32 val;
      require(attr.m_size == sizeof(val));
      memcpy(&val, rowptr, sizeof(val));
      sprintf(bufptrc, "%u", val);
      break;
    }
    break;
  case NdbDictionary::Column::Bigint:
    {
      int64 val;
      require(attr.m_size == sizeof(val));
      memcpy(&val, rowptr, sizeof(val));
      sprintf(bufptrc, "%lld", val);
      break;
    }
    break;
  case NdbDictionary::Column::Bigunsigned:
    {
      uint64 val;
      require(attr.m_size == sizeof(val));
      memcpy(&val, rowptr, sizeof(val));
      sprintf(bufptrc, "%llu", val);
      break;
    }
    break;
  case NdbDictionary::Column::Double:
    {
      double val;
      require(attr.m_size == sizeof(val));
      memcpy(&val, rowptr, sizeof(val));
      sprintf(bufptrc, "%.02f", val);
      break;
    }
    break;
  case NdbDictionary::Column::Varchar:
    {
      uint len = rowptr[0];
      add_char(&rowptr[1], len);
      break;
    }
    break;
  case NdbDictionary::Column::Longvarchar:
    {
      uint len = rowptr[0] + (rowptr[1] << 8);
      add_char(&rowptr[2], len);
      break;
    }
    break;
  case NdbDictionary::Column::Text:
    {
      require(attr.m_isblob);
      const Blob* blob = row->m_blobs[attr.m_blobno];
      add_char(blob->m_data, blob->m_blobsize);
      break;
    }
  default:
    require(false);
    break;
  }
  m_buf.m_len += strlen(bufptrc);
}

void
NdbImportCsv::Output::add_char(const uchar* rowdata, uint len)
{
  log3("add_char " << len << " " << (char*)rowdata);
  const Spec& spec = m_spec;
  require(spec.m_fields_escaped_by != 0);
  uchar esc = spec.m_fields_escaped_by[0];
  uchar* bufptr = &m_buf.m_data[m_buf.m_start + m_buf.m_len];
  uchar* p = bufptr;
  for (uint i = 0; i < len; i++)
  {
    uchar c = rowdata[i];
    if (m_escapes[c])
    {
      *p++ = esc;
      *p++ = m_escapes[c];
    }
    else
      *p++ = c;
  }
  *p = 0;
}

void
NdbImportCsv::Output::add_quote()
{
  const Spec& spec = m_spec;
  if (spec.m_fields_enclosed_by != 0)
  {
    uchar* bufptr = &m_buf.m_data[m_buf.m_start + m_buf.m_len];
    char* bufptrc = (char*)bufptr;
    strcpy(bufptrc, (const char*)spec.m_fields_enclosed_by);
    m_buf.m_len += strlen(bufptrc);
  }
}

void
NdbImportCsv::Output::add_fieldsep()
{
  const Spec& spec = m_spec;
  uchar* bufptr = &m_buf.m_data[m_buf.m_start + m_buf.m_len];
  char* bufptrc = (char*)bufptr;
  strcpy(bufptrc, (const char*)spec.m_fields_terminated_by);
  m_buf.m_len += strlen(bufptrc);
}

void
NdbImportCsv::Output::add_lineend()
{
  const Spec& spec = m_spec;
  uchar* bufptr = &m_buf.m_data[m_buf.m_start + m_buf.m_len];
  char* bufptrc = (char*)bufptr;
  strcpy(bufptrc, (const char*)spec.m_lines_terminated_by);
  m_buf.m_len += strlen(bufptrc);
}

NdbOut&
operator<<(NdbOut& out, const NdbImportCsv::Output& output)
{
  out << "output";
  out << " len=" << output.m_buf.m_len;
  return out;
}

// unittest

#ifdef TEST_NDBIMPORTCSV

#include <NdbTap.hpp>

typedef NdbImport::OptCsv OptCsv;
typedef NdbImportUtil::Name UtilName;
typedef NdbImportUtil::Buf UtilBuf;
typedef NdbImportUtil::File UtilFile;
typedef NdbImportUtil::Attr UtilAttr;
typedef NdbImportUtil::Attrs UtilAttrs;
typedef NdbImportUtil::Table UtilTable;
typedef NdbImportUtil::RowList UtilRowList;
typedef NdbImportUtil::RowMap UtilRowMap;
typedef NdbImportUtil::Stats UtilStats;
typedef NdbImportCsv::Spec CsvSpec;
typedef NdbImportCsv::Input CsvInput;
typedef NdbImportCsv::Line CsvLine;
typedef NdbImportCsv::Field CsvField;

static void
makeoptcsv(OptCsv& optcsv)
{
  optcsv.m_fields_terminated_by = ",";
  optcsv.m_fields_enclosed_by = "\"";
  optcsv.m_fields_optionally_enclosed_by = "\"";
  optcsv.m_fields_escaped_by = "\\\\";
  optcsv.m_lines_terminated_by = "\\n";
}

// table (a int unsigned primary key, b varchar(10) not null)

static void
maketable(UtilTable& table)
{
  table.add_pseudo_attr("a", NdbDictionary::Column::Unsigned);
  table.add_pseudo_attr("b", NdbDictionary::Column::Varchar, 10);
}

struct MyRes {
  uint fieldcnt;
  const char* field[20];        // fields, 0 for NULL
  MyRes(uint cnt, ...) {
    va_list ap;
    va_start(ap, cnt);
    fieldcnt = cnt;
    for (uint n = 0; n < cnt; n++) {
      const char* f = va_arg(ap, const char*);
      field[n] = f;
    }
  }
};

struct MyCsv {
  uint error;   // 0-ok 1-error
  uint linecnt; // valid lines
  uint partial; // bytes in last partial line
  const char* buf;
  MyRes res;
};

static MyCsv mycsvlist[] = {
  { 0, 0, 0, "",
    MyRes(0) },
  { 0, 1, 0, "123,abc\n",
    MyRes(2, "123", "abc") },
  { 0, 2, 0, "123,abc\n456,def\n",
    MyRes(4, "123", "abc", "456", "def") },
  { 0, 1, 7, "123,abc\n456,def",
    MyRes(2, "123", "abc") },
  { 0, 2, 0, "123,\"abc\"\n456,def\n",
    MyRes(4, "123", "abc", "456", "def") },
  { 0, 2, 0, "123,\"a\"\"c\"\n456,def\n",
    MyRes(4, "123", "a\"c", "456", "def") },
  { 0, 1, 0, "123,\"a,c\"\n",
    MyRes(2, "123", "a,c") },
  { 0, 1, 0, "123,\\N\n",
    MyRes(2, "123", 0) },
  { 0, 1, 0, "123,\"\\N\"\n",
    MyRes(2, "123", 0) },
  { 0, 1, 0, "123,\\N\\N\n",
    MyRes(2, "123", "NN") },
  { 0, 1, 0, "123,\\0\\b\\n\\r\\t\\Z\\N\n",
    MyRes(2, "123", "\000\010\012\015\011\032N") },
};

static const uint mycsvcnt = sizeof(mycsvlist)/sizeof(mycsvlist[0]);

static int
testinput1()
{
  NdbImportUtil util;
  NdbOut& out = *util.c_log;
  util.c_opt.m_log_level = 4;
  out << "testinput1" << endl;
  NdbImportCsv csv(util);
  OptCsv optcsv;
  makeoptcsv(optcsv);
  CsvSpec csvspec;
  require(csv.set_spec(csvspec, optcsv, OptCsv::ModeInput) == 0);
  UtilTable table;
  maketable(table);
  UtilStats stats(util);
  for (uint i = 0; i < mycsvcnt; i++)
  {
    out << "case " << i << endl;
    const MyCsv& mycsv = mycsvlist[i];
    UtilBuf buf;
    buf.alloc(1024, 1);
    buf.copy((const uchar*)mycsv.buf, strlen(mycsv.buf));
    const uchar* bufdata = &buf.m_data[buf.m_start];
    const char* bufdatac = (const char*)bufdata;
    uint n = strlen(bufdatac);
    if (n != 0 && bufdatac[n-1] == '\n')
      out << bufdatac;
    else
      out << bufdatac << "\\c" << endl;
    UtilRowList rows_out;
    UtilRowList rows_reject;
    UtilRowMap rowmap_in(util);
    CsvInput input(csv,
                   "csvinput",
                   csvspec,
                   table,
                   buf,
                   rows_out,
                   rows_reject,
                   rowmap_in,
                   stats);
    input.do_init();
    input.do_parse();
    if (!input.has_error())
    {
      require(mycsv.error == 0);
    }
    else
    {
      out << util.c_error << endl;
      require(mycsv.error == 1);
    }
    require(input.m_line_list.cnt() == mycsv.linecnt);
    const MyRes& myres = mycsv.res;
    uint fieldcnt = 0;
    CsvLine* line = input.m_line_list.front();
    while (line != 0)
    {
      CsvField* field = line->m_field_list.front();
      while (field != 0)
      {
        require(fieldcnt < myres.fieldcnt);
        const char* myfield = myres.field[fieldcnt];
        if (field->m_null)
        {
          require(myfield == 0);
        }
        else
        {
          require(myfield != 0);
          uint pos = field->m_pack_pos;
          uint end = field->m_pack_end;
          uint len = end - pos;
          require(memcmp(&bufdata[pos], myfield, len) == 0);
        }
        fieldcnt++;
        field = field->next();
      }
      line = line->next();
    }
    require(fieldcnt == myres.fieldcnt);
    require(buf.m_tail <= buf.m_len);
    require(buf.m_len - buf.m_tail == mycsv.partial);
    input.free_line_list(input.m_line_list);
    require(input.balanced());
  }
  return 0;
}

static int
testinput2()
{
  NdbImportUtil util;
  NdbOut& out = *util.c_log;
  util.c_opt.m_log_level = 2;
  util.c_opt.m_abort_on_error = 1;
  out << "testinput2" << endl;
  const char* path = "test.csv";
  struct stat st;
  if (stat(path, &st) == -1)
  {
    out << path << ": skip on errno " << errno << endl;
    return 0;
  }
  NdbImportCsv csv(util);
  OptCsv optcsv;
  makeoptcsv(optcsv);
  CsvSpec csvspec;
  require(csv.set_spec(csvspec, optcsv, OptCsv::ModeInput) == 0);
  UtilTable table;
  maketable(table);
  UtilBuf* buf[2];
  buf[0] = new UtilBuf(true);
  buf[1] = new UtilBuf(true);
  buf[0]->alloc(4096, 4);
  buf[1]->alloc(4096, 4);
  UtilRowList rows_out;
  UtilRowList rows_reject;
  UtilRowMap rowmap_in(util);
  UtilStats stats(util);
  CsvInput* input[2];
  input[0] = new CsvInput(csv, "csvinput-0", csvspec, table, *buf[0],
                          rows_out, rows_reject, rowmap_in, stats);
  input[1] = new CsvInput(csv, "csvinput-1", csvspec, table, *buf[1],
                          rows_out, rows_reject, rowmap_in, stats);
  input[0]->do_init();
  input[1]->do_init();
  UtilFile file(util, util.c_error);
  out << "read " << path << endl;
  file.set_path(path);
  require(file.do_open(UtilFile::Read_flags) == 0);
  uint totlen = 0;
  uint totread = 0;
  uint totlines = 0;
  uint i = 0;
  while (1)
  {
    uint j = 1 - i;
    CsvInput& input1 = *input[i];
    UtilBuf& b1 = *buf[i];
    UtilBuf& b2 = *buf[j];
    b1.reset();
    int ret = file.do_read(b1);
    require(ret == 0);
    totlen += b1.m_len;
    if (totread != 0)
    {
      out << "movetail" << " src=" << b2 << " dst=" << b1 << endl;
      require(b2.movetail(b1) == 0);
    }
    input1.do_parse();
    totread++;
    totlines += input1.m_line_list.cnt();
    input1.free_line_list(input1.m_line_list);
    if (b1.m_eof)
      break;
    i = j;
  }
  require(totlen == st.st_size);
  out << "len=" << totlen << " reads=" << totread
      << " lines=" << totlines << endl;
  require(file.do_close() == 0);
  return 0;
}

static int
testmain()
{
  ndb_init();
#ifdef VM_TRACE
  signal(SIGABRT, SIG_DFL);
  signal(SIGSEGV, SIG_DFL);
#endif
  if (testinput1() != 0)
    return -1;
  if (testinput2() != 0)
    return -1;
  return 0;
}

TAPTEST(NdbImportCsv)
{
  int ret = testmain();
  return (ret == 0);
}

#endif
