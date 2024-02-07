/*
   Copyright (c) 2008, 2024, Oracle and/or its affiliates.

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

#define OPTEXPORT
#include <errno.h>
#include <ndb_opts.h>
#include <ndb_version.h>
#include "my_alloc.h"
#include "my_default.h"
#include "portlib/NdbMem.h"
#include "portlib/ndb_password.h"
#include "unhex.h"
#include "util/require.h"

using usage_fn = void (*)(void);

static const char *load_default_groups[] = {"mysql_cluster", nullptr};

static void default_ndb_opt_short(void) { ndb_short_usage_sub(nullptr); }

/* declaration only */
void ndb_usage(usage_fn, const char *load_default_groups[],
               struct my_option *my_long_options);

static void default_ndb_opt_usage(void) {
  struct my_option my_long_options[] = {
      NdbStdOpt::usage,           NdbStdOpt::help,
      NdbStdOpt::version,         NdbStdOpt::ndb_connectstring,
      NdbStdOpt::mgmd_host,       NdbStdOpt::connectstring,
      NdbStdOpt::ndb_nodeid,      NdbStdOpt::optimized_node_selection,
      NdbStdOpt::charsets_dir,    NdbStdOpt::connect_retry_delay,
      NdbStdOpt::connect_retries, NDB_STD_OPT_DEBUG NdbStdOpt::end_of_options,
  };

  ndb_usage(default_ndb_opt_short, load_default_groups, my_long_options);
}

static usage_fn g_ndb_opt_short_usage = default_ndb_opt_short;
static usage_fn g_ndb_opt_usage = default_ndb_opt_usage;

void ndb_opt_set_usage_funcs(void (*short_usage)(void), void (*usage)(void)) {
  /* Check that the program name has been set already */
  assert(my_progname);

  if (short_usage) g_ndb_opt_short_usage = short_usage;
  if (usage) g_ndb_opt_usage = usage;
}

static inline const char *ndb_progname(void) {
  if (my_progname) return my_progname;
  return "<unknown program>";
}

void ndb_short_usage_sub(const char *extra) {
  printf("Usage: %s [OPTIONS]%s%s\n", ndb_progname(), (extra) ? " " : "",
         (extra) ? extra : "");
}

void ndb_usage(usage_fn usagefunc, const char *load_default_groups[],
               struct my_option *my_long_options) {
  (*usagefunc)();

  ndb_std_print_version();
  print_defaults(MYSQL_CONFIG_NAME, load_default_groups);
  puts("");
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
}

static void empty_long_usage_extra_func() {}

