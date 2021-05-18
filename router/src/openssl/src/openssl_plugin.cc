/*
  Copyright (c) 2020, 2021, Oracle and/or its affiliates.

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

#include <openssl/conf.h>
#include <openssl/engine.h>
#include <openssl/err.h>
#include <openssl/opensslv.h>
#include <openssl/ssl.h>

#include "mysql/harness/plugin.h"

extern "C" {

static void init(mysql_harness::PluginFuncEnv *) {
#if OPENSSL_VERSION_NUMBER < 0x10100000L
  SSL_library_init();
#else
  OPENSSL_init_ssl(0, nullptr);
#endif
  SSL_load_error_strings();
  ERR_load_crypto_strings();
}

static void deinit(mysql_harness::PluginFuncEnv *) {
  // in case any of this is needed for cleanup
#if 0
  FIPS_mode_set(0);
  CRYPTO_set_locking_callback(nullptr);
  CRYPTO_set_id_callback(nullptr);

  SSL_COMP_free_compression_methods();

  ENGINE_cleanup();

  CONF_modules_free();
  CONF_modules_unload(1);

  COMP_zlib_cleanup();

  ERR_free_strings();
  EVP_cleanup();

  CRYPTO_cleanup_all_ex_data();
#endif
}

mysql_harness::Plugin ROUTER_OPENSSL_EXPORT harness_plugin_router_openssl = {
    mysql_harness::PLUGIN_ABI_VERSION, mysql_harness::ARCHITECTURE_DESCRIPTOR,
    "openssl init plugin", VERSION_NUMBER(0, 0, 1),
    // requires
    0, nullptr,
    // conflicts
    0, nullptr,
    init,     // init
    deinit,   // deinit
    nullptr,  // start
    nullptr,  // stop
    false     // declare_readiness
};
}
