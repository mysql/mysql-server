/*
Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#include "client/include/client_program_options.h"
#include <cstddef>
#include "client/include/client_priv.h"
#include "compression.h"
#include "m_string.h"
#include "my_alloc.h"
#include "my_compiler.h"
#include "my_default.h"
#include "my_getopt.h"
#include "mysql/service_mysql_alloc.h"
#include "mysql/strings/m_ctype.h"
#include "nulls.h"
#include "print_version.h"
#include "strmake.h"
#include "welcome_copyright_notice.h"

#include "client/include/authentication_kerberos_clientopt-vars.h"
#include "client/include/authentication_webauthn_clientopt-vars.h"
#include "client/include/caching_sha2_passwordopt-vars.h"
#include "client/include/multi_factor_passwordopt-vars.h"
#include "client/include/sslopt-vars.h"
#include "client/multi_factor_passwordopt-vars.cc"

#ifdef HAVE_PWD_H
#include <pwd.h>
#endif

class Client_program_options_impl : public Client_program_options {
  friend class Client_program_options;

 protected:
  Client_program_options_impl(
      const char *section_name_arg, const char *copyright_arg,
      const char *extra_args_arg, const my_option *opts, size_t nopts,
      bool (*get_one_option_user_arg)(int optid, const struct my_option *opt,
                                      char *argument))
      : extra_args(extra_args_arg),
        copyright(copyright_arg),
        section_name(section_name_arg),
        get_one_option_user(get_one_option_user_arg)

  {
    if (nopts > 0) {
      size_t n_common_options = 0;
      for (const my_option *opt = &my_common_options[0]; opt->name != nullptr;
           opt++) {
        n_common_options++;
      }
      n_common_options += 1;  // the empty terminator

      my_option *dest = my_long_options =
          new (&argv_alloc) my_option[nopts + n_common_options + 1];
      memmove(dest, opts, nopts * sizeof(my_option));
      dest += nopts;

      memmove(dest, my_common_options, (n_common_options * sizeof(my_option)));
    } else {
      my_long_options = my_common_options;
    }

    load_default_groups[0] = strdup_root(&argv_alloc, section_name);
    load_default_groups[1] = const_cast<char *>("client");
    load_default_groups[2] = nullptr;
  }

 public:
  ~Client_program_options_impl() override {
    free_passwords();
    my_free(opt_mysql_unix_port);
#if defined(_WIN32)
    my_free(shared_memory_base_name);
#endif
    my_free(current_user);
    my_free(current_host);
    my_free(current_os_user);
    my_free(current_os_sudouser);
  }
  Client_program_options_impl(const Client_program_options_impl &) = delete;
  Client_program_options_impl &operator=(const Client_program_options_impl &) =
      delete;
  Client_program_options_impl(Client_program_options_impl &&) = delete;
  Client_program_options_impl &operator=(Client_program_options_impl &&) =
      delete;

  bool init(int *argc_ptr, char ***argv_ptr) noexcept override {
    clear_last_error();
#ifdef _WIN32
    /* Convert command line parameters from UTF16LE to UTF8MB4. */
    my_win_translate_command_line_args(&my_charset_utf8mb4_bin, argc_ptr,
                                       argv_ptr);
