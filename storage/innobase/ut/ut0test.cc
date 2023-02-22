/*****************************************************************************

Copyright (c) 2020, 2023, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

#ifdef UNIV_DEBUG
#include "ut0test.h"
#include "buf0flu.h"
#include "dict0dd.h"
#include "dict0dict.h"
#include "fil0fil.h"

#define CALL_MEMBER_FN(object, ptrToMember) ((object).*(ptrToMember))

/** A macro to define a dispatch function or a command function.  They all
have the same signature.
@param[in]   func_  the function that is being declared. */
#define DISPATCH_FUNCTION_DEF(func_)                    \
  /* @param[in] tokens   the command line */            \
  /* @return RET_PASS on success, or the error code. */ \
  Ret_t func_(std::vector<std::string> &tokens) noexcept

namespace ib {

thread_local Tester tl_interpreter;

typedef Tester::Ret_t Ret_t;

#define DISPATCH(x) m_dispatch[#x] = &Tester::x

Tester::Tester() noexcept {
  /* Kindly keep the commands in alphabetical order. */
  DISPATCH(corrupt_ondisk_page0);
  DISPATCH(corrupt_ondisk_root_page);
  DISPATCH(count_page_type);
  DISPATCH(count_used_and_free);
  DISPATCH(dblwr_force_crash);
  DISPATCH(find_fil_page_lsn);
  DISPATCH(find_flush_sync_lsn);
  DISPATCH(find_ondisk_page_type);
  DISPATCH(find_root_page_no);
  DISPATCH(find_space_id);
  DISPATCH(find_tablespace_file_name);
  DISPATCH(find_tablespace_physical_page_size);
  DISPATCH(make_ondisk_root_page_zeroes);
  DISPATCH(make_page_dirty);
  DISPATCH(open_table);
  DISPATCH(print_dblwr_has_encrypted_pages);
}

void Tester::init() noexcept {
  TLOG("Tester::init()");
  std::ostringstream sout;
  m_thd = current_thd;
  XLOG("Initialization successfully completed");
  set_output(sout);
}

void scan_page_type(space_id_t space_id,
                    std::map<page_type_t, page_no_t> &result_map) {
  mtr_t mtr;

  bool found;
  const page_size_t page_size = fil_space_get_page_size(space_id, &found);
  ut_ad(found);
  fil_space_t *space = fil_space_acquire(space_id);

  for (page_no_t page_no = 0; page_no < space->size; ++page_no) {
    const page_id_t page_id(space_id, page_no);
    mtr_start(&mtr);
    buf_block_t *block =
        buf_page_get(page_id, page_size, RW_S_LATCH, UT_LOCATION_HERE, &mtr);
    page_type_t page_type = block->get_page_type();
    result_map[page_type]++;
    mtr_commit(&mtr);
  }

  fil_space_release(space);
}

DISPATCH_FUNCTION_DEF(Tester::count_page_type) {
  std::ostringstream sout;
  mtr_t mtr;
  std::map<page_type_t, page_no_t> result_map;

  TLOG("Tester::count_page_type()");
  ut_ad(tokens.size() == 2);
  ut_ad(tokens[0] == "count_page_type");

  const std::string space_name = tokens[1];

  const space_id_t space_id = fil_space_get_id_by_name(space_name.c_str());

  scan_page_type(space_id, result_map);

  page_no_t total = 0;
  for (auto it : result_map) {
    sout << fil_get_page_type_str(it.first) << ": " << it.second << std::endl;
    total += it.second;
  }
  sout << "Total: " << total << std::endl;
  set_output(sout);

  return RET_PASS;
}

DISPATCH_FUNCTION_DEF(Tester::count_used_and_free) {
  std::ostringstream sout;
  mtr_t mtr;
  std::map<page_type_t, page_no_t> result_map;

  TLOG("Tester::count_used_and_free()");
  ut_ad(tokens.size() == 2);
  ut_ad(tokens[0] == "count_used_and_free");

  const std::string space_name = tokens[1];

  const space_id_t space_id = fil_space_get_id_by_name(space_name.c_str());

  scan_page_type(space_id, result_map);

  page_no_t total = 0;
  for (auto it : result_map) {
    total += it.second;
  }
  const page_no_t pages_free = result_map[FIL_PAGE_TYPE_ALLOCATED];
  const page_no_t used = total - pages_free;
  const double fill_factor = (used / (double)total) * 100;
  const double free_factor = (pages_free / (double)total) * 100;
  sout << "Total= " << total << ", used=" << used << ", free=" << pages_free
       << std::endl;
  sout << "Fill factor= " << fill_factor << ", free factor= " << free_factor
       << std::endl;
  set_output(sout);

  return RET_PASS;
}

dict_table_t *Tester::is_table_open(
    const std::string &table_name) const noexcept {
  for (auto iter = m_open_tables.begin(); iter != m_open_tables.end(); ++iter) {
    dict_table_t *table = *iter;
    std::string tmp(table->name.m_name);
    if (table_name == tmp) {
      return table;
    }
  }

  return nullptr;
}

Ret_t Tester::open_table(std::vector<std::string> &tokens) noexcept {
  TLOG("Tester::open_table()");
  ut_ad(tokens[0] == "open_table");
  ut_ad(tokens.size() == 2);
  std::ostringstream sout;
  Ret_t ret;

  std::string table_name = tokens[1];
  dict_table_t *table = is_table_open(table_name);
  if (table == nullptr) {
    table = dict_table_open_on_name(table_name.c_str(), false, false,
                                    DICT_ERR_IGNORE_NONE);
  }
  if (table == nullptr) {
    XLOG("FAIL: Could not open table: " << table_name);
    ret = RET_FAIL;
  } else {
    m_open_tables.push_back(table);
    XLOG("PASS: Successfully opened table=" << table_name);
    ret = RET_PASS;
  }
  set_output(sout);
  return ret;
}

Ret_t Tester::find_space_id(std::vector<std::string> &tokens) noexcept {
  TLOG("Tester::find_space_id");
  ut_ad(tokens.size() == 2);

  std::string table_name = tokens[1];
  ut_ad(tokens[0] == "find_space_id");
  dict_table_t *table = is_table_open(table_name);

  ut_ad(table != nullptr);

  space_id_t space_id = table->space;

  dict_index_t *clust_index = table->first_index();
  page_no_t root_page_no = clust_index->page;

  TLOG("table_name=" << table_name << ", space_id=" << space_id
                     << ", root_page_no=" << root_page_no);

  {
    std::ostringstream sout;
    sout << space_id;
    set_output(sout);
  }

  return RET_PASS;
}

Ret_t Tester::find_root_page_no(std::vector<std::string> &tokens) noexcept {
  TLOG("Tester::find_root_page_no");
  ut_ad(tokens[0] == "find_root_page_no");
  ut_ad(tokens.size() == 2);

  std::string table_name = tokens[1];
  dict_table_t *table = is_table_open(table_name);

  ut_ad(table != nullptr);

  space_id_t space_id = table->space;

  dict_index_t *clust_index = table->first_index();
  page_no_t root_page_no = clust_index->page;

  TLOG("table_name=" << table_name << ", space_id=" << space_id
                     << ", root_page_no=" << root_page_no);

  {
    std::ostringstream sout;
    sout << root_page_no;
    set_output(sout);
  }

  return RET_PASS;
}

Ret_t Tester::find_fil_page_lsn(std::vector<std::string> &tokens) noexcept {
  ut_ad(tokens[0] == "find_fil_page_lsn");
  std::string space_id_str = tokens[1];
  std::string page_no_str = tokens[2];
  space_id_t space_id = std::stoul(space_id_str);
  space_id_t page_no = std::stoul(page_no_str);

  page_id_t page_id(space_id, page_no);
  bool found;
  page_size_t page_size = fil_space_get_page_size(space_id, &found);
  ut_ad(found);

  mtr_t mtr;
  mtr_start(&mtr);
  buf_block_t *block =
      buf_page_get(page_id, page_size, RW_X_LATCH, UT_LOCATION_HERE, &mtr);
  lsn_t newest_lsn = block->page.get_newest_lsn();
  mtr_commit(&mtr);

  std::ostringstream sout;
  sout << newest_lsn;
  set_output(sout);

  return RET_PASS;
}

Ret_t Tester::find_ondisk_page_type(std::vector<std::string> &tokens) noexcept {
  TLOG("Tester::find_ondisk_page_type()");
  ut_ad(tokens.size() == 3);

  ut_ad(tokens[0] == "find_ondisk_page_type");
  const std::string space_id_str = tokens[1];
  const std::string page_no_str = tokens[2];

  const space_id_t space_id = std::stoul(space_id_str);
  space_id_t page_no = std::stoul(page_no_str);

  /* Calculate the offset here. */
  bool found;
  page_size_t page_size = fil_space_get_page_size(space_id, &found);
  ut_ad(found);

  const page_id_t page_id(space_id, page_no);

  /* The buffer into which file page header is read. */
  alignas(OS_FILE_LOG_BLOCK_SIZE) std::array<byte, OS_FILE_LOG_BLOCK_SIZE> buf;

  fil_space_t *space = fil_space_get(space_id);
  ut_ad(space != nullptr);

  const fil_node_t *node = space->get_file_node(&page_no);
  ut_ad(node->is_open);

  const os_offset_t offset = page_no * page_size.physical();

  /* When the space file is currently open we are not able to write to it
  directly on Windows. We must use the currently opened handle. Moreover,
  on Windows a file opened for the asynchronous access must be accessed only in
  a way that allows asynchronous completion of the request. This requires that
  the i/o buffer be aligned to OS block size and also its size divisible by OS
  block size. */

  IORequest read_io_type(IORequest::READ);
  const dberr_t err = os_file_read(read_io_type, node->name, node->handle,
                                   buf.data(), offset, OS_FILE_LOG_BLOCK_SIZE);
  if (err != DB_SUCCESS) {
    page_type_t page_type = fil_page_get_type(buf.data());
    TLOG("Could not read page_id=" << page_id << ", page_type=" << page_type
                                   << ", err=" << err);
    /* Since we do not pass encryption information in IORequest, if page is
    encrypted on disk, it cannot be decrypted by i/o layer. But the encrypted
    data will be available in the provided buffer. */

    if (err == DB_IO_DECRYPT_FAIL) {
      /* We expect this only for encrypted pages.  For this function, this error
      is OK because we will only read one header field and the header is not
      encrypted. */
      ut_ad(Encryption::is_encrypted_page(buf.data()));
    } else {
      return RET_FAIL;
    }
  }

  const byte *page = buf.data();
  const page_type_t type = fil_page_get_type(page);
  const char *page_type = fil_get_page_type_str(type);

  TLOG("page_type=" << type);
  TLOG("page_type=" << page_type);

  std::ostringstream sout;
  sout << page_type;
  set_output(sout);

  return RET_PASS;
}

Ret_t Tester::find_tablespace_file_name(
    std::vector<std::string> &tokens) noexcept {
  std::ostringstream sout;

  TLOG("Tester::find_tablespace_file_name()");
  ut_ad(tokens.size() == 2);
  ut_ad(tokens[0] == "find_tablespace_file_name");

  std::string space_name = tokens[1];
  space_id_t space_id = fil_space_get_id_by_name(space_name.c_str());
  char *filename = fil_space_get_first_path(space_id);
  sout << filename;
  set_output(sout);
  ut::free(filename);
  return RET_PASS;
}

DISPATCH_FUNCTION_DEF(Tester::find_tablespace_physical_page_size) {
  std::ostringstream sout;

  TLOG("Tester::find_tablespace_physical_page_size()");
  ut_ad(tokens.size() == 2);
  ut_ad(tokens[0] == "find_tablespace_physical_page_size");

  std::string space_name = tokens[1];
  space_id_t space_id = fil_space_get_id_by_name(space_name.c_str());
  bool found;
  page_size_t page_size = fil_space_get_page_size(space_id, &found);
  ut_ad(found);
  sout << page_size.physical();
  set_output(sout);
  return RET_PASS;
}

DISPATCH_FUNCTION_DEF(Tester::make_ondisk_root_page_zeroes) {
  TLOG("Tester::make_ondisk_root_page_zeroes()");
  ut_ad(tokens.size() == 2);
  ut_ad(tokens[0] == "make_ondisk_root_page_zeroes");

  const std::string table_name = tokens[1];
  const dict_table_t *const table = is_table_open(table_name);

  ut_ad(table != nullptr);

  const dict_index_t *const clust_index = table->first_index();
  const page_no_t root_page_no = clust_index->page;
  const page_size_t page_size = dict_table_page_size(table);

  return clear_page_prefix(table->space, root_page_no, page_size.physical());
}

Ret_t Tester::corrupt_ondisk_root_page(
    std::vector<std::string> &tokens) noexcept {
  std::ostringstream sout;

  TLOG("Tester::corrupt_ondisk_root_page()");
  ut_ad(tokens.size() == 2);
  ut_ad(tokens[0] == "corrupt_ondisk_root_page");

  const std::string table_name = tokens[1];
  const dict_table_t *table = is_table_open(table_name);

  if (table == nullptr) {
    std::vector<std::string> cmd{"open_table", table_name};
    Ret_t st = open_table(cmd);

    if (st != RET_PASS) {
      XLOG("Failed to open table: " << table_name);
      set_output(sout);
      return st;
    }

    table = is_table_open(table_name);
  }

  if (table == nullptr) {
    XLOG("Failed to open table: " << table_name);
    set_output(sout);
    return RET_FAIL;
  }

  const dict_index_t *const clust_index = table->first_index();
  const page_no_t root_page_no = clust_index->page;

  return clear_page_prefix(table->space, root_page_no, FIL_PAGE_DATA);
}

Ret_t Tester::clear_page_prefix(const space_id_t space_id, page_no_t page_no,
                                const size_t prefix_length) {
  TLOG("Tester::clear_page_prefix()");
  ut::aligned_array_pointer<byte, OS_FILE_LOG_BLOCK_SIZE> mem;
  /* We read before write, as writes have to have length divisible by
  OS_FILE_LOG_BLOCK_SIZE thus we need to learn the content of non-zeroed suffix.
  Also, it's easier to spot errors during read than write, and this requires
  reading at least the FIL_PAGE_DATA first bytes */
  const auto buf_size =
      ut_uint64_align_up(prefix_length, OS_FILE_LOG_BLOCK_SIZE);

  mem.alloc(ut::Count(buf_size));
  const page_id_t page_id{space_id, page_no};
  fil_space_t *space = fil_space_get(space_id);

  // Note: this call adjusts page_no, so it becomes relative to the node
  fil_node_t *node = space->get_file_node(&page_no);
  ut_ad(node->is_open);

  const page_size_t page_size(space->flags);
  const size_t page_size_bytes = page_size.physical();
  ut_a(buf_size <= page_size_bytes);

  // Note: we use updated page_no here, as os_aio needs offset relative to node
  const os_offset_t offset = page_no * page_size_bytes;

  /* When the space file is currently open we are not able to write to it
  directly on Windows. We must use the currently opened handle. Moreover,
  on Windows a file opened for the AIO access must be accessed only by AIO
  methods. The AIO requires the operations to be aligned and with size
  divisible by OS block size, so we first read block of the first page, to
  corrupt it and write back. */

  byte *buf = mem;
  IORequest read_io_type(IORequest::READ);
  dberr_t err = os_file_read(read_io_type, node->name, node->handle, buf,
                             offset, buf_size);
  if (err != DB_SUCCESS) {
    page_type_t page_type = fil_page_get_type(buf);
    TLOG("Could not read page_id=" << page_id << ", page type=" << page_type
                                   << ", err=" << err);

    if (err == DB_IO_DECRYPT_FAIL) {
      /* We expect this only for encrypted pages. Since this function doesn't
      actually read (or use) the contents, this error is OK. */
      ut_ad(Encryption::is_encrypted_page(buf));
    } else {
      return RET_FAIL;
    }
  }

  ut_ad(prefix_length <= buf_size);
  memset(buf, 0x00, prefix_length);

  IORequest write_io_type(IORequest::WRITE);
  err = os_file_write(write_io_type, node->name, node->handle, buf, offset,
                      buf_size);
  if (err == DB_SUCCESS) {
    TLOG("Successfully zeroed prefix of page_id=" << page_id << ", prefix="
                                                  << prefix_length);
  } else {
    TLOG("Could not write zeros to page_id=" << page_id << ", err=" << err);
    return RET_FAIL;
  }
  return RET_PASS;
}

DISPATCH_FUNCTION_DEF(Tester::dblwr_force_crash) {
  TLOG("Tester::dblwr_force_crash()");
  ut_ad(tokens.size() == 3);
  ut_ad(tokens[0] == "dblwr_force_crash");

  std::string space_id_str = tokens[1];
  std::string page_no_str = tokens[2];
  space_id_t space_id = std::stoul(space_id_str);
  space_id_t page_no = std::stoul(page_no_str);

  page_id_t page_id(space_id, page_no);
  dblwr::Force_crash = page_id;
  return RET_PASS;
}

Ret_t Tester::corrupt_ondisk_page0(std::vector<std::string> &tokens) noexcept {
  TLOG("Tester::corrupt_ondisk_page0()");
  ut_ad(tokens.size() == 2);
  ut_ad(tokens[0] == "corrupt_ondisk_page0");

  const std::string table_name = tokens[1];
  const dict_table_t *const table = is_table_open(table_name);
  ut_ad(table != nullptr);

  return clear_page_prefix(table->space, 0, FIL_PAGE_DATA);
}

DISPATCH_FUNCTION_DEF(Tester::make_page_dirty) {
  TLOG("Tester::make_page_dirty()");
  ut_ad(tokens.size() == 3);
  ut_ad(tokens[0] == "make_page_dirty");

  mtr_t mtr;
  std::string space_id_str = tokens[1];
  std::string page_no_str = tokens[2];
  space_id_t space_id = std::stoul(space_id_str);
  space_id_t page_no = std::stoul(page_no_str);

  page_id_t page_id(space_id, page_no);

  fil_space_t *space = fil_space_acquire_silent(space_id);

  if (space == nullptr) {
    return RET_FAIL;
  }

  if (page_no > space->size) {
    fil_space_release(space);
    return RET_FAIL;
  }

  mtr.start();

  buf_block_t *block = buf_page_get(page_id, page_size_t(space->flags),
                                    RW_X_LATCH, UT_LOCATION_HERE, &mtr);

  if (block != nullptr) {
    byte *page = block->frame;
    page_type_t page_type = fil_page_get_type(page);

    /* Don't dirty a page that is not yet used. */
    if (page_type != FIL_PAGE_TYPE_ALLOCATED) {
      ib::info(ER_IB_MSG_574)
          << "Dirtying page: " << page_id
          << ", page_type=" << fil_get_page_type_str(page_type);

      mlog_write_ulint(page + FIL_PAGE_TYPE, page_type, MLOG_2BYTES, &mtr);
    }
  }

  mtr.commit();

  fil_space_release(space);

  if (block != nullptr) {
    buf_flush_sync_all_buf_pools();
  }

  return RET_PASS;
}

Ret_t Tester::print_dblwr_has_encrypted_pages(
    std::vector<std::string> &) noexcept {
  std::ostringstream sout;
  if (dblwr::has_encrypted_pages()) {
    std::string m1("Double write file has encrypted pages.");
    TLOG(m1);
    sout << m1;
  } else {
    std::string m1("Double write file has NO encrypted pages.");
    TLOG(m1);
    sout << m1;
  }

  set_output(sout);
  return RET_PASS;
}

void Tester::close_table(dict_table_t *table) noexcept {
  for (auto iter = m_open_tables.begin(); iter != m_open_tables.end(); ++iter) {
    dict_table_t *tmp = *iter;
    if (tmp == table) {
      m_open_tables.erase(iter);
      break;
    }
  }

  dict_table_close(table, false, false);
}

void Tester::destroy() noexcept {
  for (auto iter = m_open_tables.begin(); iter != m_open_tables.end(); ++iter) {
    dict_table_t *table = *iter;
    dict_table_close(table, false, false);
  }

  m_open_tables.clear();
}

Ret_t Tester::find_flush_sync_lsn(std::vector<std::string> &tokens) noexcept {
  ut_ad(tokens.size() == 1);
  ut_ad(tokens[0] == "find_flush_sync_lsn");

  std::ostringstream sout;
  sout << get_flush_sync_lsn();
  set_output(sout);
  return RET_PASS;
}

Ret_t Tester::run(const std::string &cmdline) noexcept {
  Ret_t ret = RET_PASS;
  std::vector<std::string> tokens;

  std::istringstream sin(cmdline);

  while (!sin.eof()) {
    std::string token;
    std::getline(sin, token, ' ');
    tokens.push_back(token);
  }

  std::string command = tokens[0];

  /* Save the current command token.  This will be the value of the
  innodb_interpreter variable. */
  m_command = command;

  if (command == "init") {
    init();
  } else if (command == "destroy") {
    destroy();
  } else if (command == "buf_flush_sync_all_buf_pools") {
    buf_flush_sync_all_buf_pools();
    TLOG("Executed buf_flush_sync_all_buf_pools()");
    ret = RET_PASS;
  } else {
    ret = RET_CMD_TBD;
  }

  /* Second approach. */
  if (ret == RET_CMD_TBD) {
    auto cmd = m_dispatch.find(command);

    if (cmd != m_dispatch.end()) {
      ret = CALL_MEMBER_FN(*this, cmd->second)(tokens);
    } else {
      ret = RET_CMD_TBD;
    }
  }

  if (ret == RET_CMD_TBD) {
    ret = RET_FAIL;
  }

  return ret;
}

void Tester::update_thd_variable() noexcept {
  char **output = thd_innodb_interpreter_output(m_thd);
  *output = const_cast<char *>(m_log.c_str());

  char **output2 = thd_innodb_interpreter(m_thd);
  *output2 = const_cast<char *>(m_command.c_str());
}

void Tester::set_output(const std::ostringstream &sout) noexcept {
  m_log = sout.str();
}

void Tester::set_output(const std::string &log) noexcept { m_log = log; }

void Tester::clear_output() noexcept { m_log = ""; }

void Tester::append_output(const std::string &log) noexcept { m_log += log; }

int interpreter_run(const char *command) noexcept {
  return (int)tl_interpreter.run(command);
}

}  // namespace ib

void ib_interpreter_update(MYSQL_THD thd [[maybe_unused]],
                           SYS_VAR *var [[maybe_unused]],
                           void *var_ptr [[maybe_unused]],
                           const void *save [[maybe_unused]]) {
  TLOG("ib_interpreter_update");

  /* Point the THD variables - innodb_interpreter and innodb_interpreter_output
  to the correct values. */
  ib::tl_interpreter.update_thd_variable();
}

int ib_interpreter_check(THD *thd [[maybe_unused]],
                         SYS_VAR *var [[maybe_unused]], void *save,
                         struct st_mysql_value *value) {
  TLOG("ib_interpreter_check");
  char buff[STRING_BUFFER_USUAL_SIZE];
  int len = sizeof(buff);

  ut_a(save != nullptr);
  ut_a(value != nullptr);

  const char *cmd = value->val_str(value, buff, &len);

  int ret = ib::interpreter_run(cmd ? cmd : "");

  TLOG("ib_interpreter_check() is returning: " << ret);
  *static_cast<const char **>(save) = cmd;
  return ret;
}
#endif /* UNIV_DEBUG */
