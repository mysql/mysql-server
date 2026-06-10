/*
  Copyright (c) 2022, 2026, Oracle and/or its affiliates.

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

#include "my_compiler.h"

#include <string>
#include <string_view>
#include <vector>

#include "my_alloc.h"
#include "my_default.h"
#include "my_getopt.h"
#include "my_sys.h"
#include "tls_ciphers.h"
#include "typelib.h"

#include <mysql.h>

#include "tm_client_options.h"

/* Client options */

static const char *config_file = "my";

bool opt_help = false;

bool opt_trace_enabled = true;

const char *opt_otel_resource_attributes = nullptr;

const char *default_opt_otel_resource_attributes = "";

unsigned long opt_otel_log_level = OTEL_DEFAULT_LOG_LEVEL;

unsigned long opt_otel_exporter_otlp_traces_protocol = OTLP_DEFAULT_PROTOCOL;

const char *opt_otel_exporter_otlp_traces_endpoint = "";

const char *opt_otel_exporter_otlp_traces_certificates = nullptr;

const char *opt_otel_exporter_otlp_traces_client_key = nullptr;

const char *opt_otel_exporter_otlp_traces_client_certificates = nullptr;

unsigned long opt_otel_exporter_otlp_traces_min_tls = OTLP_TLS_DEFAULT;
unsigned long opt_otel_exporter_otlp_traces_max_tls = OTLP_TLS_DEFAULT;
const char *opt_otel_exporter_otlp_traces_cipher = default_tls12_ciphers;
const char *opt_otel_exporter_otlp_traces_cipher_suite = default_tls13_ciphers;

const char *opt_otel_exporter_otlp_traces_headers = nullptr;

unsigned long opt_otel_exporter_otlp_traces_compression =
    OTLP_DEFAULT_COMPRESSION;

long opt_otel_exporter_otlp_traces_timeout = 0;

long opt_otel_bsp_schedule_delay = 0;
long opt_otel_bsp_max_queue_size = 0;
long opt_otel_bsp_max_export_batch_size = 0;

static const char *telemetry_client_groups[] = {"telemetry_client", nullptr};
static MEM_ROOT argv_alloc{PSI_NOT_INSTRUMENTED, 512};

static const char *otlp_log_level_enums[] = {
    /* 0 */ "silent",
    /* 1 */ "error",
    /* 2 */ "warning",
    /* 3 */ "info",
    /* 4 */ "debug",
    /* EOF */ nullptr};

static TYPELIB otlp_log_level_typelib = {5, "otlp_log_level_typelib",
                                         otlp_log_level_enums, nullptr};

static const char *otlp_tls[] = {
    /* 0 */ "default", /* OTLP_TLS_DEFAULT */
    /* 1 */ "1.2",     /* OTLP_TLS_12 */
    /* 2 */ "1.3",     /* OTLP_TLS_13 */
    /* EOF */ nullptr};

static TYPELIB otlp_tls_typelib = {3, "otlp_tls", otlp_tls, nullptr};

static const char *otlp_traces_protocol_enums[] = {
    /* 0 */ "http/protobuf",
    /* 1 */ "http/json",
    /* EOF */ nullptr};

static TYPELIB otlp_traces_protocol_typelib = {
    2, "otlp_protocol_typelib", otlp_traces_protocol_enums, nullptr};

static const char *otlp_traces_compression[] = {
    /* 0 */ "none",
    /* 1 */ "gzip",
    /* EOF */ nullptr};

static TYPELIB otlp_traces_compression_typelib = {
    2, "otlp_traces_compression", otlp_traces_compression, nullptr};

static struct my_option my_long_options[] = {
    {"otel-help", 0, "Print help", &opt_help, &opt_help, nullptr, GET_BOOL,
     NO_ARG, 0, 0, 0, nullptr, 0, nullptr},

    {"otel-trace", 0, "With telemetry traces", &opt_trace_enabled,
     &opt_trace_enabled, nullptr, GET_BOOL, NO_ARG, 1, 0, 0, nullptr, 0,
     nullptr},

    {"otel-resource-attributes", 0,
     "Key-value pairs, in W3C Baggage format, to identify the MySQL client "
     "instance.",
     &opt_otel_resource_attributes, &default_opt_otel_resource_attributes,
     nullptr, GET_STR, REQUIRED_ARG, 0, 0, 0, nullptr, 0, nullptr},

