/*
   Copyright (c) 2008, 2020, Oracle and/or its affiliates.

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

#define OPTEXPORT
#include <ndb_opts.h>

#include <errno.h>
#include <ndb_version.h>
#include "my_alloc.h"
#include "my_default.h"
#include "portlib/ndb_password.h"
#include "portlib/NdbMem.h"

static const char *load_default_groups[]= { "mysql_cluster", 0 };

static void default_ndb_opt_short(void)
{
  ndb_short_usage_sub(NULL);
}

extern "C"     /* declaration only */
void ndb_usage(void (*usagefunc)(void), const char *load_default_groups[],
               struct my_option *my_long_options);

static void default_ndb_opt_usage(void)
{
  struct my_option my_long_options[] =
    {
      NDB_STD_OPTS("ndbapi_program")
    };

  ndb_usage(default_ndb_opt_short, load_default_groups, my_long_options);
}

static void (*g_ndb_opt_short_usage)(void)= default_ndb_opt_short;
static void (*g_ndb_opt_usage)(void)= default_ndb_opt_usage;

extern "C"
void ndb_opt_set_usage_funcs(void (*short_usage)(void),
                             void (*usage)(void))
{
  /* Check that the program name has been set already */
  assert(my_progname);

  if(short_usage)
    g_ndb_opt_short_usage= short_usage;
  if(usage)
    g_ndb_opt_usage= usage;
}

static inline
const char* ndb_progname(void)
{
  if (my_progname)
    return my_progname;
  return "<unknown program>";
}

extern "C"
void ndb_short_usage_sub(const char* extra)
{
  printf("Usage: %s [OPTIONS]%s%s\n", ndb_progname(),
         (extra)?" ":"",
         (extra)?extra:"");
}

extern "C"
void ndb_usage(void (*usagefunc)(void), const char *load_default_groups[],
               struct my_option *my_long_options)
{
  (*usagefunc)();

  ndb_std_print_version();
  print_defaults(MYSQL_CONFIG_NAME,load_default_groups);
  puts("");
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
}

static
void empty_long_usage_extra_func()
{
}

extern "C"
bool
ndb_std_get_one_option(int optid, const struct my_option *opt, char *argument)
{
  if (opt->app_type != nullptr)
  {
    return ndb_option::get_one_option(optid, opt, argument);
  }
  switch (optid) {
#ifndef DBUG_OFF
  case '#':
    if (!opt_debug)
      opt_debug= "d:t";
    DBUG_SET_INITIAL(argument ? argument : opt_debug);
    opt_ndb_endinfo= 1;
    break;
#endif
  case 'V':
    ndb_std_print_version();
    exit(0);
  case '?':
    (*g_ndb_opt_usage)();
    exit(0);
  }
  return 0;
}

extern "C"
void ndb_std_print_version()
{
#ifndef DBUG_OFF
  const char *suffix= "-debug";
#else
  const char *suffix= "";
#endif
  printf("MySQL distrib %s%s, for %s (%s)\n",
         NDB_VERSION_STRING,suffix,SYSTEM_TYPE,MACHINE_TYPE);
}

extern "C"
bool ndb_is_load_default_arg_separator(const char* arg)
{
  /*
    load_default() in 5.5+ returns an extra arg which has to
    be skipped when processing the argv array
   */
  if (my_getopt_is_args_separator(arg))
    return TRUE;
  return FALSE;
}

static Ndb_opts * registeredNdbOpts;

static void ndb_opts_usage()
{
  registeredNdbOpts->usage();
}

void
Ndb_opts::registerUsage(Ndb_opts *r)
{
  assert(registeredNdbOpts == NULL);
  registeredNdbOpts = r;
  ndb_opt_set_usage_funcs(default_ndb_opt_short, ndb_opts_usage);
}

void Ndb_opts::release()
{
  registeredNdbOpts = NULL;
}

Ndb_opts::Ndb_opts(int & argc_ref, char** & argv_ref,
                   struct my_option * long_options,
                   const char * default_groups[])
:
  opts_mem_root(),
  main_argc_ptr(& argc_ref),
  main_argv_ptr(& argv_ref),
  mycnf_default_groups(default_groups ? default_groups : load_default_groups),
  options(long_options),
  short_usage_fn(g_ndb_opt_short_usage),
  long_usage_extra_fn(empty_long_usage_extra_func)
{
  my_getopt_use_args_separator = true;
  my_load_defaults(MYSQL_CONFIG_NAME,  mycnf_default_groups,
                   main_argc_ptr, main_argv_ptr,  &opts_mem_root, NULL);
  my_getopt_use_args_separator = false;
  Ndb_opts::registerUsage(this);
}

Ndb_opts::~Ndb_opts()
{
  Ndb_opts::release();
}

int Ndb_opts::handle_options(bool (*get_opt_fn)
                             (int, const struct my_option *, char *)) const
{
  return ::handle_options(main_argc_ptr, main_argv_ptr, options, get_opt_fn);
}

