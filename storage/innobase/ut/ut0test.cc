/*****************************************************************************

Copyright (c) 2020 Oracle and/or its affiliates.

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
  buf_block_t *block = buf_page_get(page_id, page_size, RW_X_LATCH, &mtr);
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
  std::string space_id_str = tokens[1];
  std::string page_no_str = tokens[2];

  space_id_t space_id = std::stoul(space_id_str);
  space_id_t page_no = std::stoul(page_no_str);

  page_id_t page_id(space_id, page_no);

  char *filename = fil_space_get_first_path(space_id);
  TLOG("filename=" << filename);
  FILE *fin = fopen(filename, "r");

  if (fin == NULL) {
    TLOG("fopen() failed. file=" << filename);
    ut_free(filename);
    return RET_FAIL;
  }

  bool found;
  page_size_t page_size = fil_space_get_page_size(space_id, &found);
  ut_ad(found);

  // page_size_t page_size = dict_table_page_size(table);
  ulint offset = page_no * page_size.physical();
  int ret = fseek(fin, offset, SEEK_SET);
  ut_ad(ret == 0);

  byte buf[FIL_PAGE_DATA];
  size_t n = fread(buf, FIL_PAGE_DATA, 1, fin);
  ut_ad(n == 1);
  fclose(fin);

  byte *page = buf;
  page_type_t type = fil_page_get_type(page);
  const char *page_type = fil_get_page_type_str(type);

  TLOG("page_type=" << type);
  TLOG("page_type=" << page_type);

  std::ostringstream sout;
  sout << page_type;
  set_output(sout);

  ut_free(filename);
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
  ut_free(filename);
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

  std::string table_name = tokens[1];
  dict_table_t *table = is_table_open(table_name);

  ut_ad(table != nullptr);

  space_id_t space_id = table->space;

  dict_index_t *clust_index = table->first_index();
  page_no_t root_page_no = clust_index->page;

  page_id_t page_id(space_id, root_page_no);

  char *filename = fil_space_get_first_path(table->space);
  TLOG("filename=" << filename);
  FILE *fin = fopen(filename, "r+");
  ut_ad(fin != NULL);

  page_size_t page_size = dict_table_page_size(table);
  size_t nbytes = page_size.logical();

  ulint offset = root_page_no * nbytes;

  std::unique_ptr<byte[]> buf(new byte[nbytes]);

  /* Make the contents zeroes. */
  memset(buf.get(), 0x00, nbytes);
  int st = fseek(fin, offset, SEEK_SET);
  ut_ad(st == 0);

  size_t n = fwrite(buf.get(), nbytes, 1, fin);
  ut_ad(n == 1);

  fflush(fin);
  TLOG("Filled with zeroes: page_id=" << page_id);
  fclose(fin);

  ut_free(filename);
  return RET_PASS;
}