    {"otel-log-level", 0,
     "telemetry log level: silent, error, warning, info or debug",
     &opt_otel_log_level, &opt_otel_log_level, &otlp_log_level_typelib,
     GET_ENUM, REQUIRED_ARG, OTEL_DEFAULT_LOG_LEVEL, OTEL_MIN_LOG_LEVEL,
     OTEL_MAX_LOG_LEVEL, nullptr, 0, nullptr},

    {"otel-exporter-otlp-traces-protocol", 0, "http/protobuf or http/json",
     &opt_otel_exporter_otlp_traces_protocol,
     &opt_otel_exporter_otlp_traces_protocol, &otlp_traces_protocol_typelib,
     GET_ENUM, REQUIRED_ARG, OTLP_DEFAULT_PROTOCOL, OTLP_MIN_PROTOCOL,
     OTLP_MAX_PROTOCOL, nullptr, 0, nullptr},

    {"otel-exporter-otlp-traces-endpoint", 0,
     "Target URL to send client telemetry spans to",
     &opt_otel_exporter_otlp_traces_endpoint,
     &opt_otel_exporter_otlp_traces_endpoint, nullptr, GET_STR, REQUIRED_ARG, 0,
     0, 0, nullptr, 0, nullptr},

    {"otel-exporter-otlp-traces-certificates", 0, "Path to SSL CA certificates",
     &opt_otel_exporter_otlp_traces_certificates,
     &opt_otel_exporter_otlp_traces_certificates, nullptr, GET_STR,
     REQUIRED_ARG, 0, 0, 0, nullptr, 0, nullptr},

    {"otel-exporter-otlp-traces-client-key", 0, "Path to SSL client key",
     &opt_otel_exporter_otlp_traces_client_key,
     &opt_otel_exporter_otlp_traces_client_key, nullptr, GET_STR, REQUIRED_ARG,
     0, 0, 0, nullptr, 0, nullptr},

    {"otel-exporter-otlp-traces-client-certificates", 0,
     "Path to SSL client certificates",
     &opt_otel_exporter_otlp_traces_client_certificates,
     &opt_otel_exporter_otlp_traces_client_certificates, nullptr, GET_STR,
     REQUIRED_ARG, 0, 0, 0, nullptr, 0, nullptr},

    {"otel-exporter-otlp-traces-min-tls", 0, "Minimum TLS version",
     &opt_otel_exporter_otlp_traces_min_tls,
     &opt_otel_exporter_otlp_traces_min_tls, &otlp_tls_typelib, GET_ENUM,
     REQUIRED_ARG, OTLP_TLS_DEFAULT, OTLP_MIN_TLS, OTLP_MAX_TLS, nullptr, 0,
     nullptr},

    {"otel-exporter-otlp-traces-max-tls", 0, "Maximum TLS version",
     &opt_otel_exporter_otlp_traces_max_tls,
     &opt_otel_exporter_otlp_traces_max_tls, &otlp_tls_typelib, GET_ENUM,
     REQUIRED_ARG, OTLP_TLS_DEFAULT, OTLP_MIN_TLS, OTLP_MAX_TLS, nullptr, 0,
     nullptr},

    {"otel-exporter-otlp-traces-cipher", 0, "TLS Cipher (for TLS 1.2)",
     &opt_otel_exporter_otlp_traces_cipher,
     &opt_otel_exporter_otlp_traces_cipher, nullptr, GET_STR, REQUIRED_ARG, 0,
     0, 0, nullptr, 0, nullptr},

    {"otel-exporter-otlp-traces-cipher-suite", 0, "TLS Cipher (for TLS 1.3)",
     &opt_otel_exporter_otlp_traces_cipher_suite,
     &opt_otel_exporter_otlp_traces_cipher_suite, nullptr, GET_STR,
     REQUIRED_ARG, 0, 0, 0, nullptr, 0, nullptr},

    {"otel-exporter-otlp-traces-headers", 0,
     "Key-value pairs as header associated with http requests",
     &opt_otel_exporter_otlp_traces_headers,
     &opt_otel_exporter_otlp_traces_headers, nullptr, GET_STR, REQUIRED_ARG, 0,
     0, 0, nullptr, 0, nullptr},

    {"otel-exporter-otlp-traces-compression", 0, "none or gzip",
     &opt_otel_exporter_otlp_traces_compression,
     &opt_otel_exporter_otlp_traces_compression,
     &otlp_traces_compression_typelib, GET_ENUM, REQUIRED_ARG,
     OTLP_DEFAULT_COMPRESSION, OTLP_MIN_COMPRESSION, OTLP_MAX_COMPRESSION,
     nullptr, 0, nullptr},