void Ndb_opts::set_usage_funcs(void (*short_fn)(void),
                               void (*long_fn)(void))
{
  short_usage_fn = short_fn;
  if(long_fn) long_usage_extra_fn = long_fn;
}

void Ndb_opts::usage() const
{
  long_usage_extra_fn();
  ndb_usage(short_usage_fn, mycnf_default_groups, options);
}

// ndb_option

ndb_option* ndb_option::m_first = nullptr;
ndb_option* ndb_option::m_last = nullptr;

ndb_option::ndb_option()
: m_prev(nullptr),
  m_next(nullptr)
{}

bool ndb_option::get_one_option(int optid, const my_option *opt, char *arg)
{
  if (opt->app_type == nullptr)
  {
    // Nothing to do, no error
    return false;
  }
  /*
   * Make sure your option definition only set app_type to nullptr or pointing
   * to a ndb_option object, else your code is broken.
   */
  ndb_option* opt_obj = static_cast<ndb_option*>(opt->app_type);
  return opt_obj->get_option(optid, opt, arg);
}

bool ndb_option::post_process_options()
{
  bool failed = false;
  ndb_option* p = m_first;
  while (!failed && p != nullptr)
  {
    failed = p->post_process();
    p = p->m_next;
  }
  return failed;
}

void ndb_option::push_back()
{
  if (m_next != nullptr ||
      m_prev != nullptr ||
      this == m_first)
  {
    erase();
  }
  if (m_last != nullptr)
  {
    m_prev = m_last;
    m_last->m_next = this;
    m_last = this;
  }
  else
  {
    m_first = m_last = this;
  }
}

void ndb_option::erase()
{
  if (m_prev != nullptr)
  {
    m_prev->m_next = m_next;
  }
  if (m_next != nullptr)
  {
    m_next->m_prev = m_prev;
  }
  if (m_last == this)
  {
    m_last = m_prev;
  }
  if (m_first == this)
  {
    m_first = m_next;
  }
  m_next = m_prev = nullptr;
}

// ndb_password_state

ndb_password_state::ndb_password_state(const char prefix[],
                                       const char prompt[])
: m_password(nullptr),
  m_status(NO_PASSWORD),
  m_option_count(0),
  m_password_length(0),
  m_prefix(prefix)
{
  if (prompt != nullptr)
  {
    m_prompt.assign(prompt);
  }
  else if (prefix != nullptr)
  {
    m_prompt.assfmt("Enter %s password: ", prefix);
  }
  else
  {
    m_prompt.assign("Enter password: ");
  }
}

void ndb_password_state::set_password(const char src[], size_t len)
{
  require(len <= MAX_PWD_LEN);
  memcpy(m_password_buffer, src, len);
  m_password_buffer[len] = 0;
  m_password_length = len;
}

void ndb_password_state::clear_password()
{
  NdbMem_SecureClear(m_password_buffer, sizeof(m_password_buffer));
}

int ndb_password_state::get_from_tty()
{
  int r = ndb_get_password_from_tty(m_prompt.c_str(),
                                    m_password_buffer,
                                    MAX_PWD_LEN);
  if (r >= 0)
  {
    m_password_length = r;
    m_password_buffer[r] = 0;
  }
  else switch (ndb_get_password_error(r))
  {
  default:
    abort();
    break;
  case ndb_get_password_error::system_error:
    set_error(ERR_BAD_TTY);
    break;
  case ndb_get_password_error::too_long:
    set_error(ERR_TOO_LONG);
    break;
  case ndb_get_password_error::bad_char:
    set_error(ERR_BAD_CHAR);
    break;
  case ndb_get_password_error::no_end:
    set_error(ERR_NO_END);
    break;
  }
  if (r < 0)
  {
    clear_password();
  }
  return r;
}

int ndb_password_state::get_from_stdin()
{
  int r = ndb_get_password_from_stdin(m_prompt.c_str(),
                                      m_password_buffer,
                                      MAX_PWD_LEN);
  if (r >= 0)
  {
    m_password_length = r;
    m_password_buffer[r] = 0;
  }
  else switch (ndb_get_password_error(r))
  {
  default:
    abort();
    break;
  case ndb_get_password_error::system_error:
    set_error(ERR_BAD_STDIN);
    break;
  case ndb_get_password_error::too_long:
    set_error(ERR_TOO_LONG);
    break;
  case ndb_get_password_error::bad_char:
    set_error(ERR_BAD_CHAR);
    break;
  case ndb_get_password_error::no_end:
    set_error(ERR_NO_END);
    break;
  }
  if (r < 0)
  {
    clear_password();
  }
  return r;
}

