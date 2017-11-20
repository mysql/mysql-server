/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "xprotocol_plugin.h"

#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/compiler/plugin.h>


int main(int argc, char* argv[]) {
  Chain_file_output xprotocol_tags("xprotocol_tags.h");
  XProtocol_plugin  xprotocol_plugin(&xprotocol_tags);

  return google::protobuf::compiler::PluginMain(argc, argv, &xprotocol_plugin);
}
