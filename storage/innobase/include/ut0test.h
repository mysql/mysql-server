/*****************************************************************************

Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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
/*

ABOUT:

If you want to execute some arbitrary code on the server side, for testing
purposes (mtr tests), then this module is for that purpose.  There is no
need to introduce more debug variables to execute code in InnoDB.  Many such
uses can be accomplished using this module.

OVERVIEW:

The idea of this module is to be able to write mtr test case like:

Example Code:

SET SESSION innodb_interpreter = 'init';
SET SESSION innodb_interpreter = 'open_table test/t1';

--echo # Identify the space_id of the given table.
SET SESSION innodb_interpreter = 'find_space_id test/t1';
SELECT @@session.innodb_interpreter_output INTO @space_id;

--echo # Find the root page number of the given table.
SET SESSION innodb_interpreter = 'find_root_page_no test/t1';
SELECT @@session.innodb_interpreter_output INTO @page_no;

--echo # Find the on-disk page type of the given page.
SET @cmd = CONCAT('find_ondisk_page_type ', @space_id, ' ', @page_no);
SET SESSION innodb_interpreter = @cmd;
SELECT @@session.innodb_interpreter_output INTO @page_type;
SELECT @page_type;

SET SESSION innodb_interpreter = 'corrupt_ondisk_root_page test/t1';
SET SESSION innodb_interpreter = 'destroy';

INTRODUCTION:

There are two system variables innodb_interpreter and
innodb_interpreter_output.  We issue commands via innodb_interpreter variable
and we access its output via innodb_interpreter_output variable. Refer to the
above example.

USAGE:

The basic way to use this infrastructure is as follows:

SET SESSION innodb_interpreter = 'command arg1 arg2 ... argN';
SELECT @@session.innodb_interpreter_output;

We send some command and arguments to the innodb_interpreter variable.  And we
look at the results in the innodb_interpreter_output variable.

ADDING NEW COMMANDS:

If you want to add a new command, here are the list of things you need to do in
this module:

1. Decide the command name.  Let 'hello_world' be the command name and let is
take 3 arguments - arg1, arg2 and arg3.  So the command will be invoked as
follows:

   SET SESSION innodb_interpreter = 'hello_world arg1 arg2 arg3';

2. Add member function with same name as the command to the Tester class. Its
   signature would be as follows:

   [[nodiscard]] Ret_t hello_world(std::vector<std::string> &tokens) noexcept;

   The command has 4 tokens - the command name followed by 3 arguments.  The
   vector will be of size 4.

3. In the constructor Tester::Tester() add the line:

   DISPATCH(hello_world);

4. While implementing the member function hello_world(), populate the member
   Tester::m_log with the output you want to give the user for the given
   command.

5. Execute the command as follows:

   SET SESSION innodb_interpreter = 'hello_world one two three';

   Check its output as follows:

   SELECT @@session.innodb_interpreter_output;

NOTE: You can make the command either stateful or stateless.  It is up to you.

*/

#ifndef ut0test_h
#define ut0test_h
#ifdef UNIV_DEBUG

#include <string>
#include <vector>
#include "current_thd.h"
#include "fil0fil.h"
#include "mysql/plugin.h"

/** This is the prefix used for the log messages that will be updated in the
innodb_interpreter_output system variable. */
#define TPREFIX "[ib::Tester] "

#define TLOG(x)                                                              \
  {                                                                          \
    std::cout << "[TLOG] thread=" << std::this_thread::get_id() << ": " << x \
              << std::endl;                                                  \
  }

/** A macro to log output in both the server error log (via std::cout, the
standard output) and the server system variable innodb_interpreter_output.
To use this macro, there must be a std::ostringstream object "sout" available
in the scope in which it is used. */
#define XLOG(x) \
  {             \
    TLOG(x)     \
    sout << x;  \
  }

namespace ib {

/** This class contains implementations of the commands that can be executed
at server side by passing them via the innodb_interpreter system variable. */
struct Tester {
 public:
  /** The return status code used by the various commands in this module. */
  enum Ret_t {
    RET_PASS = 0,
    RET_FAIL = 1,