    {"otel-exporter-otlp-traces-timeout", 0,
     "Export trace timeout, in milliseconds",
     &opt_otel_exporter_otlp_traces_timeout,
     &opt_otel_exporter_otlp_traces_timeout, nullptr, GET_LONG, REQUIRED_ARG,
     10000 /* default 10 sec */, 1000 /* min 1 sec */, 300000 /* max 5 min */,
     nullptr, 0, nullptr},

    {"otel-bsp-schedule-delay", 0,
     "Delay interval between two consecutive exports, in milliseconds",
     &opt_otel_bsp_schedule_delay, &opt_otel_bsp_schedule_delay, nullptr,
     GET_LONG, REQUIRED_ARG, 5000 /* default 5 sec */, 1000 /* min 1 sec */,
     60000 /* max 60 sec */, nullptr, 0, nullptr},

    {"otel-bsp-max-queue-size", 0, "Maximum queue size",
     &opt_otel_bsp_max_queue_size, &opt_otel_bsp_max_queue_size, nullptr,
     GET_LONG, REQUIRED_ARG, 2048 /* default */, 128 /* min */, 32768 /* max */,
     nullptr, 0, nullptr},

    {"otel-bsp-max-export-batch-size", 0, "Maximum batch size",
     &opt_otel_bsp_max_export_batch_size, &opt_otel_bsp_max_export_batch_size,
     nullptr, GET_LONG, REQUIRED_ARG, 512 /* default */, 16 /* min */,
     2048 /* max */, nullptr, 0, nullptr},

    {nullptr, 0, nullptr, nullptr, nullptr, nullptr, GET_NO_ARG, NO_ARG, 0, 0,
     0, nullptr, 0, nullptr}};

/*
  For debug only, do not use in production.
*/
// #define DEBUG_OPTIONS

#ifdef DEBUG_OPTIONS
static void debug_dump_options(const char *msg, int argc, char **argv) {
  (void)fprintf(stderr, "Dumping options (%s)\n", msg);
  (void)fprintf(stderr, "argc = %d\n", argc);
  for (int i = 0; i < argc; i++) {
    (void)fprintf(stderr, "argv[%d] = %s\n", i, argv[i]);
  }
}
#endif

/*
 * MAINTAINER:
 * Please also check file:
 * internal/components/telemetry/tm_system_variables.cc
 */
static int validate_tls_cipher(const char *actual, const char *supported,
                               std::vector<std::string> &all_illegal) {
  int found = 0;

  all_illegal.clear();

  /* "FOO", "FOO:BAR" */
  std::string ciphers{actual};

  /* ":AAA:BBB:CCC:" */
  std::string haystack{":"};
  haystack.append(supported);
  haystack.append(":");

  /* "FOO", "BAR" */
  std::string needle;
  /* ":FOO:", ":BAR:" */
  std::string delimited_needle;

  std::string::size_type delim;

  while (!ciphers.empty()) {
    delim = ciphers.find(':');

    if (delim != std::string::npos) {
      needle = ciphers.substr(0, delim);
      ciphers = ciphers.substr(delim + 1);
    } else {
      needle = ciphers;
      ciphers.clear();
    }

    delimited_needle = ":";
    delimited_needle.append(needle);
    delimited_needle.append(":");

    /*
     * IMPORTANT:
     * We search for delimited needle ":XXX-YYY:"
     * inside delimited haystack ":AAA-BBB-CCC:XXX-YYY-ZZZ:",
     * because we do not want to be confused by "XXX-YYY" found in
     * "XXX-YYY-ZZZ".
     */
    if (haystack.find(delimited_needle) == std::string::npos) {
      all_illegal.push_back(needle);
    } else {
      found++;
    }
  }

  if (found == 0) {
    all_illegal.emplace_back("(empty)");
  }

  if (!all_illegal.empty()) {
    return 1;
  }

  return 0;
}

static int validate_client_options() {
  const char *option_name;
  const char *actual;
  const char *supported;
  std::vector<std::string> all_illegal;
  int rc;

  option_name = "otel-exporter-otlp-traces-cipher";
  actual = opt_otel_exporter_otlp_traces_cipher;
  supported = default_tls12_ciphers;

  rc = validate_tls_cipher(actual, supported, all_illegal);
  if (rc != 0) {
    (void)fprintf(stderr,
                  "Illegal option value found for %s\n"
                  "  - actual = <%s>\n"
                  "  - supported = <%s>\n",
                  option_name, actual, supported);
    for (const std::string &illegal : all_illegal) {
      (void)fprintf(stderr, "  - illegal = <%s>\n", illegal.c_str());
    }

    return rc;
  }

  option_name = "otel-exporter-otlp-traces-cipher-suite";
  actual = opt_otel_exporter_otlp_traces_cipher_suite;
  supported = default_tls13_ciphers;

  rc = validate_tls_cipher(actual, supported, all_illegal);
  if (rc != 0) {
    (void)fprintf(stderr,
                  "Illegal option value found for %s\n"
                  "  - actual = <%s>\n"
                  "  - supported = <%s>\n",
                  option_name, actual, supported);
    for (const std::string &illegal : all_illegal) {
      (void)fprintf(stderr, "  - illegal = <%s>\n", illegal.c_str());
    }

    return rc;
  }

  return 0;
}

