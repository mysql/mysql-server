/*
   Copyright (c) 2004, 2021, Oracle and/or its affiliates.

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

#ifdef __cplusplus
extern "C" {
#endif

#ifdef OPTEXPORT
#define OPT_EXTERN(T,V,I) T V I
#else
#define OPT_EXTERN(T,V,I) extern T V
#endif

#define NONE
OPT_EXTERN(int,opt_ndb_nodeid,NONE);
OPT_EXTERN(bool,opt_ndb_endinfo,=0);
OPT_EXTERN(bool,opt_core,NONE);
OPT_EXTERN(bool,opt_ndb_optimized_node_selection,NONE);
OPT_EXTERN(const char *,opt_ndb_connectstring,=0);
OPT_EXTERN(int, opt_connect_retry_delay,NONE);
OPT_EXTERN(int, opt_connect_retries,NONE);

#ifndef NDEBUG
OPT_EXTERN(const char *,opt_debug,= 0);
#endif

#if defined VM_TRACE
#define OPT_WANT_CORE_DEFAULT 1
#else
#define OPT_WANT_CORE_DEFAULT 0
#endif

#define NDB_STD_OPTS_COMMON \
  { "usage", '?', "Display this help and exit.", \
    0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0 }, \
  { "help", '?', "Display this help and exit.", \
    0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0 }, \
  { "version", 'V', "Output version information and exit.", 0, 0, 0, \
    GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0 }, \
  { "ndb-connectstring", OPT_NDB_CONNECTSTRING, \
    "Set connect string for connecting to ndb_mgmd. " \
    "Syntax: \"[nodeid=<id>;][host=]<hostname>[:<port>]\". " \
    "Overrides specifying entries in NDB_CONNECTSTRING and my.cnf", \
    (uchar**) &opt_ndb_connectstring, (uchar**) &opt_ndb_connectstring, \
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },\
  { "ndb-mgmd-host", NDB_OPT_NOSHORT, \
    "same as --ndb-connectstring", \
    (uchar**) &opt_ndb_connectstring, (uchar**) &opt_ndb_connectstring, 0, \
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },\
  { "ndb-nodeid", NDB_OPT_NOSHORT, \
    "Set node id for this node. Overrides node id specified " \
    "in --ndb-connectstring.", \
    (uchar**) &opt_ndb_nodeid, (uchar**) &opt_ndb_nodeid, 0, \
    GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },\
  {"ndb-optimized-node-selection", NDB_OPT_NOSHORT,\
    "Select nodes for transactions in a more optimal way",\
    (uchar**) &opt_ndb_optimized_node_selection,\
    (uchar**) &opt_ndb_optimized_node_selection, 0,\
    GET_BOOL, OPT_ARG, 1, 0, 0, 0, 0, 0},\
  { "connect-string", OPT_NDB_CONNECTSTRING, "same as --ndb-connectstring",\
    (uchar**) &opt_ndb_connectstring, (uchar**) &opt_ndb_connectstring, \
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },\
  { "core-file", NDB_OPT_NOSHORT, "Write core on errors.",\
    (uchar**) &opt_core, (uchar**) &opt_core, 0,\
    GET_BOOL, NO_ARG, OPT_WANT_CORE_DEFAULT, 0, 0, 0, 0, 0},\
  {"character-sets-dir", NDB_OPT_NOSHORT,\
     "Directory where character sets are.", (uchar**) &charsets_dir,\
     (uchar**) &charsets_dir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},\
  {"connect-retry-delay", NDB_OPT_NOSHORT, \
     "Set connection time out." \
     " This is the number of seconds after which the tool tries" \
     " reconnecting to the cluster.", \
     (uchar**) &opt_connect_retry_delay, (uchar**) &opt_connect_retry_delay, 0, GET_INT, \
     REQUIRED_ARG, 5, 1, INT_MAX, 0, 0, 0},\
  {"connect-retries", NDB_OPT_NOSHORT, \
     "Set connection retries." \
     " This is the number of times the tool tries connecting" \
     " to the cluster.", \
     (uchar**) &opt_connect_retries, (uchar**) &opt_connect_retries, 0, GET_INT, \
     REQUIRED_ARG, 12, 0, INT_MAX, 0, 0, 0}

#ifndef NDEBUG
#define NDB_STD_OPTS(prog_name) \
  { "debug", '#', "Output debug log. Often this is 'd:t:o,filename'.", \
    (uchar**) &opt_debug, (uchar**) &opt_debug, \
    0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0 }, \
  NDB_STD_OPTS_COMMON
#else
#define NDB_STD_OPTS(prog_name) NDB_STD_OPTS_COMMON
#endif

void ndb_std_print_version();

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

void ndb_opt_set_usage_funcs(void (*short_usage)(void),
                             void (*usage)(void));
bool
ndb_std_get_one_option(int optid,
		       const struct my_option *opt MY_ATTRIBUTE((unused)),
                       char *argument);

void ndb_short_usage_sub(const char* extra);

bool ndb_is_load_default_arg_separator(const char* arg);

#ifdef __cplusplus
}

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
 * ndb_option::post_process_options() after hte call do Ndb_opts::handle_options().
 *
 * If an option need post processing, the get_option() function must register
 * option in a list by calling ndb_option::push_back().
 *
 * Options are post processed in the order they are parsed and registered in
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
  virtual ~ndb_option() {}
protected:
  virtual bool get_option(int optid, const my_option *opt, char *arg) = 0;
  virtual bool post_process() = 0;
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
  ndb_password_state(const char prefix[], const char prompt[]);
  char* get_password() const { return m_password; }
  size_t get_password_length() const { return m_password_length; }
  bool have_password_option() const { return (m_option_count > 0); }
  BaseString get_error_message() const;
private:
  friend class ndb_password_option;
  friend class ndb_password_from_stdin_option;
  enum status {
    NO_PASSWORD = 0,
    HAVE_PASSWORD = 1,
    ERR_MULTIPLE_SOURCES = -1,
    ERR_BAD_STDIN = -2,
    ERR_BAD_TTY = -3,
    ERR_TOO_LONG = -4,
    ERR_BAD_CHAR = -5,
    ERR_NO_END = -6};
  enum password_source { PS_NONE, PS_ARG, PS_TTY, PS_STDIN };
  static constexpr size_t MAX_PWD_LEN = 1023;

  const char* get_prefix() const { return m_prefix.c_str(); }
  size_t get_prefix_length() const { return m_prefix.length(); }
  int get_from_tty();
  int get_from_stdin();
  void set_password(const char src[], size_t len);
  void clear_password();
  void add_option_usage() { m_option_count++; }
  void remove_option_usage() { m_option_count--; }
  bool is_in_error() const { return m_status < 0; }
  void set_error(enum status err) { m_status = err; }
  void commit_password();
private:
  BaseString m_prompt;
  char* m_password;
  enum status m_status;
  int m_option_count; // How many options that is about to set password
  size_t m_password_length;
  char m_password_buffer[MAX_PWD_LEN + 1];
  BaseString m_prefix;
};

class ndb_password_option: ndb_option
{
public:
  ndb_password_option(ndb_password_state& pwd_buf);
  bool get_option(int optid, const my_option *opt, char *arg) override;
  bool post_process() override;
private:
  ndb_password_state& m_password_state;
  // One of PS_NONE, PS_ARG, PS_TTY
  ndb_password_state::password_source m_password_source;
};

class ndb_password_from_stdin_option: ndb_option
{
public:
  ndb_password_from_stdin_option(ndb_password_state& pwd_buf);
  bool get_option(int optid, const my_option *opt, char *arg) override;
  bool post_process() override;

  bool opt_value;
private:
  ndb_password_state& m_password_state;
  // One of PS_NONE, PS_STDIN
  ndb_password_state::password_source m_password_source;
};

class Ndb_opts {
public:
  Ndb_opts(int & argc_ref, char** & argv_ref,
           struct my_option * long_options,
           const char * default_groups[] = 0);

  ~Ndb_opts();

  void set_usage_funcs(void(*short_usage_fn)(void),
                       void(* long_usage_fn)(void) = 0);

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

#endif

#endif /*_NDB_OPTS_H */