Ret_t Tester::corrupt_ondisk_root_page(
    std::vector<std::string> &tokens) noexcept {
  std::ostringstream sout;

  TLOG("Tester::corrupt_ondisk_root_page()");
  ut_ad(tokens.size() == 2);
  ut_ad(tokens[0] == "corrupt_ondisk_root_page");

  std::string table_name = tokens[1];
  dict_table_t *table = is_table_open(table_name);

  if (table == nullptr) {
    std::vector<std::string> cmd{"open_table", table_name};
    Ret_t st = open_table(cmd);

    if (st != RET_PASS) {
      XLOG("Failed to open table: " << table_name);
      return st;
    }

    table = is_table_open(table_name);
  }

  if (table == nullptr) {
    XLOG("Failed to open table: " << table_name);
    return RET_FAIL;
  }

  space_id_t space_id = table->space;

  dict_index_t *clust_index = table->first_index();
  page_no_t root_page_no = clust_index->page;

  page_id_t page_id(space_id, root_page_no);

  char *filename = fil_space_get_first_path(table->space);
  TLOG("filename=" << filename);

  page_size_t page_size = dict_table_page_size(table);
  size_t nbytes = page_size.logical();

  close_table(table);

  FILE *fin = fopen(filename, "r+");

  if (fin == NULL) {
    XLOG("Failed to open file: " << filename << "(" << strerror(errno) << ")");
    ut_ad(fin != NULL);
    ut_free(filename);
    return RET_FAIL;
  }

  ulint offset = root_page_no * nbytes;

  std::unique_ptr<byte[]> buf(new byte[FIL_PAGE_DATA]);

  /* Corrupt the contents. */
  memset(buf.get(), 0x11, FIL_PAGE_DATA);
  int st = fseek(fin, offset, SEEK_SET);
  ut_ad(st == 0);

  size_t n = fwrite(buf.get(), FIL_PAGE_DATA, 1, fin);
  ut_ad(n == 1);

  fflush(fin);
  TLOG("Successfully corrupted page_id=" << page_id);
  fclose(fin);

  ut_free(filename);
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

  std::string table_name = tokens[1];
  dict_table_t *table = is_table_open(table_name);

  ut_ad(table != nullptr);

  space_id_t space_id = table->space;
  page_id_t page_id(space_id, 0);

  fil_space_t *space = fil_space_get(space_id);
  ut_ad(space != nullptr);

  page_no_t page_no = 0;
  fil_node_t *node = space->get_file_node(&page_no);
  if (node->is_open) {
    /* When the space file is currently open we are not able to write to it
     directly on Windows. We must use the currently opened handle. Moreover,
     on Windows a file opened for the AIO access must be accessed only by AIO
     methods. The AIO requires the operations to be aligned and with size
     divisible by OS block size, so we first read block of the first page, to
     corrupt it and write back. */

    IORequest read_io_type(IORequest::READ);
    IORequest write_io_type(IORequest::WRITE);
    std::array<byte, OS_FILE_LOG_BLOCK_SIZE> buf;
    ssize_t nbytes = os_aio_func(
        read_io_type, AIO_mode::SYNC, node->name, node->handle, buf.data(), 0,
        OS_FILE_LOG_BLOCK_SIZE, false, nullptr, nullptr);
    if (nbytes != OS_FILE_LOG_BLOCK_SIZE) {
      TLOG("Could not corrupt page_id=" << page_id
                                        << ", error reading first page");
    }
    memset(buf.data(), 0x00, FIL_PAGE_DATA);

    nbytes = os_aio_func(write_io_type, AIO_mode::SYNC, node->name,
                         node->handle, buf.data(), 0, OS_FILE_LOG_BLOCK_SIZE,
                         false, nullptr, nullptr);
    if (nbytes == OS_FILE_LOG_BLOCK_SIZE) {
      TLOG("Successfully corrupted page_id=" << page_id);
    } else {
      TLOG("Could not corrupt page_id=" << page_id);
    }
  } else {
    std::array<byte, FIL_PAGE_DATA> buf;
    memset(buf.data(), 0x00, FIL_PAGE_DATA);

    char *filename = fil_space_get_first_path(table->space);
    TLOG("filename=" << filename);
    FILE *fin = fopen(filename, "r+");
    ut_ad(fin != NULL);

    ulint offset = 0;

    /* Corrupt the contents. */
    int st = fseek(fin, offset, SEEK_SET);
    ut_ad(st == 0);

    size_t n = fwrite(buf.data(), FIL_PAGE_DATA, 1, fin);
    ut_ad(n == 1);

    fflush(fin);
    TLOG("Successfully corrupted page_id=" << page_id);
    fclose(fin);
    ut_free(filename);
  }

  return RET_PASS;
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

  buf_block_t *block =
      buf_page_get(page_id, page_size_t(space->flags), RW_X_LATCH, &mtr);

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
    std::vector<std::string> &tokens) noexcept {
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

void ib_interpreter_update(MYSQL_THD thd, SYS_VAR *var, void *var_ptr,
                           const void *save) {
  TLOG("ib_interpreter_update");

  /* Point the THD variables - innodb_interpreter and innodb_interpreter_output
  to the correct values. */
  ib::tl_interpreter.update_thd_variable();
}

int ib_interpreter_check(THD *thd, SYS_VAR *var, void *save,
                         struct st_mysql_value *value) {
  TLOG("ib_interpreter_check");
  char buff[STRING_BUFFER_USUAL_SIZE];
  int len = sizeof(buff);

  ut_a(save != nullptr);
  ut_a(value != nullptr);

  const char *cmd = value->val_str(value, buff, &len);

  int ret = ib::interpreter_run(cmd);

  TLOG("ib_interpreter_check() is returning: " << ret);
  *static_cast<const char **>(save) = cmd;
  return ret;
}
#endif /* UNIV_DEBUG */
