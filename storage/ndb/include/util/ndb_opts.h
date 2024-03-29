/*
   Copyright (c) 2004, 2023, Oracle and/or its affiliates.

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

#ifndef _NDB_OPTS_H
#define _NDB_OPTS_H

#include <ndb_global.h>

#include "my_alloc.h" // MEM_ROOT
#include "my_sys.h"   // loglevel needed by my_getopt.h
#include "my_getopt.h"
#include "util/BaseString.hpp"
#include "util/require.h"

#ifdef OPTEXPORT
#define OPT_EXTERN(T,V,I) T V I
#else
#define OPT_EXTERN(T,V,I) extern T V
#endif

#define NONE
OPT_EXTERN(int,opt_ndb_nodeid,NONE);
OPT_EXTERN(bool,opt_ndb_endinfo,=0);
OPT_EXTERN(bool,opt_ndb_optimized_node_selection,NONE);
OPT_EXTERN(const char *,opt_ndb_connectstring,=0);
OPT_EXTERN(int, opt_connect_retry_delay,NONE);
OPT_EXTERN(int, opt_connect_retries,NONE);
OPT_EXTERN(const char *,opt_charsets_dir,=0);

#ifndef NDEBUG
OPT_EXTERN(const char *,opt_debug,= 0);
#endif

enum ndb_std_options {
  /*
    --ndb-connectstring=<connectstring> has short form 'c'
  */
  OPT_NDB_CONNECTSTRING = 'c',

  /*
    For arguments that have neither a short form option or need
    special processing in 'get_one_option' callback
  */
  NDB_OPT_NOSHORT = 256,

 /*
   should always be last in this enum and will be used as the
   start value by programs which use 'ndb_std_get_one_option' and
   need to define their own arguments with special processing
 */
  NDB_STD_OPTIONS_LAST
};


namespace NdbStdOpt {

static constexpr struct my_option usage =
  { "usage", '?', "Display this help and exit.",
    nullptr, nullptr, nullptr, GET_NO_ARG, NO_ARG,
    0, 0, 0, nullptr, 0, nullptr };

static constexpr struct my_option help =
  { "help", '?', "Display this help and exit.",
    nullptr, nullptr, nullptr, GET_NO_ARG, NO_ARG,
    0, 0, 0, nullptr, 0, nullptr };

static constexpr struct my_option version =
  { "version", 'V', "Output version information and exit.",
    nullptr, nullptr, nullptr, GET_NO_ARG, NO_ARG,
    0, 0, 0, nullptr, 0, nullptr };

static constexpr struct my_option ndb_connectstring =
  { "ndb-connectstring", OPT_NDB_CONNECTSTRING,
    "Set connect string for connecting to ndb_mgmd. "
    "Syntax: \"[nodeid=<id>;][host=]<hostname>[:<port>]\". "
    "Overrides specifying entries in NDB_CONNECTSTRING and my.cnf",
    &opt_ndb_connectstring, nullptr, nullptr, GET_STR, REQUIRED_ARG,
    0, 0, 0, nullptr, 0, nullptr };

static constexpr struct my_option mgmd_host =
  { "ndb-mgmd-host", NDB_OPT_NOSHORT, "",
    &opt_ndb_connectstring, nullptr, nullptr, GET_STR, REQUIRED_ARG,
    0, 0, 0, nullptr, 0, nullptr };

static constexpr struct my_option connectstring =
  { "connect-string", NDB_OPT_NOSHORT, "",
    &opt_ndb_connectstring, nullptr, nullptr, GET_STR, REQUIRED_ARG,
    0, 0, 0, nullptr, 0, nullptr };

static constexpr struct my_option ndb_nodeid =
  { "ndb-nodeid", NDB_OPT_NOSHORT,
    "Set node id for this node. Overrides node id specified "
    "in --ndb-connectstring.",
    &opt_ndb_nodeid, nullptr, nullptr, GET_INT, REQUIRED_ARG,
    0, 0, 0, nullptr, 0, nullptr };

static constexpr struct my_option optimized_node_selection =
  { "ndb-optimized-node-selection", NDB_OPT_NOSHORT,
    "Select nodes for transactions in a more optimal way",
    &opt_ndb_optimized_node_selection, nullptr, nullptr, GET_BOOL, OPT_ARG,
    1, 0, 0, nullptr, 0, nullptr};

static constexpr struct my_option charsets_dir =
  { "character-sets-dir", NDB_OPT_NOSHORT, "Directory where character sets are.",
    & opt_charsets_dir, nullptr, nullptr, GET_STR, REQUIRED_ARG,
    0, 0, 0, nullptr, 0, nullptr};

static constexpr struct my_option connect_retry_delay =
  { "connect-retry-delay", NDB_OPT_NOSHORT,
    "Set connection time out."
    " This is the number of seconds after which the tool tries"
    " reconnecting to the cluster.",
    &opt_connect_retry_delay, nullptr, nullptr, GET_INT, REQUIRED_ARG,
    5, 1, INT_MAX, nullptr, 0, nullptr};

static constexpr struct my_option connect_retries =
  { "connect-retries", NDB_OPT_NOSHORT, "Set connection retries."
    " This is the number of times the tool tries connecting"
    " to the cluster. -1 for eternal retries",
    &opt_connect_retries, nullptr, nullptr, GET_INT, REQUIRED_ARG,
    12, -1, INT_MAX, nullptr, 0, nullptr};

#ifndef NDEBUG
static constexpr struct my_option debug =
  { "debug", '#', "Output debug log. Often this is 'd:t:o,filename'.",
    &opt_debug, nullptr, nullptr, GET_STR, OPT_ARG,
    0, 0, 0, nullptr, 0, nullptr };
#endif

static constexpr struct my_option end_of_options =
  { nullptr, 0, nullptr,
    nullptr, nullptr, nullptr, GET_NO_ARG, NO_ARG,
    0, 0, 0, nullptr, 0, nullptr};

} // namespace