#endif

    my_getopt_use_args_separator = true;
    if (0 != load_defaults("my", const_cast<const char **>(load_default_groups),
                           argc_ptr, argv_ptr, &argv_alloc)) {
      error_handler("Can't process the defaults groups");
      return true;
    }
    my_getopt_use_args_separator = false;

    get_current_os_user();
    get_current_os_sudouser();

    if (handle_options(argc_ptr, argv_ptr, my_long_options,
                       static_get_one_option) != 0) {
      error_handler("Can't process command line options");
      return true;
    }
    return false;
  }

  bool apply(MYSQL *mysql) noexcept override {
    clear_last_error();
    if (opt_compress) {
      mysql_options(mysql, MYSQL_OPT_COMPRESS, NullS);
    }
    if (nullptr != opt_compress_algorithm) {
      mysql_options(mysql, MYSQL_OPT_COMPRESSION_ALGORITHMS,
                    opt_compress_algorithm);
    }

    mysql_options(mysql, MYSQL_OPT_ZSTD_COMPRESSION_LEVEL,
                  &opt_zstd_compress_level);

    if (SSL_SET_OPTIONS(mysql)) {
      error_handler(SSL_SET_OPTIONS_ERROR);
      return true;
    }

    if (0 != opt_protocol) {
      mysql_options(mysql, MYSQL_OPT_PROTOCOL,
                    reinterpret_cast<char *>(&opt_protocol));
    }

#if defined(_WIN32)
    if (shared_memory_base_name)
      mysql_options(mysql, MYSQL_SHARED_MEMORY_BASE_NAME,
                    shared_memory_base_name);
#endif

    if (nullptr != opt_plugin_dir && 0 != *opt_plugin_dir) {
      mysql_options(mysql, MYSQL_PLUGIN_DIR, opt_plugin_dir);
    }

    if (nullptr != opt_default_auth && 0 != *opt_default_auth) {
      mysql_options(mysql, MYSQL_DEFAULT_AUTH, opt_default_auth);
    }

    set_server_public_key(mysql);
    set_get_server_public_key_option(mysql);

    mysql_options(mysql, MYSQL_OPT_CONNECT_ATTR_RESET, nullptr);
    mysql_options4(mysql, MYSQL_OPT_CONNECT_ATTR_ADD, "program_name",
                   section_name);
    if (nullptr != current_os_user) {
      mysql_options4(mysql, MYSQL_OPT_CONNECT_ATTR_ADD, "os_user",
                     current_os_user);
    }
    if (nullptr != current_os_sudouser) {
      mysql_options4(mysql, MYSQL_OPT_CONNECT_ATTR_ADD, "os_sudouser",
                     current_os_sudouser);
    }

    set_password_options(mysql);

    struct st_mysql_client_plugin *oci_iam_plugin = mysql_client_find_plugin(
        mysql, "authentication_oci_client", MYSQL_CLIENT_AUTHENTICATION_PLUGIN);

    /* set authentication_oci_client config profile option if required */
    if (opt_authentication_oci_client_config_profile != nullptr) {
      if (nullptr == oci_iam_plugin) {
        error_handler("Cannot load the authentication_oci_client plugin.");
        return true;
      }
      if (0 != mysql_plugin_options(
                   oci_iam_plugin, "authentication-oci-client-config-profile",
                   opt_authentication_oci_client_config_profile)) {
        error_handler(
            "Failed to set config profile for authentication_oci_client "
            "plugin.");
        return true;
      }
    }
    /* set OCI config file option if required */
    if (opt_oci_config_file != nullptr) {
      if (nullptr == oci_iam_plugin) {
        error_handler("Cannot load the authentication_oci_client plugin.");
        return true;
      }
      if (0 != mysql_plugin_options(oci_iam_plugin, "oci-config-file",
                                    opt_oci_config_file)) {
        error_handler(
            "Failed to set config file for authentication_oci_client plugin.");
        return true;
      }
    }

    /* set authentication_openid_connect_client ID token file option if required
     */
    if (opt_authentication_openid_connect_client_id_token_file != nullptr) {
      struct st_mysql_client_plugin *openid_connect_plugin =
          mysql_client_find_plugin(mysql,
                                   "authentication_openid_connect_client",
                                   MYSQL_CLIENT_AUTHENTICATION_PLUGIN);
      if (nullptr == openid_connect_plugin) {
        error_handler(
            "Cannot load the authentication_openid_connect_client plugin.");
        return true;
      }
      if (0 != mysql_plugin_options(
                   openid_connect_plugin, "id-token-file",
                   opt_authentication_openid_connect_client_id_token_file)) {
        error_handler(
            "Failed to set id token file for "
            "authentication_openid_connect_client plugin.");
        return true;
      }
    }

    char error[256]{0};
#if defined(_WIN32)
    if (0 != set_authentication_kerberos_client_mode(mysql, error, 255)) {
      error_handler(error);
      return true;
    }
#endif
    if (0 != set_authentication_webauthn_options(mysql, error, 255)) {
      error_handler(error);
      return true;
    }
    return false;
  }

  bool connect(MYSQL *mysql, unsigned long client_flag) noexcept override {
    clear_last_error();
    if (nullptr == mysql_real_connect(mysql, current_host, current_user,
                                      nullptr, nullptr, opt_mysql_port,
                                      opt_mysql_unix_port, client_flag)) {
      return true;
    }

    return ssl_client_check_post_connect_ssl_setup(
        mysql, [&](const char *what) { error_handler(what); });
  }

 protected:
  MEM_ROOT argv_alloc{PSI_NOT_INSTRUMENTED, 512};

  void error_handler(const char *what) {
    assert(last_error == nullptr);
    last_error = strdup_root(&argv_alloc, what);
  }

  // Get the current OS user name.
  void get_current_os_user() {
    const char *user;

#ifdef _WIN32
    char buf[255];
    WCHAR wbuf[255];
    DWORD wbuf_len = sizeof(wbuf) / sizeof(WCHAR);
    size_t len;
    uint dummy_errors;

    if (GetUserNameW(wbuf, &wbuf_len)) {
      len =
          my_convert(buf, sizeof(buf) - 1, &my_charset_utf8mb4_bin,
                     (char *)wbuf, wbuf_len * sizeof(WCHAR),
                     get_charset_by_name("utf16le_bin", MYF(0)), &dummy_errors);
      buf[len] = 0;
      user = buf;
    } else {
      user = "UNKNOWN USER";
    }
#else
#ifdef HAVE_GETPWUID
    struct passwd *pw = getpwuid(geteuid());

    if (pw != nullptr) {
      user = pw->pw_name;
    } else
#endif
        if (nullptr == (user = getenv("USER")) &&
            nullptr == (user = getenv("LOGNAME")) &&
            nullptr == (user = getenv("LOGIN"))) {
      user = "UNKNOWN USER";
    }
#endif /* _WIN32 */
    current_os_user = my_strdup(PSI_NOT_INSTRUMENTED, user, MYF(MY_WME));
  }

  // Get the current OS sudo user name (only for non-Windows platforms).
  void get_current_os_sudouser() {
#ifndef _WIN32
    if (nullptr != getenv("SUDO_USER")) {
      current_os_sudouser =
          my_strdup(PSI_NOT_INSTRUMENTED, getenv("SUDO_USER"), MYF(MY_WME));
    }
#endif /* !_WIN32 */
  }

  static bool static_get_one_option(int optid, const struct my_option *opt,
                                    char *argument) {
    return dynamic_cast<Client_program_options_impl *>(singleton)
        ->get_one_option(optid, opt, argument);
  }

  bool get_one_option(int optid, const struct my_option *opt, char *argument) {
    switch (optid) {
      case OPT_CHARSETS_DIR:
        strmake(mysql_charsets_dir, argument, sizeof(mysql_charsets_dir) - 1);
        charsets_dir = mysql_charsets_dir;
        break;
      case OPT_ENABLE_CLEARTEXT_PLUGIN:
        using_opt_enable_cleartext_plugin = true;
        break;
      case OPT_MYSQL_PROTOCOL:
        opt_protocol =
            find_type_or_exit(argument, &sql_protocol_typelib, opt->name);
        break;
        PARSE_COMMAND_LINE_PASSWORD_OPTION;
      case '#':
        DBUG_PUSH(argument ? argument : default_dbug_option);
        break;
      case 'W':
#ifdef _WIN32
        opt_protocol = MYSQL_PROTOCOL_PIPE;
#endif
        break;
#include "client/include/sslopt-case.h"

#include "client/include/authentication_kerberos_clientopt-case.h"
#include "client/include/authentication_webauthn_clientopt-case.h"

      case 'V':
        usage(1);
        exit(0);
      case 'I':
      case '?':
        usage(0);
        exit(0);
      default:
        if (nullptr != get_one_option_user) {
          return get_one_option_user(optid, opt, argument);
        }
        break;
    }
    return false;
  }

  const char *extra_args = nullptr;
  const char *copyright = nullptr;
  const char *section_name = nullptr;

  void usage(int version) {
    print_version();

    if (0 != version) {
      return;
    }
    puts(copyright);
    if (nullptr != extra_args) {
      printf("Usage: %s [OPTIONS] %s\n", my_progname, extra_args);
    } else {
      printf("Usage: %s [OPTIONS]\n", my_progname);
    }
    my_print_help(my_long_options);
    print_defaults("my", const_cast<const char **>(load_default_groups));
    my_print_variables(my_long_options);
  }

  const char *default_dbug_option = "d:t:o,/tmp/mysqldm.trace";