bool ndb_std_get_one_option(int optid, const struct my_option *opt,
                            char *argument) {
  if (opt->app_type != nullptr) {
    return ndb_option::get_one_option(optid, opt, argument);
  }
  switch (optid) {
#ifndef NDEBUG
    case '#':
      if (!opt_debug) opt_debug = "d:t";
      DBUG_SET_INITIAL(argument ? argument : opt_debug);
      opt_ndb_endinfo = 1;
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

void ndb_std_print_version() {
#ifndef NDEBUG
  const char *suffix = "-debug";
#else
  const char *suffix = "";
#endif
  printf("MySQL distrib %s%s, for %s (%s)\n", NDB_VERSION_STRING, suffix,
         SYSTEM_TYPE, MACHINE_TYPE);
}

bool ndb_is_load_default_arg_separator(const char *arg) {
  /*
    load_default() in 5.5+ returns an extra arg which has to
    be skipped when processing the argv array
   */
  if (my_getopt_is_args_separator(arg)) return true;
  return false;
}

static Ndb_opts *registeredNdbOpts;

static void ndb_opts_usage() { registeredNdbOpts->usage(); }

void Ndb_opts::registerUsage(Ndb_opts *r) {
  assert(registeredNdbOpts == nullptr);
  registeredNdbOpts = r;
  ndb_opt_set_usage_funcs(default_ndb_opt_short, ndb_opts_usage);
}

void Ndb_opts::release() { registeredNdbOpts = nullptr; }

Ndb_opts::Ndb_opts(int &argc_ref, char **&argv_ref,
                   struct my_option *long_options, const char *default_groups[])
    : opts_mem_root(),
      main_argc_ptr(&argc_ref),
      main_argv_ptr(&argv_ref),
      mycnf_default_groups(default_groups ? default_groups
                                          : load_default_groups),
      options(long_options),
      short_usage_fn(g_ndb_opt_short_usage),
      long_usage_extra_fn(empty_long_usage_extra_func) {
  my_getopt_use_args_separator = true;
  my_load_defaults(MYSQL_CONFIG_NAME, mycnf_default_groups, main_argc_ptr,
                   main_argv_ptr, &opts_mem_root, nullptr);
  my_getopt_use_args_separator = false;
  Ndb_opts::registerUsage(this);
}

Ndb_opts::~Ndb_opts() { Ndb_opts::release(); }

int Ndb_opts::handle_options(bool (*get_opt_fn)(int, const struct my_option *,
                                                char *)) const {
  return ::handle_options(main_argc_ptr, main_argv_ptr, options, get_opt_fn);
}

void Ndb_opts::set_usage_funcs(void (*short_fn)(void), void (*long_fn)(void)) {
  if (short_fn) short_usage_fn = short_fn;
  if (long_fn) long_usage_extra_fn = long_fn;
}

void Ndb_opts::usage() const {
  long_usage_extra_fn();
  ndb_usage(short_usage_fn, mycnf_default_groups, options);
}

const char *Ndb_opts::get_defaults_extra_file() const {
  return my_defaults_extra_file;
}

const char *Ndb_opts::get_defaults_file() const { return my_defaults_file; }

const char *Ndb_opts::get_defaults_group_suffix() const {
  return my_defaults_group_suffix;
}

// ndb_option

ndb_option *ndb_option::m_first = nullptr;
ndb_option *ndb_option::m_last = nullptr;

ndb_option::ndb_option() : m_prev(nullptr), m_next(nullptr) {}

bool ndb_option::get_one_option(int optid, const my_option *opt, char *arg) {
  if (opt->app_type == nullptr) {
    // Nothing to do, no error
    return false;
  }
  /*
   * Make sure your option definition only set app_type to nullptr or pointing
   * to a ndb_option object, else your code is broken.
   */
  ndb_option *opt_obj = static_cast<ndb_option *>(opt->app_type);
  return opt_obj->get_option(optid, opt, arg);
}

bool ndb_option::post_process_options() {
  bool failed = false;
  ndb_option *p = m_first;
  while (!failed && p != nullptr) {
    failed = p->post_process();
    p = p->m_next;
  }
  return failed;
}

void ndb_option::reset_options() {
  ndb_option *p = m_first;
  while (p != nullptr) {
    p->reset();
    p = p->m_next;
  }
}

void ndb_option::push_back() {
  if (m_next != nullptr || m_prev != nullptr || this == m_first) {
    erase();
  }
  if (m_last != nullptr) {
    m_prev = m_last;
    m_last->m_next = this;
    m_last = this;
  } else {
    m_first = m_last = this;
  }
}

void ndb_option::erase() {
  if (m_prev != nullptr) {
    m_prev->m_next = m_next;
  }
  if (m_next != nullptr) {
    m_next->m_prev = m_prev;
  }
  if (m_last == this) {
    m_last = m_prev;
  }
  if (m_first == this) {
    m_first = m_next;
  }
  m_next = m_prev = nullptr;
}

// ndb_password_state

ndb_password_state::ndb_password_state(const char prefix[], const char prompt[],
                                       kind_t kind)
    : m_password(nullptr),
      m_kind(kind),
      m_status(NO_PASSWORD),
      m_option_count(0),
      m_password_length(0),
      m_prefix(prefix) {
  if (prompt != nullptr) {
    m_prompt.assign(prompt);
  } else if (prefix != nullptr) {
    m_prompt.assfmt("Enter %s %s: ", prefix, kind_str());
  } else {
    m_prompt.assfmt("Enter %s: ", kind_str());
  }
}
void ndb_password_state::reset() {
  m_password = nullptr;
  m_status = NO_PASSWORD;
  m_option_count = 0;
  m_password_length = 0;
  clear_password();
}

static unsigned char unhex_char(unsigned char ch) {
  if (ch <= '9') return ch - '0';
  return (ch - 'A' + 10) & 0xF;
}

int ndb_password_state::set_key(const char src[], size_t len) {
  // Note, src maybe m_password_buffer
  require(is_key());
  if (len % 2 == 1) {
    set_error(ERR_ODD_HEX_LENGTH);
    return ERR_ODD_HEX_LENGTH;
  }
  if (len > 2 * MAX_KEY_LEN) {
    set_error(ERR_TOO_LONG);
    return ERR_TOO_LONG;
  }
  for (size_t i = 0, j = 0; i < len; i++) {
    unsigned char ch = src[i];
    if (!isxdigit(ch)) {
      set_error(ERR_BAD_CHAR);
      return ERR_BAD_CHAR;
    }
    if (i % 2 == 0) {
      m_password_buffer[j] = unhex_char(ch) << 4;
    } else {
      m_password_buffer[j] |= unhex_char(ch);
      j++;
    }
  }
  m_password_length = len / 2;
  set_status(PENDING_PASSWORD);
  return PENDING_PASSWORD;
}

int ndb_password_state::set_password(const char src[], size_t len) {
  require(is_password());
  if (len > MAX_PWD_LEN) {
    set_error(ERR_TOO_LONG);
    return ERR_TOO_LONG;
  }
  if (src != m_password_buffer) memcpy(m_password_buffer, src, len);
  m_password_buffer[len] = 0;
  m_password_length = len;
  set_status(PENDING_PASSWORD);
  return PENDING_PASSWORD;
}

void ndb_password_state::clear_password() {
  NdbMem_SecureClear(m_password_buffer, sizeof(m_password_buffer));
}

int ndb_password_state::get_from_tty() {
  int r = ndb_get_password_from_tty(m_prompt.c_str(), m_password_buffer,
                                    sizeof(m_password_buffer));
  if (r >= 0) {
    if (is_password()) {
      return set_password(m_password_buffer, r);
    } else {
      return set_key(m_password_buffer, r);
    }
  }

  clear_password();
  switch (ndb_get_password_error(r)) {
    default:
      abort();
      break;
    case ndb_get_password_error::system_error:
      set_error(ERR_BAD_TTY);
      return ERR_BAD_TTY;
    case ndb_get_password_error::too_long:
      set_error(ERR_TOO_LONG);
      return ERR_TOO_LONG;
    case ndb_get_password_error::bad_char:
      set_error(ERR_BAD_CHAR);
      return ERR_BAD_CHAR;
    case ndb_get_password_error::no_end:
      set_error(ERR_NO_END);
      return ERR_NO_END;
  }
  return r;
}

int ndb_password_state::get_from_stdin() {
  int r = ndb_get_password_from_stdin(m_prompt.c_str(), m_password_buffer,
                                      sizeof(m_password_buffer));
  if (r >= 0) {
    if (is_password()) {
      return set_password(m_password_buffer, r);
    } else {
      return set_key(m_password_buffer, r);
    }
  }

  clear_password();
  switch (ndb_get_password_error(r)) {
    default:
      abort();
      break;
    case ndb_get_password_error::system_error:
      set_error(ERR_BAD_STDIN);
      return ERR_BAD_STDIN;
    case ndb_get_password_error::too_long:
      set_error(ERR_TOO_LONG);
      return ERR_TOO_LONG;
    case ndb_get_password_error::bad_char:
      set_error(ERR_BAD_CHAR);
      return ERR_BAD_CHAR;
    case ndb_get_password_error::no_end:
      set_error(ERR_NO_END);
      return ERR_NO_END;
  }
}

BaseString ndb_password_state::get_error_message() const {
  BaseString msg;
  switch (m_status) {
    case NO_PASSWORD:
    case PENDING_PASSWORD:
    case HAVE_PASSWORD:
      // No error
      break;
    case ERR_MULTIPLE_SOURCES:
      msg.assfmt(
          "Multiple options for same %s used.  Select one of "
          "--%s-%s and --%s-%s-from-stdin.",
          kind_str(), m_prefix.c_str(), kind_str(), m_prefix.c_str(),
          kind_str());
      break;
    case ERR_BAD_STDIN:
      msg.assfmt("Failed to read %s %s from stdin (errno %d).",
                 m_prefix.c_str(), kind_str(), errno);
      break;
    case ERR_BAD_TTY:
      msg.assfmt("Failed to read %s %s from tty (errno %d).", m_prefix.c_str(),
                 kind_str(), errno);
      break;
    case ERR_BAD_CHAR:
      msg.assfmt("%s %s has some bad character.", m_prefix.c_str(), kind_str());
      break;
    case ERR_TOO_LONG:
      msg.assfmt("%s %s too long.", m_prefix.c_str(), kind_str());
      break;
    case ERR_NO_END:
      msg.assfmt("%s %s has no end.", m_prefix.c_str(), kind_str());
      break;
    case ERR_ODD_HEX_LENGTH:
      msg.assfmt("%s %s need even number of hex digits.", m_prefix.c_str(),
                 kind_str());
      break;
    default:
      msg.assfmt("Unknown error for %s %s.", m_prefix.c_str(), kind_str());
  }
  return msg;
}

void ndb_password_state::commit_password() {
  require(m_status == PENDING_PASSWORD);
  require(m_password_length <= MAX_PWD_LEN);
  m_password = m_password_buffer;
  m_status = HAVE_PASSWORD;
}

const ndb_password_state::byte *ndb_password_state::get_key() const {
  require(is_key());
  return reinterpret_cast<const byte *>(m_password);
}

size_t ndb_password_state::get_key_length() const {
  require(is_key());
  return m_password_length;
}

bool ndb_password_state::verify_option_name(const char opt_name[],
                                            const char extra[]) const {
  if (opt_name == nullptr) return false;
  const char *part = opt_name;
  size_t part_length = get_prefix_length();
  if (strncmp(part, get_prefix(), part_length) != 0) return false;
  part += part_length;
  if (part[0] != '-') return false;
  part++;
  part_length = strlen(kind_str());
  if (strncmp(part, kind_str(), part_length) != 0) return false;
  part += part_length;
  if (extra != nullptr && strcmp(part, extra) != 0) return false;
  if (extra == nullptr && part[0] != '\0') return false;
  return true;
}

void ndb_password_state::remove_option_usage() {
  require(m_option_count > 0);
  m_option_count--;
}

// ndb_password_option

ndb_password_option::ndb_password_option(ndb_password_state &password_state)
    : m_password_state(password_state),
      m_password_source(ndb_password_state::PS_NONE) {}

bool ndb_password_option::get_option(int /*optid*/, const my_option *opt,
                                     char *arg) {
  require(m_password_state.verify_option_name(opt->name));
  if (m_password_source != ndb_password_state::PS_NONE) {
    erase();
    m_password_state.clear_password();
    m_password_state.remove_option_usage();
    m_password_source = ndb_password_state::PS_NONE;
  }
  if (arg == disabled_my_option) {
    return false;
  }
  if (arg == nullptr) {
    m_password_source = ndb_password_state::PS_TTY;
    m_password_state.add_option_usage();
    push_back();
    return false;
  }
  size_t arg_len = strlen(arg);
  if (m_password_state.is_password()) {
    m_password_state.set_password(arg, arg_len);
    m_password_source = ndb_password_state::PS_ARG;
    m_password_state.add_option_usage();
    push_back();
  } else {
    require(m_password_state.is_key());
    m_password_state.set_key(arg, arg_len);
    m_password_source = ndb_password_state::PS_ARG;
    m_password_state.add_option_usage();
    push_back();
  }
  NdbMem_SecureClear(arg, arg_len + 1);
  bool failed = (m_password_source != ndb_password_state::PS_ARG);
  return failed;
}

bool ndb_password_option::post_process() {
  require(m_password_source != ndb_password_state::PS_NONE);
  if (m_password_state.m_option_count > 1) {
    m_password_state.set_error(ndb_password_state::ERR_MULTIPLE_SOURCES);
    m_password_state.clear_password();
    require(m_password_state.is_in_error());
    return true;
  }
  if (m_password_source == ndb_password_state::PS_TTY) {
    int r = m_password_state.get_from_tty();
    if (r < 0) {
      m_password_state.clear_password();
      require(m_password_state.is_in_error());
      return true;
    }
  }
  if (m_password_source != ndb_password_state::PS_NONE) {
    if (m_password_state.is_in_error()) {
      return true;
    }
  }
  m_password_state.commit_password();
  return false;
}

void ndb_password_option::reset() {
  m_password_source = ndb_password_state::PS_NONE;
}

ndb_password_from_stdin_option::ndb_password_from_stdin_option(
    ndb_password_state &password_state)
    : opt_value(false),
      m_password_state(password_state),
      m_password_source(ndb_password_state::PS_NONE) {}

bool ndb_password_from_stdin_option::get_option(int /*optid*/,
                                                const my_option *opt,
                                                char *arg) {
  require(m_password_state.verify_option_name(opt->name, "-from-stdin"));
  if (m_password_source != ndb_password_state::PS_NONE) {
    erase();
    m_password_state.remove_option_usage();
    m_password_source = ndb_password_state::PS_NONE;
  }
  if (arg == disabled_my_option) {
    return false;
  }
  m_password_source = ndb_password_state::PS_STDIN;
  m_password_state.add_option_usage();
  push_back();
  return false;
}

bool ndb_password_from_stdin_option::post_process() {
  require(m_password_source != ndb_password_state::PS_NONE);
  if (m_password_state.m_option_count > 1) {
    m_password_state.set_error(ndb_password_state::ERR_MULTIPLE_SOURCES);
    m_password_state.clear_password();
    require(m_password_state.is_in_error());
    return true;
  }
  if (m_password_source == ndb_password_state::PS_STDIN) {
    int r = m_password_state.get_from_stdin();
    if (r < 0) {
      m_password_state.clear_password();
      require(m_password_state.is_in_error());
      return true;
    }
  }
  if (m_password_source != ndb_password_state::PS_NONE) {
    if (m_password_state.is_in_error()) {
      return true;
    }
  }
  m_password_state.commit_password();
  return false;
}

void ndb_password_from_stdin_option::reset() {
  opt_value = false, m_password_source = ndb_password_state::PS_NONE;
}