#ifdef NDEBUG
#define NDB_STD_OPT_DEBUG
#else
#define NDB_STD_OPT_DEBUG NdbStdOpt::debug ,
#endif

void ndb_std_print_version();

void ndb_opt_set_usage_funcs(void (*short_usage)(void),
                             void (*usage)(void));
bool
ndb_std_get_one_option(int optid,
		       const struct my_option *opt [[maybe_unused]],
                       char *argument);

void ndb_short_usage_sub(const char* extra);

bool ndb_is_load_default_arg_separator(const char* arg);


/*
 * ndb_option
 *
 * When adding the same non trivial command line options to several programs
 * one can derive a class from ndb_option and then pass an instance in
 * my_option::app_type supported by my_getopt.
 *
 * Note that when using Ndb_opts with default get opt function one must set
 * appt_type to either nullptr or an instance of a class derived from
 * ndb_option.
 *
 * One need to implement get_option() function which will be called when
 * option is parsed, due to call Ndb_opts::handle_options().
 *
 * Also post_process() function should be implemented if one need to process
 * option after all options have been parsed.
 *
 * If any option need post processing the application must call
 * ndb_option::post_process_options() after the call do Ndb_opts::handle_options().
 *
 * If an option need post processing, the get_option() function must register
 * option in a list by calling ndb_option::push_back().
 *
 * Options are post processed in the order they are parsed and registered in
 * the list.
 *
 * In a similar way reset_option() function should be implemented if one need to
 * reset the option state to its default.
 *
 * If any option need to be reset the application must call
 * ndb_option::reset_options() after the call do Ndb_opts::handle_options().
 *
 * If an option need to be reset, the get_option() function must register
 * option in a list by calling ndb_option::push_back().
 *
 * Options are reset in the order they are parsed and registered in
 * the list.
 *
 * See also ndb_password_option and ndb_password_option_from_stdin below.
 */