int register_client_options(const char *defaults_file,
                            const char *defaults_group_suffix,
                            const char *defaults_extra_file,
                            int *client_argc_ptr, char **client_argv) {
#ifdef DEBUG_OPTIONS
  debug_dump_options("CLIENT INPUT", *client_argc_ptr, client_argv);
#endif

  const int client_argc = *client_argc_ptr;
  bool save_my_getopt_skip_unknown;

  const int fake_argv_size = 4 + client_argc;
  std::unique_ptr<char *[]> const fake_argv(new char *[fake_argv_size]);

  int argc = 0;
  char **argv = fake_argv.get();
  char fake_program[] = {'m', 'y', 's', 'q', 'l', '\0'};

  std::string option_defaults_file;
  std::string option_defaults_group_suffix;
  std::string option_defaults_extra_file;

  // TODO: investigate, link with my_sys from a shared lib.
  my_init();

  argv[argc] = fake_program;
  argc++;

  if (defaults_file != nullptr) {
    option_defaults_file = "--defaults-file=";
    option_defaults_file += defaults_file;

    argv[argc] = option_defaults_file.data();
    argc++;
  }

  if (defaults_group_suffix != nullptr) {
    option_defaults_group_suffix = "--defaults-group-suffix=";
    option_defaults_group_suffix += defaults_group_suffix;

    argv[argc] = option_defaults_group_suffix.data();
    argc++;
  }

  if (defaults_extra_file != nullptr) {
    option_defaults_extra_file = "--defaults-extra-file=";
    option_defaults_extra_file += defaults_extra_file;

    argv[argc] = option_defaults_extra_file.data();
    argc++;
  }

  /*
    Append unconsumed input client line arguments to the fake command line
  */
  if (client_argc > 0) {
    for (int i = 0; i < client_argc; i++) {
      argv[argc] = client_argv[i];
      argc++;
    }
  }

#ifdef DEBUG_OPTIONS
  debug_dump_options("COMMAND LINE 1", argc, argv);
#endif

  int rc = load_defaults(config_file, telemetry_client_groups, &argc, &argv,
                         &argv_alloc);
  if (rc != 0) {
    (void)fprintf(stderr,
                  "telemetry_client: Failed to read configuration file.\n");
    opt_trace_enabled = false;
    return 1;
  }

#ifdef DEBUG_OPTIONS
  debug_dump_options("COMMAND LINE 2", argc, argv);
#endif

  /*
    Ignore unknown options, they are to be returned to the client.
  */
  save_my_getopt_skip_unknown = my_getopt_skip_unknown;
  my_getopt_skip_unknown = true;
  rc = handle_options(&argc, &argv, my_long_options, nullptr);
  my_getopt_skip_unknown = save_my_getopt_skip_unknown;

#ifdef DEBUG_OPTIONS
  debug_dump_options("COMMAND LINE 3", argc, argv);
#endif

  if (rc != 0) {
    (void)fprintf(stderr,
                  "telemetry_client: Failed to read configuration options.\n");
    opt_trace_enabled = false;
    return 2;
  }

  rc = validate_client_options();

  if (rc != 0) {
    (void)fprintf(
        stderr,
        "telemetry_client: Failed to validate configuration options.\n");
    opt_trace_enabled = false;
    return 2;
  }

  if (opt_help) {
    (void)fprintf(stderr, "=== TELEMETRY_CLIENT PLUGIN VARIABLES ===\n");
    my_print_variables(my_long_options);
    (void)fprintf(stderr, "\n");
  }

  /*
    Return unconsumed options back to the client, as output.
  */
  if (client_argc > 0) {
    for (int i = 0; i < argc; i++) {
      client_argv[i] = argv[i];
    }

    *client_argc_ptr = argc;
  }

#ifdef DEBUG_OPTIONS
  debug_dump_options("CLIENT OUTPUT", *client_argc_ptr, client_argv);
#endif

  return 0;
}
