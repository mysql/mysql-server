/*
  Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "mysqlrouter/router_protobuf_export.h"

#include "my_compiler.h"
#include "scope_guard.h"

MY_COMPILER_DIAGNOSTIC_PUSH()
MY_COMPILER_CLANG_DIAGNOSTIC_IGNORE("-Wdeprecated-dynamic-exception-spec")
MY_COMPILER_CLANG_DIAGNOSTIC_IGNORE(
    "-Winconsistent-missing-destructor-override")
#include <google/protobuf/message_lite.h>  // ShutdownProtobufLibrary()
MY_COMPILER_DIAGNOSTIC_POP()

#include "mysql/harness/plugin.h"

// deinit the protobuf-library on
//
// - deinit of this plugin and,
// - unload of this plugin
static Scope_guard static_guard([]() {
  google::protobuf::ShutdownProtobufLibrary();
});

extern "C" {

mysql_harness::Plugin ROUTER_PROTOBUF_EXPORT harness_plugin_router_protobuf = {
    mysql_harness::PLUGIN_ABI_VERSION, mysql_harness::ARCHITECTURE_DESCRIPTOR,
    "", VERSION_NUMBER(0, 0, 1),
    // requires
    0, nullptr,
    // conflicts
    0, nullptr,
    nullptr,  // init
    [](mysql_harness::PluginFuncEnv *) { static_guard.reset(); },
    nullptr,  // start
    nullptr,  // stop
    false,    // declare_readiness
    0, nullptr,
    nullptr,  // expose_configuration
};
}