class ndb_option
{
public:
  ndb_option();
  static bool get_one_option(int optid, const my_option *opt, char *arg);
  static bool post_process_options();
  static void reset_options();
  virtual ~ndb_option() {}
protected:
  virtual bool get_option(int optid, const my_option *opt, char *arg) = 0;
  virtual bool post_process() = 0;
  virtual void reset() = 0;
  void push_back();
  void erase();
private:
  /*
   * Intrusive double linked list keeping option order for post processing.
   * --skip-XXX removes option from list.
   */
  static ndb_option* m_first;
  static ndb_option* m_last;
  ndb_option* m_prev;
  ndb_option* m_next;
};

/*
 * Currently there are essential three ways to pass a password to application
 * using command line options:
 *
 * program --xxx-password=SECRET
 * program --xxx-password              (reading one line from terminal)
 * program --xxx-password-from-stdin   (read one line from stdin)
 *
 * The two first forms are handled by ndb_password_option, and the last by
 * ndb_password_from_stdin.
 *
 * Both classes should use a common instance of ndb_password_state.
 *
 * When reading password from stdin or terminal first line without end of line
 * markers are used as password.
 *
 * Multiple password options can be given on command line and in defaults file,
 * but at most one must be active when all options are parsed.
 *
 * If same option is given several times, the last is the one that counts.
 *
 * To unset an option add --skip-option.
 *
 * Example, the command line below will result in that password xxx is read
 * from terminal:
 *
 * $ echo TOP-SECRET | program --xxx-password=SECRET \
 *                             --xxx-password-from-stdin \
 *                             --xxx-password \
 *                             --skip--xxx-password-from-stdin ...
 * Enter xxx password:
 *
 * The reading from stdin and terminal is not done while parsing the option but
 * done while post processing options after all options have been parsed.  This
 * to not read more than at most once from a file or terminal for a password.
 *
 * For programs taking two password (such as ndbxfrm) the post processing is
 * done in the same order that respective active command line options was given.
 *
 * At most one password can be read from stdin, unless stdin in is a terminal.
 *
 * Example,
 * $ ndbxfrm --decrypt-password --encrypt-password ...
 * Enter decrypt password:
 * Enter encrypt password:
 */

class ndb_password_state
{
public:
  using byte = unsigned char;

  ndb_password_state(const char prefix[], const char prompt[])
      : ndb_password_state(prefix, prompt, PASSWORD) {}
  const byte* get_key() const;
  size_t get_key_length() const;
  const char* get_password() const { return m_password; }
  size_t get_password_length() { return m_password_length; }
  bool have_password_option() const { return (m_option_count > 0); }
  BaseString get_error_message() const;
  bool is_password() const { return (m_kind == PASSWORD); }
  bool is_key() const { return (m_kind == KEY); }
  void reset();

protected:
  enum kind_t { PASSWORD = 0, KEY = 1 };
  ndb_password_state(const char prefix[], const char prompt[], kind_t kind);

private:
  friend class ndb_password_option;
  friend class ndb_password_from_stdin_option;
  enum status
  {
    NO_PASSWORD = 0,
    HAVE_PASSWORD = 1, // m_password points to valid password
    /*
     * PENDING_PASSWORD - m_password_buffer contains a valid password, not yet
     * committed to m_password.
     */
    PENDING_PASSWORD = 2,
    ERR_MULTIPLE_SOURCES = -1,
    ERR_BAD_STDIN = -2,
    ERR_BAD_TTY = -3,
    ERR_TOO_LONG = -4,
    ERR_BAD_CHAR = -5,
    ERR_NO_END = -6,
    ERR_ODD_HEX_LENGTH = -7
  };
  enum password_source { PS_NONE, PS_ARG, PS_TTY, PS_STDIN };
  static constexpr size_t PWD_BUF_SIZE = 1025;
  static constexpr size_t MAX_KEY_LEN = 512;
  /*
   * PWD_BUF_SIZE must be big enough to handle two hex digits per key byte
   * plus terminating new line when reading from stdin or tty.
   */
  static_assert(2 * MAX_KEY_LEN + 1 <= PWD_BUF_SIZE);
  static constexpr size_t MAX_PWD_LEN = 1024;
  // PWD_BUF_SIZE need also to count terminating new line or null character.
  static_assert(MAX_PWD_LEN + 1 <= PWD_BUF_SIZE);