#if defined(_WIN32)
  char *shared_memory_base_name = nullptr;
#endif
  uint opt_protocol{0};
  char *opt_plugin_dir = nullptr;
  char *opt_default_auth = nullptr;
  char *opt_mysql_unix_port = nullptr;
  uint opt_mysql_port = 0;
  uint opt_zstd_compress_level = default_zstd_compression_level;
  char *opt_compress_algorithm = nullptr;
  char *load_default_groups[3];
  bool opt_compress = false;
  uint opt_enable_cleartext_plugin = 0;
  bool using_opt_enable_cleartext_plugin = false;
  char *current_host = nullptr;
  char *current_user = nullptr;
  char *opt_oci_config_file = nullptr;
  char *opt_authentication_oci_client_config_profile = nullptr;
  char *opt_authentication_openid_connect_client_id_token_file = nullptr;
  char mysql_charsets_dir[FN_REFLEN + 1];
  char *current_os_user = nullptr;
  char *current_os_sudouser = nullptr;
  my_option *my_long_options;
  bool (*get_one_option_user)(int optid, const struct my_option *opt,
                              char *argument) = nullptr;
  struct my_option my_common_options[256] = {
    {"help", '?', "Display this help and exit.", nullptr, nullptr, nullptr,
     GET_NO_ARG, NO_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"character-sets-dir", OPT_CHARSETS_DIR,
     "Directory for character set files.",
     reinterpret_cast<void *>(&charsets_dir),
     reinterpret_cast<void *>(&charsets_dir), nullptr, GET_STR, REQUIRED_ARG, 0,
     0, 0, nullptr, 0, nullptr},
    {"compress", 'C', "Use compression in server/client protocol.",
     &opt_compress, &opt_compress, nullptr, GET_BOOL, NO_ARG, 0, 0, 0, nullptr,
     0, nullptr},
#ifdef NDEBUG
    {"debug", '#', "This is a non-debug version. Catch this and exit.", nullptr,
     nullptr, nullptr, GET_DISABLED, OPT_ARG, 0, 0, 0, nullptr, 0, nullptr},
#else
    {"debug", '#', "Output debug log.",
     reinterpret_cast<void *>(&default_dbug_option),
     reinterpret_cast<void *>(&default_dbug_option), nullptr, GET_STR, OPT_ARG,
     0, 0, 0, nullptr, 0, nullptr},
#endif
    {"enable_cleartext_plugin", OPT_ENABLE_CLEARTEXT_PLUGIN,
     "Enable/disable the clear text authentication plugin.",
     &opt_enable_cleartext_plugin, &opt_enable_cleartext_plugin, nullptr,
     GET_BOOL, OPT_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"host", 'h', "Connect to host.", reinterpret_cast<void *>(&current_host),
     reinterpret_cast<void *>(&current_host), nullptr, GET_STR_ALLOC,
     REQUIRED_ARG, 0, 0, 0, nullptr, 0, nullptr},
#include "client/include/multi_factor_passwordopt-longopts.h"
#ifdef _WIN32
    {"pipe", 'W', "Use named pipes to connect to server.", nullptr, nullptr,
     nullptr, GET_NO_ARG, NO_ARG, 0, 0, 0, nullptr, 0, nullptr},
#endif
    {"port", 'P',
     "Port number to use for connection or 0 for default to, in "
     "order of preference, my.cnf, $MYSQL_TCP_PORT, "
#if MYSQL_PORT_DEFAULT == 0
     "/etc/services, "
#endif
     "built-in default (" STRINGIFY_ARG(MYSQL_PORT) ").",
     &opt_mysql_port, &opt_mysql_port, nullptr, GET_UINT, REQUIRED_ARG, 0, 0, 0,
     nullptr, 0, nullptr},
    {"protocol", OPT_MYSQL_PROTOCOL,
     "The protocol to use for connection (tcp, socket, pipe, memory).", nullptr,
     nullptr, nullptr, GET_STR, REQUIRED_ARG, 0, 0, 0, nullptr, 0, nullptr},
#if defined(_WIN32)
    {"shared-memory-base-name", OPT_SHARED_MEMORY_BASE_NAME,
     "Base name of shared memory.", &shared_memory_base_name,
     &shared_memory_base_name, nullptr, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0,
     nullptr, 0, nullptr},
#endif
    {"socket", 'S', "The socket file to use for connection.",
     reinterpret_cast<void *>(&opt_mysql_unix_port),
     reinterpret_cast<void *>(&opt_mysql_unix_port), nullptr, GET_STR_ALLOC,
     REQUIRED_ARG, 0, 0, 0, nullptr, 0, nullptr},
#include "client/include/caching_sha2_passwordopt-longopts.h"
#include "client/include/sslopt-longopts.h"

    {"user", 'u', "User for login if not current user.",
     reinterpret_cast<void *>(&current_user),
     reinterpret_cast<void *>(&current_user), nullptr, GET_STR_ALLOC,
     REQUIRED_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"version", 'V', "Output version information and exit.", nullptr, nullptr,
     nullptr, GET_NO_ARG, NO_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"plugin_dir", OPT_PLUGIN_DIR, "Directory for client-side plugins.",
     reinterpret_cast<void *>(&opt_plugin_dir),
     reinterpret_cast<void *>(&opt_plugin_dir), nullptr, GET_STR, REQUIRED_ARG,
     0, 0, 0, nullptr, 0, nullptr},
    {"default_auth", OPT_DEFAULT_AUTH,
     "Default authentication client-side plugin to use.",
     reinterpret_cast<void *>(&opt_default_auth),
     reinterpret_cast<void *>(&opt_default_auth), nullptr, GET_STR,
     REQUIRED_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"compression-algorithms", 0,
     "Use compression algorithm in server/client protocol. Valid values "
     "are any combination of 'zstd','zlib','uncompressed'.",
     reinterpret_cast<void *>(&opt_compress_algorithm),
     reinterpret_cast<void *>(&opt_compress_algorithm), nullptr, GET_STR,
     REQUIRED_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"zstd-compression-level", 0,
     "Use this compression level in the client/server protocol, in case "
     "--compression-algorithms=zstd. Valid range is between 1 and 22, "
     "inclusive. Default is 3.",
     &opt_zstd_compress_level, &opt_zstd_compress_level, nullptr, GET_UINT,
     REQUIRED_ARG, 3, 1, 22, nullptr, 0, nullptr},
    {"authentication-oci-client-config-profile", 0,
     "Specifies the configuration profile whose configuration options are to "
     "be read from the OCI configuration file. Default is DEFAULT.",
     reinterpret_cast<void *>(&opt_authentication_oci_client_config_profile),
     reinterpret_cast<void *>(&opt_authentication_oci_client_config_profile),
     nullptr, GET_STR, REQUIRED_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"oci-config-file", 0,
     "Specifies the location of the OCI configuration file. Default for "
     "Linux is ~/.oci/config and %HOME/.oci/config on Windows.",
     reinterpret_cast<void *>(&opt_oci_config_file),
     reinterpret_cast<void *>(&opt_oci_config_file), nullptr, GET_STR,
     REQUIRED_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"authentication-openid-connect-client-id-token-file", 0,
     "Specifies the location of the ID token file.",
     reinterpret_cast<void *>(
         &opt_authentication_openid_connect_client_id_token_file),
     reinterpret_cast<void *>(
         &opt_authentication_openid_connect_client_id_token_file),
     nullptr, GET_STR, REQUIRED_ARG, 0, 0, 0, nullptr, 0, nullptr},
#include "client/include/authentication_kerberos_clientopt-longopts.h"
#include "client/include/authentication_webauthn_clientopt-longopts.h"
    {nullptr, 0, nullptr, nullptr, nullptr, nullptr, GET_NO_ARG, NO_ARG, 0, 0,
     0, nullptr, 0, nullptr}
  };
};

Client_program_options *Client_program_options::create(
    const char *section_name, const char *copyright_start,
    const char *extra_args, const my_option *opts, size_t nopts,
    bool (*get_one_option_user_arg)(int optid, const struct my_option *opt,
                                    char *argument)) {
  return singleton = new Client_program_options_impl(
             section_name, copyright_start, extra_args, opts, nopts,
             get_one_option_user_arg);
}