    /* The command is yet to handled. */
    RET_CMD_TBD = 2
  };

  /** Default constructor. */
  Tester() noexcept;

  /** Run the given command.
   @param[in]  cmd   the command to run.
   @return 0 on success.
   @return error code on failure. */
  [[nodiscard]] Ret_t run(ulong cmd) noexcept;

  /** Run the given command.
   @param[in]  cmd   the command to run.
   @return 0 on success.
   @return error code on failure. */
  [[nodiscard]] Ret_t run(const std::string &cmd) noexcept;

  /** Get the last generated output.
  @return the last generated output. */
  [[nodiscard]] const char *get_last_log() const noexcept {
    return m_log.c_str();
  }

  /** Let the thread-variable innodb_interpreter_output point to the current
  output.  */
  void update_thd_variable() noexcept;

 private:
  /** Initialize the internal state of the tester. */
  void init() noexcept;

  /** Open the specified table.
  @param[in]   tokens   the given command line
  @return RET_PASS on success, or the error code. */
  [[nodiscard]] Ret_t open_table(std::vector<std::string> &tokens) noexcept;

  /** Close the given table.
  @param[in]   table   the table object */
  void close_table(dict_table_t *table) noexcept;

  /** Find the space_id of the given table.
  @param[in]   tokens   the given command line
  @return RET_PASS on success, or the error code. */
  [[nodiscard]] Ret_t find_space_id(std::vector<std::string> &tokens) noexcept;

  /** Find the root page of the given table.
  @param[in]   tokens   the given command line
  @return RET_PASS on success, or the error code. */
  [[nodiscard]] Ret_t find_root_page_no(
      std::vector<std::string> &tokens) noexcept;

  /** Find the on-disk page type of the given page.
  @param[in]   tokens   the given command line
  @return RET_PASS on success, or the error code. */
  [[nodiscard]] Ret_t find_ondisk_page_type(
      std::vector<std::string> &tokens) noexcept;

  /** Find the FIL_PAGE_LSN of the given page.
  @param[in]   tokens   the given command line
  @return RET_PASS on success, or the error code. */
  [[nodiscard]] Ret_t find_fil_page_lsn(
      std::vector<std::string> &tokens) noexcept;

  /** Find the flush sync lsn from the buffer pool module.
  @param[in]   tokens   the given command line
  @return RET_PASS on success, or the error code. */
  [[nodiscard]] Ret_t find_flush_sync_lsn(
      std::vector<std::string> &tokens) noexcept;

  /** Print the page type of pages in dblwr file to server error log.
  @param[in]   tokens   the given command line
  @return RET_PASS on success, or the error code. */
  [[nodiscard]] Ret_t print_dblwr_has_encrypted_pages(
      std::vector<std::string> &tokens) noexcept;

  /** Obtain the file name of the given tablespace
  @param[in]   tokens   the given command line
  @return RET_PASS on success, or the error code. */
  [[nodiscard]] Ret_t find_tablespace_file_name(
      std::vector<std::string> &tokens) noexcept;

  /** A macro to declare a dispatch function or a command function.  They all
  have the same signature.
  @param[in]   func_  the function that is being declared. */
#define DISPATCH_FUNCTION(func_)                        \
  /* @param[in] tokens   the command line */            \
  /* @return RET_PASS on success, or the error code. */ \
  [[nodiscard]] Ret_t func_(std::vector<std::string> &tokens) noexcept

  /** Count various page_types for given tablespace. */
  DISPATCH_FUNCTION(count_page_type);

  /** Count various page_types for given tablespace. */
  DISPATCH_FUNCTION(count_used_and_free);

  /** Obtain the page size of the given tablespace. */
  DISPATCH_FUNCTION(find_tablespace_physical_page_size);

  /** Fill the root page of the given table with zeroes. */
  DISPATCH_FUNCTION(make_ondisk_root_page_zeroes);

  /** Make the page dirty.  It takes two arguments.
     make_page_dirty space_id page_no */
  DISPATCH_FUNCTION(make_page_dirty);