  const char* get_prefix() const { return m_prefix.c_str(); }
  size_t get_prefix_length() const { return m_prefix.length(); }
  int get_from_tty();
  int get_from_stdin();
  const char* kind_str() const { return kind_name[m_kind]; }
  int set_key(const char src[], size_t len);
  int set_password(const char src[], size_t len);
  void clear_password();
  void add_option_usage() { m_option_count++; }
  void remove_option_usage();
  bool is_in_error() const { return m_status < 0; }
  void set_error(enum status err) { require(int{err} < 0); m_status = err; }
  void set_status(enum status s) { require(int{s} >= 0); m_status = s; }
  void commit_password();
  bool verify_option_name(const char opt_name[],
                          const char extra[] = nullptr) const;

private:
  static constexpr const char* kind_name[2] = {"password", "key"};
  BaseString m_prompt;
  char* m_password;
  const kind_t m_kind;
  enum status m_status;
  int m_option_count;  // How many options that is about to set password
  size_t m_password_length;
  char m_password_buffer[PWD_BUF_SIZE];
  BaseString m_prefix;
};

class ndb_key_state : public ndb_password_state
{
public:
  ndb_key_state(const char prefix[], const char prompt[])
      : ndb_password_state(prefix, prompt, ndb_password_state::KEY) {}
};

class ndb_password_option: ndb_option
{
public:
  ndb_password_option(ndb_password_state& pwd_buf);
  bool get_option(int optid, const my_option *opt, char *arg) override;
  bool post_process() override;
  void reset() override;
private:
  ndb_password_state& m_password_state;
  // One of PS_NONE, PS_ARG, PS_TTY
  ndb_password_state::password_source m_password_source;
};

class ndb_key_option : public ndb_password_option
{
public:
  ndb_key_option(ndb_key_state& pwd_buf) : ndb_password_option(pwd_buf) {}
};

class ndb_password_from_stdin_option: ndb_option
{
public:
  ndb_password_from_stdin_option(ndb_password_state& pwd_buf);
  bool get_option(int optid, const my_option *opt, char *arg) override;
  bool post_process() override;
  void reset() override;

  bool opt_value;
private:
  ndb_password_state& m_password_state;
  // One of PS_NONE, PS_STDIN
  ndb_password_state::password_source m_password_source;
};

class ndb_key_from_stdin_option : public ndb_password_from_stdin_option
{
public:
  ndb_key_from_stdin_option(ndb_key_state& pwd_buf)
      : ndb_password_from_stdin_option(pwd_buf) {}
};

class Ndb_opts {
public:
  Ndb_opts(int & argc_ref, char** & argv_ref,
           struct my_option * long_options,
           const char * default_groups[] = nullptr);

  ~Ndb_opts();

  void set_usage_funcs(void(*short_usage_fn)(void),
                       void(* long_usage_fn)(void) = nullptr);

  int handle_options(bool (*get_opt_fn)(int, const struct my_option *,
                                        char *) = ndb_std_get_one_option) const;
  void usage() const;

  static void registerUsage(Ndb_opts *);
  static void release();

private:
  struct MEM_ROOT opts_mem_root;
  int * main_argc_ptr;
  char *** main_argv_ptr;
  const char ** mycnf_default_groups;
  struct my_option * options;
  void (*short_usage_fn)(void), (*long_usage_extra_fn)(void);
};

#endif /*_NDB_OPTS_H */