BaseString ndb_password_state::get_error_message() const
{
  BaseString msg;
  switch (m_status)
  {
  case NO_PASSWORD:
  case HAVE_PASSWORD:
    // No error
    break;
  case ERR_MULTIPLE_SOURCES:
    msg.assfmt("Multiple options for same password used.  Select one of "
               "--%s-password and --%s-password-from-stdin.",
               m_prefix.c_str(), m_prefix.c_str());
    break;
  case ERR_BAD_STDIN:
    msg.assfmt("Failed to read %s password from stdin (errno %d).", m_prefix.c_str(), errno);
    break;
  case ERR_BAD_TTY:
    msg.assfmt("Failed to read %s password from tty (errno %d).", m_prefix.c_str(), errno);
    break;
  case ERR_BAD_CHAR:
    msg.assfmt("%s password has some bad character.", m_prefix.c_str());
    break;
  case ERR_TOO_LONG:
    msg.assfmt("%s password too long.", m_prefix.c_str());
    break;
  case ERR_NO_END:
    msg.assfmt("%s password has no end.", m_prefix.c_str());
    break;
  default:
    msg.assfmt("Unknown error for %s password.", m_prefix.c_str());
  }
  return msg;
}

void ndb_password_state::commit_password()
{
  require(m_status == NO_PASSWORD);
  require(m_password_length <= MAX_PWD_LEN);
  m_password = m_password_buffer;
  m_status = HAVE_PASSWORD;
}


// ndb_password_option

ndb_password_option::ndb_password_option(ndb_password_state& password_state)
: m_password_state(password_state),
  m_password_source(ndb_password_state::PS_NONE)
{}

bool ndb_password_option::get_option(int optid,
                                     const my_option *opt,
                                     char *arg)
{
  require(opt->name != nullptr &&
          strncmp(opt->name,
                  m_password_state.get_prefix(),
                  m_password_state.get_prefix_length()) == 0 &&
          strcmp(opt->name + m_password_state.get_prefix_length(),
                 "-password") == 0);
  if (m_password_source != ndb_password_state::PS_NONE)
  {
    erase();
    m_password_state.clear_password();
    m_password_state.remove_option_usage();
    m_password_source = ndb_password_state::PS_NONE;
  }
  if (arg == disabled_my_option)
  {
    return false;
  }
  if (arg == nullptr)
  {
    m_password_source = ndb_password_state::PS_TTY;
    m_password_state.add_option_usage();
    push_back();
    return false;
  }
  size_t arg_len = strlen(arg);
  if (arg_len <= ndb_password_state::MAX_PWD_LEN)
  {
    m_password_state.set_password(arg, arg_len);
    m_password_source = ndb_password_state::PS_ARG;
    m_password_state.add_option_usage();
    push_back();
  }
  else
  {
    m_password_state.set_error(ndb_password_state::ERR_TOO_LONG);
  }
  NdbMem_SecureClear(arg, arg_len + 1);
  bool failed = (m_password_source != ndb_password_state::PS_ARG);
  return failed;
}

bool ndb_password_option::post_process()
{
  if (m_password_state.is_in_error())
  {
    return true;
  }
  require(m_password_source != ndb_password_state::PS_NONE);
  if (m_password_state.m_option_count > 1)
  {
    m_password_state.set_error(ndb_password_state::ERR_MULTIPLE_SOURCES);
    m_password_state.clear_password();
    return true;
  }
  if (m_password_source == ndb_password_state::PS_TTY)
  {
    int r = m_password_state.get_from_tty();
    if (r < 0)
    {
      m_password_state.clear_password();
      return true;
    }
  }
  m_password_state.commit_password();
  return false;
}

ndb_password_from_stdin_option::ndb_password_from_stdin_option(
                                    ndb_password_state& password_state)
: opt_value(false),
  m_password_state(password_state),
  m_password_source(ndb_password_state::PS_NONE)
{}

bool ndb_password_from_stdin_option::get_option(int optid,
                                                const my_option *opt,
                                                char *arg)
{
  require(opt->name != nullptr &&
          strncmp(opt->name,
                  m_password_state.get_prefix(),
                  m_password_state.get_prefix_length()) == 0 &&
          strcmp(opt->name + m_password_state.get_prefix_length(),
                 "-password-from-stdin") == 0);
  if (m_password_source != ndb_password_state::PS_NONE)
  {
    erase();
    m_password_state.remove_option_usage();
    m_password_source = ndb_password_state::PS_NONE;
  }
  if (arg == disabled_my_option)
  {
    return false;
  }
  m_password_source = ndb_password_state::PS_STDIN;
  m_password_state.add_option_usage();
  push_back();
  return false;
}

bool ndb_password_from_stdin_option::post_process()
{
  if (m_password_state.is_in_error())
  {
    return true;
  }
  require(m_password_source != ndb_password_state::PS_NONE);
  if (m_password_state.m_option_count > 1)
  {
    m_password_state.set_error(ndb_password_state::ERR_MULTIPLE_SOURCES);
    m_password_state.clear_password();
    return true;
  }
  if (m_password_source == ndb_password_state::PS_STDIN)
  {
    int r = m_password_state.get_from_stdin();
    if (r < 0)
    {
      m_password_state.clear_password();
      return true;
    }
  }
  m_password_state.commit_password();
  return false;
}