  /** Corrupt the root page of the given table.
  @param[in]   tokens   the given command line
  @return RET_PASS on success, or the error code. */
  [[nodiscard]] Ret_t corrupt_ondisk_root_page(
      std::vector<std::string> &tokens) noexcept;

  /** Corrupt the first page of the given tablespace.
  @param[in]   tokens   the given command line
  @return RET_PASS on success, or the error code. */
  [[nodiscard]] Ret_t corrupt_ondisk_page0(
      std::vector<std::string> &tokens) noexcept;

  /** Set the dblwr::Force_crash to the desired page.  This will
  crash the server after flushing the page to dblwr.
  @return RET_PASS on success, or the error code. */
  DISPATCH_FUNCTION(dblwr_force_crash);

  /** Destroy the tester object. */
  void destroy() noexcept;

 private:
  /** Check if the given table is already opened.
  @param[in]  table_name  name of the table to open.
  @return if table is already open return its pointer.
  @return if table is NOT already open return nullptr. */
  [[nodiscard]] dict_table_t *is_table_open(
      const std::string &table_name) const noexcept;

  /** Set the output value of the interpreter.
  @param[in]  sout   the output string stream containing the output string. */
  void set_output(const std::ostringstream &sout) noexcept;

  /** Set the output value of the interpreter to the given value.
  @param[in]  log   the output string */
  void set_output(const std::string &log) noexcept;

  /** Append the given string to the output value of the interpreter.
  @param[in]  log   the output string to be appended*/
  void append_output(const std::string &log) noexcept;

  /** Make the output empty. */
  void clear_output() noexcept;

  /** Make the first prefix_length bytes of the given page as zeroes.
  @param[in] space_id  the tablespace identifier of the page to be zeroed.
  @param[in] page_no   page number within the given tablespace.
  @param[in] prefix_length  the length of the page, from beginning, to be
  zeroed.
  @return RET_PASS on success, or error code on failure. */
  Ret_t clear_page_prefix(const space_id_t space_id, page_no_t page_no,
                          const size_t prefix_length);

 private:
  /** List of open tables. */
  std::list<dict_table_t *, ut::allocator<dict_table_t *>> m_open_tables{};

  /** Current thread object. */
  THD *m_thd{};

  /** The actual log data that is shared with the client via the thread-variable
  innodb_interpreter_output. (Search for the variable interpreter_output in
  the file ha_innodb.cc) */
  std::string m_log{};

  /** The latest command executed. */
  std::string m_command{};

  using Function_executor = Ret_t (Tester::*)(std::vector<std::string> &);
  using Pair = std::pair<const std::string, Function_executor>;
  using Allocator = ut::allocator<Pair>;

  /** Mapping b/w the command name and the function to execute. */
  std::map<std::string, Function_executor, std::less<std::string>, Allocator>
      m_dispatch;
};

/** The main function to execute the commands in the tester.
@param[in]  command  the command to execute.
@return the error code. */
[[nodiscard]] int interpreter_run(const char *command) noexcept;

}  // namespace ib

/** Update the innodb_interpreter_output system variable to let the user
access the output generated by the tester. Refer to mysql_var_update_func() for
details of the function signature.
@param[in]  thd            thread handle
@param[in]  var            dynamic variable being altered
@param[in]  var_ptr        pointer to dynamic variable
@param[in]  save           pointer to temporary storage. */
void ib_interpreter_update(MYSQL_THD thd, SYS_VAR *var, void *var_ptr,
                           const void *save);

/**Check whether given command is valid for the InnoDB interpreter
Refer to mysql_var_check_func() for more details.
@param[in]      thd             thread handle
@param[in]      var             pointer to system variable
@param[out]     save            immediate result for update function
@param[in]      value           incoming string
@return 0 for valid command. */
int ib_interpreter_check(THD *thd, SYS_VAR *var, void *save,
                         struct st_mysql_value *value);

#endif  /* UNIV_DEBUG */
#endif  // ut0test_h
