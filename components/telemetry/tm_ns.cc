/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#include <cstring>

// For HAVE_SETNS
#include "my_config.h"

#ifdef HAVE_SETNS
#include <fcntl.h>
#include <sched.h>
#include <unistd.h>
#include <climits>
#endif

#include "tm_global.h"
#include "tm_log.h"
#include "tm_ns.h"
#include "tm_system_variables.h"

namespace telemetry {

#ifdef HAVE_SETNS

// See open_network_namespace() in sql-common/net_ns.cc
// Note that /var/run is deprecated, use /run instead.
// See https://lwn.net/Articles/436012/
int open_network_namespace(const std::string &network_namespace, int *fd) {
  char path_to_ns_file[PATH_MAX];
  if (std::strstr(network_namespace.c_str(), "..") != nullptr) {
    // This is a security precaution.
    // We do not allow names like "../../peek/at/other/file",
    // which would escape outside of the /run/netns namespace,
    // and cause a regular arbitrary file to be opened.
    log_error("%s: Network namespace name invalid '%s'", component_name,
              network_namespace.c_str());
    *fd = NO_FD;
    return 1;
  }
  int requested_len = snprintf(path_to_ns_file, sizeof(path_to_ns_file),
                               "/run/netns/%s", network_namespace.c_str());
  if (requested_len + 1 > PATH_MAX) {
    log_error("%s: Network namespace name too long '%s'", component_name,
              network_namespace.c_str());
    *fd = NO_FD;
    return 1;
  }

  *fd = open(path_to_ns_file, O_RDONLY);

  if (*fd == -1) {
    char errbuf[PATH_MAX];
    const char *errmsg;
    errmsg = strerror_r(errno, errbuf, sizeof(errbuf));
    log_error("%s: Failed to open file '%s': %s", component_name,
              path_to_ns_file, errmsg);
    *fd = NO_FD;
    return 1;
  }

  log_info("%s: Opened network namespace '%s' as fd %d", component_name,
           network_namespace.c_str(), *fd);
  return 0;
}

// See set_network_namespace() in sql-common/net_ns.cc
int set_network_namespace(int fd) {
  if (setns(fd, CLONE_NEWNET) != 0) {
    char errbuf[PATH_MAX];
    const char *errmsg;
    errmsg = strerror_r(errno, errbuf, sizeof(errbuf));
    log_error("%s: setns failed for fd %d: %s", component_name, fd, errmsg);
    return 1;
  }

  log_info("%s: Set network namespace to fd %d", component_name, fd);
  return 0;
}

int close_network_namespace(int fd) {
  if (close(fd) != 0) {
    char errbuf[PATH_MAX];
    const char *errmsg;
    errmsg = strerror_r(errno, errbuf, sizeof(errbuf));
    log_error("%s: close network namespace failed for fd %d: %s",
              component_name, fd, errmsg);
    return 1;
  }

  log_info("%s: closed network namespace fd %d", component_name, fd);
  return 0;
}

#endif /* HAVE_SETNS */

int setup_network_namespaces() {
  std::string name;

  g_traces_network_namespace = NO_FD;
  g_metrics_network_namespace = NO_FD;
  g_logs_network_namespace = NO_FD;

  name = sv_otel_exporter_otlp_traces_network_namespace;
  if (!name.empty()) {
#ifdef HAVE_SETNS
    if (open_network_namespace(name, &g_traces_network_namespace) != 0) {
      return 1;
    }
#else
    log_warning("%s: traces network namespace not supported", component_name);
#endif /* HAVE_SETNS */
  }

  name = sv_otel_exporter_otlp_metrics_network_namespace;
  if (!name.empty()) {
#ifdef HAVE_SETNS
    if (open_network_namespace(name, &g_metrics_network_namespace) != 0) {
      return 1;
    }
#else
    log_warning("%s: metrics network namespace not supported", component_name);
#endif /* HAVE_SETNS */
  }

  name = sv_otel_exporter_otlp_logs_network_namespace;
  if (!name.empty()) {
#ifdef HAVE_SETNS
    if (open_network_namespace(name, &g_logs_network_namespace) != 0) {
      return 1;
    }
#else
    log_warning("%s: logs network namespace not supported", component_name);
#endif /* HAVE_SETNS */
  }

  return 0;
}

int cleanup_network_namespaces() {
#ifdef HAVE_SETNS
  if (g_traces_network_namespace != NO_FD) {
    close_network_namespace(g_traces_network_namespace);
    g_traces_network_namespace = NO_FD;
  }

  if (g_metrics_network_namespace != NO_FD) {
    close_network_namespace(g_metrics_network_namespace);
    g_metrics_network_namespace = NO_FD;
  }

  if (g_logs_network_namespace != NO_FD) {
    close_network_namespace(g_logs_network_namespace);
    g_logs_network_namespace = NO_FD;
  }
#endif /* HAVE_SETNS */

  return 0;
}

}  // namespace telemetry
