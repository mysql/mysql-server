/*
  Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "mysqlrouter/router_openssl_export.h"

#include <memory>

#include "mysql/harness/plugin.h"
#include "mysql/harness/tls_context.h"

extern "C" {

std::unique_ptr<TlsLibraryContext> tls_library_context;

static void init(mysql_harness::PluginFuncEnv *) {
  // let the TlsLibraryContext constructor do the SSL initialization
  tls_library_context = std::make_unique<TlsLibraryContext>();
}

static void deinit(mysql_harness::PluginFuncEnv *) {
  // let the TlsLibraryContext destructor do the SSL cleanup
  tls_library_context.reset();
}

mysql_harness::Plugin ROUTER_OPENSSL_EXPORT harness_plugin_router_openssl = {
    mysql_harness::PLUGIN_ABI_VERSION,
    mysql_harness::ARCHITECTURE_DESCRIPTOR,
    "openssl init plugin",
    VERSION_NUMBER(0, 0, 1),
    // requires
    0,
    nullptr,
    // conflicts
    0,
    nullptr,
    init,     // init
    deinit,   // deinit
    nullptr,  // start
    nullptr,  // stop
    false,    // declare_readiness
    0,
    nullptr,
};
}
